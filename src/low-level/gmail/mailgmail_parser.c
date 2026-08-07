/*
 * libEtPan! -- a mail stuff library
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailgmail_parser.h"

#include <libetpan/mailjson.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>

static int gmail_json_error(int r)
{
  switch (r) {
  case MAILJSON_NO_ERROR:
    return MAILGMAIL_NO_ERROR;
  case MAILJSON_ERROR_PARSE:
    return MAILGMAIL_ERROR_PARSE;
  case MAILJSON_ERROR_MEMORY:
    return MAILGMAIL_ERROR_MEMORY;
  default:
    return MAILGMAIL_ERROR_BAD_STATE;
  }
}

static int parse_root(const char * data, size_t len, mailjson_value ** result)
{
  int r;

  r = mailjson_parse(data, len, result);
  if (r != MAILJSON_NO_ERROR)
    return gmail_json_error(r);
  if (!mailjson_is_object(* result)) {
    mailjson_free(* result);
    * result = NULL;
    return MAILGMAIL_ERROR_PARSE;
  }

  return MAILGMAIL_NO_ERROR;
}

static int get_object(mailjson_value * object, const char * key,
    mailjson_value ** result)
{
  int r;

  r = mailjson_object_get(object, key, result);
  return gmail_json_error(r);
}

static int string_for_key(mailjson_value * object, const char * key,
    char ** result)
{
  int r;

  r = mailjson_object_get_string_dup(object, key, result);
  return gmail_json_error(r);
}

static int uint32_for_key(mailjson_value * object, const char * key,
    uint32_t * result)
{
  mailjson_value * value;
  int64_t number;
  int r;

  value = NULL;
  r = get_object(object, key, &value);
  if (r != MAILGMAIL_NO_ERROR)
    return r;
  if (value == NULL)
    return MAILGMAIL_NO_ERROR;
  if (!mailjson_is_integer(value)) {
    mailjson_free(value);
    return MAILGMAIL_ERROR_PARSE;
  }

  r = gmail_json_error(mailjson_integer_value(value, &number));
  mailjson_free(value);
  if (r != MAILGMAIL_NO_ERROR)
    return r;
  if ((number < 0) || (number > UINT32_MAX))
    return MAILGMAIL_ERROR_PARSE;

  * result = (uint32_t) number;
  return MAILGMAIL_NO_ERROR;
}

static int add_string_to_list(clist * list, const char * value)
{
  char * copy;

  copy = strdup(value);
  if (copy == NULL)
    return MAILGMAIL_ERROR_MEMORY;

  if (clist_append(list, copy) < 0) {
    free(copy);
    return MAILGMAIL_ERROR_MEMORY;
  }

  return MAILGMAIL_NO_ERROR;
}

static int string_array_for_key(mailjson_value * object, const char * key,
    clist * result)
{
  mailjson_value * array;
  size_t count;
  size_t i;
  int r;

  array = NULL;
  r = get_object(object, key, &array);
  if (r != MAILGMAIL_NO_ERROR)
    return r;
  if (array == NULL)
    return MAILGMAIL_NO_ERROR;
  if (!mailjson_is_array(array)) {
    mailjson_free(array);
    return MAILGMAIL_ERROR_PARSE;
  }

  count = mailjson_array_size(array);
  for (i = 0; i < count; i ++) {
    mailjson_value * value;
    char * text;

    value = NULL;
    r = gmail_json_error(mailjson_array_get(array, i, &value));
    if (r != MAILGMAIL_NO_ERROR)
      goto cleanup;
    if ((value == NULL) || !mailjson_is_string(value)) {
      mailjson_free(value);
      r = MAILGMAIL_ERROR_PARSE;
      goto cleanup;
    }

    text = NULL;
    r = gmail_json_error(mailjson_string_dup(value, &text));
    mailjson_free(value);
    if (r != MAILGMAIL_NO_ERROR)
      goto cleanup;
    r = add_string_to_list(result, text);
    free(text);
    if (r != MAILGMAIL_NO_ERROR)
      goto cleanup;
  }

 cleanup:
  mailjson_free(array);
  return r;
}

static int parse_label_value(mailjson_value * value,
    struct mailgmail_label ** result)
{
  struct mailgmail_label * label;
  int r;

  if (!mailjson_is_object(value))
    return MAILGMAIL_ERROR_PARSE;

  label = mailgmail_label_new();
  if (label == NULL)
    return MAILGMAIL_ERROR_MEMORY;

  r = string_for_key(value, "id", &label->id);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  r = string_for_key(value, "name", &label->name);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  r = string_for_key(value, "type", &label->type);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  r = string_for_key(value, "messageListVisibility",
      &label->message_list_visibility);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  r = string_for_key(value, "labelListVisibility",
      &label->label_list_visibility);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  r = uint32_for_key(value, "messagesTotal", &label->messages_total);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  r = uint32_for_key(value, "messagesUnread", &label->messages_unread);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  r = uint32_for_key(value, "threadsTotal", &label->threads_total);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  r = uint32_for_key(value, "threadsUnread", &label->threads_unread);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;

  * result = label;
  return MAILGMAIL_NO_ERROR;

 err:
  mailgmail_label_free(label);
  return r;
}

int mailgmail_parser_parse_error_message(const char * data, size_t len,
    char ** result)
{
  mailjson_value * root;
  int r;

  if ((data == NULL) || (result == NULL))
    return MAILGMAIL_ERROR_BAD_STATE;

  * result = NULL;
  root = NULL;
  r = parse_root(data, len, &root);
  if (r != MAILGMAIL_NO_ERROR)
    return r;
  r = string_for_key(root, "message", result);
  mailjson_free(root);
  return r;
}

int mailgmail_parser_parse_profile(const char * data, size_t len,
    struct mailgmail_profile ** result)
{
  mailjson_value * root;
  struct mailgmail_profile * profile;
  int r;

  if ((data == NULL) || (result == NULL))
    return MAILGMAIL_ERROR_BAD_STATE;

  * result = NULL;
  root = NULL;
  r = parse_root(data, len, &root);
  if (r != MAILGMAIL_NO_ERROR)
    return r;

  profile = mailgmail_profile_new();
  if (profile == NULL) {
    mailjson_free(root);
    return MAILGMAIL_ERROR_MEMORY;
  }

  r = string_for_key(root, "emailAddress", &profile->email_address);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  r = uint32_for_key(root, "messagesTotal", &profile->messages_total);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  r = uint32_for_key(root, "threadsTotal", &profile->threads_total);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  r = string_for_key(root, "historyId", &profile->history_id);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;

  * result = profile;
  mailjson_free(root);
  return MAILGMAIL_NO_ERROR;

 err:
  mailgmail_profile_free(profile);
  mailjson_free(root);
  return r;
}

int mailgmail_parser_parse_label(const char * data, size_t len,
    struct mailgmail_label ** result)
{
  mailjson_value * root;
  int r;

  if ((data == NULL) || (result == NULL))
    return MAILGMAIL_ERROR_BAD_STATE;

  * result = NULL;
  root = NULL;
  r = parse_root(data, len, &root);
  if (r != MAILGMAIL_NO_ERROR)
    return r;
  r = parse_label_value(root, result);
  mailjson_free(root);
  return r;
}

int mailgmail_parser_parse_label_list(const char * data, size_t len,
    struct mailgmail_label_list ** result)
{
  mailjson_value * root;
  mailjson_value * labels;
  struct mailgmail_label_list * label_list;
  size_t count;
  size_t i;
  int r;

  if ((data == NULL) || (result == NULL))
    return MAILGMAIL_ERROR_BAD_STATE;

  * result = NULL;
  root = NULL;
  r = parse_root(data, len, &root);
  if (r != MAILGMAIL_NO_ERROR)
    return r;

  labels = NULL;
  r = get_object(root, "labels", &labels);
  if (r != MAILGMAIL_NO_ERROR)
    goto cleanup_root;
  if ((labels == NULL) || !mailjson_is_array(labels)) {
    r = MAILGMAIL_ERROR_PARSE;
    goto cleanup_labels;
  }

  label_list = mailgmail_label_list_new();
  if (label_list == NULL) {
    r = MAILGMAIL_ERROR_MEMORY;
    goto cleanup_labels;
  }

  count = mailjson_array_size(labels);
  for (i = 0; i < count; i ++) {
    mailjson_value * value;
    struct mailgmail_label * label;

    value = NULL;
    r = gmail_json_error(mailjson_array_get(labels, i, &value));
    if (r != MAILGMAIL_NO_ERROR)
      goto err_list;
    r = parse_label_value(value, &label);
    mailjson_free(value);
    if (r != MAILGMAIL_NO_ERROR)
      goto err_list;
    if (clist_append(label_list->labels, label) < 0) {
      mailgmail_label_free(label);
      r = MAILGMAIL_ERROR_MEMORY;
      goto err_list;
    }
  }

  * result = label_list;
  r = MAILGMAIL_NO_ERROR;
  goto cleanup_labels;

 err_list:
  mailgmail_label_list_free(label_list);
 cleanup_labels:
  mailjson_free(labels);
 cleanup_root:
  mailjson_free(root);
  return r;
}

int mailgmail_parser_parse_message_list(const char * data, size_t len,
    struct mailgmail_message_list ** result)
{
  mailjson_value * root;
  mailjson_value * messages;
  struct mailgmail_message_list * list;
  int r;

  if ((data == NULL) || (result == NULL))
    return MAILGMAIL_ERROR_BAD_STATE;

  * result = NULL;
  root = NULL;
  r = parse_root(data, len, &root);
  if (r != MAILGMAIL_NO_ERROR)
    return r;

  list = mailgmail_message_list_new();
  if (list == NULL) {
    mailjson_free(root);
    return MAILGMAIL_ERROR_MEMORY;
  }

  r = string_for_key(root, "nextPageToken", &list->next_page_token);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  r = uint32_for_key(root, "resultSizeEstimate",
      &list->result_size_estimate);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;

  messages = NULL;
  r = get_object(root, "messages", &messages);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  if (messages != NULL) {
    size_t count;
    size_t i;

    if (!mailjson_is_array(messages)) {
      mailjson_free(messages);
      r = MAILGMAIL_ERROR_PARSE;
      goto err;
    }
    count = mailjson_array_size(messages);
    for (i = 0; i < count; i ++) {
      mailjson_value * value;
      char * id;
      char * thread_id;
      struct mailgmail_message_summary * summary;

      value = NULL;
      id = NULL;
      thread_id = NULL;
      r = gmail_json_error(mailjson_array_get(messages, i, &value));
      if (r != MAILGMAIL_NO_ERROR)
        goto err_messages;
      if ((value == NULL) || !mailjson_is_object(value)) {
        r = MAILGMAIL_ERROR_PARSE;
        goto err_message_value;
      }
      r = string_for_key(value, "id", &id);
      if (r != MAILGMAIL_NO_ERROR)
        goto err_message_value;
      r = string_for_key(value, "threadId", &thread_id);
      if (r != MAILGMAIL_NO_ERROR)
        goto err_message_value;
      if ((id != NULL) && (thread_id != NULL)) {
        summary = mailgmail_message_summary_new(id, thread_id);
        if (summary == NULL) {
          r = MAILGMAIL_ERROR_MEMORY;
          goto err_message_value;
        }
        if (clist_append(list->messages, summary) < 0) {
          mailgmail_message_summary_free(summary);
          r = MAILGMAIL_ERROR_MEMORY;
          goto err_message_value;
        }
      }
    err_message_value:
      free(id);
      free(thread_id);
      mailjson_free(value);
      if (r != MAILGMAIL_NO_ERROR)
        goto err_messages;
    }
  err_messages:
    mailjson_free(messages);
    if (r != MAILGMAIL_NO_ERROR)
      goto err;
  }

  * result = list;
  mailjson_free(root);
  return MAILGMAIL_NO_ERROR;

 err:
  mailgmail_message_list_free(list);
  mailjson_free(root);
  return r;
}

static int parse_header_value(mailjson_value * value,
    struct mailgmail_message_header ** result)
{
  struct mailgmail_message_header * header;
  char * name;
  char * header_value;
  int r;

  if (!mailjson_is_object(value))
    return MAILGMAIL_ERROR_PARSE;

  name = NULL;
  header_value = NULL;
  r = string_for_key(value, "name", &name);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  r = string_for_key(value, "value", &header_value);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  if ((name == NULL) || (header_value == NULL)) {
    r = MAILGMAIL_ERROR_PARSE;
    goto err;
  }

  header = mailgmail_message_header_new(name, header_value);
  free(name);
  free(header_value);
  if (header == NULL)
    return MAILGMAIL_ERROR_MEMORY;

  * result = header;
  return MAILGMAIL_NO_ERROR;

 err:
  free(name);
  free(header_value);
  return r;
}

static int append_headers(mailjson_value * part_value,
    struct mailgmail_message_part * part)
{
  mailjson_value * headers;
  size_t count;
  size_t i;
  int r;

  headers = NULL;
  r = get_object(part_value, "headers", &headers);
  if (r != MAILGMAIL_NO_ERROR)
    return r;
  if (headers == NULL)
    return MAILGMAIL_NO_ERROR;
  if (!mailjson_is_array(headers)) {
    mailjson_free(headers);
    return MAILGMAIL_NO_ERROR;
  }

  count = mailjson_array_size(headers);
  for (i = 0; i < count; i ++) {
    mailjson_value * value;
    struct mailgmail_message_header * header;

    value = NULL;
    r = gmail_json_error(mailjson_array_get(headers, i, &value));
    if (r != MAILGMAIL_NO_ERROR)
      goto cleanup;
    r = parse_header_value(value, &header);
    mailjson_free(value);
    if (r != MAILGMAIL_NO_ERROR)
      goto cleanup;
    if (clist_append(part->headers, header) < 0) {
      mailgmail_message_header_free(header);
      r = MAILGMAIL_ERROR_MEMORY;
      goto cleanup;
    }
  }

 cleanup:
  mailjson_free(headers);
  return r;
}

static int parse_message_part_value(mailjson_value * value,
    struct mailgmail_message_part ** result);

static int append_parts(mailjson_value * part_value,
    struct mailgmail_message_part * part)
{
  mailjson_value * parts;
  size_t count;
  size_t i;
  int r;

  parts = NULL;
  r = get_object(part_value, "parts", &parts);
  if (r != MAILGMAIL_NO_ERROR)
    return r;
  if (parts == NULL)
    return MAILGMAIL_NO_ERROR;
  if (!mailjson_is_array(parts)) {
    mailjson_free(parts);
    return MAILGMAIL_NO_ERROR;
  }

  count = mailjson_array_size(parts);
  for (i = 0; i < count; i ++) {
    mailjson_value * value;
    struct mailgmail_message_part * child;

    value = NULL;
    r = gmail_json_error(mailjson_array_get(parts, i, &value));
    if (r != MAILGMAIL_NO_ERROR)
      goto cleanup;
    r = parse_message_part_value(value, &child);
    mailjson_free(value);
    if (r != MAILGMAIL_NO_ERROR)
      goto cleanup;
    if (clist_append(part->parts, child) < 0) {
      mailgmail_message_part_free(child);
      r = MAILGMAIL_ERROR_MEMORY;
      goto cleanup;
    }
  }

 cleanup:
  mailjson_free(parts);
  return r;
}

static int parse_part_body(mailjson_value * part_value,
    struct mailgmail_message_part * part)
{
  mailjson_value * body;
  int r;

  body = NULL;
  r = get_object(part_value, "body", &body);
  if (r != MAILGMAIL_NO_ERROR)
    return r;
  if (body == NULL)
    return MAILGMAIL_NO_ERROR;
  if (!mailjson_is_object(body)) {
    mailjson_free(body);
    return MAILGMAIL_NO_ERROR;
  }

  part->body = mailgmail_message_part_body_new();
  if (part->body == NULL) {
    mailjson_free(body);
    return MAILGMAIL_ERROR_MEMORY;
  }

  r = string_for_key(body, "attachmentId", &part->body->attachment_id);
  if (r != MAILGMAIL_NO_ERROR)
    goto cleanup;
  r = uint32_for_key(body, "size", &part->body->size);
  if (r != MAILGMAIL_NO_ERROR)
    goto cleanup;
  r = string_for_key(body, "data", &part->body->data);

 cleanup:
  mailjson_free(body);
  return r;
}

static int parse_message_part_value(mailjson_value * value,
    struct mailgmail_message_part ** result)
{
  struct mailgmail_message_part * part;
  int r;

  if (!mailjson_is_object(value))
    return MAILGMAIL_ERROR_PARSE;

  part = mailgmail_message_part_new();
  if (part == NULL)
    return MAILGMAIL_ERROR_MEMORY;

  r = string_for_key(value, "partId", &part->part_id);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  r = string_for_key(value, "mimeType", &part->mime_type);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  r = string_for_key(value, "filename", &part->filename);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  r = parse_part_body(value, part);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  r = append_headers(value, part);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  r = append_parts(value, part);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;

  * result = part;
  return MAILGMAIL_NO_ERROR;

 err:
  mailgmail_message_part_free(part);
  return r;
}

int mailgmail_parser_parse_message(const char * data, size_t len,
    struct mailgmail_message ** result)
{
  mailjson_value * root;
  mailjson_value * payload;
  struct mailgmail_message * message;
  int r;

  if ((data == NULL) || (result == NULL))
    return MAILGMAIL_ERROR_BAD_STATE;

  * result = NULL;
  root = NULL;
  r = parse_root(data, len, &root);
  if (r != MAILGMAIL_NO_ERROR)
    return r;

  message = mailgmail_message_new();
  if (message == NULL) {
    mailjson_free(root);
    return MAILGMAIL_ERROR_MEMORY;
  }

  r = string_for_key(root, "id", &message->id);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  r = string_for_key(root, "threadId", &message->thread_id);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  r = string_array_for_key(root, "labelIds", message->label_ids);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  r = string_for_key(root, "snippet", &message->snippet);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  r = string_for_key(root, "historyId", &message->history_id);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  r = string_for_key(root, "internalDate", &message->internal_date);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  r = uint32_for_key(root, "sizeEstimate", &message->size_estimate);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  r = string_for_key(root, "raw", &message->raw);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;

  payload = NULL;
  r = get_object(root, "payload", &payload);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  if ((payload != NULL) && mailjson_is_object(payload)) {
    r = parse_message_part_value(payload, &message->payload);
    if (r != MAILGMAIL_NO_ERROR) {
      mailjson_free(payload);
      goto err;
    }
  }
  mailjson_free(payload);

  * result = message;
  mailjson_free(root);
  return MAILGMAIL_NO_ERROR;

 err:
  mailgmail_message_free(message);
  mailjson_free(root);
  return r;
}

int mailgmail_parser_parse_attachment(const char * data, size_t len,
    struct mailgmail_attachment ** result)
{
  mailjson_value * root;
  struct mailgmail_attachment * attachment;
  int r;

  if ((data == NULL) || (result == NULL))
    return MAILGMAIL_ERROR_BAD_STATE;

  * result = NULL;
  root = NULL;
  r = parse_root(data, len, &root);
  if (r != MAILGMAIL_NO_ERROR)
    return r;

  attachment = mailgmail_attachment_new();
  if (attachment == NULL) {
    mailjson_free(root);
    return MAILGMAIL_ERROR_MEMORY;
  }

  r = string_for_key(root, "attachmentId", &attachment->attachment_id);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  r = uint32_for_key(root, "size", &attachment->size);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;
  r = string_for_key(root, "data", &attachment->data);
  if (r != MAILGMAIL_NO_ERROR)
    goto err;

  * result = attachment;
  mailjson_free(root);
  return MAILGMAIL_NO_ERROR;

 err:
  mailgmail_attachment_free(attachment);
  mailjson_free(root);
  return r;
}
