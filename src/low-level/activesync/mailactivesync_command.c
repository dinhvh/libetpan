/*
 * libEtPan! -- a mail stuff library
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailactivesync_command.h"

#include <libetpan/mmapstring.h>

#include "base64.h"
#include "mailactivesync_codes.h"
#include "mailactivesync_wbxml.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

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

static int replace_string(char ** target, const char * value)
{
  char * copy;

  copy = NULL;
  if (value != NULL) {
    copy = strdup(value);
    if (copy == NULL)
      return MAILACTIVESYNC_ERROR_MEMORY;
  }

  free(* target);
  * target = copy;
  return MAILACTIVESYNC_NO_ERROR;
}

static int server_url_host_is(mailactivesync * session, const char * expected)
{
  const char * url;
  const char * host;
  const char * end;
  size_t expected_len;

  if ((session == NULL) || (session->as_server_url == NULL) ||
      (expected == NULL))
    return 0;

  url = session->as_server_url;
  host = strstr(url, "://");
  host = host != NULL ? host + 3 : url;
  end = host;
  while ((* end != '\0') && (* end != '/') && (* end != ':') &&
      (* end != '?'))
    end ++;

  expected_len = strlen(expected);
  return ((size_t) (end - host) == expected_len) &&
      (strncasecmp(host, expected, expected_len) == 0);
}

static int append_cookie_escaped(MMAPString * buffer, const char * value)
{
  static const char hex[] = "0123456789ABCDEF";
  const unsigned char * cur;

  if (value == NULL)
    return MAILACTIVESYNC_NO_ERROR;

  for (cur = (const unsigned char *) value; * cur != '\0'; cur ++) {
    if (isalnum(* cur) || (* cur == '-') || (* cur == '_') ||
        (* cur == '.') || (* cur == '~') || (* cur == '@')) {
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

static int request_add_anchor_mailbox_cookie(mailactivesync * session,
    struct mailactivesync_http_request * request)
{
  MMAPString * value;
  int r;

  if ((session->as_login == NULL) ||
      !server_url_host_is(session, "eas.outlook.com"))
    return MAILACTIVESYNC_NO_ERROR;

  value = mmap_string_sized_new(strlen(session->as_login) + 32);
  if (value == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  if (mmap_string_append(value, "DefaultAnchorMailbox=") == NULL) {
    mmap_string_free(value);
    return MAILACTIVESYNC_ERROR_MEMORY;
  }

  r = append_cookie_escaped(value, session->as_login);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    mmap_string_free(value);
    return r;
  }

  r = mailactivesync_http_request_add_header(request, "Cookie", value->str);
  mmap_string_free(value);
  return r;
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

static int build_command_url_ex(mailactivesync * session,
    const char * command,
    const char * collection_id,
    const char * item_id,
    const char * long_id,
    const char * occurrence,
    const char * options,
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
  if (item_id != NULL) {
    r = append_query_pair(buffer, "ItemId", item_id, &first);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
  }
  if (long_id != NULL) {
    r = append_query_pair(buffer, "LongId", long_id, &first);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
  }
  if (occurrence != NULL) {
    r = append_query_pair(buffer, "Occurrence", occurrence, &first);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
  }
  if (options != NULL) {
    r = append_query_pair(buffer, "Options", options, &first);
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

static int build_command_url(mailactivesync * session,
    const char * command,
    const char * collection_id,
    char ** result)
{
  return build_command_url_ex(session, command, collection_id, NULL, NULL,
      NULL, NULL, result);
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

static int request_add_common_headers_with_content_type(mailactivesync * session,
    struct mailactivesync_http_request * request,
    const char * content_type,
    int has_wbxml_body)
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
      session->as_user_agent != NULL ? session->as_user_agent :
      "libEtPan ActiveSync");
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;
  r = request_add_anchor_mailbox_cookie(session, request);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  r = mailactivesync_http_request_add_header(request, "Connection", "close");
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  r = mailactivesync_http_request_add_header(request, "Content-Type",
      content_type);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  if (has_wbxml_body) {
    r = mailactivesync_http_request_add_header(request, "Accept",
        "application/vnd.ms-sync.wbxml");
    if (r != MAILACTIVESYNC_NO_ERROR)
      return r;
  }

  return MAILACTIVESYNC_NO_ERROR;
}

static int request_add_common_headers(mailactivesync * session,
    struct mailactivesync_http_request * request, int has_wbxml_body)
{
  return request_add_common_headers_with_content_type(session, request,
      "application/vnd.ms-sync.wbxml", has_wbxml_body);
}

static int response_body_is_wbxml(struct mailactivesync_http_response * response)
{
  return (response != NULL) && (response->body != NULL) &&
      (response->body_len >= 4) &&
      (response->body[0] == 0x03) && (response->body[1] == 0x01) &&
      (response->body[2] == 0x6A) && (response->body[3] == 0x00);
}

static int http_status_to_error(mailactivesync * session,
    struct mailactivesync_http_response * response)
{
  int status_code;
  const char * redirect_url;
  const char * authenticate_header;

  if (response == NULL)
    return MAILACTIVESYNC_ERROR_HTTP;

  status_code = response->status_code;
  if ((session != NULL) && (status_code != 451))
    replace_string(&session->as_last_redirect_url, NULL);
  if ((session != NULL) && (status_code != 401) && (status_code != 403))
    replace_string(&session->as_last_authenticate_header, NULL);
  if ((status_code >= 200) && (status_code < 300))
    return MAILACTIVESYNC_NO_ERROR;
  if ((status_code == 401) || (status_code == 403)) {
    authenticate_header = mailactivesync_http_response_header_value(response,
        "WWW-Authenticate");
    if ((session != NULL) &&
        (replace_string(&session->as_last_authenticate_header,
            authenticate_header) != MAILACTIVESYNC_NO_ERROR))
      return MAILACTIVESYNC_ERROR_MEMORY;
    return MAILACTIVESYNC_ERROR_UNAUTHORIZED;
  }
  if (status_code == 449)
    return MAILACTIVESYNC_ERROR_PROVISION_REQUIRED;
  if (status_code == 451) {
    redirect_url = mailactivesync_http_response_header_value(response,
        "X-MS-Location");
    if (redirect_url != NULL) {
      if ((session != NULL) &&
          (replace_string(&session->as_last_redirect_url, redirect_url) !=
              MAILACTIVESYNC_NO_ERROR))
        return MAILACTIVESYNC_ERROR_MEMORY;
      return MAILACTIVESYNC_ERROR_REDIRECT;
    }
    return MAILACTIVESYNC_ERROR_HTTP;
  }

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

static void command_string_item_free(void * value, void * data)
{
  (void) data;
  free(value);
}

static void command_string_list_free(clist * list)
{
  if (list == NULL)
    return;
  clist_foreach(list, command_string_item_free, NULL);
  clist_free(list);
}

static int command_string_list_clone(clist * source, clist ** result)
{
  clist * clone;
  clistiter * cur;

  if ((source == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  clone = clist_new();
  if (clone == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  for (cur = clist_begin(source); cur != NULL; cur = clist_next(cur)) {
    const char * value;
    char * copy;

    value = clist_content(cur);
    copy = strdup(value);
    if (copy == NULL) {
      command_string_list_free(clone);
      return MAILACTIVESYNC_ERROR_MEMORY;
    }
    if (clist_append(clone, copy) < 0) {
      free(copy);
      command_string_list_free(clone);
      return MAILACTIVESYNC_ERROR_MEMORY;
    }
  }

  * result = clone;
  return MAILACTIVESYNC_NO_ERROR;
}

static int session_cache_advertised_commands(mailactivesync * session,
    struct mailactivesync_options * options)
{
  clist * commands;
  int r;

  if ((session == NULL) || (options == NULL) || (options->commands == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  commands = NULL;
  r = command_string_list_clone(options->commands, &commands);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  command_string_list_free(session->as_advertised_commands);
  session->as_advertised_commands = commands;
  return MAILACTIVESYNC_NO_ERROR;
}

static int session_advertises_command(mailactivesync * session,
    const char * command)
{
  clistiter * cur;

  if ((session == NULL) || (command == NULL))
    return 0;
  if (session->as_advertised_commands == NULL)
    return 1;

  for (cur = clist_begin(session->as_advertised_commands); cur != NULL;
      cur = clist_next(cur)) {
    const char * advertised_command;

    advertised_command = clist_content(cur);
    if ((advertised_command != NULL) &&
        (strcmp(advertised_command, command) == 0))
      return 1;
  }

  return 0;
}

static int require_advertised_command(mailactivesync * session,
    const char * command)
{
  if (!session_advertises_command(session, command))
    return MAILACTIVESYNC_ERROR_NOT_IMPLEMENTED;

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

  r = http_status_to_error(session, response);
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

  r = session_cache_advertised_commands(session, options);
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
  r = require_advertised_command(session, command);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

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

  r = http_status_to_error(session, * response);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    mailactivesync_http_response_free(* response);
    * response = NULL;
  }

 cleanup:
  mailactivesync_http_request_free(request);
  return r;
}

static int command_post_with_timeout(mailactivesync * session,
    const char * command,
    const char * collection_id,
    const unsigned char * request_body,
    size_t request_body_len,
    time_t timeout,
    struct mailactivesync_http_response ** response)
{
  struct mailactivesync_http_request * request;
  char * url;
  int r;

  if ((session == NULL) || (command == NULL) || (response == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * response = NULL;
  r = require_advertised_command(session, command);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  url = NULL;
  r = build_command_url(session, command, collection_id, &url);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  request = mailactivesync_http_request_new("POST", url);
  free(url);
  if (request == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  if (timeout > 0)
    request->timeout = timeout;

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

  r = http_status_to_error(session, * response);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    mailactivesync_http_response_free(* response);
    * response = NULL;
  }

 cleanup:
  mailactivesync_http_request_free(request);
  return r;
}

static int command_post_raw(mailactivesync * session,
    const char * command,
    const char * collection_id,
    const char * item_id,
    const char * long_id,
    const char * occurrence,
    const char * options,
    const char * content_type,
    const unsigned char * request_body,
    size_t request_body_len,
    struct mailactivesync_http_response ** response)
{
  struct mailactivesync_http_request * request;
  char * url;
  int r;

  if ((session == NULL) || (command == NULL) || (content_type == NULL) ||
      (response == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * response = NULL;
  r = require_advertised_command(session, command);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  url = NULL;
  r = build_command_url_ex(session, command, collection_id, item_id, long_id,
      occurrence, options, &url);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  request = mailactivesync_http_request_new("POST", url);
  free(url);
  if (request == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  r = request_add_common_headers_with_content_type(session, request,
      content_type, 0);
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

  r = http_status_to_error(session, * response);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    mailactivesync_http_response_free(* response);
    * response = NULL;
  }

 cleanup:
  mailactivesync_http_request_free(request);
  return r;
}

static struct mailactivesync_wbxml_node * node_child(
    struct mailactivesync_wbxml_node * node,
    uint8_t code_page, uint8_t token)
{
  clistiter * cur;

  if ((node == NULL) || (node->children == NULL))
    return NULL;

  for (cur = clist_begin(node->children); cur != NULL; cur = clist_next(cur)) {
    struct mailactivesync_wbxml_node * child;

    child = clist_content(cur);
    if ((child->code_page == code_page) && (child->token == token))
      return child;
  }

  return NULL;
}

static const char * node_child_text(struct mailactivesync_wbxml_node * node,
    uint8_t code_page, uint8_t token)
{
  struct mailactivesync_wbxml_node * child;

  child = node_child(node, code_page, token);
  if (child == NULL)
    return NULL;

  return child->text;
}

static int node_child_int(struct mailactivesync_wbxml_node * node,
    uint8_t code_page, uint8_t token)
{
  const char * value;

  value = node_child_text(node, code_page, token);
  if (value == NULL)
    return 0;

  return atoi(value);
}

static int node_add_text(struct mailactivesync_wbxml_node * parent,
    uint8_t code_page, uint8_t token, const char * text)
{
  struct mailactivesync_wbxml_node * child;
  int r;

  child = mailactivesync_wbxml_node_new_text(code_page, token, text);
  if (child == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  r = mailactivesync_wbxml_node_add_child(parent, child);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    mailactivesync_wbxml_node_free(child);
    return r;
  }

  return MAILACTIVESYNC_NO_ERROR;
}

static int node_add_empty(struct mailactivesync_wbxml_node * parent,
    uint8_t code_page, uint8_t token)
{
  struct mailactivesync_wbxml_node * child;
  int r;

  child = mailactivesync_wbxml_node_new(code_page, token);
  if (child == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  r = mailactivesync_wbxml_node_add_child(parent, child);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    mailactivesync_wbxml_node_free(child);
    return r;
  }

  return MAILACTIVESYNC_NO_ERROR;
}

static int node_add_opaque(struct mailactivesync_wbxml_node * parent,
    uint8_t code_page, uint8_t token,
    const unsigned char * data, size_t data_len)
{
  struct mailactivesync_wbxml_node * child;
  int r;

  child = mailactivesync_wbxml_node_new_opaque(code_page, token, data,
      data_len);
  if (child == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  r = mailactivesync_wbxml_node_add_child(parent, child);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    mailactivesync_wbxml_node_free(child);
    return r;
  }

  return MAILACTIVESYNC_NO_ERROR;
}

static int node_add_uint(struct mailactivesync_wbxml_node * parent,
    uint8_t code_page, uint8_t token, uint32_t value)
{
  char buffer[32];

  snprintf(buffer, sizeof(buffer), "%u", value);
  return node_add_text(parent, code_page, token, buffer);
}

static struct mailactivesync_wbxml_node * node_clone(
    struct mailactivesync_wbxml_node * node)
{
  struct mailactivesync_wbxml_node * cloned;
  clistiter * cur;

  if (node == NULL)
    return NULL;

  if (node->opaque != NULL) {
    cloned = mailactivesync_wbxml_node_new_opaque(node->code_page,
        node->token, node->opaque, node->opaque_len);
  }
  else if (node->text != NULL) {
    cloned = mailactivesync_wbxml_node_new_text(node->code_page,
        node->token, node->text);
  }
  else {
    cloned = mailactivesync_wbxml_node_new(node->code_page, node->token);
  }
  if (cloned == NULL)
    return NULL;

  for (cur = node->children != NULL ? clist_begin(node->children) : NULL;
      cur != NULL; cur = clist_next(cur)) {
    struct mailactivesync_wbxml_node * child;
    struct mailactivesync_wbxml_node * cloned_child;

    child = clist_content(cur);
    cloned_child = node_clone(child);
    if (cloned_child == NULL) {
      mailactivesync_wbxml_node_free(cloned);
      return NULL;
    }
    if (mailactivesync_wbxml_node_add_child(cloned, cloned_child) !=
        MAILACTIVESYNC_NO_ERROR) {
      mailactivesync_wbxml_node_free(cloned_child);
      mailactivesync_wbxml_node_free(cloned);
      return NULL;
    }
  }

  return cloned;
}

static char * dup_node_text(struct mailactivesync_wbxml_node * node,
    uint8_t code_page, uint8_t token)
{
  const char * value;

  value = node_child_text(node, code_page, token);
  if (value == NULL)
    return NULL;

  return strdup(value);
}

static int encode_document(struct mailactivesync_wbxml_node * root,
    unsigned char ** result, size_t * result_len)
{
  struct mailactivesync_wbxml_document * document;
  int r;

  document = mailactivesync_wbxml_document_new();
  if (document == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  document->root = root;
  r = mailactivesync_wbxml_encode(document, result, result_len);
  document->root = NULL;
  mailactivesync_wbxml_document_free(document);

  return r;
}

static int post_wbxml_document_response(mailactivesync * session,
    const char * command, const char * collection_id,
    struct mailactivesync_wbxml_node * root,
    struct mailactivesync_http_response ** result)
{
  unsigned char * encoded;
  size_t encoded_len;
  int r;

  if ((session == NULL) || (command == NULL) || (root == NULL) ||
      (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  encoded = NULL;
  encoded_len = 0;

  r = encode_document(root, &encoded, &encoded_len);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;

  r = mailactivesync_command_post(session, command, collection_id,
      encoded, encoded_len, result);

 cleanup:
  free(encoded);
  return r;
}

static int post_wbxml_document(mailactivesync * session, const char * command,
    const char * collection_id, struct mailactivesync_wbxml_node * root,
    struct mailactivesync_wbxml_document ** result)
{
  struct mailactivesync_http_response * response;
  int r;

  if ((session == NULL) || (command == NULL) || (root == NULL) ||
      (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  response = NULL;

  r = post_wbxml_document_response(session, command, collection_id, root,
      &response);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;

  if ((response->body == NULL) || (response->body_len == 0)) {
    r = MAILACTIVESYNC_ERROR_PROTOCOL;
    goto cleanup;
  }

  if (!response_body_is_wbxml(response)) {
    r = MAILACTIVESYNC_ERROR_RESPONSE_NOT_WBXML;
    goto cleanup;
  }

  r = mailactivesync_wbxml_decode(response->body, response->body_len, result);

 cleanup:
  mailactivesync_http_response_free(response);
  return r;
}

static int append_string_to_list(clist * list, const char * value)
{
  char * copied;

  if ((list == NULL) || (value == NULL))
    return MAILACTIVESYNC_ERROR_PROTOCOL;

  copied = strdup(value);
  if (copied == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  if (clist_append(list, copied) < 0) {
    free(copied);
    return MAILACTIVESYNC_ERROR_MEMORY;
  }

  return MAILACTIVESYNC_NO_ERROR;
}

static struct mailactivesync_folder_sync_result *
folder_sync_result_new(void)
{
  struct mailactivesync_folder_sync_result * result;

  result = malloc(sizeof(* result));
  if (result == NULL)
    return NULL;

  result->sync_key = NULL;
  result->status = 0;
  result->added = clist_new();
  result->updated = clist_new();
  result->deleted = clist_new();
  if ((result->added == NULL) || (result->updated == NULL) ||
      (result->deleted == NULL)) {
    mailactivesync_folder_sync_result_free(result);
    return NULL;
  }

  return result;
}

static struct mailactivesync_folder_mutation_result *
folder_mutation_result_new(void)
{
  struct mailactivesync_folder_mutation_result * result;

  result = calloc(1, sizeof(* result));
  return result;
}

static struct mailactivesync_move_items_result * move_items_result_new(void)
{
  struct mailactivesync_move_items_result * result;

  result = malloc(sizeof(* result));
  if (result == NULL)
    return NULL;

  result->status = 0;
  result->responses = clist_new();
  if (result->responses == NULL) {
    mailactivesync_move_items_result_free(result);
    return NULL;
  }

  return result;
}

static struct mailactivesync_ping_result * ping_result_new(void)
{
  struct mailactivesync_ping_result * result;

  result = malloc(sizeof(* result));
  if (result == NULL)
    return NULL;

  result->status = 0;
  result->heartbeat_interval = 0;
  result->max_folders = 0;
  result->changed_collection_ids = clist_new();
  if (result->changed_collection_ids == NULL) {
    mailactivesync_ping_result_free(result);
    return NULL;
  }

  return result;
}

static int parse_folder_change(struct mailactivesync_wbxml_node * node,
    clist * list)
{
  struct mailactivesync_folder * folder;
  char * server_id;
  char * parent_id;
  char * display_name;
  int type;

  server_id = dup_node_text(node, MAILACTIVESYNC_CP_FOLDERHIERARCHY,
      MAILACTIVESYNC_FOLDER_SERVER_ID);
  parent_id = dup_node_text(node, MAILACTIVESYNC_CP_FOLDERHIERARCHY,
      MAILACTIVESYNC_FOLDER_PARENT_ID);
  display_name = dup_node_text(node, MAILACTIVESYNC_CP_FOLDERHIERARCHY,
      MAILACTIVESYNC_FOLDER_DISPLAY_NAME);
  type = node_child_int(node, MAILACTIVESYNC_CP_FOLDERHIERARCHY,
      MAILACTIVESYNC_FOLDER_TYPE);

  if (server_id == NULL) {
    free(parent_id);
    free(display_name);
    return MAILACTIVESYNC_ERROR_PROTOCOL;
  }

  folder = mailactivesync_folder_new(server_id, parent_id, display_name, type);
  if (folder == NULL) {
    free(server_id);
    free(parent_id);
    free(display_name);
    return MAILACTIVESYNC_ERROR_MEMORY;
  }

  if (clist_append(list, folder) < 0) {
    mailactivesync_folder_free(folder);
    return MAILACTIVESYNC_ERROR_MEMORY;
  }

  return MAILACTIVESYNC_NO_ERROR;
}

static int parse_folder_changes(struct mailactivesync_wbxml_node * changes,
    struct mailactivesync_folder_sync_result * result)
{
  clistiter * cur;
  int r;

  if (changes == NULL)
    return MAILACTIVESYNC_NO_ERROR;

  for (cur = clist_begin(changes->children); cur != NULL;
      cur = clist_next(cur)) {
    struct mailactivesync_wbxml_node * child;

    child = clist_content(cur);
    if (child->code_page != MAILACTIVESYNC_CP_FOLDERHIERARCHY)
      continue;

    switch (child->token) {
    case MAILACTIVESYNC_FOLDER_ADD:
      r = parse_folder_change(child, result->added);
      if (r != MAILACTIVESYNC_NO_ERROR)
        return r;
      break;
    case MAILACTIVESYNC_FOLDER_UPDATE:
      r = parse_folder_change(child, result->updated);
      if (r != MAILACTIVESYNC_NO_ERROR)
        return r;
      break;
    case MAILACTIVESYNC_FOLDER_DELETE:
      r = append_string_to_list(result->deleted,
          node_child_text(child, MAILACTIVESYNC_CP_FOLDERHIERARCHY,
              MAILACTIVESYNC_FOLDER_SERVER_ID));
      if (r != MAILACTIVESYNC_NO_ERROR)
        return r;
      break;
    default:
      break;
    }
  }

  return MAILACTIVESYNC_NO_ERROR;
}

static int parse_folder_mutation_response(
    struct mailactivesync_wbxml_node * root,
    uint8_t root_token,
    int require_server_id,
    struct mailactivesync_folder_mutation_result ** result)
{
  struct mailactivesync_folder_mutation_result * parsed;
  const char * value;

  if ((root == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;
  if ((root->code_page != MAILACTIVESYNC_CP_FOLDERHIERARCHY) ||
      (root->token != root_token))
    return MAILACTIVESYNC_ERROR_PROTOCOL;

  value = node_child_text(root, MAILACTIVESYNC_CP_FOLDERHIERARCHY,
      MAILACTIVESYNC_FOLDER_STATUS);
  if (value == NULL)
    return MAILACTIVESYNC_ERROR_PROTOCOL;

  parsed = folder_mutation_result_new();
  if (parsed == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;
  parsed->status = atoi(value);

  value = node_child_text(root, MAILACTIVESYNC_CP_FOLDERHIERARCHY,
      MAILACTIVESYNC_FOLDER_SYNC_KEY);
  if (value != NULL) {
    parsed->sync_key = strdup(value);
    if (parsed->sync_key == NULL)
      goto err_memory;
  }

  value = node_child_text(root, MAILACTIVESYNC_CP_FOLDERHIERARCHY,
      MAILACTIVESYNC_FOLDER_SERVER_ID);
  if (value != NULL) {
    parsed->server_id = strdup(value);
    if (parsed->server_id == NULL)
      goto err_memory;
  }

  if (parsed->status == 1) {
    if ((parsed->sync_key == NULL) ||
        (require_server_id && (parsed->server_id == NULL))) {
      mailactivesync_folder_mutation_result_free(parsed);
      return MAILACTIVESYNC_ERROR_PROTOCOL;
    }
  }

  * result = parsed;
  return MAILACTIVESYNC_NO_ERROR;

 err_memory:
  mailactivesync_folder_mutation_result_free(parsed);
  return MAILACTIVESYNC_ERROR_MEMORY;
}

int mailactivesync_command_folder_sync(mailactivesync * session,
    const char * sync_key,
    struct mailactivesync_folder_sync_result ** result)
{
  struct mailactivesync_wbxml_document * response_document;
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_folder_sync_result * parsed;
  const char * response_sync_key;
  int r;

  if ((session == NULL) || (sync_key == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  response_document = NULL;
  root = mailactivesync_wbxml_node_new(
      MAILACTIVESYNC_CP_FOLDERHIERARCHY,
      MAILACTIVESYNC_FOLDER_FOLDER_SYNC);
  if (root == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  r = node_add_text(root, MAILACTIVESYNC_CP_FOLDERHIERARCHY,
      MAILACTIVESYNC_FOLDER_SYNC_KEY, sync_key);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup_root;

  r = post_wbxml_document(session, "FolderSync", NULL, root,
      &response_document);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup_root;

  if ((response_document->root == NULL) ||
      (response_document->root->code_page !=
          MAILACTIVESYNC_CP_FOLDERHIERARCHY) ||
      (response_document->root->token !=
          MAILACTIVESYNC_FOLDER_FOLDER_SYNC)) {
    r = MAILACTIVESYNC_ERROR_PROTOCOL;
    goto cleanup_response;
  }

  parsed = folder_sync_result_new();
  if (parsed == NULL) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto cleanup_response;
  }

  response_sync_key = node_child_text(response_document->root,
      MAILACTIVESYNC_CP_FOLDERHIERARCHY, MAILACTIVESYNC_FOLDER_SYNC_KEY);
  if (response_sync_key != NULL) {
    parsed->sync_key = strdup(response_sync_key);
    if (parsed->sync_key == NULL) {
      mailactivesync_folder_sync_result_free(parsed);
      r = MAILACTIVESYNC_ERROR_MEMORY;
      goto cleanup_response;
    }
  }
  parsed->status = node_child_int(response_document->root,
      MAILACTIVESYNC_CP_FOLDERHIERARCHY, MAILACTIVESYNC_FOLDER_STATUS);

  r = parse_folder_changes(node_child(response_document->root,
      MAILACTIVESYNC_CP_FOLDERHIERARCHY, MAILACTIVESYNC_FOLDER_CHANGES),
      parsed);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    mailactivesync_folder_sync_result_free(parsed);
    goto cleanup_response;
  }

  * result = parsed;

 cleanup_response:
  mailactivesync_wbxml_document_free(response_document);
 cleanup_root:
  mailactivesync_wbxml_node_free(root);
  return r;
}

static int run_folder_mutation(mailactivesync * session,
    const char * command_name,
    uint8_t root_token,
    struct mailactivesync_wbxml_node * root,
    int require_server_id,
    struct mailactivesync_folder_mutation_result ** result)
{
  struct mailactivesync_wbxml_document * response_document;
  int r;

  if ((session == NULL) || (command_name == NULL) || (root == NULL) ||
      (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  response_document = NULL;

  r = post_wbxml_document(session, command_name, NULL, root,
      &response_document);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;

  r = parse_folder_mutation_response(response_document->root, root_token,
      require_server_id, result);
  if ((r == MAILACTIVESYNC_NO_ERROR) && (* result != NULL))
    r = mailactivesync_folder_mutation_status_to_error((* result)->status);

 cleanup:
  mailactivesync_wbxml_document_free(response_document);
  mailactivesync_wbxml_node_free(root);
  return r;
}

int mailactivesync_command_folder_create(mailactivesync * session,
    const char * sync_key,
    const char * parent_id,
    const char * display_name,
    int type,
    struct mailactivesync_folder_mutation_result ** result)
{
  struct mailactivesync_wbxml_node * root;
  int r;

  if ((session == NULL) || (sync_key == NULL) || (parent_id == NULL) ||
      (display_name == NULL) || (type <= 0) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  root = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_FOLDERHIERARCHY,
      MAILACTIVESYNC_FOLDER_FOLDER_CREATE);
  if (root == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  r = node_add_text(root, MAILACTIVESYNC_CP_FOLDERHIERARCHY,
      MAILACTIVESYNC_FOLDER_SYNC_KEY, sync_key);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup_root;
  r = node_add_text(root, MAILACTIVESYNC_CP_FOLDERHIERARCHY,
      MAILACTIVESYNC_FOLDER_PARENT_ID, parent_id);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup_root;
  r = node_add_text(root, MAILACTIVESYNC_CP_FOLDERHIERARCHY,
      MAILACTIVESYNC_FOLDER_DISPLAY_NAME, display_name);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup_root;
  r = node_add_uint(root, MAILACTIVESYNC_CP_FOLDERHIERARCHY,
      MAILACTIVESYNC_FOLDER_TYPE, (uint32_t) type);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup_root;

  return run_folder_mutation(session, "FolderCreate",
      MAILACTIVESYNC_FOLDER_FOLDER_CREATE, root, 1, result);

 cleanup_root:
  mailactivesync_wbxml_node_free(root);
  return r;
}

int mailactivesync_command_folder_update(mailactivesync * session,
    const char * sync_key,
    const char * server_id,
    const char * parent_id,
    const char * display_name,
    struct mailactivesync_folder_mutation_result ** result)
{
  struct mailactivesync_wbxml_node * root;
  int r;

  if ((session == NULL) || (sync_key == NULL) || (server_id == NULL) ||
      (parent_id == NULL) || (display_name == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  root = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_FOLDERHIERARCHY,
      MAILACTIVESYNC_FOLDER_FOLDER_UPDATE);
  if (root == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  r = node_add_text(root, MAILACTIVESYNC_CP_FOLDERHIERARCHY,
      MAILACTIVESYNC_FOLDER_SYNC_KEY, sync_key);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup_root;
  r = node_add_text(root, MAILACTIVESYNC_CP_FOLDERHIERARCHY,
      MAILACTIVESYNC_FOLDER_SERVER_ID, server_id);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup_root;
  r = node_add_text(root, MAILACTIVESYNC_CP_FOLDERHIERARCHY,
      MAILACTIVESYNC_FOLDER_PARENT_ID, parent_id);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup_root;
  r = node_add_text(root, MAILACTIVESYNC_CP_FOLDERHIERARCHY,
      MAILACTIVESYNC_FOLDER_DISPLAY_NAME, display_name);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup_root;

  return run_folder_mutation(session, "FolderUpdate",
      MAILACTIVESYNC_FOLDER_FOLDER_UPDATE, root, 0, result);

 cleanup_root:
  mailactivesync_wbxml_node_free(root);
  return r;
}

int mailactivesync_command_folder_delete(mailactivesync * session,
    const char * sync_key,
    const char * server_id,
    struct mailactivesync_folder_mutation_result ** result)
{
  struct mailactivesync_wbxml_node * root;
  int r;

  if ((session == NULL) || (sync_key == NULL) || (server_id == NULL) ||
      (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  root = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_FOLDERHIERARCHY,
      MAILACTIVESYNC_FOLDER_FOLDER_DELETE);
  if (root == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  r = node_add_text(root, MAILACTIVESYNC_CP_FOLDERHIERARCHY,
      MAILACTIVESYNC_FOLDER_SYNC_KEY, sync_key);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup_root;
  r = node_add_text(root, MAILACTIVESYNC_CP_FOLDERHIERARCHY,
      MAILACTIVESYNC_FOLDER_SERVER_ID, server_id);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup_root;

  return run_folder_mutation(session, "FolderDelete",
      MAILACTIVESYNC_FOLDER_FOLDER_DELETE, root, 0, result);

 cleanup_root:
  mailactivesync_wbxml_node_free(root);
  return r;
}

static int build_move_items_request(clist * moves,
    struct mailactivesync_wbxml_node ** result)
{
  struct mailactivesync_wbxml_node * root;
  clistiter * cur;
  int r;

  if ((moves == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  if (clist_count(moves) == 0)
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  root = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_MOVE,
      MAILACTIVESYNC_MOVE_MOVE_ITEMS);
  if (root == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  for (cur = clist_begin(moves); cur != NULL; cur = clist_next(cur)) {
    struct mailactivesync_move * move;
    struct mailactivesync_wbxml_node * node;

    move = clist_content(cur);
    if ((move == NULL) || (move->src_msg_id == NULL) ||
        (move->src_folder_id == NULL) || (move->dst_folder_id == NULL)) {
      r = MAILACTIVESYNC_ERROR_BAD_STATE;
      goto err;
    }

    node = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_MOVE,
        MAILACTIVESYNC_MOVE_MOVE);
    if (node == NULL) {
      r = MAILACTIVESYNC_ERROR_MEMORY;
      goto err;
    }

    r = node_add_text(node, MAILACTIVESYNC_CP_MOVE,
        MAILACTIVESYNC_MOVE_SRCMSGID, move->src_msg_id);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err_node;
    r = node_add_text(node, MAILACTIVESYNC_CP_MOVE,
        MAILACTIVESYNC_MOVE_SRCFLDID, move->src_folder_id);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err_node;
    r = node_add_text(node, MAILACTIVESYNC_CP_MOVE,
        MAILACTIVESYNC_MOVE_DSTFLDID, move->dst_folder_id);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err_node;

    r = mailactivesync_wbxml_node_add_child(root, node);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err_node;
    node = NULL;
    continue;

   err_node:
    mailactivesync_wbxml_node_free(node);
    goto err;
  }

  * result = root;
  return MAILACTIVESYNC_NO_ERROR;

 err:
  mailactivesync_wbxml_node_free(root);
  return r;
}

static int parse_move_items_response_item(
    struct mailactivesync_wbxml_node * node,
    clist * list)
{
  struct mailactivesync_move_response * response;
  const char * status;

  if ((node == NULL) || (list == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  status = node_child_text(node, MAILACTIVESYNC_CP_MOVE,
      MAILACTIVESYNC_MOVE_STATUS);
  if ((node_child_text(node, MAILACTIVESYNC_CP_MOVE,
          MAILACTIVESYNC_MOVE_SRCMSGID) == NULL) || (status == NULL))
    return MAILACTIVESYNC_ERROR_PROTOCOL;

  response = calloc(1, sizeof(* response));
  if (response == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  response->src_msg_id = dup_node_text(node, MAILACTIVESYNC_CP_MOVE,
      MAILACTIVESYNC_MOVE_SRCMSGID);
  response->dst_msg_id = dup_node_text(node, MAILACTIVESYNC_CP_MOVE,
      MAILACTIVESYNC_MOVE_DSTMSGID);
  response->status = atoi(status);
  if (response->src_msg_id == NULL) {
    mailactivesync_move_response_free(response);
    return MAILACTIVESYNC_ERROR_MEMORY;
  }

  if (clist_append(list, response) < 0) {
    mailactivesync_move_response_free(response);
    return MAILACTIVESYNC_ERROR_MEMORY;
  }

  return MAILACTIVESYNC_NO_ERROR;
}

static int parse_move_items_response(
    struct mailactivesync_wbxml_document * response_document,
    struct mailactivesync_move_items_result ** result)
{
  struct mailactivesync_move_items_result * parsed;
  clistiter * cur;
  const char * top_status;
  int response_error;
  int r;

  if ((response_document == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  if ((response_document->root == NULL) ||
      (response_document->root->code_page != MAILACTIVESYNC_CP_MOVE) ||
      (response_document->root->token != MAILACTIVESYNC_MOVE_MOVE_ITEMS))
    return MAILACTIVESYNC_ERROR_PROTOCOL;

  parsed = move_items_result_new();
  if (parsed == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  top_status = node_child_text(response_document->root,
      MAILACTIVESYNC_CP_MOVE, MAILACTIVESYNC_MOVE_STATUS);
  parsed->status = top_status != NULL ? atoi(top_status) : 3;
  response_error = mailactivesync_move_items_status_to_error(parsed->status);

  for (cur = response_document->root->children != NULL ?
      clist_begin(response_document->root->children) : NULL;
      cur != NULL; cur = clist_next(cur)) {
    struct mailactivesync_wbxml_node * child;
    struct mailactivesync_move_response * item;

    child = clist_content(cur);
    if ((child->code_page != MAILACTIVESYNC_CP_MOVE) ||
        (child->token != MAILACTIVESYNC_MOVE_RESPONSE))
      continue;

    r = parse_move_items_response_item(child, parsed->responses);
    if (r != MAILACTIVESYNC_NO_ERROR) {
      mailactivesync_move_items_result_free(parsed);
      return r;
    }

    item = clist_content(clist_end(parsed->responses));
    r = mailactivesync_move_items_status_to_error(item->status);
    if ((r != MAILACTIVESYNC_NO_ERROR) &&
        (response_error == MAILACTIVESYNC_NO_ERROR))
      response_error = r;
    if ((parsed->status == 3) && (item->status != 3))
      parsed->status = item->status;
  }

  if (clist_count(parsed->responses) == 0) {
    mailactivesync_move_items_result_free(parsed);
    return MAILACTIVESYNC_ERROR_PROTOCOL;
  }

  * result = parsed;
  return response_error;
}

static struct mailactivesync_move * find_move_by_src_msg_id(clist * moves,
    const char * src_msg_id)
{
  clistiter * cur;

  if ((moves == NULL) || (src_msg_id == NULL))
    return NULL;

  for (cur = clist_begin(moves); cur != NULL; cur = clist_next(cur)) {
    struct mailactivesync_move * move;

    move = clist_content(cur);
    if ((move != NULL) && (move->src_msg_id != NULL) &&
        (strcmp(move->src_msg_id, src_msg_id) == 0))
      return move;
  }

  return NULL;
}

static int move_items_result_add_request_context(
    struct mailactivesync_move_items_result * result,
    clist * moves)
{
  clistiter * cur;
  int r;

  if ((result == NULL) || (moves == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  for (cur = clist_begin(result->responses); cur != NULL;
      cur = clist_next(cur)) {
    struct mailactivesync_move_response * response;
    struct mailactivesync_move * move;

    response = clist_content(cur);
    move = find_move_by_src_msg_id(moves, response->src_msg_id);
    if (move == NULL)
      continue;

    r = replace_string(&response->src_folder_id, move->src_folder_id);
    if (r != MAILACTIVESYNC_NO_ERROR)
      return r;
    r = replace_string(&response->dst_folder_id, move->dst_folder_id);
    if (r != MAILACTIVESYNC_NO_ERROR)
      return r;
  }

  return MAILACTIVESYNC_NO_ERROR;
}

int mailactivesync_command_move_items(mailactivesync * session,
    clist * moves,
    struct mailactivesync_move_items_result ** result)
{
  struct mailactivesync_wbxml_document * response_document;
  struct mailactivesync_wbxml_node * root;
  int response_error;
  int r;

  if ((session == NULL) || (moves == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  response_document = NULL;
  root = NULL;
  response_error = MAILACTIVESYNC_NO_ERROR;

  r = build_move_items_request(moves, &root);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  r = post_wbxml_document(session, "MoveItems", NULL, root,
      &response_document);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;

  r = parse_move_items_response(response_document, result);
  if ((r != MAILACTIVESYNC_NO_ERROR) && (* result == NULL))
    goto cleanup;
  response_error = r;

  r = move_items_result_add_request_context(* result, moves);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    mailactivesync_move_items_result_free(* result);
    * result = NULL;
    goto cleanup;
  }
  r = response_error;

 cleanup:
  mailactivesync_wbxml_document_free(response_document);
  mailactivesync_wbxml_node_free(root);
  return r;
}

static int build_ping_request(struct mailactivesync_ping_request * request,
    struct mailactivesync_wbxml_node ** result)
{
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_node * folders;
  clistiter * cur;
  int r;

  if ((request == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  if ((request->heartbeat_interval == 0) &&
      ((request->collection_ids == NULL) ||
        (clist_count(request->collection_ids) == 0)))
    return MAILACTIVESYNC_NO_ERROR;

  root = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_PING,
      MAILACTIVESYNC_PING_PING);
  if (root == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  if (request->heartbeat_interval != 0) {
    r = node_add_uint(root, MAILACTIVESYNC_CP_PING,
        MAILACTIVESYNC_PING_HEARTBEAT_INTERVAL,
        request->heartbeat_interval);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
  }

  if ((request->collection_ids != NULL) &&
      (clist_count(request->collection_ids) > 0)) {
    folders = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_PING,
        MAILACTIVESYNC_PING_FOLDERS);
    if (folders == NULL) {
      r = MAILACTIVESYNC_ERROR_MEMORY;
      goto err;
    }

    for (cur = clist_begin(request->collection_ids); cur != NULL;
        cur = clist_next(cur)) {
      struct mailactivesync_wbxml_node * folder;
      const char * collection_id;

      collection_id = clist_content(cur);
      if (collection_id == NULL) {
        mailactivesync_wbxml_node_free(folders);
        r = MAILACTIVESYNC_ERROR_BAD_STATE;
        goto err;
      }

      folder = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_PING,
          MAILACTIVESYNC_PING_FOLDER);
      if (folder == NULL) {
        mailactivesync_wbxml_node_free(folders);
        r = MAILACTIVESYNC_ERROR_MEMORY;
        goto err;
      }

      r = node_add_text(folder, MAILACTIVESYNC_CP_PING,
          MAILACTIVESYNC_PING_ID, collection_id);
      if (r != MAILACTIVESYNC_NO_ERROR)
        goto err_folder;
      r = node_add_text(folder, MAILACTIVESYNC_CP_PING,
          MAILACTIVESYNC_PING_CLASS, "Email");
      if (r != MAILACTIVESYNC_NO_ERROR)
        goto err_folder;

      r = mailactivesync_wbxml_node_add_child(folders, folder);
      if (r != MAILACTIVESYNC_NO_ERROR)
        goto err_folder;
      folder = NULL;
      continue;

     err_folder:
      mailactivesync_wbxml_node_free(folder);
      mailactivesync_wbxml_node_free(folders);
      goto err;
    }

    r = mailactivesync_wbxml_node_add_child(root, folders);
    if (r != MAILACTIVESYNC_NO_ERROR) {
      mailactivesync_wbxml_node_free(folders);
      goto err;
    }
  }

  * result = root;
  return MAILACTIVESYNC_NO_ERROR;

 err:
  mailactivesync_wbxml_node_free(root);
  return r;
}

static int parse_ping_changed_folders(
    struct mailactivesync_wbxml_node * folders,
    clist * changed_collection_ids)
{
  clistiter * cur;
  int r;

  if ((folders == NULL) || (changed_collection_ids == NULL))
    return MAILACTIVESYNC_NO_ERROR;

  for (cur = folders->children != NULL ? clist_begin(folders->children) :
      NULL; cur != NULL; cur = clist_next(cur)) {
    struct mailactivesync_wbxml_node * folder;

    folder = clist_content(cur);
    if ((folder->code_page != MAILACTIVESYNC_CP_PING) ||
        (folder->token != MAILACTIVESYNC_PING_FOLDER))
      continue;
    if (folder->text == NULL)
      return MAILACTIVESYNC_ERROR_PROTOCOL;

    r = append_string_to_list(changed_collection_ids, folder->text);
    if (r != MAILACTIVESYNC_NO_ERROR)
      return r;
  }

  return MAILACTIVESYNC_NO_ERROR;
}

static int parse_ping_response(
    struct mailactivesync_wbxml_document * response_document,
    struct mailactivesync_ping_result ** result)
{
  struct mailactivesync_ping_result * parsed;
  const char * status;
  int r;

  if ((response_document == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  if ((response_document->root == NULL) ||
      (response_document->root->code_page != MAILACTIVESYNC_CP_PING) ||
      (response_document->root->token != MAILACTIVESYNC_PING_PING))
    return MAILACTIVESYNC_ERROR_PROTOCOL;

  status = node_child_text(response_document->root, MAILACTIVESYNC_CP_PING,
      MAILACTIVESYNC_PING_STATUS);
  if (status == NULL)
    return MAILACTIVESYNC_ERROR_PROTOCOL;

  parsed = ping_result_new();
  if (parsed == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  parsed->status = atoi(status);
  parsed->heartbeat_interval = (uint32_t) node_child_int(
      response_document->root, MAILACTIVESYNC_CP_PING,
      MAILACTIVESYNC_PING_HEARTBEAT_INTERVAL);
  parsed->max_folders = (uint32_t) node_child_int(response_document->root,
      MAILACTIVESYNC_CP_PING, MAILACTIVESYNC_PING_MAX_FOLDERS);

  r = parse_ping_changed_folders(node_child(response_document->root,
      MAILACTIVESYNC_CP_PING, MAILACTIVESYNC_PING_FOLDERS),
      parsed->changed_collection_ids);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    mailactivesync_ping_result_free(parsed);
    return r;
  }

  * result = parsed;
  return MAILACTIVESYNC_NO_ERROR;
}

int mailactivesync_command_ping(mailactivesync * session,
    struct mailactivesync_ping_request * request,
    struct mailactivesync_ping_result ** result)
{
  struct mailactivesync_http_response * response;
  struct mailactivesync_wbxml_document * response_document;
  struct mailactivesync_wbxml_node * root;
  unsigned char * encoded;
  size_t encoded_len;
  time_t timeout;
  int r;

  if ((session == NULL) || (request == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  response = NULL;
  response_document = NULL;
  root = NULL;
  encoded = NULL;
  encoded_len = 0;

  r = build_ping_request(request, &root);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;
  if (root != NULL) {
    r = encode_document(root, &encoded, &encoded_len);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto cleanup;
  }

  timeout = request->heartbeat_interval != 0 ?
      (time_t) request->heartbeat_interval + 30 : 60;
  r = command_post_with_timeout(session, "Ping", NULL, encoded, encoded_len,
      timeout, &response);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;

  if ((response->body == NULL) || (response->body_len == 0)) {
    r = MAILACTIVESYNC_ERROR_PROTOCOL;
    goto cleanup;
  }
  if (!response_body_is_wbxml(response)) {
    r = MAILACTIVESYNC_ERROR_RESPONSE_NOT_WBXML;
    goto cleanup;
  }

  r = mailactivesync_wbxml_decode(response->body, response->body_len,
      &response_document);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;

  r = parse_ping_response(response_document, result);

 cleanup:
  free(encoded);
  mailactivesync_wbxml_document_free(response_document);
  mailactivesync_http_response_free(response);
  mailactivesync_wbxml_node_free(root);
  return r;
}

static struct mailactivesync_sync_result * sync_result_new(void)
{
  struct mailactivesync_sync_result * result;

  result = malloc(sizeof(* result));
  if (result == NULL)
    return NULL;

  result->collection_id = NULL;
  result->sync_key = NULL;
  result->status = 0;
  result->more_available = 0;
  result->empty_response = 0;
  result->sync_key_from_response = 0;
  result->limit = 0;
  result->added = clist_new();
  result->changed = clist_new();
  result->deleted = clist_new();
  result->command_responses = clist_new();
  result->collections = clist_new();
  if ((result->added == NULL) || (result->changed == NULL) ||
      (result->deleted == NULL) || (result->command_responses == NULL) ||
      (result->collections == NULL)) {
    mailactivesync_sync_result_free(result);
    return NULL;
  }

  return result;
}

static int copy_node_payload(char ** result, size_t * result_len,
    struct mailactivesync_wbxml_node * node)
{
  char * copied;
  size_t len;

  if ((result == NULL) || (result_len == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  * result_len = 0;
  if (node == NULL)
    return MAILACTIVESYNC_NO_ERROR;

  if (node->opaque != NULL) {
    len = node->opaque_len;
    copied = malloc(len + 1);
    if (copied == NULL)
      return MAILACTIVESYNC_ERROR_MEMORY;
    memcpy(copied, node->opaque, len);
  }
  else if (node->text != NULL) {
    len = strlen(node->text);
    copied = malloc(len + 1);
    if (copied == NULL)
      return MAILACTIVESYNC_ERROR_MEMORY;
    memcpy(copied, node->text, len);
  }
  else {
    return MAILACTIVESYNC_NO_ERROR;
  }

  copied[len] = '\0';
  * result = copied;
  * result_len = len;
  return MAILACTIVESYNC_NO_ERROR;
}

static int parse_attachment(struct mailactivesync_wbxml_node * node,
    struct mailactivesync_attachment ** result)
{
  struct mailactivesync_attachment * attachment;

  if ((node == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  attachment = calloc(1, sizeof(* attachment));
  if (attachment == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  attachment->display_name = dup_node_text(node,
      MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_DISPLAY_NAME);
  attachment->file_reference = dup_node_text(node,
      MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_FILE_REFERENCE);
  attachment->method = node_child_int(node, MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_METHOD);
  attachment->content_id = dup_node_text(node,
      MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_CONTENT_ID);
  attachment->content_location = dup_node_text(node,
      MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_CONTENT_LOCATION);
  attachment->is_inline = node_child_int(node,
      MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_IS_INLINE);
  attachment->content_type = dup_node_text(node,
      MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_CONTENT_TYPE);
  attachment->estimated_data_size = (uint32_t) node_child_int(node,
      MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_ESTIMATED_DATA_SIZE);

  * result = attachment;
  return MAILACTIVESYNC_NO_ERROR;
}

static void parse_free_attachment_item(void * value, void * data)
{
  (void) data;
  mailactivesync_attachment_free(value);
}

static void parse_free_body_part_item(void * value, void * data)
{
  (void) data;
  mailactivesync_body_part_free(value);
}

static int parse_attachments(struct mailactivesync_wbxml_node * node,
    clist ** result)
{
  struct mailactivesync_wbxml_node * attachments_node;
  clist * attachments;
  clistiter * cur;

  if ((node == NULL) || (result == NULL))
    return MAILACTIVESYNC_NO_ERROR;

  * result = NULL;
  attachments_node = node_child(node, MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_ATTACHMENTS);
  if (attachments_node == NULL)
    return MAILACTIVESYNC_NO_ERROR;

  attachments = clist_new();
  if (attachments == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  for (cur = attachments_node->children != NULL ?
      clist_begin(attachments_node->children) : NULL;
      cur != NULL; cur = clist_next(cur)) {
    struct mailactivesync_wbxml_node * child;
    struct mailactivesync_attachment * attachment;
    int r;

    child = clist_content(cur);
    if ((child == NULL) ||
        (child->code_page != MAILACTIVESYNC_CP_AIRSYNCBASE) ||
        (child->token != MAILACTIVESYNC_AIRSYNCBASE_ATTACHMENT))
      continue;

    attachment = NULL;
    r = parse_attachment(child, &attachment);
    if (r != MAILACTIVESYNC_NO_ERROR) {
      clist_foreach(attachments, parse_free_attachment_item, NULL);
      clist_free(attachments);
      return r;
    }

    if (clist_append(attachments, attachment) < 0) {
      mailactivesync_attachment_free(attachment);
      clist_foreach(attachments, parse_free_attachment_item, NULL);
      clist_free(attachments);
      return MAILACTIVESYNC_ERROR_MEMORY;
    }
  }

  * result = attachments;
  return MAILACTIVESYNC_NO_ERROR;
}

static int parse_body(struct mailactivesync_wbxml_node * node,
    struct mailactivesync_airsyncbase_body ** result)
{
  struct mailactivesync_airsyncbase_body * body;
  struct mailactivesync_wbxml_node * data;
  int r;

  if ((node == NULL) || (result == NULL))
    return MAILACTIVESYNC_NO_ERROR;

  body = calloc(1, sizeof(* body));
  if (body == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  body->type = node_child_int(node, MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_TYPE);
  body->estimated_data_size = (uint32_t) node_child_int(node,
      MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_ESTIMATED_DATA_SIZE);
  body->truncated = node_child_int(node, MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_TRUNCATED);
  body->native_body_type = node_child_int(node, MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_NATIVE_BODY_TYPE);
  body->content_type = dup_node_text(node, MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_CONTENT_TYPE);
  body->preview = dup_node_text(node, MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_PREVIEW);

  data = node_child(node, MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_DATA);
  r = copy_node_payload(&body->data, &body->data_len, data);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    mailactivesync_airsyncbase_body_free(body);
    return r;
  }
  r = parse_attachments(node, &body->attachments);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    mailactivesync_airsyncbase_body_free(body);
    return r;
  }

  * result = body;
  return MAILACTIVESYNC_NO_ERROR;
}

static int parse_body_part(struct mailactivesync_wbxml_node * node,
    struct mailactivesync_body_part ** result)
{
  struct mailactivesync_body_part * body_part;
  struct mailactivesync_wbxml_node * data;
  int r;

  if ((node == NULL) || (result == NULL))
    return MAILACTIVESYNC_NO_ERROR;

  body_part = calloc(1, sizeof(* body_part));
  if (body_part == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  body_part->status = node_child_int(node, MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_STATUS);
  body_part->type = node_child_int(node, MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_TYPE);
  body_part->estimated_data_size = (uint32_t) node_child_int(node,
      MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_ESTIMATED_DATA_SIZE);
  body_part->truncated = node_child_int(node, MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_TRUNCATED);
  body_part->preview = dup_node_text(node, MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_PREVIEW);

  data = node_child(node, MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_DATA);
  r = copy_node_payload(&body_part->data, &body_part->data_len, data);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    mailactivesync_body_part_free(body_part);
    return r;
  }

  * result = body_part;
  return MAILACTIVESYNC_NO_ERROR;
}

static int parse_body_parts(struct mailactivesync_wbxml_node * parent,
    clist ** result)
{
  clist * body_parts;
  clistiter * cur;

  if ((parent == NULL) || (result == NULL))
    return MAILACTIVESYNC_NO_ERROR;

  body_parts = NULL;
  for (cur = parent->children != NULL ? clist_begin(parent->children) : NULL;
      cur != NULL; cur = clist_next(cur)) {
    struct mailactivesync_wbxml_node * child;
    struct mailactivesync_body_part * body_part;
    int r;

    child = clist_content(cur);
    if ((child->code_page != MAILACTIVESYNC_CP_AIRSYNCBASE) ||
        (child->token != MAILACTIVESYNC_AIRSYNCBASE_BODY_PART))
      continue;

    if (body_parts == NULL) {
      body_parts = clist_new();
      if (body_parts == NULL)
        return MAILACTIVESYNC_ERROR_MEMORY;
    }

    body_part = NULL;
    r = parse_body_part(child, &body_part);
    if (r != MAILACTIVESYNC_NO_ERROR) {
      clist_foreach(body_parts, parse_free_body_part_item, NULL);
      clist_free(body_parts);
      return r;
    }
    if (clist_append(body_parts, body_part) < 0) {
      mailactivesync_body_part_free(body_part);
      clist_foreach(body_parts, parse_free_body_part_item, NULL);
      clist_free(body_parts);
      return MAILACTIVESYNC_ERROR_MEMORY;
    }
  }

  * result = body_parts;
  return MAILACTIVESYNC_NO_ERROR;
}

static int parse_legacy_email_body(
    struct mailactivesync_wbxml_node * application_data,
    struct mailactivesync_airsyncbase_body ** result)
{
  struct mailactivesync_airsyncbase_body * body;
  struct mailactivesync_wbxml_node * data;
  int r;

  if ((application_data == NULL) || (result == NULL))
    return MAILACTIVESYNC_NO_ERROR;

  data = node_child(application_data, MAILACTIVESYNC_CP_EMAIL,
      MAILACTIVESYNC_EMAIL_BODY);
  if (data == NULL)
    return MAILACTIVESYNC_NO_ERROR;

  body = calloc(1, sizeof(* body));
  if (body == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  body->type = MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_PLAIN_TEXT;
  body->estimated_data_size = (uint32_t) node_child_int(application_data,
      MAILACTIVESYNC_CP_EMAIL, MAILACTIVESYNC_EMAIL_BODY_SIZE);
  body->truncated = node_child_int(application_data, MAILACTIVESYNC_CP_EMAIL,
      MAILACTIVESYNC_EMAIL_BODY_TRUNCATED);

  r = copy_node_payload(&body->data, &body->data_len, data);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    mailactivesync_airsyncbase_body_free(body);
    return r;
  }

  * result = body;
  return MAILACTIVESYNC_NO_ERROR;
}

static void parse_message_flag(struct mailactivesync_wbxml_node * application_data,
    struct mailactivesync_message * message)
{
  struct mailactivesync_wbxml_node * flag;
  const char * status;

  flag = node_child(application_data, MAILACTIVESYNC_CP_EMAIL,
      MAILACTIVESYNC_EMAIL_FLAG);
  if (flag == NULL)
    return;

  status = node_child_text(flag, MAILACTIVESYNC_CP_EMAIL,
      MAILACTIVESYNC_EMAIL_STATUS);
  if (status != NULL) {
    message->flag_status = atoi(status);
    message->flagged = message->flag_status != 0;
  }
  else {
    message->flagged = (flag->children != NULL) &&
        (clist_count(flag->children) > 0);
  }
  message->flag_type = dup_node_text(flag, MAILACTIVESYNC_CP_EMAIL,
      MAILACTIVESYNC_EMAIL_FLAG_TYPE);
  message->flag_complete_time = dup_node_text(flag, MAILACTIVESYNC_CP_EMAIL,
      MAILACTIVESYNC_EMAIL_COMPLETE_TIME);
}

static int parse_message_application_data(
    struct mailactivesync_wbxml_node * application_data,
    struct mailactivesync_message * message)
{
  struct mailactivesync_wbxml_node * categories;
  clistiter * cur;
  int r;

  if (application_data == NULL)
    return MAILACTIVESYNC_NO_ERROR;

  message->subject = dup_node_text(application_data,
      MAILACTIVESYNC_CP_EMAIL, MAILACTIVESYNC_EMAIL_SUBJECT);
  message->from = dup_node_text(application_data,
      MAILACTIVESYNC_CP_EMAIL, MAILACTIVESYNC_EMAIL_FROM);
  message->to = dup_node_text(application_data,
      MAILACTIVESYNC_CP_EMAIL, MAILACTIVESYNC_EMAIL_TO);
  message->cc = dup_node_text(application_data,
      MAILACTIVESYNC_CP_EMAIL, MAILACTIVESYNC_EMAIL_CC);
  message->reply_to = dup_node_text(application_data,
      MAILACTIVESYNC_CP_EMAIL, MAILACTIVESYNC_EMAIL_REPLY_TO);
  message->date_received = dup_node_text(application_data,
      MAILACTIVESYNC_CP_EMAIL, MAILACTIVESYNC_EMAIL_DATE_RECEIVED);
  message->display_to = dup_node_text(application_data,
      MAILACTIVESYNC_CP_EMAIL, MAILACTIVESYNC_EMAIL_DISPLAY_TO);
  message->message_class = dup_node_text(application_data,
      MAILACTIVESYNC_CP_EMAIL, MAILACTIVESYNC_EMAIL_MESSAGE_CLASS);
  message->thread_topic = dup_node_text(application_data,
      MAILACTIVESYNC_CP_EMAIL, MAILACTIVESYNC_EMAIL_THREAD_TOPIC);
  message->internet_cpid = dup_node_text(application_data,
      MAILACTIVESYNC_CP_EMAIL, MAILACTIVESYNC_EMAIL_INTERNET_CPID);
  message->content_class = dup_node_text(application_data,
      MAILACTIVESYNC_CP_EMAIL, MAILACTIVESYNC_EMAIL_CONTENT_CLASS);
  message->importance = node_child_int(application_data,
      MAILACTIVESYNC_CP_EMAIL, MAILACTIVESYNC_EMAIL_IMPORTANCE);
  message->read = node_child_int(application_data,
      MAILACTIVESYNC_CP_EMAIL, MAILACTIVESYNC_EMAIL_READ);
  parse_message_flag(application_data, message);

  r = copy_node_payload(&message->conversation_id,
      &message->conversation_id_len,
      node_child(application_data, MAILACTIVESYNC_CP_EMAIL2,
          MAILACTIVESYNC_EMAIL2_CONVERSATION_ID));
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;
  r = copy_node_payload(&message->conversation_index,
      &message->conversation_index_len,
      node_child(application_data, MAILACTIVESYNC_CP_EMAIL2,
          MAILACTIVESYNC_EMAIL2_CONVERSATION_INDEX));
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  categories = node_child(application_data, MAILACTIVESYNC_CP_EMAIL,
      MAILACTIVESYNC_EMAIL_CATEGORIES);
  if (categories != NULL) {
    message->categories = clist_new();
    if (message->categories == NULL)
      return MAILACTIVESYNC_ERROR_MEMORY;

    for (cur = categories->children != NULL ? clist_begin(categories->children) :
        NULL; cur != NULL; cur = clist_next(cur)) {
      struct mailactivesync_wbxml_node * category;
      char * value;

      category = clist_content(cur);
      if ((category->code_page != MAILACTIVESYNC_CP_EMAIL) ||
          (category->token != MAILACTIVESYNC_EMAIL_CATEGORY) ||
          (category->text == NULL))
        continue;

      value = strdup(category->text);
      if (value == NULL)
        return MAILACTIVESYNC_ERROR_MEMORY;
      if (clist_append(message->categories, value) < 0) {
        free(value);
        return MAILACTIVESYNC_ERROR_MEMORY;
      }
    }
  }

  r = parse_body(node_child(application_data, MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_BODY), &message->body);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;
  if ((message->body != NULL) && (message->body->attachments == NULL)) {
    r = parse_attachments(application_data, &message->body->attachments);
    if (r != MAILACTIVESYNC_NO_ERROR)
      return r;
  }
  if (message->body == NULL) {
    r = parse_legacy_email_body(application_data, &message->body);
    if (r != MAILACTIVESYNC_NO_ERROR)
      return r;
  }
  r = parse_body_parts(application_data, &message->body_parts);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  if ((message->body != NULL) &&
      (message->body->type ==
          MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_MIME) &&
      (message->body->data != NULL)) {
    message->mime = malloc(message->body->data_len + 1);
    if (message->mime == NULL)
      return MAILACTIVESYNC_ERROR_MEMORY;
    memcpy(message->mime, message->body->data, message->body->data_len);
    message->mime[message->body->data_len] = '\0';
    message->mime_len = message->body->data_len;
  }
  else {
    r = copy_node_payload(&message->mime, &message->mime_len,
        node_child(application_data, MAILACTIVESYNC_CP_EMAIL,
            MAILACTIVESYNC_EMAIL_MIME_DATA));
    if (r != MAILACTIVESYNC_NO_ERROR)
      return r;
  }
  if (message->mime != NULL) {
    message->mime_size = (uint32_t) node_child_int(application_data,
        MAILACTIVESYNC_CP_EMAIL, MAILACTIVESYNC_EMAIL_MIME_SIZE);
    message->mime_truncated = node_child_int(application_data,
        MAILACTIVESYNC_CP_EMAIL, MAILACTIVESYNC_EMAIL_MIME_TRUNCATED);
  }

  return MAILACTIVESYNC_NO_ERROR;
}

static int parse_message_change(struct mailactivesync_wbxml_node * node,
    clist * list)
{
  struct mailactivesync_message * message;
  int r;

  message = calloc(1, sizeof(* message));
  if (message == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  message->server_id = dup_node_text(node, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_SERVER_ID);
  if (message->server_id == NULL) {
    mailactivesync_message_free(message);
    return MAILACTIVESYNC_ERROR_PROTOCOL;
  }

  r = parse_message_application_data(node_child(node,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_APPLICATION_DATA),
      message);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    mailactivesync_message_free(message);
    return r;
  }

  if (clist_append(list, message) < 0) {
    mailactivesync_message_free(message);
    return MAILACTIVESYNC_ERROR_MEMORY;
  }

  return MAILACTIVESYNC_NO_ERROR;
}

static int sync_command_token(int type, uint8_t * token)
{
  if (token == NULL)
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  switch (type) {
  case MAILACTIVESYNC_SYNC_COMMAND_ADD:
    * token = MAILACTIVESYNC_AIRSYNC_ADD;
    return MAILACTIVESYNC_NO_ERROR;
  case MAILACTIVESYNC_SYNC_COMMAND_CHANGE:
    * token = MAILACTIVESYNC_AIRSYNC_CHANGE;
    return MAILACTIVESYNC_NO_ERROR;
  case MAILACTIVESYNC_SYNC_COMMAND_DELETE:
    * token = MAILACTIVESYNC_AIRSYNC_DELETE;
    return MAILACTIVESYNC_NO_ERROR;
  case MAILACTIVESYNC_SYNC_COMMAND_FETCH:
    * token = MAILACTIVESYNC_AIRSYNC_FETCH;
    return MAILACTIVESYNC_NO_ERROR;
  default:
    return MAILACTIVESYNC_ERROR_BAD_STATE;
  }
}

static int build_sync_application_data(
    struct mailactivesync_sync_command * command,
    struct mailactivesync_wbxml_node ** result)
{
  struct mailactivesync_wbxml_node * app_data;
  clistiter * cur;
  int r;

  if ((command == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  if ((command->application_data == NULL) ||
      (clist_count(command->application_data) == 0))
    return MAILACTIVESYNC_NO_ERROR;

  app_data = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_APPLICATION_DATA);
  if (app_data == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  for (cur = clist_begin(command->application_data); cur != NULL;
      cur = clist_next(cur)) {
    struct mailactivesync_wbxml_node * child;
    struct mailactivesync_wbxml_node * cloned_child;

    child = clist_content(cur);
    cloned_child = node_clone(child);
    if (cloned_child == NULL) {
      r = MAILACTIVESYNC_ERROR_MEMORY;
      goto err;
    }
    r = mailactivesync_wbxml_node_add_child(app_data, cloned_child);
    if (r != MAILACTIVESYNC_NO_ERROR) {
      mailactivesync_wbxml_node_free(cloned_child);
      goto err;
    }
  }

  * result = app_data;
  return MAILACTIVESYNC_NO_ERROR;

 err:
  mailactivesync_wbxml_node_free(app_data);
  return r;
}

static int build_sync_client_command(
    struct mailactivesync_sync_command * command,
    struct mailactivesync_wbxml_node ** result)
{
  struct mailactivesync_wbxml_node * node;
  struct mailactivesync_wbxml_node * app_data;
  uint8_t token;
  int r;

  if ((command == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  r = sync_command_token(command->type, &token);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  if (((command->type == MAILACTIVESYNC_SYNC_COMMAND_ADD) &&
        (command->client_id == NULL)) ||
      ((command->type != MAILACTIVESYNC_SYNC_COMMAND_ADD) &&
        (command->server_id == NULL)))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  node = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC, token);
  if (node == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  if (command->client_id != NULL) {
    r = node_add_text(node, MAILACTIVESYNC_CP_AIRSYNC,
        MAILACTIVESYNC_AIRSYNC_CLIENT_ID, command->client_id);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
  }
  if (command->server_id != NULL) {
    r = node_add_text(node, MAILACTIVESYNC_CP_AIRSYNC,
        MAILACTIVESYNC_AIRSYNC_SERVER_ID, command->server_id);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
  }
  if (command->collection_class != NULL) {
    r = node_add_text(node, MAILACTIVESYNC_CP_AIRSYNC,
        MAILACTIVESYNC_AIRSYNC_CLASS, command->collection_class);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
  }

  app_data = NULL;
  r = build_sync_application_data(command, &app_data);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  if (app_data != NULL) {
    r = mailactivesync_wbxml_node_add_child(node, app_data);
    if (r != MAILACTIVESYNC_NO_ERROR) {
      mailactivesync_wbxml_node_free(app_data);
      goto err;
    }
  }

  * result = node;
  return MAILACTIVESYNC_NO_ERROR;

 err:
  mailactivesync_wbxml_node_free(node);
  return r;
}

static int build_sync_client_commands(
    struct mailactivesync_wbxml_node * collection,
    struct mailactivesync_sync_request * request)
{
  struct mailactivesync_wbxml_node * commands;
  clistiter * cur;
  int r;

  if ((collection == NULL) || (request == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;
  if ((request->client_commands == NULL) ||
      (clist_count(request->client_commands) == 0))
    return MAILACTIVESYNC_NO_ERROR;

  commands = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_COMMANDS);
  if (commands == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  for (cur = clist_begin(request->client_commands); cur != NULL;
      cur = clist_next(cur)) {
    struct mailactivesync_sync_command * command;
    struct mailactivesync_wbxml_node * command_node;

    command = clist_content(cur);
    command_node = NULL;
    r = build_sync_client_command(command, &command_node);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
    r = mailactivesync_wbxml_node_add_child(commands, command_node);
    if (r != MAILACTIVESYNC_NO_ERROR) {
      mailactivesync_wbxml_node_free(command_node);
      goto err;
    }
  }

  r = mailactivesync_wbxml_node_add_child(collection, commands);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;

  return MAILACTIVESYNC_NO_ERROR;

 err:
  mailactivesync_wbxml_node_free(commands);
  return r;
}

static int session_uses_activesync_25(mailactivesync * session)
{
  return (session != NULL) && (session->as_protocol_version != NULL) &&
      (strcmp(session->as_protocol_version, "2.5") == 0);
}

static int session_supports_sync_wait(mailactivesync * session)
{
  if ((session == NULL) || (session->as_protocol_version == NULL))
    return 1;
  return (strcmp(session->as_protocol_version, "2.5") != 0) &&
      (strcmp(session->as_protocol_version, "12.0") != 0);
}

static int session_supports_sync_conversation_mode(mailactivesync * session)
{
  if ((session == NULL) || (session->as_protocol_version == NULL))
    return 1;
  return (strcmp(session->as_protocol_version, "14.0") == 0) ||
      (strcmp(session->as_protocol_version, "14.1") == 0) ||
      (strcmp(session->as_protocol_version, "16.0") == 0) ||
      (strcmp(session->as_protocol_version, "16.1") == 0);
}

static int sync_request_has_body_preferences(
    struct mailactivesync_sync_request * request)
{
  return (request != NULL) &&
      (request->body_preferences != NULL) &&
      (clist_count(request->body_preferences) > 0);
}

static int sync_request_has_supported_properties(
    struct mailactivesync_sync_request * request)
{
  return (request != NULL) &&
      (request->supported_properties != NULL) &&
      (clist_count(request->supported_properties) > 0);
}

static int build_sync_body_preference(
    struct mailactivesync_wbxml_node * options,
    struct mailactivesync_body_preference * preference)
{
  struct mailactivesync_wbxml_node * body_preference;
  int r;

  if ((options == NULL) || (preference == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  body_preference = mailactivesync_wbxml_node_new(
      MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_BODY_PREFERENCE);
  if (body_preference == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  r = node_add_uint(body_preference, MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_TYPE, (uint32_t) preference->type);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  if (preference->truncation_size != 0) {
    r = node_add_uint(body_preference, MAILACTIVESYNC_CP_AIRSYNCBASE,
        MAILACTIVESYNC_AIRSYNCBASE_TRUNCATION_SIZE,
        preference->truncation_size);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
  }
  if (preference->all_or_none) {
    r = node_add_text(body_preference, MAILACTIVESYNC_CP_AIRSYNCBASE,
        MAILACTIVESYNC_AIRSYNCBASE_ALL_OR_NONE, "1");
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
  }

  r = mailactivesync_wbxml_node_add_child(options, body_preference);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;

  return MAILACTIVESYNC_NO_ERROR;

 err:
  mailactivesync_wbxml_node_free(body_preference);
  return r;
}

static int build_sync_supported(
    struct mailactivesync_wbxml_node * collection,
    struct mailactivesync_sync_request * request)
{
  struct mailactivesync_wbxml_node * supported;
  clistiter * cur;
  int r;

  if ((collection == NULL) || (request == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;
  if (!sync_request_has_supported_properties(request))
    return MAILACTIVESYNC_NO_ERROR;

  supported = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_SUPPORTED);
  if (supported == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  for (cur = clist_begin(request->supported_properties); cur != NULL;
      cur = clist_next(cur)) {
    struct mailactivesync_wbxml_node * property;
    struct mailactivesync_wbxml_node * cloned;

    property = clist_content(cur);
    cloned = node_clone(property);
    if (cloned == NULL) {
      r = MAILACTIVESYNC_ERROR_MEMORY;
      goto err;
    }
    r = mailactivesync_wbxml_node_add_child(supported, cloned);
    if (r != MAILACTIVESYNC_NO_ERROR) {
      mailactivesync_wbxml_node_free(cloned);
      goto err;
    }
  }

  r = mailactivesync_wbxml_node_add_child(collection, supported);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;

  return MAILACTIVESYNC_NO_ERROR;

 err:
  mailactivesync_wbxml_node_free(supported);
  return r;
}

static int build_sync_collection_node(mailactivesync * session,
    struct mailactivesync_sync_request * request,
    struct mailactivesync_wbxml_node ** result)
{
  struct mailactivesync_wbxml_node * collection;
  struct mailactivesync_wbxml_node * active_collection;
  struct mailactivesync_wbxml_node * options;
  const char * collection_class;
  int activesync_25;
  int r;

  * result = NULL;
  activesync_25 = session_uses_activesync_25(session);
  if ((request->has_wait || request->has_heartbeat_interval) &&
      !session_supports_sync_wait(session))
    return MAILACTIVESYNC_ERROR_NOT_IMPLEMENTED;
  if (request->has_conversation_mode &&
      !session_supports_sync_conversation_mode(session))
    return MAILACTIVESYNC_ERROR_NOT_IMPLEMENTED;
  collection = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_COLLECTION);
  active_collection = collection;
  if (collection == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  r = node_add_text(active_collection, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_SYNC_KEY,
      request->sync_key != NULL ? request->sync_key : "0");
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  r = node_add_text(active_collection, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_COLLECTION_ID, request->collection_id);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  collection_class = request->collection_class != NULL ?
      request->collection_class :
      sync_request_has_body_preferences(request) ? "Email" : NULL;
  if (activesync_25 && (collection_class != NULL)) {
    r = node_add_text(active_collection, MAILACTIVESYNC_CP_AIRSYNC,
        MAILACTIVESYNC_AIRSYNC_CLASS, collection_class);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
  }
  if (request->has_deletes_as_moves) {
    r = node_add_text(active_collection, MAILACTIVESYNC_CP_AIRSYNC,
        MAILACTIVESYNC_AIRSYNC_DELETES_AS_MOVES,
        request->deletes_as_moves ? "1" : "0");
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
  }
  r = node_add_text(active_collection, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_GET_CHANGES,
      request->get_changes ? "1" : "0");
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  if (request->window_size != 0) {
    r = node_add_uint(active_collection, MAILACTIVESYNC_CP_AIRSYNC,
        MAILACTIVESYNC_AIRSYNC_WINDOW_SIZE, request->window_size);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
  }
  if (request->has_conversation_mode) {
    r = node_add_text(active_collection, MAILACTIVESYNC_CP_AIRSYNC,
        MAILACTIVESYNC_AIRSYNC_CONVERSATION_MODE,
        request->conversation_mode ? "1" : "0");
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
  }

  if (sync_request_has_body_preferences(request) ||
      (!activesync_25 && (request->collection_class != NULL)) ||
      request->has_filter_type ||
      request->has_conflict ||
      request->has_rights_management_support) {
    options = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
        MAILACTIVESYNC_AIRSYNC_OPTIONS);
    if (options == NULL) {
      r = MAILACTIVESYNC_ERROR_MEMORY;
      goto err;
    }
    if (!activesync_25 && (collection_class != NULL)) {
      r = node_add_text(options, MAILACTIVESYNC_CP_AIRSYNC,
          MAILACTIVESYNC_AIRSYNC_CLASS, collection_class);
      if (r != MAILACTIVESYNC_NO_ERROR) {
        mailactivesync_wbxml_node_free(options);
        goto err;
      }
    }
    if (request->has_filter_type) {
      r = node_add_uint(options, MAILACTIVESYNC_CP_AIRSYNC,
          MAILACTIVESYNC_AIRSYNC_FILTER_TYPE, request->filter_type);
      if (r != MAILACTIVESYNC_NO_ERROR) {
        mailactivesync_wbxml_node_free(options);
        goto err;
      }
    }
    if (request->has_conflict) {
      r = node_add_uint(options, MAILACTIVESYNC_CP_AIRSYNC,
          MAILACTIVESYNC_AIRSYNC_CONFLICT, request->conflict);
      if (r != MAILACTIVESYNC_NO_ERROR) {
        mailactivesync_wbxml_node_free(options);
        goto err;
      }
    }
    if (request->has_rights_management_support) {
      r = node_add_text(options, MAILACTIVESYNC_CP_RIGHTSMANAGEMENT,
          MAILACTIVESYNC_RIGHTSMANAGEMENT_RIGHTS_MANAGEMENT_SUPPORT,
          request->rights_management_support ? "1" : "0");
      if (r != MAILACTIVESYNC_NO_ERROR) {
        mailactivesync_wbxml_node_free(options);
        goto err;
      }
    }
    if (sync_request_has_body_preferences(request)) {
      clistiter * cur;

      r = node_add_text(options, MAILACTIVESYNC_CP_AIRSYNC,
          MAILACTIVESYNC_AIRSYNC_MIME_SUPPORT, "2");
      if (r != MAILACTIVESYNC_NO_ERROR) {
        mailactivesync_wbxml_node_free(options);
        goto err;
      }
      for (cur = clist_begin(request->body_preferences); cur != NULL;
          cur = clist_next(cur)) {
        r = build_sync_body_preference(options, clist_content(cur));
        if (r != MAILACTIVESYNC_NO_ERROR) {
          mailactivesync_wbxml_node_free(options);
          goto err;
        }
      }
    }
    r = mailactivesync_wbxml_node_add_child(active_collection, options);
    if (r != MAILACTIVESYNC_NO_ERROR) {
      mailactivesync_wbxml_node_free(options);
      goto err;
    }
  }

  r = build_sync_supported(active_collection, request);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;

  r = build_sync_client_commands(active_collection, request);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;

  * result = collection;
  return MAILACTIVESYNC_NO_ERROR;

 err:
  mailactivesync_wbxml_node_free(collection);
  return r;
}

static int build_sync_request_multi(mailactivesync * session,
    clist * requests,
    struct mailactivesync_wbxml_node ** result)
{
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_node * collections;
  struct mailactivesync_sync_request * first_request;
  clistiter * cur;
  int r;

  if ((requests == NULL) || (clist_count(requests) == 0) ||
      (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  first_request = clist_content(clist_begin(requests));
  root = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_SYNC);
  collections = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_COLLECTIONS);
  if ((root == NULL) || (collections == NULL)) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto err;
  }

  for (cur = clist_begin(requests); cur != NULL; cur = clist_next(cur)) {
    struct mailactivesync_wbxml_node * collection;

    collection = NULL;
    r = build_sync_collection_node(session, clist_content(cur), &collection);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
    r = mailactivesync_wbxml_node_add_child(collections, collection);
    if (r != MAILACTIVESYNC_NO_ERROR) {
      mailactivesync_wbxml_node_free(collection);
      goto err;
    }
  }

  r = mailactivesync_wbxml_node_add_child(root, collections);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  collections = NULL;

  if (first_request->has_wait) {
    r = node_add_uint(root, MAILACTIVESYNC_CP_AIRSYNC,
        MAILACTIVESYNC_AIRSYNC_WAIT, first_request->wait);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
  }
  if (first_request->has_heartbeat_interval) {
    r = node_add_uint(root, MAILACTIVESYNC_CP_AIRSYNC,
        MAILACTIVESYNC_AIRSYNC_HEARTBEAT_INTERVAL,
        first_request->heartbeat_interval);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
  }

  * result = root;
  return MAILACTIVESYNC_NO_ERROR;

 err:
  mailactivesync_wbxml_node_free(collections);
  mailactivesync_wbxml_node_free(root);
  return r;
}

static int build_sync_request(mailactivesync * session,
    struct mailactivesync_sync_request * request,
    struct mailactivesync_wbxml_node ** result)
{
  clist * requests;
  int r;

  if ((request == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  requests = clist_new();
  if (requests == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;
  if (clist_append(requests, request) < 0) {
    clist_free(requests);
    return MAILACTIVESYNC_ERROR_MEMORY;
  }

  r = build_sync_request_multi(session, requests, result);
  clist_free(requests);
  return r;
}

static int parse_sync_commands(struct mailactivesync_wbxml_node * commands,
    struct mailactivesync_sync_result * result)
{
  clistiter * cur;
  int r;

  if (commands == NULL)
    return MAILACTIVESYNC_NO_ERROR;

  for (cur = clist_begin(commands->children); cur != NULL;
      cur = clist_next(cur)) {
    struct mailactivesync_wbxml_node * child;

    child = clist_content(cur);
    if (child->code_page != MAILACTIVESYNC_CP_AIRSYNC)
      continue;

    switch (child->token) {
    case MAILACTIVESYNC_AIRSYNC_ADD:
      r = parse_message_change(child, result->added);
      if (r != MAILACTIVESYNC_NO_ERROR)
        return r;
      break;
    case MAILACTIVESYNC_AIRSYNC_CHANGE:
      r = parse_message_change(child, result->changed);
      if (r != MAILACTIVESYNC_NO_ERROR)
        return r;
      break;
    case MAILACTIVESYNC_AIRSYNC_DELETE:
    case MAILACTIVESYNC_AIRSYNC_SOFT_DELETE:
      r = append_string_to_list(result->deleted,
          node_child_text(child, MAILACTIVESYNC_CP_AIRSYNC,
              MAILACTIVESYNC_AIRSYNC_SERVER_ID));
      if (r != MAILACTIVESYNC_NO_ERROR)
        return r;
      break;
    default:
      break;
    }
  }

  return MAILACTIVESYNC_NO_ERROR;
}

static int parse_sync_command_response(
    struct mailactivesync_wbxml_node * node,
    int type,
    clist * list)
{
  struct mailactivesync_sync_command_response * response;
  struct mailactivesync_wbxml_node * application_data;
  int r;

  if ((node == NULL) || (list == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  response = calloc(1, sizeof(* response));
  if (response == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  response->type = type;
  response->status = node_child_int(node, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_STATUS);
  response->client_id = dup_node_text(node, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_CLIENT_ID);
  response->server_id = dup_node_text(node, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_SERVER_ID);
  response->collection_class = dup_node_text(node, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_CLASS);

  application_data = node_child(node, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_APPLICATION_DATA);
  if (application_data != NULL) {
    response->message = calloc(1, sizeof(* response->message));
    if (response->message == NULL) {
      mailactivesync_sync_command_response_free(response);
      return MAILACTIVESYNC_ERROR_MEMORY;
    }
    if (response->server_id != NULL) {
      response->message->server_id = strdup(response->server_id);
      if (response->message->server_id == NULL) {
        mailactivesync_sync_command_response_free(response);
        return MAILACTIVESYNC_ERROR_MEMORY;
      }
    }
    r = parse_message_application_data(application_data, response->message);
    if (r != MAILACTIVESYNC_NO_ERROR) {
      mailactivesync_sync_command_response_free(response);
      return r;
    }
  }

  if (clist_append(list, response) < 0) {
    mailactivesync_sync_command_response_free(response);
    return MAILACTIVESYNC_ERROR_MEMORY;
  }

  return MAILACTIVESYNC_NO_ERROR;
}

static int parse_sync_responses(struct mailactivesync_wbxml_node * responses,
    struct mailactivesync_sync_result * result)
{
  clistiter * cur;
  int r;

  if (responses == NULL)
    return MAILACTIVESYNC_NO_ERROR;

  for (cur = responses->children != NULL ? clist_begin(responses->children) :
      NULL; cur != NULL; cur = clist_next(cur)) {
    struct mailactivesync_wbxml_node * child;
    int type;

    child = clist_content(cur);
    if (child->code_page != MAILACTIVESYNC_CP_AIRSYNC)
      continue;

    switch (child->token) {
    case MAILACTIVESYNC_AIRSYNC_ADD:
      type = MAILACTIVESYNC_SYNC_COMMAND_ADD;
      break;
    case MAILACTIVESYNC_AIRSYNC_CHANGE:
      type = MAILACTIVESYNC_SYNC_COMMAND_CHANGE;
      break;
    case MAILACTIVESYNC_AIRSYNC_DELETE:
      type = MAILACTIVESYNC_SYNC_COMMAND_DELETE;
      break;
    case MAILACTIVESYNC_AIRSYNC_FETCH:
      type = MAILACTIVESYNC_SYNC_COMMAND_FETCH;
      break;
    default:
      continue;
    }

    r = parse_sync_command_response(child, type, result->command_responses);
    if (r != MAILACTIVESYNC_NO_ERROR)
      return r;
  }

  return MAILACTIVESYNC_NO_ERROR;
}

static int parse_sync_collection_result(
    struct mailactivesync_wbxml_node * collection,
    struct mailactivesync_sync_result * parsed)
{
  const char * collection_id;
  const char * response_sync_key;
  int r;

  if ((collection == NULL) || (parsed == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  collection_id = node_child_text(collection, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_COLLECTION_ID);
  if (collection_id != NULL) {
    parsed->collection_id = strdup(collection_id);
    if (parsed->collection_id == NULL)
      return MAILACTIVESYNC_ERROR_MEMORY;
  }

  response_sync_key = node_child_text(collection, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_SYNC_KEY);
  if (response_sync_key != NULL) {
    parsed->sync_key = strdup(response_sync_key);
    if (parsed->sync_key == NULL)
      return MAILACTIVESYNC_ERROR_MEMORY;
    parsed->sync_key_from_response = 1;
  }
  parsed->status = node_child_int(collection, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_STATUS);
  parsed->more_available = node_child(collection, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_MORE_AVAILABLE) != NULL;

  r = parse_sync_commands(node_child(collection, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_COMMANDS), parsed);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  return parse_sync_responses(node_child(collection, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_RESPONSES), parsed);
}

static int build_provision_request(const char * policy_key, int ack,
    struct mailactivesync_wbxml_node ** result)
{
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_node * policies;
  struct mailactivesync_wbxml_node * policy;
  int r;

  if (result == NULL)
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  root = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_PROVISION,
      MAILACTIVESYNC_PROVISION_PROVISION);
  policies = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_PROVISION,
      MAILACTIVESYNC_PROVISION_POLICIES);
  policy = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_PROVISION,
      MAILACTIVESYNC_PROVISION_POLICY);
  if ((root == NULL) || (policies == NULL) || (policy == NULL)) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto err;
  }

  r = node_add_text(policy, MAILACTIVESYNC_CP_PROVISION,
      MAILACTIVESYNC_PROVISION_POLICY_TYPE,
      "MS-EAS-Provisioning-WBXML");
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;

  if (ack) {
    if (policy_key == NULL) {
      r = MAILACTIVESYNC_ERROR_BAD_STATE;
      goto err;
    }
    r = node_add_text(policy, MAILACTIVESYNC_CP_PROVISION,
        MAILACTIVESYNC_PROVISION_POLICY_KEY, policy_key);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
    r = node_add_text(policy, MAILACTIVESYNC_CP_PROVISION,
        MAILACTIVESYNC_PROVISION_STATUS, "1");
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
  }

  r = mailactivesync_wbxml_node_add_child(policies, policy);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  policy = NULL;
  r = mailactivesync_wbxml_node_add_child(root, policies);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  policies = NULL;

  * result = root;
  return MAILACTIVESYNC_NO_ERROR;

 err:
  mailactivesync_wbxml_node_free(policy);
  mailactivesync_wbxml_node_free(policies);
  mailactivesync_wbxml_node_free(root);
  return r;
}

static int parse_provision_response(
    struct mailactivesync_wbxml_document * response_document,
    struct mailactivesync_provision_result ** result)
{
  struct mailactivesync_provision_result * parsed;
  struct mailactivesync_wbxml_node * policies;
  struct mailactivesync_wbxml_node * policy;
  const char * policy_key;

  if ((response_document == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  if ((response_document->root == NULL) ||
      (response_document->root->code_page != MAILACTIVESYNC_CP_PROVISION) ||
      (response_document->root->token != MAILACTIVESYNC_PROVISION_PROVISION))
    return MAILACTIVESYNC_ERROR_PROTOCOL;

  parsed = calloc(1, sizeof(* parsed));
  if (parsed == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  parsed->status = node_child_int(response_document->root,
      MAILACTIVESYNC_CP_PROVISION, MAILACTIVESYNC_PROVISION_STATUS);
  policies = node_child(response_document->root, MAILACTIVESYNC_CP_PROVISION,
      MAILACTIVESYNC_PROVISION_POLICIES);
  policy = node_child(policies, MAILACTIVESYNC_CP_PROVISION,
      MAILACTIVESYNC_PROVISION_POLICY);
  parsed->policy_status = node_child_int(policy, MAILACTIVESYNC_CP_PROVISION,
      MAILACTIVESYNC_PROVISION_STATUS);
  policy_key = node_child_text(policy, MAILACTIVESYNC_CP_PROVISION,
      MAILACTIVESYNC_PROVISION_POLICY_KEY);
  if (policy_key != NULL) {
    parsed->policy_key = strdup(policy_key);
    if (parsed->policy_key == NULL) {
      mailactivesync_provision_result_free(parsed);
      return MAILACTIVESYNC_ERROR_MEMORY;
    }
  }

  * result = parsed;
  return MAILACTIVESYNC_NO_ERROR;
}

static int post_provision(mailactivesync * session, const char * policy_key,
    int ack, struct mailactivesync_provision_result ** result)
{
  struct mailactivesync_wbxml_node * request;
  struct mailactivesync_wbxml_document * response;
  int r;

  request = NULL;
  response = NULL;

  r = build_provision_request(policy_key, ack, &request);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  r = post_wbxml_document(session, "Provision", NULL, request, &response);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;

  r = parse_provision_response(response, result);

 cleanup:
  mailactivesync_wbxml_document_free(response);
  mailactivesync_wbxml_node_free(request);
  return r;
}

int mailactivesync_command_provision(mailactivesync * session,
    struct mailactivesync_provision_result ** result)
{
  struct mailactivesync_provision_result * initial_result;
  struct mailactivesync_provision_result * ack_result;
  const char * final_policy_key;
  int r;

  if ((session == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  initial_result = NULL;
  ack_result = NULL;

  r = post_provision(session, NULL, 0, &initial_result);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;
  if ((initial_result->status != 1) || (initial_result->policy_status != 1) ||
      (initial_result->policy_key == NULL)) {
    r = MAILACTIVESYNC_ERROR_PROVISION_REQUIRED;
    goto cleanup;
  }

  r = post_provision(session, initial_result->policy_key, 1, &ack_result);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;
  if ((ack_result->status != 1) || (ack_result->policy_status != 1)) {
    r = MAILACTIVESYNC_ERROR_PROVISION_REQUIRED;
    goto cleanup;
  }

  final_policy_key = ack_result->policy_key != NULL ?
      ack_result->policy_key : initial_result->policy_key;
  r = replace_string(&session->as_policy_key, final_policy_key);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;

  * result = ack_result;
  ack_result = NULL;

 cleanup:
  mailactivesync_provision_result_free(ack_result);
  mailactivesync_provision_result_free(initial_result);
  return r;
}

static int settings_add_optional_text(struct mailactivesync_wbxml_node * node,
    uint8_t token, const char * value)
{
  if (value == NULL)
    return MAILACTIVESYNC_NO_ERROR;

  return node_add_text(node, MAILACTIVESYNC_CP_SETTINGS, token, value);
}

static int build_settings_device_information_request(
    const struct mailactivesync_device_information * device_information,
    struct mailactivesync_wbxml_node ** result)
{
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_node * device_node;
  struct mailactivesync_wbxml_node * set_node;
  int r;

  if ((device_information == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  root = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_SETTINGS,
      MAILACTIVESYNC_SETTINGS_SETTINGS);
  device_node = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_SETTINGS,
      MAILACTIVESYNC_SETTINGS_DEVICE_INFORMATION);
  set_node = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_SETTINGS,
      MAILACTIVESYNC_SETTINGS_SET);
  if ((root == NULL) || (device_node == NULL) || (set_node == NULL)) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto err;
  }

  r = settings_add_optional_text(set_node, MAILACTIVESYNC_SETTINGS_MODEL,
      device_information->model);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  r = settings_add_optional_text(set_node, MAILACTIVESYNC_SETTINGS_IMEI,
      device_information->imei);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  r = settings_add_optional_text(set_node,
      MAILACTIVESYNC_SETTINGS_FRIENDLY_NAME,
      device_information->friendly_name);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  r = settings_add_optional_text(set_node, MAILACTIVESYNC_SETTINGS_OS,
      device_information->os);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  r = settings_add_optional_text(set_node,
      MAILACTIVESYNC_SETTINGS_OS_LANGUAGE,
      device_information->os_language);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  r = settings_add_optional_text(set_node,
      MAILACTIVESYNC_SETTINGS_PHONE_NUMBER,
      device_information->phone_number);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  r = settings_add_optional_text(set_node,
      MAILACTIVESYNC_SETTINGS_USER_AGENT,
      device_information->user_agent);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  r = settings_add_optional_text(set_node,
      MAILACTIVESYNC_SETTINGS_MOBILE_OPERATOR,
      device_information->mobile_operator);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;

  r = mailactivesync_wbxml_node_add_child(device_node, set_node);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  set_node = NULL;
  r = mailactivesync_wbxml_node_add_child(root, device_node);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  device_node = NULL;

  * result = root;
  return MAILACTIVESYNC_NO_ERROR;

 err:
  mailactivesync_wbxml_node_free(set_node);
  mailactivesync_wbxml_node_free(device_node);
  mailactivesync_wbxml_node_free(root);
  return r;
}

int mailactivesync_command_settings_set_device_information(
    mailactivesync * session,
    const struct mailactivesync_device_information * device_information,
    struct mailactivesync_settings_result ** result)
{
  struct mailactivesync_wbxml_node * request;
  struct mailactivesync_wbxml_document * response;
  struct mailactivesync_wbxml_node * device_node;
  struct mailactivesync_settings_result * parsed;
  int r;

  if ((session == NULL) || (device_information == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  request = NULL;
  response = NULL;
  parsed = NULL;

  r = build_settings_device_information_request(device_information, &request);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  r = post_wbxml_document(session, "Settings", NULL, request, &response);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;

  if ((response->root == NULL) ||
      (response->root->code_page != MAILACTIVESYNC_CP_SETTINGS) ||
      (response->root->token != MAILACTIVESYNC_SETTINGS_SETTINGS)) {
    r = MAILACTIVESYNC_ERROR_PROTOCOL;
    goto cleanup;
  }

  parsed = calloc(1, sizeof(* parsed));
  if (parsed == NULL) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto cleanup;
  }

  parsed->status = node_child_int(response->root,
      MAILACTIVESYNC_CP_SETTINGS, MAILACTIVESYNC_SETTINGS_STATUS);
  device_node = node_child(response->root, MAILACTIVESYNC_CP_SETTINGS,
      MAILACTIVESYNC_SETTINGS_DEVICE_INFORMATION);
  parsed->device_information_status = node_child_int(device_node,
      MAILACTIVESYNC_CP_SETTINGS, MAILACTIVESYNC_SETTINGS_STATUS);

  * result = parsed;
  parsed = NULL;

 cleanup:
  mailactivesync_settings_result_free(parsed);
  mailactivesync_wbxml_document_free(response);
  mailactivesync_wbxml_node_free(request);
  return r;
}

static int build_settings_user_information_get_request(
    struct mailactivesync_wbxml_node ** result)
{
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_node * user_node;
  struct mailactivesync_wbxml_node * get_node;
  int r;

  root = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_SETTINGS,
      MAILACTIVESYNC_SETTINGS_SETTINGS);
  user_node = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_SETTINGS,
      MAILACTIVESYNC_SETTINGS_USER_INFORMATION);
  get_node = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_SETTINGS,
      MAILACTIVESYNC_SETTINGS_GET);
  if ((root == NULL) || (user_node == NULL) || (get_node == NULL)) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto err;
  }

  r = mailactivesync_wbxml_node_add_child(user_node, get_node);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  get_node = NULL;
  r = mailactivesync_wbxml_node_add_child(root, user_node);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  user_node = NULL;

  * result = root;
  return MAILACTIVESYNC_NO_ERROR;

 err:
  mailactivesync_wbxml_node_free(get_node);
  mailactivesync_wbxml_node_free(user_node);
  mailactivesync_wbxml_node_free(root);
  return r;
}

static int settings_append_text_node(clist ** list,
    struct mailactivesync_wbxml_node * node)
{
  char * value;

  if ((node == NULL) || (node->text == NULL))
    return MAILACTIVESYNC_NO_ERROR;

  if (* list == NULL) {
    * list = clist_new();
    if (* list == NULL)
      return MAILACTIVESYNC_ERROR_MEMORY;
  }

  value = strdup(node->text);
  if (value == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;
  if (clist_append(* list, value) < 0) {
    free(value);
    return MAILACTIVESYNC_ERROR_MEMORY;
  }

  return MAILACTIVESYNC_NO_ERROR;
}

static int parse_settings_email_addresses(
    struct mailactivesync_wbxml_node * email_addresses,
    char ** primary_smtp_address,
    clist ** smtp_addresses)
{
  clistiter * cur;

  if ((email_addresses == NULL) || (email_addresses->children == NULL))
    return MAILACTIVESYNC_NO_ERROR;

  for (cur = clist_begin(email_addresses->children); cur != NULL;
      cur = clist_next(cur)) {
    struct mailactivesync_wbxml_node * child;

    child = clist_content(cur);
    if ((child->code_page == MAILACTIVESYNC_CP_SETTINGS) &&
        (child->token == MAILACTIVESYNC_SETTINGS_PRIMARY_SMTP_ADDRESS) &&
        (child->text != NULL)) {
      free(* primary_smtp_address);
      * primary_smtp_address = strdup(child->text);
      if (* primary_smtp_address == NULL)
        return MAILACTIVESYNC_ERROR_MEMORY;
    }
    else if ((child->code_page == MAILACTIVESYNC_CP_SETTINGS) &&
        (child->token == MAILACTIVESYNC_SETTINGS_SMTP_ADDRESS)) {
      int r;

      r = settings_append_text_node(smtp_addresses, child);
      if (r != MAILACTIVESYNC_NO_ERROR)
        return r;
    }
  }

  return MAILACTIVESYNC_NO_ERROR;
}

static int parse_settings_account(struct mailactivesync_wbxml_node * node,
    struct mailactivesync_settings_account ** result)
{
  struct mailactivesync_settings_account * account;
  struct mailactivesync_wbxml_node * email_addresses;
  int r;

  account = calloc(1, sizeof(* account));
  if (account == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  account->account_id = dup_node_text(node, MAILACTIVESYNC_CP_SETTINGS,
      MAILACTIVESYNC_SETTINGS_ACCOUNT_ID);
  account->account_name = dup_node_text(node, MAILACTIVESYNC_CP_SETTINGS,
      MAILACTIVESYNC_SETTINGS_ACCOUNT_NAME);
  account->user_display_name = dup_node_text(node,
      MAILACTIVESYNC_CP_SETTINGS, MAILACTIVESYNC_SETTINGS_USER_DISPLAY_NAME);
  account->send_disabled = node_child_int(node, MAILACTIVESYNC_CP_SETTINGS,
      MAILACTIVESYNC_SETTINGS_SEND_DISABLED);

  email_addresses = node_child(node, MAILACTIVESYNC_CP_SETTINGS,
      MAILACTIVESYNC_SETTINGS_EMAIL_ADDRESSES);
  r = parse_settings_email_addresses(email_addresses,
      &account->primary_smtp_address, &account->smtp_addresses);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    mailactivesync_settings_account_free(account);
    return r;
  }

  * result = account;
  return MAILACTIVESYNC_NO_ERROR;
}

static int parse_settings_accounts(struct mailactivesync_wbxml_node * accounts,
    clist ** result)
{
  clistiter * cur;

  if ((accounts == NULL) || (accounts->children == NULL))
    return MAILACTIVESYNC_NO_ERROR;

  if (* result == NULL) {
    * result = clist_new();
    if (* result == NULL)
      return MAILACTIVESYNC_ERROR_MEMORY;
  }

  for (cur = clist_begin(accounts->children); cur != NULL;
      cur = clist_next(cur)) {
    struct mailactivesync_wbxml_node * child;
    struct mailactivesync_settings_account * account;
    int r;

    child = clist_content(cur);
    if ((child->code_page != MAILACTIVESYNC_CP_SETTINGS) ||
        (child->token != MAILACTIVESYNC_SETTINGS_ACCOUNT))
      continue;

    account = NULL;
    r = parse_settings_account(child, &account);
    if (r != MAILACTIVESYNC_NO_ERROR)
      return r;
    if (clist_append(* result, account) < 0) {
      mailactivesync_settings_account_free(account);
      return MAILACTIVESYNC_ERROR_MEMORY;
    }
  }

  return MAILACTIVESYNC_NO_ERROR;
}

static int parse_settings_user_information(
    struct mailactivesync_wbxml_node * root,
    struct mailactivesync_settings_result * parsed)
{
  struct mailactivesync_wbxml_node * user_node;
  struct mailactivesync_wbxml_node * get_node;
  struct mailactivesync_wbxml_node * email_addresses;
  struct mailactivesync_wbxml_node * accounts;
  int r;

  user_node = node_child(root, MAILACTIVESYNC_CP_SETTINGS,
      MAILACTIVESYNC_SETTINGS_USER_INFORMATION);
  if (user_node == NULL)
    return MAILACTIVESYNC_ERROR_PROTOCOL;

  parsed->user_information_status = node_child_int(user_node,
      MAILACTIVESYNC_CP_SETTINGS, MAILACTIVESYNC_SETTINGS_STATUS);
  get_node = node_child(user_node, MAILACTIVESYNC_CP_SETTINGS,
      MAILACTIVESYNC_SETTINGS_GET);
  if (get_node == NULL)
    get_node = user_node;

  email_addresses = node_child(get_node, MAILACTIVESYNC_CP_SETTINGS,
      MAILACTIVESYNC_SETTINGS_EMAIL_ADDRESSES);
  r = parse_settings_email_addresses(email_addresses,
      &parsed->primary_smtp_address, &parsed->smtp_addresses);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  accounts = node_child(get_node, MAILACTIVESYNC_CP_SETTINGS,
      MAILACTIVESYNC_SETTINGS_ACCOUNTS);
  return parse_settings_accounts(accounts, &parsed->accounts);
}

int mailactivesync_command_settings_get_user_information(
    mailactivesync * session,
    struct mailactivesync_settings_result ** result)
{
  struct mailactivesync_wbxml_node * request;
  struct mailactivesync_wbxml_document * response;
  struct mailactivesync_settings_result * parsed;
  int r;

  if ((session == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  request = NULL;
  response = NULL;
  parsed = NULL;

  r = build_settings_user_information_get_request(&request);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  r = post_wbxml_document(session, "Settings", NULL, request, &response);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;

  if ((response->root == NULL) ||
      (response->root->code_page != MAILACTIVESYNC_CP_SETTINGS) ||
      (response->root->token != MAILACTIVESYNC_SETTINGS_SETTINGS)) {
    r = MAILACTIVESYNC_ERROR_PROTOCOL;
    goto cleanup;
  }

  parsed = calloc(1, sizeof(* parsed));
  if (parsed == NULL) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto cleanup;
  }

  parsed->status = node_child_int(response->root,
      MAILACTIVESYNC_CP_SETTINGS, MAILACTIVESYNC_SETTINGS_STATUS);
  r = parse_settings_user_information(response->root, parsed);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;

  * result = parsed;
  parsed = NULL;

 cleanup:
  mailactivesync_settings_result_free(parsed);
  mailactivesync_wbxml_document_free(response);
  mailactivesync_wbxml_node_free(request);
  return r;
}

static int get_item_estimate_uses_legacy_collection_options(
    mailactivesync * session)
{
  if ((session == NULL) || (session->as_protocol_version == NULL))
    return 0;
  return (strcmp(session->as_protocol_version, "2.5") == 0) ||
      (strcmp(session->as_protocol_version, "12.0") == 0) ||
      (strcmp(session->as_protocol_version, "12.1") == 0);
}

static int build_get_item_estimate_collection(mailactivesync * session,
    const struct mailactivesync_get_item_estimate_collection_request * request,
    struct mailactivesync_wbxml_node ** result)
{
  struct mailactivesync_wbxml_node * collection;
  struct mailactivesync_wbxml_node * options;
  int legacy_options;
  int r;

  if ((request == NULL) || (request->collection_id == NULL) ||
      (request->sync_key == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  options = NULL;
  legacy_options = get_item_estimate_uses_legacy_collection_options(session);
  collection = mailactivesync_wbxml_node_new(
      MAILACTIVESYNC_CP_GETITEMESTIMATE,
      MAILACTIVESYNC_GETITEMESTIMATE_COLLECTION);
  if (collection == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  if ((request->collection_class != NULL) && legacy_options) {
    r = node_add_text(collection, MAILACTIVESYNC_CP_GETITEMESTIMATE,
        MAILACTIVESYNC_GETITEMESTIMATE_CLASS, request->collection_class);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
  }
  r = node_add_text(collection, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_SYNC_KEY, request->sync_key);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  r = node_add_text(collection, MAILACTIVESYNC_CP_GETITEMESTIMATE,
      MAILACTIVESYNC_GETITEMESTIMATE_COLLECTION_ID, request->collection_id);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  if (request->has_filter_type && legacy_options) {
    r = node_add_uint(collection, MAILACTIVESYNC_CP_AIRSYNC,
        MAILACTIVESYNC_AIRSYNC_FILTER_TYPE, request->filter_type);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
  }
  if (request->has_conversation_mode) {
    r = node_add_text(collection, MAILACTIVESYNC_CP_AIRSYNC,
        MAILACTIVESYNC_AIRSYNC_CONVERSATION_MODE,
        request->conversation_mode ? "1" : "0");
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
  }
  if (!legacy_options &&
      ((request->collection_class != NULL) || request->has_filter_type)) {
    options = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
        MAILACTIVESYNC_AIRSYNC_OPTIONS);
    if (options == NULL) {
      r = MAILACTIVESYNC_ERROR_MEMORY;
      goto err;
    }
    if (request->collection_class != NULL) {
      r = node_add_text(options, MAILACTIVESYNC_CP_AIRSYNC,
          MAILACTIVESYNC_AIRSYNC_CLASS, request->collection_class);
      if (r != MAILACTIVESYNC_NO_ERROR)
        goto err;
    }
    if (request->has_filter_type) {
      r = node_add_uint(options, MAILACTIVESYNC_CP_AIRSYNC,
          MAILACTIVESYNC_AIRSYNC_FILTER_TYPE, request->filter_type);
      if (r != MAILACTIVESYNC_NO_ERROR)
        goto err;
    }
    r = mailactivesync_wbxml_node_add_child(collection, options);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
    options = NULL;
  }

  * result = collection;
  return MAILACTIVESYNC_NO_ERROR;

 err:
  mailactivesync_wbxml_node_free(options);
  mailactivesync_wbxml_node_free(collection);
  return r;
}

static int build_get_item_estimate_request(mailactivesync * session,
    clist * collection_requests, struct mailactivesync_wbxml_node ** result)
{
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_node * collections;
  clistiter * cur;
  int r;

  if ((collection_requests == NULL) ||
      (clist_count(collection_requests) == 0) ||
      (clist_count(collection_requests) > 300) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  root = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_GETITEMESTIMATE,
      MAILACTIVESYNC_GETITEMESTIMATE_GET_ITEM_ESTIMATE);
  collections = mailactivesync_wbxml_node_new(
      MAILACTIVESYNC_CP_GETITEMESTIMATE,
      MAILACTIVESYNC_GETITEMESTIMATE_COLLECTIONS);
  if ((root == NULL) || (collections == NULL)) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto err;
  }

  for (cur = clist_begin(collection_requests); cur != NULL;
      cur = clist_next(cur)) {
    struct mailactivesync_wbxml_node * collection;

    collection = NULL;
    r = build_get_item_estimate_collection(session, clist_content(cur),
        &collection);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
    r = mailactivesync_wbxml_node_add_child(collections, collection);
    if (r != MAILACTIVESYNC_NO_ERROR) {
      mailactivesync_wbxml_node_free(collection);
      goto err;
    }
  }
  r = mailactivesync_wbxml_node_add_child(root, collections);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  collections = NULL;

  * result = root;
  return MAILACTIVESYNC_NO_ERROR;

 err:
  mailactivesync_wbxml_node_free(collections);
  mailactivesync_wbxml_node_free(root);
  return r;
}

static struct mailactivesync_get_item_estimate_result *
get_item_estimate_result_new(void)
{
  struct mailactivesync_get_item_estimate_result * result;

  result = calloc(1, sizeof(* result));
  if (result == NULL)
    return NULL;

  result->collections = clist_new();
  if (result->collections == NULL) {
    mailactivesync_get_item_estimate_result_free(result);
    return NULL;
  }

  return result;
}

static int parse_get_item_estimate_response_node(
    struct mailactivesync_wbxml_node * response,
    struct mailactivesync_get_item_estimate_result * result)
{
  struct mailactivesync_get_item_estimate_collection * parsed;
  struct mailactivesync_wbxml_node * collection;
  const char * value;

  if ((response == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  parsed = calloc(1, sizeof(* parsed));
  if (parsed == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  parsed->status = node_child_int(response, MAILACTIVESYNC_CP_GETITEMESTIMATE,
      MAILACTIVESYNC_GETITEMESTIMATE_STATUS);
  collection = node_child(response, MAILACTIVESYNC_CP_GETITEMESTIMATE,
      MAILACTIVESYNC_GETITEMESTIMATE_COLLECTION);
  if (parsed->status == 0) {
    parsed->status = node_child_int(collection,
        MAILACTIVESYNC_CP_GETITEMESTIMATE,
        MAILACTIVESYNC_GETITEMESTIMATE_STATUS);
  }
  value = node_child_text(collection, MAILACTIVESYNC_CP_GETITEMESTIMATE,
      MAILACTIVESYNC_GETITEMESTIMATE_COLLECTION_ID);
  if (value != NULL) {
    parsed->collection_id = strdup(value);
    if (parsed->collection_id == NULL)
      goto err_memory;
  }
  value = node_child_text(collection, MAILACTIVESYNC_CP_GETITEMESTIMATE,
      MAILACTIVESYNC_GETITEMESTIMATE_CLASS);
  if (value != NULL) {
    parsed->collection_class = strdup(value);
    if (parsed->collection_class == NULL)
      goto err_memory;
  }
  parsed->estimate = (uint32_t) node_child_int(collection,
      MAILACTIVESYNC_CP_GETITEMESTIMATE,
      MAILACTIVESYNC_GETITEMESTIMATE_ESTIMATE);

  if (clist_append(result->collections, parsed) < 0)
    goto err_memory;
  return MAILACTIVESYNC_NO_ERROR;

 err_memory:
  free(parsed->collection_id);
  free(parsed->collection_class);
  free(parsed);
  return MAILACTIVESYNC_ERROR_MEMORY;
}

static int parse_get_item_estimate_responses(
    struct mailactivesync_wbxml_node * root,
    struct mailactivesync_get_item_estimate_result * result)
{
  clistiter * cur;
  int r;

  if ((root == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  for (cur = root->children != NULL ? clist_begin(root->children) : NULL;
      cur != NULL; cur = clist_next(cur)) {
    struct mailactivesync_wbxml_node * child;

    child = clist_content(cur);
    if ((child->code_page != MAILACTIVESYNC_CP_GETITEMESTIMATE) ||
        (child->token != MAILACTIVESYNC_GETITEMESTIMATE_RESPONSE))
      continue;

    r = parse_get_item_estimate_response_node(child, result);
    if (r != MAILACTIVESYNC_NO_ERROR)
      return r;
  }

  if (clist_count(result->collections) > 0) {
    struct mailactivesync_get_item_estimate_collection * first;

    first = clist_content(clist_begin(result->collections));
    if (first != NULL) {
      result->collection_status = first->status;
      result->estimate = first->estimate;
    }
  }

  return MAILACTIVESYNC_NO_ERROR;
}

int mailactivesync_command_get_item_estimate_multi(mailactivesync * session,
    clist * collection_requests,
    struct mailactivesync_get_item_estimate_result ** result)
{
  struct mailactivesync_http_response * http_response;
  struct mailactivesync_wbxml_node * request;
  struct mailactivesync_wbxml_document * response;
  struct mailactivesync_get_item_estimate_result * parsed;
  int r;

  if ((session == NULL) || (collection_requests == NULL) ||
      (clist_count(collection_requests) == 0) ||
      (clist_count(collection_requests) > 300) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  http_response = NULL;
  request = NULL;
  response = NULL;
  parsed = NULL;

  r = build_get_item_estimate_request(session, collection_requests, &request);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  r = post_wbxml_document_response(session, "GetItemEstimate", NULL, request,
      &http_response);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;

  parsed = get_item_estimate_result_new();
  if (parsed == NULL) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto cleanup;
  }

  if ((http_response->body == NULL) || (http_response->body_len == 0)) {
    parsed->status = 1;
    parsed->collection_status = 1;
    parsed->empty_response = 1;
    * result = parsed;
    parsed = NULL;
    goto cleanup;
  }

  if (!response_body_is_wbxml(http_response)) {
    r = MAILACTIVESYNC_ERROR_RESPONSE_NOT_WBXML;
    goto cleanup;
  }

  r = mailactivesync_wbxml_decode(http_response->body,
      http_response->body_len, &response);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;

  if ((response->root == NULL) ||
      (response->root->code_page != MAILACTIVESYNC_CP_GETITEMESTIMATE) ||
      (response->root->token !=
          MAILACTIVESYNC_GETITEMESTIMATE_GET_ITEM_ESTIMATE)) {
    r = MAILACTIVESYNC_ERROR_PROTOCOL;
    goto cleanup;
  }

  parsed->status = node_child_int(response->root,
      MAILACTIVESYNC_CP_GETITEMESTIMATE,
      MAILACTIVESYNC_GETITEMESTIMATE_STATUS);
  r = parse_get_item_estimate_responses(response->root, parsed);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;

  * result = parsed;
  parsed = NULL;

 cleanup:
  mailactivesync_get_item_estimate_result_free(parsed);
  mailactivesync_wbxml_document_free(response);
  mailactivesync_http_response_free(http_response);
  mailactivesync_wbxml_node_free(request);
  return r;
}

int mailactivesync_command_get_item_estimate(mailactivesync * session,
    const char * collection_id,
    const char * sync_key,
    struct mailactivesync_get_item_estimate_result ** result)
{
  struct mailactivesync_get_item_estimate_collection_request request;
  clist * requests;
  int r;

  if ((session == NULL) || (collection_id == NULL) || (sync_key == NULL) ||
      (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  memset(&request, 0, sizeof(request));
  request.collection_id = collection_id;
  request.sync_key = sync_key;
  requests = clist_new();
  if (requests == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;
  if (clist_append(requests, &request) < 0) {
    clist_free(requests);
    return MAILACTIVESYNC_ERROR_MEMORY;
  }

  r = mailactivesync_command_get_item_estimate_multi(session, requests,
      result);
  clist_free(requests);
  return r;
}

static struct mailactivesync_mail_search_result * mail_search_result_new(void)
{
  struct mailactivesync_mail_search_result * result;

  result = calloc(1, sizeof(* result));
  if (result == NULL)
    return NULL;

  result->items = clist_new();
  if (result->items == NULL) {
    free(result);
    return NULL;
  }

  return result;
}

static int build_mail_search_request(
    const struct mailactivesync_mail_search_request * request,
    struct mailactivesync_wbxml_node ** result)
{
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_node * store;
  struct mailactivesync_wbxml_node * query;
  struct mailactivesync_wbxml_node * and_node;
  struct mailactivesync_wbxml_node * options;
  char range[64];
  int r;

  if ((request == NULL) || (request->collection_id == NULL) ||
      (request->free_text == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  root = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_SEARCH,
      MAILACTIVESYNC_SEARCH_SEARCH);
  store = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_SEARCH,
      MAILACTIVESYNC_SEARCH_STORE);
  query = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_SEARCH,
      MAILACTIVESYNC_SEARCH_QUERY);
  and_node = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_SEARCH,
      MAILACTIVESYNC_SEARCH_AND);
  options = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_SEARCH,
      MAILACTIVESYNC_SEARCH_OPTIONS);
  if ((root == NULL) || (store == NULL) || (query == NULL) ||
      (and_node == NULL) || (options == NULL)) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto err;
  }

  r = node_add_text(store, MAILACTIVESYNC_CP_SEARCH,
      MAILACTIVESYNC_SEARCH_NAME, "Mailbox");
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  r = node_add_text(and_node, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_CLASS, "Email");
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  r = node_add_text(and_node, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_COLLECTION_ID, request->collection_id);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  r = node_add_text(and_node, MAILACTIVESYNC_CP_SEARCH,
      MAILACTIVESYNC_SEARCH_FREE_TEXT, request->free_text);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  r = mailactivesync_wbxml_node_add_child(query, and_node);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  and_node = NULL;
  r = mailactivesync_wbxml_node_add_child(store, query);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  query = NULL;

  if (request->rebuild_results) {
    r = node_add_empty(options, MAILACTIVESYNC_CP_SEARCH,
        MAILACTIVESYNC_SEARCH_REBUILD_RESULTS);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
  }
  snprintf(range, sizeof(range), "%lu-%lu",
      (unsigned long) request->range_start,
      (unsigned long) request->range_end);
  r = node_add_text(options, MAILACTIVESYNC_CP_SEARCH,
      MAILACTIVESYNC_SEARCH_RANGE, range);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  r = mailactivesync_wbxml_node_add_child(store, options);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  options = NULL;
  r = mailactivesync_wbxml_node_add_child(root, store);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  store = NULL;

  * result = root;
  return MAILACTIVESYNC_NO_ERROR;

 err:
  mailactivesync_wbxml_node_free(options);
  mailactivesync_wbxml_node_free(and_node);
  mailactivesync_wbxml_node_free(query);
  mailactivesync_wbxml_node_free(store);
  mailactivesync_wbxml_node_free(root);
  return r;
}

static int parse_mail_search_item(struct mailactivesync_wbxml_node * node,
    struct mailactivesync_mail_search_result * result)
{
  struct mailactivesync_mail_search_item * item;
  struct mailactivesync_wbxml_node * properties;
  int r;

  if ((node == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  properties = node_child(node, MAILACTIVESYNC_CP_SEARCH,
      MAILACTIVESYNC_SEARCH_PROPERTIES);
  if ((node_child_text(node, MAILACTIVESYNC_CP_AIRSYNC,
        MAILACTIVESYNC_AIRSYNC_SERVER_ID) == NULL) &&
      (node_child_text(node, MAILACTIVESYNC_CP_SEARCH,
        MAILACTIVESYNC_SEARCH_LONG_ID) == NULL) &&
      (properties == NULL))
    return MAILACTIVESYNC_NO_ERROR;

  item = calloc(1, sizeof(* item));
  if (item == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  item->collection_id = dup_node_text(node, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_COLLECTION_ID);
  item->server_id = dup_node_text(node, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_SERVER_ID);
  item->long_id = dup_node_text(node, MAILACTIVESYNC_CP_SEARCH,
      MAILACTIVESYNC_SEARCH_LONG_ID);
  if (properties != NULL) {
    item->message = calloc(1, sizeof(* item->message));
    if (item->message == NULL) {
      r = MAILACTIVESYNC_ERROR_MEMORY;
      goto err;
    }
    if (item->server_id != NULL) {
      item->message->server_id = strdup(item->server_id);
      if (item->message->server_id == NULL) {
        r = MAILACTIVESYNC_ERROR_MEMORY;
        goto err;
      }
    }
    r = parse_message_application_data(properties, item->message);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
  }

  if (clist_append(result->items, item) < 0) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto err;
  }

  return MAILACTIVESYNC_NO_ERROR;

 err:
  mailactivesync_mail_search_item_free(item);
  return r;
}

static int parse_mail_search_store(struct mailactivesync_wbxml_node * store,
    struct mailactivesync_mail_search_result * result)
{
  clistiter * cur;
  const char * status;
  const char * total;
  int r;

  if ((store == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_PROTOCOL;

  status = node_child_text(store, MAILACTIVESYNC_CP_SEARCH,
      MAILACTIVESYNC_SEARCH_STATUS);
  if (status == NULL)
    return MAILACTIVESYNC_ERROR_PROTOCOL;
  result->status = atoi(status);

  result->range = dup_node_text(store, MAILACTIVESYNC_CP_SEARCH,
      MAILACTIVESYNC_SEARCH_RANGE);
  total = node_child_text(store, MAILACTIVESYNC_CP_SEARCH,
      MAILACTIVESYNC_SEARCH_TOTAL);
  if (total != NULL)
    result->total = (uint32_t) strtoul(total, NULL, 10);

  for (cur = store->children != NULL ? clist_begin(store->children) : NULL;
      cur != NULL; cur = clist_next(cur)) {
    struct mailactivesync_wbxml_node * child;

    child = clist_content(cur);
    if ((child->code_page != MAILACTIVESYNC_CP_SEARCH) ||
        (child->token != MAILACTIVESYNC_SEARCH_RESULT))
      continue;
    r = parse_mail_search_item(child, result);
    if (r != MAILACTIVESYNC_NO_ERROR)
      return r;
  }

  return mailactivesync_global_status_to_error(result->status);
}

int mailactivesync_command_mail_search(mailactivesync * session,
    const struct mailactivesync_mail_search_request * request,
    struct mailactivesync_mail_search_result ** result)
{
  struct mailactivesync_wbxml_node * request_node;
  struct mailactivesync_wbxml_document * response;
  struct mailactivesync_wbxml_node * response_node;
  struct mailactivesync_wbxml_node * store;
  struct mailactivesync_mail_search_result * parsed;
  int r;

  if ((session == NULL) || (request == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  request_node = NULL;
  response = NULL;
  parsed = NULL;

  r = build_mail_search_request(request, &request_node);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  r = post_wbxml_document(session, "Search", NULL, request_node, &response);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;

  if ((response->root == NULL) ||
      (response->root->code_page != MAILACTIVESYNC_CP_SEARCH) ||
      (response->root->token != MAILACTIVESYNC_SEARCH_SEARCH)) {
    r = MAILACTIVESYNC_ERROR_PROTOCOL;
    goto cleanup;
  }

  response_node = node_child(response->root, MAILACTIVESYNC_CP_SEARCH,
      MAILACTIVESYNC_SEARCH_RESPONSE);
  store = node_child(response_node, MAILACTIVESYNC_CP_SEARCH,
      MAILACTIVESYNC_SEARCH_STORE);
  parsed = mail_search_result_new();
  if (parsed == NULL) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto cleanup;
  }

  r = parse_mail_search_store(store, parsed);
  * result = parsed;
  parsed = NULL;

 cleanup:
  mailactivesync_mail_search_result_free(parsed);
  mailactivesync_wbxml_document_free(response);
  mailactivesync_wbxml_node_free(request_node);
  return r;
}

static struct mailactivesync_mail_find_result * mail_find_result_new(void)
{
  struct mailactivesync_mail_find_result * result;

  result = calloc(1, sizeof(* result));
  if (result == NULL)
    return NULL;

  result->items = clist_new();
  if (result->items == NULL) {
    free(result);
    return NULL;
  }

  return result;
}

static int build_mail_find_request(
    const struct mailactivesync_mail_find_request * request,
    struct mailactivesync_wbxml_node ** result)
{
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_node * execute_search;
  struct mailactivesync_wbxml_node * criterion;
  struct mailactivesync_wbxml_node * query;
  struct mailactivesync_wbxml_node * options;
  char range[64];
  int r;

  if ((request == NULL) || (request->search_id == NULL) ||
      (request->collection_id == NULL) || (request->query == NULL) ||
      (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  root = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_FIND,
      MAILACTIVESYNC_FIND_FIND);
  execute_search = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_FIND,
      MAILACTIVESYNC_FIND_EXECUTE_SEARCH);
  criterion = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_FIND,
      MAILACTIVESYNC_FIND_MAILBOX_SEARCH_CRITERION);
  query = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_FIND,
      MAILACTIVESYNC_FIND_QUERY);
  options = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_FIND,
      MAILACTIVESYNC_FIND_OPTIONS);
  if ((root == NULL) || (execute_search == NULL) || (criterion == NULL) ||
      (query == NULL) || (options == NULL)) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto err;
  }

  r = node_add_text(root, MAILACTIVESYNC_CP_FIND,
      MAILACTIVESYNC_FIND_SEARCH_ID, request->search_id);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  r = node_add_text(query, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_CLASS, "Email");
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  r = node_add_text(query, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_COLLECTION_ID, request->collection_id);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  r = node_add_text(query, MAILACTIVESYNC_CP_FIND,
      MAILACTIVESYNC_FIND_FREE_TEXT, request->query);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  r = mailactivesync_wbxml_node_add_child(criterion, query);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  query = NULL;

  snprintf(range, sizeof(range), "%lu-%lu",
      (unsigned long) request->range_start,
      (unsigned long) request->range_end);
  r = node_add_text(options, MAILACTIVESYNC_CP_FIND,
      MAILACTIVESYNC_FIND_RANGE, range);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  if (request->deep_traversal) {
    r = node_add_empty(options, MAILACTIVESYNC_CP_FIND,
        MAILACTIVESYNC_FIND_DEEP_TRAVERSAL);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
  }
  r = mailactivesync_wbxml_node_add_child(criterion, options);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  options = NULL;
  r = mailactivesync_wbxml_node_add_child(execute_search, criterion);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  criterion = NULL;
  r = mailactivesync_wbxml_node_add_child(root, execute_search);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  execute_search = NULL;

  * result = root;
  return MAILACTIVESYNC_NO_ERROR;

 err:
  mailactivesync_wbxml_node_free(options);
  mailactivesync_wbxml_node_free(query);
  mailactivesync_wbxml_node_free(criterion);
  mailactivesync_wbxml_node_free(execute_search);
  mailactivesync_wbxml_node_free(root);
  return r;
}

static int parse_mail_find_item(struct mailactivesync_wbxml_node * node,
    struct mailactivesync_mail_find_result * result)
{
  struct mailactivesync_mail_find_item * item;
  struct mailactivesync_wbxml_node * properties;
  int r;

  if ((node == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  properties = node_child(node, MAILACTIVESYNC_CP_FIND,
      MAILACTIVESYNC_FIND_PROPERTIES);
  if ((node_child_text(node, MAILACTIVESYNC_CP_AIRSYNC,
        MAILACTIVESYNC_AIRSYNC_SERVER_ID) == NULL) &&
      (properties == NULL))
    return MAILACTIVESYNC_NO_ERROR;

  item = calloc(1, sizeof(* item));
  if (item == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  item->collection_class = dup_node_text(node, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_CLASS);
  item->collection_id = dup_node_text(node, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_COLLECTION_ID);
  item->server_id = dup_node_text(node, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_SERVER_ID);
  if (properties != NULL) {
    item->preview = dup_node_text(properties, MAILACTIVESYNC_CP_FIND,
        MAILACTIVESYNC_FIND_PREVIEW);
    item->display_cc = dup_node_text(properties, MAILACTIVESYNC_CP_FIND,
        MAILACTIVESYNC_FIND_DISPLAY_CC);
    item->display_bcc = dup_node_text(properties, MAILACTIVESYNC_CP_FIND,
        MAILACTIVESYNC_FIND_DISPLAY_BCC);
    item->has_attachments = node_child_int(properties,
        MAILACTIVESYNC_CP_FIND, MAILACTIVESYNC_FIND_HAS_ATTACHMENTS);
    item->message = calloc(1, sizeof(* item->message));
    if (item->message == NULL) {
      r = MAILACTIVESYNC_ERROR_MEMORY;
      goto err;
    }
    if (item->server_id != NULL) {
      item->message->server_id = strdup(item->server_id);
      if (item->message->server_id == NULL) {
        r = MAILACTIVESYNC_ERROR_MEMORY;
        goto err;
      }
    }
    r = parse_message_application_data(properties, item->message);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
  }

  if (clist_append(result->items, item) < 0) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto err;
  }

  return MAILACTIVESYNC_NO_ERROR;

 err:
  mailactivesync_mail_find_item_free(item);
  return r;
}

static int parse_mail_find_response(struct mailactivesync_wbxml_node * response,
    struct mailactivesync_mail_find_result * result)
{
  clistiter * cur;
  const char * total;
  int r;

  if ((response == NULL) || (result == NULL))
    return MAILACTIVESYNC_NO_ERROR;

  result->response_status = node_child_int(response, MAILACTIVESYNC_CP_FIND,
      MAILACTIVESYNC_FIND_STATUS);
  result->store = dup_node_text(response, MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_STORE);
  result->range = dup_node_text(response, MAILACTIVESYNC_CP_FIND,
      MAILACTIVESYNC_FIND_RANGE);
  total = node_child_text(response, MAILACTIVESYNC_CP_FIND,
      MAILACTIVESYNC_FIND_TOTAL);
  if (total != NULL)
    result->total = (uint32_t) strtoul(total, NULL, 10);

  for (cur = response->children != NULL ? clist_begin(response->children) :
      NULL; cur != NULL; cur = clist_next(cur)) {
    struct mailactivesync_wbxml_node * child;

    child = clist_content(cur);
    if ((child->code_page != MAILACTIVESYNC_CP_FIND) ||
        (child->token != MAILACTIVESYNC_FIND_RESULT))
      continue;
    r = parse_mail_find_item(child, result);
    if (r != MAILACTIVESYNC_NO_ERROR)
      return r;
  }

  return MAILACTIVESYNC_NO_ERROR;
}

int mailactivesync_command_mail_find(mailactivesync * session,
    const struct mailactivesync_mail_find_request * request,
    struct mailactivesync_mail_find_result ** result)
{
  struct mailactivesync_wbxml_node * request_node;
  struct mailactivesync_wbxml_document * response;
  struct mailactivesync_wbxml_node * response_node;
  struct mailactivesync_mail_find_result * parsed;
  int r;

  if ((session == NULL) || (request == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  request_node = NULL;
  response = NULL;
  parsed = NULL;

  r = build_mail_find_request(request, &request_node);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  r = post_wbxml_document(session, "Find", NULL, request_node, &response);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;

  if ((response->root == NULL) ||
      (response->root->code_page != MAILACTIVESYNC_CP_FIND) ||
      (response->root->token != MAILACTIVESYNC_FIND_FIND)) {
    r = MAILACTIVESYNC_ERROR_PROTOCOL;
    goto cleanup;
  }

  parsed = mail_find_result_new();
  if (parsed == NULL) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto cleanup;
  }

  parsed->status = node_child_int(response->root, MAILACTIVESYNC_CP_FIND,
      MAILACTIVESYNC_FIND_STATUS);
  response_node = node_child(response->root, MAILACTIVESYNC_CP_FIND,
      MAILACTIVESYNC_FIND_RESPONSE);
  r = parse_mail_find_response(response_node, parsed);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;

  if (parsed->status != 1)
    r = mailactivesync_global_status_to_error(parsed->status);
  else if (parsed->response_status != 0)
    r = mailactivesync_global_status_to_error(parsed->response_status);
  else
    r = MAILACTIVESYNC_NO_ERROR;
  * result = parsed;
  parsed = NULL;

 cleanup:
  mailactivesync_mail_find_result_free(parsed);
  mailactivesync_wbxml_document_free(response);
  mailactivesync_wbxml_node_free(request_node);
  return r;
}

static struct mailactivesync_validate_cert_result *
validate_cert_result_new(void)
{
  struct mailactivesync_validate_cert_result * result;

  result = calloc(1, sizeof(* result));
  if (result == NULL)
    return NULL;
  result->certificates = clist_new();
  if (result->certificates == NULL) {
    free(result);
    return NULL;
  }

  return result;
}

static int add_validate_cert_certificate_list(
    struct mailactivesync_wbxml_node * parent,
    uint8_t token,
    clist * certificates)
{
  struct mailactivesync_wbxml_node * container;
  clistiter * cur;
  int r;

  if ((parent == NULL) || (certificates == NULL) ||
      (clist_count(certificates) == 0))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  container = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_VALIDATECERT,
      token);
  if (container == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  for (cur = clist_begin(certificates); cur != NULL; cur = clist_next(cur)) {
    const char * certificate;

    certificate = clist_content(cur);
    if (certificate == NULL) {
      r = MAILACTIVESYNC_ERROR_BAD_STATE;
      goto err;
    }
    r = node_add_text(container, MAILACTIVESYNC_CP_VALIDATECERT,
        MAILACTIVESYNC_VALIDATECERT_CERTIFICATE, certificate);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
  }

  r = mailactivesync_wbxml_node_add_child(parent, container);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;

  return MAILACTIVESYNC_NO_ERROR;

 err:
  mailactivesync_wbxml_node_free(container);
  return r;
}

static int build_validate_cert_request(
    const struct mailactivesync_validate_cert_request * request,
    struct mailactivesync_wbxml_node ** result)
{
  struct mailactivesync_wbxml_node * root;
  int r;

  if ((request == NULL) || (request->certificates == NULL) ||
      (clist_count(request->certificates) == 0) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  root = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_VALIDATECERT,
      MAILACTIVESYNC_VALIDATECERT_VALIDATE_CERT);
  if (root == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  if ((request->certificate_chain != NULL) &&
      (clist_count(request->certificate_chain) > 0)) {
    r = add_validate_cert_certificate_list(root,
        MAILACTIVESYNC_VALIDATECERT_CERTIFICATE_CHAIN,
        request->certificate_chain);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
  }

  r = add_validate_cert_certificate_list(root,
      MAILACTIVESYNC_VALIDATECERT_CERTIFICATES,
      request->certificates);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;

  if (request->check_crl >= 0) {
    r = node_add_uint(root, MAILACTIVESYNC_CP_VALIDATECERT,
        MAILACTIVESYNC_VALIDATECERT_CHECK_CRL,
        request->check_crl != 0 ? 1 : 0);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
  }

  * result = root;
  return MAILACTIVESYNC_NO_ERROR;

 err:
  mailactivesync_wbxml_node_free(root);
  return r;
}

static int validate_cert_append_status(clist * statuses, int status)
{
  int * value;

  value = malloc(sizeof(* value));
  if (value == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;
  * value = status;
  if (clist_append(statuses, value) < 0) {
    free(value);
    return MAILACTIVESYNC_ERROR_MEMORY;
  }

  return MAILACTIVESYNC_NO_ERROR;
}

static int parse_validate_cert_certificate(
    struct mailactivesync_wbxml_node * node,
    struct mailactivesync_validate_cert_result * result)
{
  struct mailactivesync_validate_cert_certificate * certificate;
  clistiter * cur;
  int response_error;
  int r;

  if ((node == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  certificate = calloc(1, sizeof(* certificate));
  if (certificate == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;
  certificate->statuses = clist_new();
  if (certificate->statuses == NULL) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto err;
  }

  response_error = MAILACTIVESYNC_NO_ERROR;
  for (cur = node->children != NULL ? clist_begin(node->children) : NULL;
      cur != NULL; cur = clist_next(cur)) {
    struct mailactivesync_wbxml_node * child;
    int status;

    child = clist_content(cur);
    if ((child->code_page != MAILACTIVESYNC_CP_VALIDATECERT) ||
        (child->token != MAILACTIVESYNC_VALIDATECERT_STATUS) ||
        (child->text == NULL))
      continue;
    status = atoi(child->text);
    r = validate_cert_append_status(certificate->statuses, status);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
    r = mailactivesync_validate_cert_certificate_status_to_error(status);
    if ((r != MAILACTIVESYNC_NO_ERROR) &&
        (response_error == MAILACTIVESYNC_NO_ERROR))
      response_error = r;
  }

  if (clist_count(certificate->statuses) == 0) {
    r = MAILACTIVESYNC_ERROR_PROTOCOL;
    goto err;
  }

  if (clist_append(result->certificates, certificate) < 0) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto err;
  }

  return response_error;

 err:
  mailactivesync_validate_cert_certificate_free(certificate);
  return r;
}

static int parse_validate_cert_result(
    struct mailactivesync_wbxml_node * root,
    struct mailactivesync_validate_cert_result * result)
{
  clistiter * cur;
  const char * status;
  int response_error;
  int r;

  if ((root == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  status = node_child_text(root, MAILACTIVESYNC_CP_VALIDATECERT,
      MAILACTIVESYNC_VALIDATECERT_STATUS);
  if (status == NULL)
    return MAILACTIVESYNC_ERROR_PROTOCOL;

  result->status = atoi(status);
  response_error = mailactivesync_validate_cert_status_to_error(
      result->status);

  for (cur = root->children != NULL ? clist_begin(root->children) : NULL;
      cur != NULL; cur = clist_next(cur)) {
    struct mailactivesync_wbxml_node * child;

    child = clist_content(cur);
    if ((child->code_page != MAILACTIVESYNC_CP_VALIDATECERT) ||
        (child->token != MAILACTIVESYNC_VALIDATECERT_CERTIFICATE))
      continue;
    r = parse_validate_cert_certificate(child, result);
    if ((r != MAILACTIVESYNC_NO_ERROR) &&
        (response_error == MAILACTIVESYNC_NO_ERROR))
      response_error = r;
  }

  return response_error;
}

int mailactivesync_command_validate_cert(mailactivesync * session,
    const struct mailactivesync_validate_cert_request * request,
    struct mailactivesync_validate_cert_result ** result)
{
  struct mailactivesync_wbxml_node * request_node;
  struct mailactivesync_wbxml_document * response;
  struct mailactivesync_validate_cert_result * parsed;
  int r;

  if ((session == NULL) || (request == NULL) ||
      (request->certificates == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  request_node = NULL;
  response = NULL;
  parsed = NULL;

  r = build_validate_cert_request(request, &request_node);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  r = post_wbxml_document(session, "ValidateCert", NULL, request_node,
      &response);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;

  if ((response->root == NULL) ||
      (response->root->code_page != MAILACTIVESYNC_CP_VALIDATECERT) ||
      (response->root->token !=
          MAILACTIVESYNC_VALIDATECERT_VALIDATE_CERT)) {
    r = MAILACTIVESYNC_ERROR_PROTOCOL;
    goto cleanup;
  }

  parsed = validate_cert_result_new();
  if (parsed == NULL) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto cleanup;
  }

  r = parse_validate_cert_result(response->root, parsed);
  * result = parsed;
  parsed = NULL;

 cleanup:
  mailactivesync_validate_cert_result_free(parsed);
  mailactivesync_wbxml_document_free(response);
  mailactivesync_wbxml_node_free(request_node);
  return r;
}

static struct mailactivesync_resolve_recipients_result *
resolve_recipients_result_new(void)
{
  struct mailactivesync_resolve_recipients_result * result;

  result = calloc(1, sizeof(* result));
  if (result == NULL)
    return NULL;

  result->responses = clist_new();
  if (result->responses == NULL) {
    free(result);
    return NULL;
  }

  return result;
}

static int build_resolve_recipients_request(
    const struct mailactivesync_resolve_recipients_request * request,
    struct mailactivesync_wbxml_node ** result)
{
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_node * options;
  clistiter * cur;
  int r;

  if ((request == NULL) || (request->recipients == NULL) ||
      (clist_count(request->recipients) == 0) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  options = NULL;
  root = mailactivesync_wbxml_node_new(
      MAILACTIVESYNC_CP_RESOLVERECIPIENTS,
      MAILACTIVESYNC_RESOLVERECIPIENTS_RESOLVE_RECIPIENTS);
  if (root == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  for (cur = clist_begin(request->recipients); cur != NULL;
      cur = clist_next(cur)) {
    const char * recipient;

    recipient = clist_content(cur);
    if (recipient == NULL) {
      r = MAILACTIVESYNC_ERROR_BAD_STATE;
      goto err;
    }
    r = node_add_text(root, MAILACTIVESYNC_CP_RESOLVERECIPIENTS,
        MAILACTIVESYNC_RESOLVERECIPIENTS_TO, recipient);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
  }

  if ((request->max_ambiguous_recipients > 0) ||
      (request->certificate_retrieval > 0) ||
      (request->max_certificates > 0)) {
    options = mailactivesync_wbxml_node_new(
        MAILACTIVESYNC_CP_RESOLVERECIPIENTS,
        MAILACTIVESYNC_RESOLVERECIPIENTS_OPTIONS);
    if (options == NULL) {
      r = MAILACTIVESYNC_ERROR_MEMORY;
      goto err;
    }
    if (request->max_ambiguous_recipients > 0) {
      r = node_add_uint(options, MAILACTIVESYNC_CP_RESOLVERECIPIENTS,
          MAILACTIVESYNC_RESOLVERECIPIENTS_MAX_AMBIGUOUS_RECIPIENTS,
          request->max_ambiguous_recipients);
      if (r != MAILACTIVESYNC_NO_ERROR)
        goto err;
    }
    if (request->certificate_retrieval > 0) {
      r = node_add_uint(options, MAILACTIVESYNC_CP_RESOLVERECIPIENTS,
          MAILACTIVESYNC_RESOLVERECIPIENTS_CERTIFICATE_RETRIEVAL,
          (uint32_t) request->certificate_retrieval);
      if (r != MAILACTIVESYNC_NO_ERROR)
        goto err;
    }
    if (request->max_certificates > 0) {
      r = node_add_uint(options, MAILACTIVESYNC_CP_RESOLVERECIPIENTS,
          MAILACTIVESYNC_RESOLVERECIPIENTS_MAX_CERTIFICATES,
          request->max_certificates);
      if (r != MAILACTIVESYNC_NO_ERROR)
        goto err;
    }
    r = mailactivesync_wbxml_node_add_child(root, options);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
    options = NULL;
  }

  * result = root;
  return MAILACTIVESYNC_NO_ERROR;

 err:
  mailactivesync_wbxml_node_free(options);
  mailactivesync_wbxml_node_free(root);
  return r;
}

static int append_node_text_copy(clist * list,
    struct mailactivesync_wbxml_node * node)
{
  char * value;

  if ((list == NULL) || (node == NULL) || (node->text == NULL))
    return MAILACTIVESYNC_NO_ERROR;

  value = strdup(node->text);
  if (value == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;
  if (clist_append(list, value) < 0) {
    free(value);
    return MAILACTIVESYNC_ERROR_MEMORY;
  }

  return MAILACTIVESYNC_NO_ERROR;
}

static int parse_resolved_recipient_certificates(
    struct mailactivesync_wbxml_node * node,
    struct mailactivesync_resolved_recipient * recipient)
{
  struct mailactivesync_wbxml_node * certificates;
  const char * certificate_count;
  const char * status;
  clistiter * cur;
  int r;

  certificates = node_child(node, MAILACTIVESYNC_CP_RESOLVERECIPIENTS,
      MAILACTIVESYNC_RESOLVERECIPIENTS_CERTIFICATES);
  if (certificates == NULL)
    return MAILACTIVESYNC_NO_ERROR;

  status = node_child_text(certificates, MAILACTIVESYNC_CP_RESOLVERECIPIENTS,
      MAILACTIVESYNC_RESOLVERECIPIENTS_STATUS);
  if (status != NULL)
    recipient->certificates_status = atoi(status);

  certificate_count = node_child_text(certificates,
      MAILACTIVESYNC_CP_RESOLVERECIPIENTS,
      MAILACTIVESYNC_RESOLVERECIPIENTS_CERTIFICATE_COUNT);
  if (certificate_count != NULL)
    recipient->certificate_count = (uint32_t) strtoul(certificate_count,
        NULL, 10);

  recipient->certificates = clist_new();
  if (recipient->certificates == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  for (cur = certificates->children != NULL ? clist_begin(certificates->children) :
      NULL; cur != NULL; cur = clist_next(cur)) {
    struct mailactivesync_wbxml_node * child;

    child = clist_content(cur);
    if ((child->code_page == MAILACTIVESYNC_CP_RESOLVERECIPIENTS) &&
        (child->token == MAILACTIVESYNC_RESOLVERECIPIENTS_CERTIFICATE)) {
      r = append_node_text_copy(recipient->certificates, child);
      if (r != MAILACTIVESYNC_NO_ERROR)
        return r;
    }
  }

  recipient->mini_certificate = dup_node_text(certificates,
      MAILACTIVESYNC_CP_RESOLVERECIPIENTS,
      MAILACTIVESYNC_RESOLVERECIPIENTS_MINI_CERTIFICATE);
  return MAILACTIVESYNC_NO_ERROR;
}

static int parse_resolved_recipient(
    struct mailactivesync_wbxml_node * node,
    clist * recipients)
{
  struct mailactivesync_resolved_recipient * recipient;
  int r;

  if ((node == NULL) || (recipients == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  recipient = calloc(1, sizeof(* recipient));
  if (recipient == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  recipient->type = node_child_int(node,
      MAILACTIVESYNC_CP_RESOLVERECIPIENTS,
      MAILACTIVESYNC_RESOLVERECIPIENTS_TYPE);
  recipient->display_name = dup_node_text(node,
      MAILACTIVESYNC_CP_RESOLVERECIPIENTS,
      MAILACTIVESYNC_RESOLVERECIPIENTS_DISPLAY_NAME);
  recipient->email_address = dup_node_text(node,
      MAILACTIVESYNC_CP_RESOLVERECIPIENTS,
      MAILACTIVESYNC_RESOLVERECIPIENTS_EMAIL_ADDRESS);

  r = parse_resolved_recipient_certificates(node, recipient);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;

  if (clist_append(recipients, recipient) < 0) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto err;
  }

  return MAILACTIVESYNC_NO_ERROR;

 err:
  mailactivesync_resolved_recipient_free(recipient);
  return r;
}

static int parse_resolve_recipients_response(
    struct mailactivesync_wbxml_node * node,
    struct mailactivesync_resolve_recipients_result * result)
{
  struct mailactivesync_resolve_recipients_response * response;
  const char * status;
  const char * recipient_count;
  clistiter * cur;
  int r;

  if ((node == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  status = node_child_text(node, MAILACTIVESYNC_CP_RESOLVERECIPIENTS,
      MAILACTIVESYNC_RESOLVERECIPIENTS_STATUS);
  if (status == NULL)
    return MAILACTIVESYNC_ERROR_PROTOCOL;

  response = calloc(1, sizeof(* response));
  if (response == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  response->recipients = clist_new();
  if (response->recipients == NULL) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto err;
  }
  response->to = dup_node_text(node, MAILACTIVESYNC_CP_RESOLVERECIPIENTS,
      MAILACTIVESYNC_RESOLVERECIPIENTS_TO);
  response->status = atoi(status);
  recipient_count = node_child_text(node,
      MAILACTIVESYNC_CP_RESOLVERECIPIENTS,
      MAILACTIVESYNC_RESOLVERECIPIENTS_RECIPIENT_COUNT);
  if (recipient_count != NULL)
    response->recipient_count = (uint32_t) strtoul(recipient_count, NULL, 10);

  for (cur = node->children != NULL ? clist_begin(node->children) : NULL;
      cur != NULL; cur = clist_next(cur)) {
    struct mailactivesync_wbxml_node * child;

    child = clist_content(cur);
    if ((child->code_page != MAILACTIVESYNC_CP_RESOLVERECIPIENTS) ||
        (child->token != MAILACTIVESYNC_RESOLVERECIPIENTS_RECIPIENT))
      continue;
    r = parse_resolved_recipient(child, response->recipients);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
  }

  if (clist_append(result->responses, response) < 0) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto err;
  }

  return mailactivesync_resolve_recipients_response_status_to_error(
      response->status);

 err:
  mailactivesync_resolve_recipients_response_free(response);
  return r;
}

static int parse_resolve_recipients_result(
    struct mailactivesync_wbxml_node * root,
    struct mailactivesync_resolve_recipients_result * result)
{
  clistiter * cur;
  const char * status;
  int response_error;
  int r;

  if ((root == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  status = node_child_text(root, MAILACTIVESYNC_CP_RESOLVERECIPIENTS,
      MAILACTIVESYNC_RESOLVERECIPIENTS_STATUS);
  if (status == NULL)
    return MAILACTIVESYNC_ERROR_PROTOCOL;
  result->status = atoi(status);
  response_error = mailactivesync_resolve_recipients_status_to_error(
      result->status);

  for (cur = root->children != NULL ? clist_begin(root->children) : NULL;
      cur != NULL; cur = clist_next(cur)) {
    struct mailactivesync_wbxml_node * child;
    int response_count;

    child = clist_content(cur);
    if ((child->code_page != MAILACTIVESYNC_CP_RESOLVERECIPIENTS) ||
        (child->token != MAILACTIVESYNC_RESOLVERECIPIENTS_RESPONSE))
      continue;
    response_count = clist_count(result->responses);
    r = parse_resolve_recipients_response(child, result);
    if ((r != MAILACTIVESYNC_NO_ERROR) &&
        (clist_count(result->responses) == response_count))
      return r;
    if ((r != MAILACTIVESYNC_NO_ERROR) &&
        (response_error == MAILACTIVESYNC_NO_ERROR))
      response_error = r;
  }

  return response_error;
}

int mailactivesync_command_resolve_recipients(mailactivesync * session,
    clist * recipients,
    uint32_t max_ambiguous_recipients,
    struct mailactivesync_resolve_recipients_result ** result)
{
  struct mailactivesync_resolve_recipients_request request;

  memset(&request, 0, sizeof(request));
  request.recipients = recipients;
  request.max_ambiguous_recipients = max_ambiguous_recipients;

  return mailactivesync_command_resolve_recipients_ext(session, &request,
      result);
}

int mailactivesync_command_resolve_recipients_ext(mailactivesync * session,
    const struct mailactivesync_resolve_recipients_request * request,
    struct mailactivesync_resolve_recipients_result ** result)
{
  struct mailactivesync_wbxml_node * request_node;
  struct mailactivesync_wbxml_document * response;
  struct mailactivesync_resolve_recipients_result * parsed;
  int r;

  if ((session == NULL) || (request == NULL) ||
      (request->recipients == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  request_node = NULL;
  response = NULL;
  parsed = NULL;

  r = build_resolve_recipients_request(request, &request_node);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  r = post_wbxml_document(session, "ResolveRecipients", NULL, request_node,
      &response);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;

  if ((response->root == NULL) ||
      (response->root->code_page != MAILACTIVESYNC_CP_RESOLVERECIPIENTS) ||
      (response->root->token !=
          MAILACTIVESYNC_RESOLVERECIPIENTS_RESOLVE_RECIPIENTS)) {
    r = MAILACTIVESYNC_ERROR_PROTOCOL;
    goto cleanup;
  }

  parsed = resolve_recipients_result_new();
  if (parsed == NULL) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto cleanup;
  }

  r = parse_resolve_recipients_result(response->root, parsed);
  * result = parsed;
  parsed = NULL;

 cleanup:
  mailactivesync_resolve_recipients_result_free(parsed);
  mailactivesync_wbxml_document_free(response);
  mailactivesync_wbxml_node_free(request_node);
  return r;
}

static int build_item_operations_fetch_node(
    const struct mailactivesync_item_operations_fetch_request * request,
    struct mailactivesync_wbxml_node ** result)
{
  struct mailactivesync_wbxml_node * fetch;
  struct mailactivesync_wbxml_node * options;
  struct mailactivesync_wbxml_node * body_options;
  int r;

  if ((request == NULL) || (request->collection_id == NULL) ||
      (request->server_id == NULL) || (result == NULL) ||
      (request->body_part_type < 0) ||
      (request->body_part_type >
          MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_MIME))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  fetch = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_FETCH);
  options = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_OPTIONS);
  body_options = mailactivesync_wbxml_node_new(
      MAILACTIVESYNC_CP_AIRSYNCBASE,
      request->body_part_type != 0 ?
          MAILACTIVESYNC_AIRSYNCBASE_BODY_PART_PREFERENCE :
          MAILACTIVESYNC_AIRSYNCBASE_BODY_PREFERENCE);
  if ((fetch == NULL) || (options == NULL) || (body_options == NULL)) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto err;
  }

  r = node_add_text(fetch, MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_STORE, "Mailbox");
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  r = node_add_text(fetch, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_COLLECTION_ID, request->collection_id);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  r = node_add_text(fetch, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_SERVER_ID, request->server_id);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  r = node_add_uint(body_options, MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_TYPE,
      request->body_part_type != 0 ?
          (uint32_t) request->body_part_type :
          MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_MIME);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  if (request->truncation_size != 0) {
    r = node_add_uint(body_options, MAILACTIVESYNC_CP_AIRSYNCBASE,
        MAILACTIVESYNC_AIRSYNCBASE_TRUNCATION_SIZE,
        request->truncation_size);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
  }
  r = mailactivesync_wbxml_node_add_child(options, body_options);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  body_options = NULL;
  r = mailactivesync_wbxml_node_add_child(fetch, options);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  options = NULL;

  * result = fetch;
  return MAILACTIVESYNC_NO_ERROR;

 err:
  mailactivesync_wbxml_node_free(body_options);
  mailactivesync_wbxml_node_free(options);
  mailactivesync_wbxml_node_free(fetch);
  return r;
}

static int build_item_operations_fetch_request(clist * requests,
    struct mailactivesync_wbxml_node ** result)
{
  struct mailactivesync_wbxml_node * root;
  clistiter * cur;
  int r;

  if ((requests == NULL) || (clist_count(requests) == 0) ||
      (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  root = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_ITEM_OPERATIONS);
  if (root == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  for (cur = clist_begin(requests); cur != NULL; cur = clist_next(cur)) {
    struct mailactivesync_wbxml_node * fetch;

    fetch = NULL;
    r = build_item_operations_fetch_node(clist_content(cur), &fetch);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
    r = mailactivesync_wbxml_node_add_child(root, fetch);
    if (r != MAILACTIVESYNC_NO_ERROR) {
      mailactivesync_wbxml_node_free(fetch);
      goto err;
    }
  }

  * result = root;
  return MAILACTIVESYNC_NO_ERROR;

 err:
  mailactivesync_wbxml_node_free(root);
  return r;
}

static struct mailactivesync_item_operations_fetch_result *
item_operations_fetch_result_new(void)
{
  struct mailactivesync_item_operations_fetch_result * result;

  result = calloc(1, sizeof(* result));
  if (result == NULL)
    return NULL;
  result->items = clist_new();
  if (result->items == NULL) {
    free(result);
    return NULL;
  }

  return result;
}

static int parse_item_operations_mailbox_fetch(
    struct mailactivesync_wbxml_node * fetch,
    const struct mailactivesync_item_operations_fetch_request * request,
    struct mailactivesync_item ** result)
{
  struct mailactivesync_wbxml_node * properties;
  struct mailactivesync_item * item;
  const char * collection_id;
  const char * server_id;
  int r;

  if ((fetch == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  item = calloc(1, sizeof(* item));
  if (item == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  item->status = node_child_int(fetch, MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_STATUS);
  collection_id = node_child_text(fetch, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_COLLECTION_ID);
  if ((collection_id == NULL) && (request != NULL))
    collection_id = request->collection_id;
  server_id = node_child_text(fetch, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_SERVER_ID);
  if ((server_id == NULL) && (request != NULL))
    server_id = request->server_id;

  if (collection_id != NULL) {
    item->collection_id = strdup(collection_id);
    if (item->collection_id == NULL) {
      r = MAILACTIVESYNC_ERROR_MEMORY;
      goto err;
    }
  }
  if (server_id != NULL) {
    item->server_id = strdup(server_id);
    if (item->server_id == NULL) {
      r = MAILACTIVESYNC_ERROR_MEMORY;
      goto err;
    }
  }

  properties = node_child(fetch, MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_PROPERTIES);
  r = parse_body(node_child(properties, MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_BODY), &item->body);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  r = parse_body_parts(properties, &item->body_parts);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;

  if ((item->body != NULL) &&
      (item->body->type ==
          MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_MIME) &&
      (item->body->data != NULL)) {
    item->mime = malloc(item->body->data_len + 1);
    if (item->mime == NULL) {
      r = MAILACTIVESYNC_ERROR_MEMORY;
      goto err;
    }
    memcpy(item->mime, item->body->data, item->body->data_len);
    item->mime[item->body->data_len] = '\0';
    item->mime_len = item->body->data_len;
  }

  * result = item;
  return MAILACTIVESYNC_NO_ERROR;

 err:
  mailactivesync_item_free(item);
  return r;
}

int mailactivesync_command_item_operations_fetch(mailactivesync * session,
    const char * collection_id,
    const char * server_id,
    struct mailactivesync_item ** result)
{
  struct mailactivesync_item_operations_fetch_request request_item;
  struct mailactivesync_item_operations_fetch_result * multi_result;
  struct mailactivesync_item * item;
  clist * requests;
  clistiter * first;
  int r;

  if ((session == NULL) || (collection_id == NULL) || (server_id == NULL) ||
      (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  multi_result = NULL;
  item = NULL;

  memset(&request_item, 0, sizeof(request_item));
  request_item.collection_id = collection_id;
  request_item.server_id = server_id;
  request_item.body_part_type = 0;
  request_item.truncation_size = 200 * 1024;

  requests = clist_new();
  if (requests == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;
  if (clist_append(requests, &request_item) < 0) {
    clist_free(requests);
    return MAILACTIVESYNC_ERROR_MEMORY;
  }

  r = mailactivesync_command_item_operations_fetch_multi(session, requests,
      &multi_result);
  clist_free(requests);
  if (multi_result == NULL)
    goto cleanup;

  first = clist_begin(multi_result->items);
  if (first == NULL) {
    if (r == MAILACTIVESYNC_NO_ERROR)
      r = MAILACTIVESYNC_ERROR_PROTOCOL;
    goto cleanup;
  }
  item = clist_content(first);
  clist_delete(multi_result->items, first);
  * result = item;
  item = NULL;

 cleanup:
  mailactivesync_item_free(item);
  mailactivesync_item_operations_fetch_result_free(multi_result);
  return r;
}

int mailactivesync_command_item_operations_fetch_body_part(
    mailactivesync * session,
    const char * collection_id,
    const char * server_id,
    int body_type,
    uint32_t truncation_size,
    struct mailactivesync_item ** result)
{
  struct mailactivesync_item_operations_fetch_request request_item;
  struct mailactivesync_item_operations_fetch_result * multi_result;
  struct mailactivesync_item * item;
  clist * requests;
  clistiter * first;
  int r;

  if ((session == NULL) || (collection_id == NULL) || (server_id == NULL) ||
      (body_type < MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_PLAIN_TEXT) ||
      (body_type > MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_MIME) ||
      (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  multi_result = NULL;
  item = NULL;

  memset(&request_item, 0, sizeof(request_item));
  request_item.collection_id = collection_id;
  request_item.server_id = server_id;
  request_item.body_part_type = body_type;
  request_item.truncation_size = truncation_size;

  requests = clist_new();
  if (requests == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;
  if (clist_append(requests, &request_item) < 0) {
    clist_free(requests);
    return MAILACTIVESYNC_ERROR_MEMORY;
  }

  r = mailactivesync_command_item_operations_fetch_multi(session, requests,
      &multi_result);
  clist_free(requests);
  if (multi_result == NULL)
    goto cleanup;

  first = clist_begin(multi_result->items);
  if (first == NULL) {
    if (r == MAILACTIVESYNC_NO_ERROR)
      r = MAILACTIVESYNC_ERROR_PROTOCOL;
    goto cleanup;
  }
  item = clist_content(first);
  clist_delete(multi_result->items, first);
  * result = item;
  item = NULL;

 cleanup:
  mailactivesync_item_free(item);
  mailactivesync_item_operations_fetch_result_free(multi_result);
  return r;
}

int mailactivesync_command_item_operations_fetch_multi(
    mailactivesync * session,
    clist * requests,
    struct mailactivesync_item_operations_fetch_result ** result)
{
  struct mailactivesync_wbxml_node * request;
  struct mailactivesync_wbxml_document * response;
  struct mailactivesync_wbxml_node * response_node;
  struct mailactivesync_item_operations_fetch_result * parsed;
  clistiter * cur;
  clistiter * request_cur;
  int response_error;
  int r;

  if ((session == NULL) || (requests == NULL) ||
      (clist_count(requests) == 0) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  request = NULL;
  response = NULL;
  parsed = NULL;

  r = build_item_operations_fetch_request(requests, &request);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  r = post_wbxml_document(session, "ItemOperations", NULL, request,
      &response);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;

  if ((response->root == NULL) ||
      (response->root->code_page != MAILACTIVESYNC_CP_ITEMOPERATIONS) ||
      (response->root->token !=
          MAILACTIVESYNC_ITEMOPERATIONS_ITEM_OPERATIONS)) {
    r = MAILACTIVESYNC_ERROR_PROTOCOL;
    goto cleanup;
  }

  parsed = item_operations_fetch_result_new();
  if (parsed == NULL) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto cleanup;
  }

  parsed->status = node_child_int(response->root,
      MAILACTIVESYNC_CP_ITEMOPERATIONS, MAILACTIVESYNC_ITEMOPERATIONS_STATUS);
  response_error = mailactivesync_item_operations_status_to_error(
      parsed->status);

  response_node = node_child(response->root, MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_RESPONSE);
  request_cur = clist_begin(requests);
  for (cur = response_node != NULL && response_node->children != NULL ?
      clist_begin(response_node->children) : NULL;
      cur != NULL; cur = clist_next(cur)) {
    struct mailactivesync_wbxml_node * fetch;
    const struct mailactivesync_item_operations_fetch_request *
        request_context;
    struct mailactivesync_item * item;
    int item_error;

    fetch = clist_content(cur);
    if ((fetch == NULL) ||
        (fetch->code_page != MAILACTIVESYNC_CP_ITEMOPERATIONS) ||
        (fetch->token != MAILACTIVESYNC_ITEMOPERATIONS_FETCH))
      continue;

    request_context = request_cur != NULL ? clist_content(request_cur) : NULL;
    item = NULL;
    r = parse_item_operations_mailbox_fetch(fetch, request_context, &item);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto cleanup;
    if (clist_append(parsed->items, item) < 0) {
      mailactivesync_item_free(item);
      r = MAILACTIVESYNC_ERROR_MEMORY;
      goto cleanup;
    }

    item_error = mailactivesync_item_operations_status_to_error(
        item->status);
    if ((item_error != MAILACTIVESYNC_NO_ERROR) &&
        (response_error == MAILACTIVESYNC_NO_ERROR))
      response_error = item_error;

    if (request_cur != NULL)
      request_cur = clist_next(request_cur);
  }

  * result = parsed;
  parsed = NULL;
  r = response_error;

 cleanup:
  mailactivesync_item_operations_fetch_result_free(parsed);
  mailactivesync_wbxml_document_free(response);
  mailactivesync_wbxml_node_free(request);
  return r;
}

static int build_item_operations_fetch_attachment_request(
    const char * file_reference,
    const char * range,
    struct mailactivesync_wbxml_node ** result)
{
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_node * fetch;
  struct mailactivesync_wbxml_node * options;
  int r;

  if ((file_reference == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  options = NULL;
  root = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_ITEM_OPERATIONS);
  fetch = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_FETCH);
  if ((root == NULL) || (fetch == NULL)) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto err;
  }

  r = node_add_text(fetch, MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_STORE, "Mailbox");
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  r = node_add_text(fetch, MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_FILE_REFERENCE, file_reference);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;

  if (range != NULL) {
    options = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_ITEMOPERATIONS,
        MAILACTIVESYNC_ITEMOPERATIONS_OPTIONS);
    if (options == NULL) {
      r = MAILACTIVESYNC_ERROR_MEMORY;
      goto err;
    }
    r = node_add_text(options, MAILACTIVESYNC_CP_ITEMOPERATIONS,
        MAILACTIVESYNC_ITEMOPERATIONS_RANGE, range);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
    r = mailactivesync_wbxml_node_add_child(fetch, options);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
    options = NULL;
  }

  r = mailactivesync_wbxml_node_add_child(root, fetch);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  fetch = NULL;

  * result = root;
  return MAILACTIVESYNC_NO_ERROR;

 err:
  mailactivesync_wbxml_node_free(options);
  mailactivesync_wbxml_node_free(fetch);
  mailactivesync_wbxml_node_free(root);
  return r;
}

static int parse_item_operations_attachment_fetch(
    struct mailactivesync_wbxml_node * fetch,
    const char * request_file_reference,
    struct mailactivesync_attachment_data ** result)
{
  struct mailactivesync_attachment_data * parsed;
  struct mailactivesync_wbxml_node * properties;
  struct mailactivesync_wbxml_node * data_node;
  int r;

  if ((fetch == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  parsed = calloc(1, sizeof(* parsed));
  if (parsed == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  parsed->status = node_child_int(fetch, MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_STATUS);
  parsed->file_reference = dup_node_text(fetch,
      MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_FILE_REFERENCE);
  if ((parsed->file_reference == NULL) && (request_file_reference != NULL)) {
    parsed->file_reference = strdup(request_file_reference);
    if (parsed->file_reference == NULL) {
      r = MAILACTIVESYNC_ERROR_MEMORY;
      goto err;
    }
  }

  properties = node_child(fetch, MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_PROPERTIES);
  parsed->range = dup_node_text(properties, MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_RANGE);
  parsed->total = (uint32_t) node_child_int(properties,
      MAILACTIVESYNC_CP_ITEMOPERATIONS, MAILACTIVESYNC_ITEMOPERATIONS_TOTAL);
  data_node = node_child(properties, MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_DATA);
  r = copy_node_payload(&parsed->data, &parsed->data_len, data_node);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;

  * result = parsed;
  return MAILACTIVESYNC_NO_ERROR;

 err:
  mailactivesync_attachment_data_free(parsed);
  return r;
}

int mailactivesync_command_item_operations_fetch_attachment(
    mailactivesync * session,
    const char * file_reference,
    const char * range,
    struct mailactivesync_attachment_data ** result)
{
  struct mailactivesync_wbxml_node * request;
  struct mailactivesync_wbxml_document * response;
  struct mailactivesync_wbxml_node * response_node;
  struct mailactivesync_wbxml_node * fetch;
  struct mailactivesync_attachment_data * parsed;
  int status;
  int r;

  if ((session == NULL) || (file_reference == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  request = NULL;
  response = NULL;
  parsed = NULL;

  r = build_item_operations_fetch_attachment_request(file_reference, range,
      &request);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  r = post_wbxml_document(session, "ItemOperations", NULL, request,
      &response);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;

  if ((response->root == NULL) ||
      (response->root->code_page != MAILACTIVESYNC_CP_ITEMOPERATIONS) ||
      (response->root->token !=
          MAILACTIVESYNC_ITEMOPERATIONS_ITEM_OPERATIONS)) {
    r = MAILACTIVESYNC_ERROR_PROTOCOL;
    goto cleanup;
  }
  status = node_child_int(response->root, MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_STATUS);
  r = mailactivesync_item_operations_status_to_error(status);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;

  response_node = node_child(response->root, MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_RESPONSE);
  fetch = node_child(response_node, MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_FETCH);
  r = parse_item_operations_attachment_fetch(fetch, file_reference, &parsed);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;

  r = mailactivesync_item_operations_status_to_error(parsed->status);
  * result = parsed;
  parsed = NULL;

 cleanup:
  mailactivesync_attachment_data_free(parsed);
  mailactivesync_wbxml_document_free(response);
  mailactivesync_wbxml_node_free(request);
  return r;
}

static unsigned int composemail_client_counter = 0;

static int protocol_version_uses_legacy_composemail(mailactivesync * session)
{
  const char * version;

  if (session == NULL)
    return 0;

  version = session->as_protocol_version;
  if (version == NULL)
    return 0;

  return (strcmp(version, "2.5") == 0) ||
      (strcmp(version, "12.0") == 0) ||
      (strcmp(version, "12.1") == 0);
}

static int composemail_client_id(char ** result)
{
  char buffer[40];

  if (result == NULL)
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  composemail_client_counter ++;
  snprintf(buffer, sizeof(buffer), "libetpan-%lu-%u",
      (unsigned long) time(NULL), composemail_client_counter);
  * result = strdup(buffer);
  if (* result == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  return MAILACTIVESYNC_NO_ERROR;
}

static int composemail_request_client_id(
    const struct mailactivesync_composemail_request * request,
    char ** generated_client_id,
    const char ** client_id)
{
  size_t len;
  int r;

  if ((request == NULL) || (generated_client_id == NULL) ||
      (client_id == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * generated_client_id = NULL;
  * client_id = NULL;
  if (request->client_id != NULL) {
    len = strlen(request->client_id);
    if ((len == 0) || (len > 40))
      return MAILACTIVESYNC_ERROR_BAD_STATE;
    * client_id = request->client_id;
    return MAILACTIVESYNC_NO_ERROR;
  }

  r = composemail_client_id(generated_client_id);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;
  * client_id = * generated_client_id;
  return MAILACTIVESYNC_NO_ERROR;
}

static int build_composemail_source(
    const struct mailactivesync_composemail_request * request,
    struct mailactivesync_wbxml_node ** result)
{
  struct mailactivesync_wbxml_node * source;
  int has_folder_item;
  int has_long_id;
  int r;

  if ((request == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  has_folder_item = (request->collection_id != NULL) &&
      (request->server_id != NULL);
  has_long_id = request->long_id != NULL;
  if ((has_folder_item == has_long_id) ||
      ((request->collection_id == NULL) != (request->server_id == NULL)))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  source = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_COMPOSEMAIL,
      MAILACTIVESYNC_COMPOSEMAIL_SOURCE);
  if (source == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  if (has_folder_item) {
    r = node_add_text(source, MAILACTIVESYNC_CP_COMPOSEMAIL,
        MAILACTIVESYNC_COMPOSEMAIL_FOLDER_ID, request->collection_id);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
    r = node_add_text(source, MAILACTIVESYNC_CP_COMPOSEMAIL,
        MAILACTIVESYNC_COMPOSEMAIL_ITEM_ID, request->server_id);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
  }
  else {
    r = node_add_text(source, MAILACTIVESYNC_CP_COMPOSEMAIL,
        MAILACTIVESYNC_COMPOSEMAIL_LONG_ID, request->long_id);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
  }

  if (request->instance_id != NULL) {
    r = node_add_text(source, MAILACTIVESYNC_CP_COMPOSEMAIL,
        MAILACTIVESYNC_COMPOSEMAIL_INSTANCE_ID, request->instance_id);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
  }

  * result = source;
  return MAILACTIVESYNC_NO_ERROR;

 err:
  mailactivesync_wbxml_node_free(source);
  return r;
}

static int build_composemail_request(uint8_t root_token,
    const struct mailactivesync_composemail_request * request,
    struct mailactivesync_wbxml_node ** result)
{
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_node * source;
  char * generated_client_id;
  const char * client_id;
  int r;

  if ((request == NULL) || (request->mime_message == NULL) ||
      (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;
  if (((root_token == MAILACTIVESYNC_COMPOSEMAIL_SMART_REPLY) ||
        (root_token == MAILACTIVESYNC_COMPOSEMAIL_SMART_FORWARD)) &&
      ((request->long_id == NULL) &&
      ((request->collection_id == NULL) || (request->server_id == NULL))))
    return MAILACTIVESYNC_ERROR_BAD_STATE;
  if ((root_token == MAILACTIVESYNC_COMPOSEMAIL_SEND_MAIL) &&
      ((request->collection_id != NULL) || (request->server_id != NULL) ||
      (request->long_id != NULL) || (request->instance_id != NULL) ||
      request->replace_mime))
    return MAILACTIVESYNC_ERROR_BAD_STATE;
  if ((request->long_id != NULL) &&
      ((request->collection_id != NULL) || (request->server_id != NULL)))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  generated_client_id = NULL;
  client_id = NULL;
  source = NULL;
  root = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_COMPOSEMAIL,
      root_token);
  if (root == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  r = composemail_request_client_id(request, &generated_client_id,
      &client_id);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  r = node_add_text(root, MAILACTIVESYNC_CP_COMPOSEMAIL,
      MAILACTIVESYNC_COMPOSEMAIL_CLIENT_ID, client_id);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;

  if ((root_token == MAILACTIVESYNC_COMPOSEMAIL_SMART_REPLY) ||
      (root_token == MAILACTIVESYNC_COMPOSEMAIL_SMART_FORWARD)) {
    r = build_composemail_source(request, &source);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
    r = mailactivesync_wbxml_node_add_child(root, source);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
    source = NULL;
  }

  if (request->save_in_sent) {
    r = node_add_empty(root, MAILACTIVESYNC_CP_COMPOSEMAIL,
        MAILACTIVESYNC_COMPOSEMAIL_SAVE_IN_SENT_ITEMS);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
  }

  if (request->replace_mime) {
    r = node_add_empty(root, MAILACTIVESYNC_CP_COMPOSEMAIL,
        MAILACTIVESYNC_COMPOSEMAIL_REPLACE_MIME);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
  }

  r = node_add_opaque(root, MAILACTIVESYNC_CP_COMPOSEMAIL,
      MAILACTIVESYNC_COMPOSEMAIL_MIME,
      (const unsigned char *) request->mime_message,
      request->mime_message_len);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;

  free(generated_client_id);
  * result = root;
  return MAILACTIVESYNC_NO_ERROR;

 err:
  free(generated_client_id);
  mailactivesync_wbxml_node_free(source);
  mailactivesync_wbxml_node_free(root);
  return r;
}

static int parse_composemail_response(
    struct mailactivesync_http_response * response,
    uint8_t expected_root_token)
{
  struct mailactivesync_wbxml_document * response_document;
  const char * status;
  int r;

  if (response == NULL)
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  if ((response->body == NULL) || (response->body_len == 0))
    return MAILACTIVESYNC_NO_ERROR;
  if (!response_body_is_wbxml(response))
    return MAILACTIVESYNC_ERROR_RESPONSE_NOT_WBXML;

  response_document = NULL;
  r = mailactivesync_wbxml_decode(response->body, response->body_len,
      &response_document);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  if ((response_document->root == NULL) ||
      (response_document->root->code_page != MAILACTIVESYNC_CP_COMPOSEMAIL) ||
      (response_document->root->token != expected_root_token)) {
    r = MAILACTIVESYNC_ERROR_PROTOCOL;
    goto cleanup;
  }

  status = node_child_text(response_document->root,
      MAILACTIVESYNC_CP_COMPOSEMAIL, MAILACTIVESYNC_COMPOSEMAIL_STATUS);
  r = status != NULL ? mailactivesync_global_status_to_error(atoi(status)) :
      MAILACTIVESYNC_NO_ERROR;

 cleanup:
  mailactivesync_wbxml_document_free(response_document);
  return r;
}

static int composemail_legacy_options(
    const struct mailactivesync_composemail_request * request,
    const char ** options)
{
  if ((request == NULL) || (options == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * options = request->save_in_sent ? "1" : NULL;
  return MAILACTIVESYNC_NO_ERROR;
}

static int composemail_legacy_command(mailactivesync * session,
    const char * command_name,
    uint8_t root_token,
    const struct mailactivesync_composemail_request * request)
{
  struct mailactivesync_http_response * response;
  const char * collection_id;
  const char * item_id;
  const char * long_id;
  const char * options;
  int r;

  if ((session == NULL) || (command_name == NULL) || (request == NULL) ||
      (request->mime_message == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;
  if ((request->client_id != NULL) || request->replace_mime)
    return MAILACTIVESYNC_ERROR_BAD_STATE;
  if ((request->long_id != NULL) &&
      ((request->collection_id != NULL) || (request->server_id != NULL)))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  collection_id = NULL;
  item_id = NULL;
  long_id = NULL;
  if ((root_token == MAILACTIVESYNC_COMPOSEMAIL_SMART_REPLY) ||
      (root_token == MAILACTIVESYNC_COMPOSEMAIL_SMART_FORWARD)) {
    if (request->long_id != NULL) {
      long_id = request->long_id;
    }
    else {
      if ((request->collection_id == NULL) || (request->server_id == NULL))
        return MAILACTIVESYNC_ERROR_BAD_STATE;
      collection_id = request->collection_id;
      item_id = request->server_id;
    }
  }
  else if ((request->collection_id != NULL) || (request->server_id != NULL) ||
      (request->long_id != NULL) || (request->instance_id != NULL)) {
    return MAILACTIVESYNC_ERROR_BAD_STATE;
  }

  r = composemail_legacy_options(request, &options);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  response = NULL;
  r = command_post_raw(session, command_name, collection_id, item_id, long_id,
      request->instance_id, options, "message/rfc822",
      (const unsigned char *) request->mime_message,
      request->mime_message_len, &response);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;

  r = parse_composemail_response(response, root_token);

 cleanup:
  mailactivesync_http_response_free(response);
  return r;
}

static int composemail_command(mailactivesync * session,
    const char * command_name,
    uint8_t root_token,
    const struct mailactivesync_composemail_request * request)
{
  struct mailactivesync_http_response * response;
  struct mailactivesync_wbxml_node * root;
  int r;

  if ((session == NULL) || (command_name == NULL) || (request == NULL) ||
      (request->mime_message == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;
  if (protocol_version_uses_legacy_composemail(session))
    return composemail_legacy_command(session, command_name, root_token,
        request);

  response = NULL;
  root = NULL;
  r = build_composemail_request(root_token, request, &root);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  r = post_wbxml_document_response(session, command_name, NULL, root,
      &response);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;

  r = parse_composemail_response(response, root_token);

 cleanup:
  mailactivesync_http_response_free(response);
  mailactivesync_wbxml_node_free(root);
  return r;
}

int mailactivesync_command_send_mail(mailactivesync * session,
    const char * mime_message,
    size_t mime_message_len,
    int save_in_sent)
{
  struct mailactivesync_composemail_request request;

  memset(&request, 0, sizeof(request));
  request.mime_message = mime_message;
  request.mime_message_len = mime_message_len;
  request.save_in_sent = save_in_sent;

  return mailactivesync_command_send_mail_ext(session, &request);
}

int mailactivesync_command_send_mail_ext(mailactivesync * session,
    const struct mailactivesync_composemail_request * request)
{
  return composemail_command(session, "SendMail",
      MAILACTIVESYNC_COMPOSEMAIL_SEND_MAIL, request);
}

int mailactivesync_command_smart_reply(mailactivesync * session,
    const char * collection_id,
    const char * server_id,
    const char * mime_message,
    size_t mime_message_len,
    int save_in_sent)
{
  struct mailactivesync_composemail_request request;

  memset(&request, 0, sizeof(request));
  request.collection_id = collection_id;
  request.server_id = server_id;
  request.mime_message = mime_message;
  request.mime_message_len = mime_message_len;
  request.save_in_sent = save_in_sent;

  return mailactivesync_command_smart_reply_ext(session, &request);
}

int mailactivesync_command_smart_reply_ext(mailactivesync * session,
    const struct mailactivesync_composemail_request * request)
{
  return composemail_command(session, "SmartReply",
      MAILACTIVESYNC_COMPOSEMAIL_SMART_REPLY, request);
}

int mailactivesync_command_smart_forward(mailactivesync * session,
    const char * collection_id,
    const char * server_id,
    const char * mime_message,
    size_t mime_message_len,
    int save_in_sent)
{
  struct mailactivesync_composemail_request request;

  memset(&request, 0, sizeof(request));
  request.collection_id = collection_id;
  request.server_id = server_id;
  request.mime_message = mime_message;
  request.mime_message_len = mime_message_len;
  request.save_in_sent = save_in_sent;

  return mailactivesync_command_smart_forward_ext(session, &request);
}

int mailactivesync_command_smart_forward_ext(mailactivesync * session,
    const struct mailactivesync_composemail_request * request)
{
  return composemail_command(session, "SmartForward",
      MAILACTIVESYNC_COMPOSEMAIL_SMART_FORWARD, request);
}

int mailactivesync_command_sync(mailactivesync * session,
    struct mailactivesync_sync_request * request,
    struct mailactivesync_sync_result ** result)
{
  struct mailactivesync_http_response * response;
  struct mailactivesync_wbxml_document * response_document;
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_node * collections;
  struct mailactivesync_wbxml_node * collection;
  struct mailactivesync_sync_result * parsed;
  const char * response_status;
  int r;

  if ((session == NULL) || (request == NULL) ||
      (request->collection_id == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  response = NULL;
  response_document = NULL;
  root = NULL;
  r = build_sync_request(session, request, &root);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  r = post_wbxml_document_response(session, "Sync", NULL, root, &response);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup_root;

  if ((response->body == NULL) || (response->body_len == 0)) {
    parsed = sync_result_new();
    if (parsed == NULL) {
      r = MAILACTIVESYNC_ERROR_MEMORY;
      goto cleanup_response;
    }
    parsed->status = 1;
    parsed->empty_response = 1;
    if (request->sync_key != NULL) {
      parsed->sync_key = strdup(request->sync_key);
      if (parsed->sync_key == NULL) {
        mailactivesync_sync_result_free(parsed);
        r = MAILACTIVESYNC_ERROR_MEMORY;
        goto cleanup_response;
      }
    }
    * result = parsed;
    goto cleanup_response;
  }

  if (!response_body_is_wbxml(response)) {
    r = MAILACTIVESYNC_ERROR_RESPONSE_NOT_WBXML;
    goto cleanup_response;
  }

  r = mailactivesync_wbxml_decode(response->body, response->body_len,
      &response_document);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup_response;

  if ((response_document->root == NULL) ||
      (response_document->root->code_page != MAILACTIVESYNC_CP_AIRSYNC) ||
      (response_document->root->token != MAILACTIVESYNC_AIRSYNC_SYNC)) {
    r = MAILACTIVESYNC_ERROR_PROTOCOL;
    goto cleanup_response;
  }

  collections = node_child(response_document->root, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_COLLECTIONS);
  collection = node_child(collections, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_COLLECTION);

  parsed = sync_result_new();
  if (parsed == NULL) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto cleanup_response;
  }

  if (collection == NULL) {
    response_status = node_child_text(response_document->root,
        MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_STATUS);
    if (response_status == NULL) {
      mailactivesync_sync_result_free(parsed);
      r = MAILACTIVESYNC_ERROR_PROTOCOL;
      goto cleanup_response;
    }

    parsed->status = atoi(response_status);
    parsed->limit = (uint32_t) node_child_int(response_document->root,
        MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_LIMIT);
    * result = parsed;
    goto cleanup_response;
  }

  r = parse_sync_collection_result(collection, parsed);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    mailactivesync_sync_result_free(parsed);
    goto cleanup_response;
  }

  * result = parsed;

 cleanup_response:
  mailactivesync_wbxml_document_free(response_document);
  mailactivesync_http_response_free(response);
 cleanup_root:
  mailactivesync_wbxml_node_free(root);
  return r;
}

int mailactivesync_command_sync_multi(mailactivesync * session,
    clist * requests,
    struct mailactivesync_sync_result ** result)
{
  struct mailactivesync_http_response * response;
  struct mailactivesync_wbxml_document * response_document;
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_node * collections;
  struct mailactivesync_sync_result * parsed;
  const char * response_status;
  clistiter * cur;
  int r;

  if ((session == NULL) || (requests == NULL) ||
      (clist_count(requests) == 0) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  response = NULL;
  response_document = NULL;
  root = NULL;
  parsed = NULL;

  r = build_sync_request_multi(session, requests, &root);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  r = post_wbxml_document_response(session, "Sync", NULL, root, &response);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;

  parsed = sync_result_new();
  if (parsed == NULL) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto cleanup;
  }

  if ((response->body == NULL) || (response->body_len == 0)) {
    parsed->status = 1;
    parsed->empty_response = 1;
    * result = parsed;
    parsed = NULL;
    goto cleanup;
  }

  if (!response_body_is_wbxml(response)) {
    r = MAILACTIVESYNC_ERROR_RESPONSE_NOT_WBXML;
    goto cleanup;
  }

  r = mailactivesync_wbxml_decode(response->body, response->body_len,
      &response_document);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;

  if ((response_document->root == NULL) ||
      (response_document->root->code_page != MAILACTIVESYNC_CP_AIRSYNC) ||
      (response_document->root->token != MAILACTIVESYNC_AIRSYNC_SYNC)) {
    r = MAILACTIVESYNC_ERROR_PROTOCOL;
    goto cleanup;
  }

  response_status = node_child_text(response_document->root,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_STATUS);
  if (response_status != NULL) {
    parsed->status = atoi(response_status);
    parsed->limit = (uint32_t) node_child_int(response_document->root,
        MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_LIMIT);
  }

  collections = node_child(response_document->root, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_COLLECTIONS);
  for (cur = collections != NULL && collections->children != NULL ?
      clist_begin(collections->children) : NULL;
      cur != NULL; cur = clist_next(cur)) {
    struct mailactivesync_wbxml_node * collection;
    struct mailactivesync_sync_result * collection_result;

    collection = clist_content(cur);
    if ((collection == NULL) ||
        (collection->code_page != MAILACTIVESYNC_CP_AIRSYNC) ||
        (collection->token != MAILACTIVESYNC_AIRSYNC_COLLECTION))
      continue;

    collection_result = sync_result_new();
    if (collection_result == NULL) {
      r = MAILACTIVESYNC_ERROR_MEMORY;
      goto cleanup;
    }
    r = parse_sync_collection_result(collection, collection_result);
    if (r != MAILACTIVESYNC_NO_ERROR) {
      mailactivesync_sync_result_free(collection_result);
      goto cleanup;
    }
    if (clist_append(parsed->collections, collection_result) < 0) {
      mailactivesync_sync_result_free(collection_result);
      r = MAILACTIVESYNC_ERROR_MEMORY;
      goto cleanup;
    }
  }

  if ((response_status == NULL) && (clist_count(parsed->collections) == 0)) {
    r = MAILACTIVESYNC_ERROR_PROTOCOL;
    goto cleanup;
  }

  * result = parsed;
  parsed = NULL;

 cleanup:
  mailactivesync_sync_result_free(parsed);
  mailactivesync_wbxml_document_free(response_document);
  mailactivesync_http_response_free(response);
  mailactivesync_wbxml_node_free(root);
  return r;
}
