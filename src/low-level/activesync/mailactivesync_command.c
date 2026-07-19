/*
 * libEtPan! -- a mail stuff library
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailactivesync_command.h"

#include <libetpan/mmapstring.h>

#include "base64.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int append_url_escaped(MMAPString * buffer, const char * value)
{
  static const char hex[] = "0123456789ABCDEF";
  const unsigned char * cur;

  if (value == NULL)
    return MAILACTIVESYNC_NO_ERROR;

  for (cur = (const unsigned char *) value; * cur != '\0'; cur ++) {
    if (isalnum(* cur) || (* cur == '-') || (* cur == '_') ||
        (* cur == '.') || (* cur == '~')) {
      if (mmap_string_append_c(buffer, (char) * cur) == NULL)
        return MAILACTIVESYNC_ERROR_MEMORY;
    }
    else {
      char escaped[4];

      escaped[0] = '%';
      escaped[1] = hex[* cur >> 4];
      escaped[2] = hex[* cur & 0x0F];
      escaped[3] = '\0';
      if (mmap_string_append(buffer, escaped) == NULL)
        return MAILACTIVESYNC_ERROR_MEMORY;
    }
  }

  return MAILACTIVESYNC_NO_ERROR;
}

static int append_query_pair(MMAPString * buffer,
    const char * name, const char * value, int * first)
{
  int r;

  if ((buffer == NULL) || (name == NULL) || (value == NULL) ||
      (first == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  if (mmap_string_append_c(buffer, * first ? '?' : '&') == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;
  * first = 0;

  r = append_url_escaped(buffer, name);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;
  if (mmap_string_append_c(buffer, '=') == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;
  return append_url_escaped(buffer, value);
}

static int build_command_url(mailactivesync * session,
    const char * command,
    const char * collection_id,
    char ** result)
{
  MMAPString * buffer;
  int first;
  int r;

  if ((session == NULL) || (session->as_server_url == NULL) ||
      (command == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  buffer = mmap_string_sized_new(strlen(session->as_server_url) + 128);
  if (buffer == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  if (mmap_string_append(buffer, session->as_server_url) == NULL) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto err;
  }

  first = strchr(session->as_server_url, '?') == NULL;
  r = append_query_pair(buffer, "Cmd", command, &first);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  r = append_query_pair(buffer, "User", session->as_login, &first);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  r = append_query_pair(buffer, "DeviceId",
      session->as_device_id != NULL ? session->as_device_id : "libetpan",
      &first);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  r = append_query_pair(buffer, "DeviceType",
      session->as_device_type != NULL ? session->as_device_type : "libetpan",
      &first);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  if (collection_id != NULL) {
    r = append_query_pair(buffer, "CollectionId", collection_id, &first);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
  }

  * result = strdup(buffer->str);
  if (* result == NULL) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto err;
  }

  mmap_string_free(buffer);
  return MAILACTIVESYNC_NO_ERROR;

 err:
  mmap_string_free(buffer);
  return r;
}

static int request_add_auth(mailactivesync * session,
    struct mailactivesync_http_request * request)
{
  MMAPString * value;
  int r;

  if (session->as_oauth_token != NULL) {
    value = mmap_string_sized_new(strlen(session->as_oauth_token) + 8);
    if (value == NULL)
      return MAILACTIVESYNC_ERROR_MEMORY;
    if ((mmap_string_append(value, "Bearer ") == NULL) ||
        (mmap_string_append(value, session->as_oauth_token) == NULL)) {
      mmap_string_free(value);
      return MAILACTIVESYNC_ERROR_MEMORY;
    }
    r = mailactivesync_http_request_add_header(request, "Authorization",
        value->str);
    mmap_string_free(value);
    return r;
  }

  if (session->as_password != NULL) {
    char * raw;
    char * encoded;
    size_t raw_len;

    raw_len = strlen(session->as_login) + strlen(session->as_password) + 2;
    raw = malloc(raw_len);
    if (raw == NULL)
      return MAILACTIVESYNC_ERROR_MEMORY;
    snprintf(raw, raw_len, "%s:%s", session->as_login, session->as_password);
    encoded = encode_base64(raw, (int) strlen(raw));
    free(raw);
    if (encoded == NULL)
      return MAILACTIVESYNC_ERROR_MEMORY;

    value = mmap_string_sized_new(strlen(encoded) + 8);
    if (value == NULL) {
      free(encoded);
      return MAILACTIVESYNC_ERROR_MEMORY;
    }
    if ((mmap_string_append(value, "Basic ") == NULL) ||
        (mmap_string_append(value, encoded) == NULL)) {
      free(encoded);
      mmap_string_free(value);
      return MAILACTIVESYNC_ERROR_MEMORY;
    }
    free(encoded);
    r = mailactivesync_http_request_add_header(request, "Authorization",
        value->str);
    mmap_string_free(value);
    return r;
  }

  return MAILACTIVESYNC_ERROR_BAD_STATE;
}

static int request_add_common_headers(mailactivesync * session,
    struct mailactivesync_http_request * request, int has_wbxml_body)
{
  int r;

  r = request_add_auth(session, request);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  r = mailactivesync_http_request_add_header(request, "MS-ASProtocolVersion",
      session->as_protocol_version != NULL ? session->as_protocol_version :
      "16.1");
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  if (session->as_policy_key != NULL) {
    r = mailactivesync_http_request_add_header(request, "X-MS-PolicyKey",
        session->as_policy_key);
    if (r != MAILACTIVESYNC_NO_ERROR)
      return r;
  }

  r = mailactivesync_http_request_add_header(request, "User-Agent",
      "libEtPan ActiveSync");
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;
  r = mailactivesync_http_request_add_header(request, "Connection", "close");
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  if (has_wbxml_body) {
    r = mailactivesync_http_request_add_header(request, "Content-Type",
        "application/vnd.ms-sync.wbxml");
    if (r != MAILACTIVESYNC_NO_ERROR)
      return r;
    r = mailactivesync_http_request_add_header(request, "Accept",
        "application/vnd.ms-sync.wbxml");
    if (r != MAILACTIVESYNC_NO_ERROR)
      return r;
  }

  return MAILACTIVESYNC_NO_ERROR;
}

static int http_status_to_error(int status_code)
{
  if ((status_code >= 200) && (status_code < 300))
    return MAILACTIVESYNC_NO_ERROR;
  if (status_code == 401)
    return MAILACTIVESYNC_ERROR_UNAUTHORIZED;

  return MAILACTIVESYNC_ERROR_HTTP;
}

static int append_string_item(clist * list, const char * start, size_t len)
{
  char * value;
  size_t begin;
  size_t end;

  begin = 0;
  end = len;
  while ((begin < end) && isspace((unsigned char) start[begin]))
    begin ++;
  while ((end > begin) && isspace((unsigned char) start[end - 1]))
    end --;
  if (begin == end)
    return MAILACTIVESYNC_NO_ERROR;

  value = malloc(end - begin + 1);
  if (value == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;
  memcpy(value, start + begin, end - begin);
  value[end - begin] = '\0';

  if (clist_append(list, value) < 0) {
    free(value);
    return MAILACTIVESYNC_ERROR_MEMORY;
  }

  return MAILACTIVESYNC_NO_ERROR;
}

static int parse_comma_list(clist * list, const char * value)
{
  const char * start;
  const char * cur;
  int r;

  if ((list == NULL) || (value == NULL))
    return MAILACTIVESYNC_NO_ERROR;

  start = value;
  for (cur = value; ; cur ++) {
    if ((* cur == ',') || (* cur == '\0')) {
      r = append_string_item(list, start, (size_t) (cur - start));
      if (r != MAILACTIVESYNC_NO_ERROR)
        return r;
      if (* cur == '\0')
        break;
      start = cur + 1;
    }
  }

  return MAILACTIVESYNC_NO_ERROR;
}

int mailactivesync_command_options(mailactivesync * session,
    struct mailactivesync_options ** result)
{
  struct mailactivesync_http_request * request;
  struct mailactivesync_http_response * response;
  struct mailactivesync_options * options;
  const char * versions;
  const char * commands;
  int r;

  if ((session == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  request = mailactivesync_http_request_new("OPTIONS",
      session->as_server_url);
  if (request == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  r = request_add_common_headers(session, request, 0);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup_request;

  response = NULL;
  r = mailactivesync_http_perform(session->as_http_transport, request,
      &response);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup_request;

  r = http_status_to_error(response->status_code);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup_response;

  options = malloc(sizeof(* options));
  if (options == NULL) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto cleanup_response;
  }
  options->protocol_versions = clist_new();
  options->commands = clist_new();
  if ((options->protocol_versions == NULL) || (options->commands == NULL)) {
    mailactivesync_options_free(options);
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto cleanup_response;
  }

  versions = mailactivesync_http_response_header_value(response,
      "MS-ASProtocolVersions");
  commands = mailactivesync_http_response_header_value(response,
      "MS-ASProtocolCommands");
  if ((versions == NULL) || (commands == NULL)) {
    mailactivesync_options_free(options);
    r = MAILACTIVESYNC_ERROR_PROTOCOL;
    goto cleanup_response;
  }

  r = parse_comma_list(options->protocol_versions, versions);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    mailactivesync_options_free(options);
    goto cleanup_response;
  }
  r = parse_comma_list(options->commands, commands);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    mailactivesync_options_free(options);
    goto cleanup_response;
  }

  * result = options;

 cleanup_response:
  mailactivesync_http_response_free(response);
 cleanup_request:
  mailactivesync_http_request_free(request);
  return r;
}

int mailactivesync_command_post(mailactivesync * session,
    const char * command,
    const char * collection_id,
    const unsigned char * request_body,
    size_t request_body_len,
    struct mailactivesync_http_response ** response)
{
  struct mailactivesync_http_request * request;
  char * url;
  int r;

  if ((session == NULL) || (command == NULL) || (response == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * response = NULL;
  url = NULL;
  r = build_command_url(session, command, collection_id, &url);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  request = mailactivesync_http_request_new("POST", url);
  free(url);
  if (request == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  r = request_add_common_headers(session, request, 1);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;
  r = mailactivesync_http_request_set_body(request, request_body,
      request_body_len);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;

  r = mailactivesync_http_perform(session->as_http_transport, request,
      response);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;

  r = http_status_to_error((* response)->status_code);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    mailactivesync_http_response_free(* response);
    * response = NULL;
  }

 cleanup:
  mailactivesync_http_request_free(request);
  return r;
}
