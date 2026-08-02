/*
 * libEtPan! -- a mail stuff library
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailgmail_url.h"

#include "mailgmail_private.h"

#include <libetpan/mmapstring.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAILGMAIL_SERVICE_ROOT "https://gmail.googleapis.com/gmail/v1"

static int append_url_escaped(MMAPString * buffer, const char * value)
{
  static const char hex[] = "0123456789ABCDEF";
  const unsigned char * cur;

  if ((buffer == NULL) || (value == NULL))
    return MAILGMAIL_ERROR_BAD_STATE;

  for (cur = (const unsigned char *) value; * cur != '\0'; cur ++) {
    if (isalnum(* cur) || (* cur == '-') || (* cur == '_') ||
        (* cur == '.') || (* cur == '~')) {
      if (mmap_string_append_c(buffer, (char) * cur) == NULL)
        return MAILGMAIL_ERROR_MEMORY;
    }
    else {
      char escaped[4];

      escaped[0] = '%';
      escaped[1] = hex[* cur >> 4];
      escaped[2] = hex[* cur & 0x0F];
      escaped[3] = '\0';
      if (mmap_string_append(buffer, escaped) == NULL)
        return MAILGMAIL_ERROR_MEMORY;
    }
  }

  return MAILGMAIL_NO_ERROR;
}

static int append_user_path(MMAPString * buffer, mailgmail * session)
{
  const char * user_id;
  int r;

  if ((buffer == NULL) || (session == NULL))
    return MAILGMAIL_ERROR_BAD_STATE;

  user_id = session->gmail_user_id != NULL ? session->gmail_user_id : "me";
  if (mmap_string_append(buffer, MAILGMAIL_SERVICE_ROOT "/users/") == NULL)
    return MAILGMAIL_ERROR_MEMORY;

  r = append_url_escaped(buffer, user_id);
  if (r != MAILGMAIL_NO_ERROR)
    return r;

  return MAILGMAIL_NO_ERROR;
}

static int append_query_separator(MMAPString * buffer, int * has_query)
{
  if ((buffer == NULL) || (has_query == NULL))
    return MAILGMAIL_ERROR_BAD_STATE;

  if (mmap_string_append_c(buffer, * has_query ? '&' : '?') == NULL)
    return MAILGMAIL_ERROR_MEMORY;
  * has_query = 1;
  return MAILGMAIL_NO_ERROR;
}

static int append_query_string(MMAPString * buffer, int * has_query,
    const char * name, const char * value)
{
  int r;

  if ((name == NULL) || (value == NULL))
    return MAILGMAIL_ERROR_BAD_STATE;

  r = append_query_separator(buffer, has_query);
  if (r != MAILGMAIL_NO_ERROR)
    return r;

  if (mmap_string_append(buffer, name) == NULL)
    return MAILGMAIL_ERROR_MEMORY;
  if (mmap_string_append_c(buffer, '=') == NULL)
    return MAILGMAIL_ERROR_MEMORY;
  return append_url_escaped(buffer, value);
}

static int append_query_uint32(MMAPString * buffer, int * has_query,
    const char * name, uint32_t value)
{
  char text[32];

  snprintf(text, sizeof(text), "%lu", (unsigned long) value);
  return append_query_string(buffer, has_query, name, text);
}

static int append_query_bool(MMAPString * buffer, int * has_query,
    const char * name, int value)
{
  return append_query_string(buffer, has_query, name, value ? "true" : "false");
}

static int finish_url(MMAPString * buffer, char ** result)
{
  char * url;

  if ((buffer == NULL) || (result == NULL))
    return MAILGMAIL_ERROR_BAD_STATE;

  url = strdup(buffer->str);
  mmap_string_free(buffer);
  if (url == NULL)
    return MAILGMAIL_ERROR_MEMORY;

  * result = url;
  return MAILGMAIL_NO_ERROR;
}

int mailgmail_url_messages_list(mailgmail * session,
    struct mailgmail_message_list_request * request,
    char ** result)
{
  MMAPString * buffer;
  clistiter * cur;
  int has_query;
  int r;

  if ((session == NULL) || (result == NULL))
    return MAILGMAIL_ERROR_BAD_STATE;

  * result = NULL;
  buffer = mmap_string_sized_new(256);
  if (buffer == NULL)
    return MAILGMAIL_ERROR_MEMORY;

  r = append_user_path(buffer, session);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  if (mmap_string_append(buffer, "/messages") == NULL) {
    r = MAILGMAIL_ERROR_MEMORY;
    goto err;
  }

  has_query = 0;
  if (request != NULL) {
    if (request->max_results > 500) {
      r = MAILGMAIL_ERROR_BAD_STATE;
      goto err;
    }
    if (request->max_results != 0) {
      r = append_query_uint32(buffer, &has_query, "maxResults",
          request->max_results);
      if (r != MAILGMAIL_NO_ERROR)
        goto err;
    }
    if (request->page_token != NULL) {
      r = append_query_string(buffer, &has_query, "pageToken",
          request->page_token);
      if (r != MAILGMAIL_NO_ERROR)
        goto err;
    }
    if (request->query != NULL) {
      r = append_query_string(buffer, &has_query, "q", request->query);
      if (r != MAILGMAIL_NO_ERROR)
        goto err;
    }
    for (cur = request->label_ids != NULL ? clist_begin(request->label_ids) :
        NULL; cur != NULL; cur = clist_next(cur)) {
      r = append_query_string(buffer, &has_query, "labelIds",
          clist_content(cur));
      if (r != MAILGMAIL_NO_ERROR)
        goto err;
    }
    if (request->include_spam_trash) {
      r = append_query_bool(buffer, &has_query, "includeSpamTrash",
          request->include_spam_trash);
      if (r != MAILGMAIL_NO_ERROR)
        goto err;
    }
  }

  return finish_url(buffer, result);

 err:
  mmap_string_free(buffer);
  return r;
}

static const char * format_name(enum mailgmail_message_format format)
{
  switch (format) {
  case MAILGMAIL_MESSAGE_FORMAT_METADATA:
    return "metadata";
  case MAILGMAIL_MESSAGE_FORMAT_MINIMAL:
    return "minimal";
  case MAILGMAIL_MESSAGE_FORMAT_RAW:
    return "raw";
  case MAILGMAIL_MESSAGE_FORMAT_FULL:
  default:
    return "full";
  }
}

int mailgmail_url_message_get(mailgmail * session,
    const char * message_id,
    struct mailgmail_message_get_request * request,
    char ** result)
{
  MMAPString * buffer;
  clistiter * cur;
  int has_query;
  int r;

  if ((session == NULL) || (message_id == NULL) || (result == NULL))
    return MAILGMAIL_ERROR_BAD_STATE;

  * result = NULL;
  buffer = mmap_string_sized_new(256);
  if (buffer == NULL)
    return MAILGMAIL_ERROR_MEMORY;

  r = append_user_path(buffer, session);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  if (mmap_string_append(buffer, "/messages/") == NULL) {
    r = MAILGMAIL_ERROR_MEMORY;
    goto err;
  }
  r = append_url_escaped(buffer, message_id);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;

  has_query = 0;
  if ((request != NULL) &&
      (request->format != MAILGMAIL_MESSAGE_FORMAT_FULL) &&
      (request->format != MAILGMAIL_MESSAGE_FORMAT_METADATA) &&
      (request->format != MAILGMAIL_MESSAGE_FORMAT_MINIMAL) &&
      (request->format != MAILGMAIL_MESSAGE_FORMAT_RAW)) {
    r = MAILGMAIL_ERROR_BAD_STATE;
    goto err;
  }
  r = append_query_string(buffer, &has_query, "format",
      request != NULL ? format_name(request->format) : "full");
  if (r != MAILGMAIL_NO_ERROR)
    goto err;

  if ((request != NULL) &&
      (request->format == MAILGMAIL_MESSAGE_FORMAT_METADATA)) {
    for (cur = request->metadata_headers != NULL ?
        clist_begin(request->metadata_headers) : NULL; cur != NULL;
        cur = clist_next(cur)) {
      r = append_query_string(buffer, &has_query, "metadataHeaders",
          clist_content(cur));
      if (r != MAILGMAIL_NO_ERROR)
        goto err;
    }
  }

  return finish_url(buffer, result);

 err:
  mmap_string_free(buffer);
  return r;
}

int mailgmail_url_labels_list(mailgmail * session, char ** result)
{
  MMAPString * buffer;
  int r;

  if ((session == NULL) || (result == NULL))
    return MAILGMAIL_ERROR_BAD_STATE;

  * result = NULL;
  buffer = mmap_string_sized_new(256);
  if (buffer == NULL)
    return MAILGMAIL_ERROR_MEMORY;

  r = append_user_path(buffer, session);
  if (r != MAILGMAIL_NO_ERROR) {
    mmap_string_free(buffer);
    return r;
  }
  if (mmap_string_append(buffer, "/labels") == NULL) {
    mmap_string_free(buffer);
    return MAILGMAIL_ERROR_MEMORY;
  }

  return finish_url(buffer, result);
}

int mailgmail_url_label_get(mailgmail * session,
    const char * label_id,
    char ** result)
{
  MMAPString * buffer;
  int r;

  if ((session == NULL) || (label_id == NULL) || (result == NULL))
    return MAILGMAIL_ERROR_BAD_STATE;

  * result = NULL;
  buffer = mmap_string_sized_new(256);
  if (buffer == NULL)
    return MAILGMAIL_ERROR_MEMORY;

  r = append_user_path(buffer, session);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  if (mmap_string_append(buffer, "/labels/") == NULL) {
    r = MAILGMAIL_ERROR_MEMORY;
    goto err;
  }
  r = append_url_escaped(buffer, label_id);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;

  return finish_url(buffer, result);

 err:
  mmap_string_free(buffer);
  return r;
}

int mailgmail_url_profile_get(mailgmail * session, char ** result)
{
  MMAPString * buffer;
  int r;

  if ((session == NULL) || (result == NULL))
    return MAILGMAIL_ERROR_BAD_STATE;

  * result = NULL;
  buffer = mmap_string_sized_new(256);
  if (buffer == NULL)
    return MAILGMAIL_ERROR_MEMORY;

  r = append_user_path(buffer, session);
  if (r != MAILGMAIL_NO_ERROR) {
    mmap_string_free(buffer);
    return r;
  }
  if (mmap_string_append(buffer, "/profile") == NULL) {
    mmap_string_free(buffer);
    return MAILGMAIL_ERROR_MEMORY;
  }

  return finish_url(buffer, result);
}

int mailgmail_url_attachment_get(mailgmail * session,
    const char * message_id,
    const char * attachment_id,
    char ** result)
{
  MMAPString * buffer;
  int r;

  if ((session == NULL) || (message_id == NULL) ||
      (attachment_id == NULL) || (result == NULL))
    return MAILGMAIL_ERROR_BAD_STATE;

  * result = NULL;
  buffer = mmap_string_sized_new(256);
  if (buffer == NULL)
    return MAILGMAIL_ERROR_MEMORY;

  r = append_user_path(buffer, session);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  if (mmap_string_append(buffer, "/messages/") == NULL) {
    r = MAILGMAIL_ERROR_MEMORY;
    goto err;
  }
  r = append_url_escaped(buffer, message_id);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  if (mmap_string_append(buffer, "/attachments/") == NULL) {
    r = MAILGMAIL_ERROR_MEMORY;
    goto err;
  }
  r = append_url_escaped(buffer, attachment_id);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;

  return finish_url(buffer, result);

 err:
  mmap_string_free(buffer);
  return r;
}
