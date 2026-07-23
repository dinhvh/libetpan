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
      "application/vnd.ms-sync.wbxml");
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

static int node_add_uint(struct mailactivesync_wbxml_node * parent,
    uint8_t code_page, uint8_t token, uint32_t value)
{
  char buffer[32];

  snprintf(buffer, sizeof(buffer), "%u", value);
  return node_add_text(parent, code_page, token, buffer);
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

static struct mailactivesync_sync_result * sync_result_new(void)
{
  struct mailactivesync_sync_result * result;

  result = malloc(sizeof(* result));
  if (result == NULL)
    return NULL;

  result->sync_key = NULL;
  result->status = 0;
  result->more_available = 0;
  result->empty_response = 0;
  result->sync_key_from_response = 0;
  result->added = clist_new();
  result->changed = clist_new();
  result->deleted = clist_new();
  if ((result->added == NULL) || (result->changed == NULL) ||
      (result->deleted == NULL)) {
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

static int parse_message_application_data(
    struct mailactivesync_wbxml_node * application_data,
    struct mailactivesync_message * message)
{
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
  message->message_class = dup_node_text(application_data,
      MAILACTIVESYNC_CP_EMAIL, MAILACTIVESYNC_EMAIL_MESSAGE_CLASS);
  message->read = node_child_int(application_data,
      MAILACTIVESYNC_CP_EMAIL, MAILACTIVESYNC_EMAIL_READ);

  r = parse_body(node_child(application_data, MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_BODY), &message->body);
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

static int session_uses_activesync_25(mailactivesync * session)
{
  return (session != NULL) && (session->as_protocol_version != NULL) &&
      (strcmp(session->as_protocol_version, "2.5") == 0);
}

static int build_sync_request(mailactivesync * session,
    struct mailactivesync_sync_request * request,
    struct mailactivesync_wbxml_node ** result)
{
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_node * collections;
  struct mailactivesync_wbxml_node * collection;
  struct mailactivesync_wbxml_node * active_collection;
  struct mailactivesync_wbxml_node * options;
  struct mailactivesync_wbxml_node * body_preference;
  const char * collection_class;
  int activesync_25;
  int r;

  * result = NULL;
  activesync_25 = session_uses_activesync_25(session);
  root = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_SYNC);
  collections = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_COLLECTIONS);
  collection = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_COLLECTION);
  active_collection = collection;
  if ((root == NULL) || (collections == NULL) || (collection == NULL)) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto err;
  }

  r = mailactivesync_wbxml_node_add_child(collections, collection);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  collection = NULL;
  r = mailactivesync_wbxml_node_add_child(root, collections);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  collections = NULL;

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
      request->body_preference != NULL ? "Email" : NULL;
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

  if ((request->body_preference != NULL) ||
      (!activesync_25 && (request->collection_class != NULL)) ||
      request->has_filter_type ||
      request->has_conflict) {
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
    if (request->body_preference != NULL) {
      body_preference = mailactivesync_wbxml_node_new(
          MAILACTIVESYNC_CP_AIRSYNCBASE,
          MAILACTIVESYNC_AIRSYNCBASE_BODY_PREFERENCE);
      if (body_preference == NULL) {
        mailactivesync_wbxml_node_free(options);
        r = MAILACTIVESYNC_ERROR_MEMORY;
        goto err;
      }
      r = node_add_uint(body_preference, MAILACTIVESYNC_CP_AIRSYNCBASE,
          MAILACTIVESYNC_AIRSYNCBASE_TYPE,
          (uint32_t) request->body_preference->type);
      if (r != MAILACTIVESYNC_NO_ERROR) {
        mailactivesync_wbxml_node_free(options);
        mailactivesync_wbxml_node_free(body_preference);
        goto err;
      }
      if (request->body_preference->truncation_size != 0) {
        r = node_add_uint(body_preference, MAILACTIVESYNC_CP_AIRSYNCBASE,
            MAILACTIVESYNC_AIRSYNCBASE_TRUNCATION_SIZE,
            request->body_preference->truncation_size);
        if (r != MAILACTIVESYNC_NO_ERROR) {
          mailactivesync_wbxml_node_free(options);
          mailactivesync_wbxml_node_free(body_preference);
          goto err;
        }
      }
      r = node_add_text(options, MAILACTIVESYNC_CP_AIRSYNC,
          MAILACTIVESYNC_AIRSYNC_MIME_SUPPORT, "2");
      if (r != MAILACTIVESYNC_NO_ERROR) {
        mailactivesync_wbxml_node_free(options);
        mailactivesync_wbxml_node_free(body_preference);
        goto err;
      }
      r = mailactivesync_wbxml_node_add_child(options, body_preference);
      if (r != MAILACTIVESYNC_NO_ERROR) {
        mailactivesync_wbxml_node_free(options);
        mailactivesync_wbxml_node_free(body_preference);
        goto err;
      }
      body_preference = NULL;
    }
    r = mailactivesync_wbxml_node_add_child(active_collection, options);
    if (r != MAILACTIVESYNC_NO_ERROR) {
      mailactivesync_wbxml_node_free(options);
      goto err;
    }
  }

  * result = root;
  return MAILACTIVESYNC_NO_ERROR;

 err:
  mailactivesync_wbxml_node_free(collections);
  mailactivesync_wbxml_node_free(collection);
  mailactivesync_wbxml_node_free(root);
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

static int build_get_item_estimate_request(const char * collection_id,
    const char * sync_key, struct mailactivesync_wbxml_node ** result)
{
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_node * collections;
  struct mailactivesync_wbxml_node * collection;
  int r;

  if ((collection_id == NULL) || (sync_key == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  root = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_GETITEMESTIMATE,
      MAILACTIVESYNC_GETITEMESTIMATE_GET_ITEM_ESTIMATE);
  collections = mailactivesync_wbxml_node_new(
      MAILACTIVESYNC_CP_GETITEMESTIMATE,
      MAILACTIVESYNC_GETITEMESTIMATE_COLLECTIONS);
  collection = mailactivesync_wbxml_node_new(
      MAILACTIVESYNC_CP_GETITEMESTIMATE,
      MAILACTIVESYNC_GETITEMESTIMATE_COLLECTION);
  if ((root == NULL) || (collections == NULL) || (collection == NULL)) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto err;
  }

  r = node_add_text(collection, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_SYNC_KEY, sync_key);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  r = node_add_text(collection, MAILACTIVESYNC_CP_GETITEMESTIMATE,
      MAILACTIVESYNC_GETITEMESTIMATE_COLLECTION_ID, collection_id);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;

  r = mailactivesync_wbxml_node_add_child(collections, collection);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  collection = NULL;
  r = mailactivesync_wbxml_node_add_child(root, collections);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  collections = NULL;

  * result = root;
  return MAILACTIVESYNC_NO_ERROR;

 err:
  mailactivesync_wbxml_node_free(collection);
  mailactivesync_wbxml_node_free(collections);
  mailactivesync_wbxml_node_free(root);
  return r;
}

int mailactivesync_command_get_item_estimate(mailactivesync * session,
    const char * collection_id,
    const char * sync_key,
    struct mailactivesync_get_item_estimate_result ** result)
{
  struct mailactivesync_http_response * http_response;
  struct mailactivesync_wbxml_node * request;
  struct mailactivesync_wbxml_document * response;
  struct mailactivesync_wbxml_node * responses;
  struct mailactivesync_wbxml_node * response_node;
  struct mailactivesync_wbxml_node * collection;
  struct mailactivesync_get_item_estimate_result * parsed;
  int r;

  if ((session == NULL) || (collection_id == NULL) || (sync_key == NULL) ||
      (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  http_response = NULL;
  request = NULL;
  response = NULL;
  parsed = NULL;

  r = build_get_item_estimate_request(collection_id, sync_key, &request);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  r = post_wbxml_document_response(session, "GetItemEstimate", NULL, request,
      &http_response);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;

  parsed = calloc(1, sizeof(* parsed));
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
  responses = node_child(response->root, MAILACTIVESYNC_CP_GETITEMESTIMATE,
      MAILACTIVESYNC_GETITEMESTIMATE_RESPONSE);
  response_node = responses;
  collection = node_child(response_node, MAILACTIVESYNC_CP_GETITEMESTIMATE,
      MAILACTIVESYNC_GETITEMESTIMATE_COLLECTION);
  parsed->collection_status = node_child_int(collection,
      MAILACTIVESYNC_CP_GETITEMESTIMATE,
      MAILACTIVESYNC_GETITEMESTIMATE_STATUS);
  parsed->estimate = (uint32_t) node_child_int(collection,
      MAILACTIVESYNC_CP_GETITEMESTIMATE,
      MAILACTIVESYNC_GETITEMESTIMATE_ESTIMATE);

  * result = parsed;
  parsed = NULL;

 cleanup:
  mailactivesync_get_item_estimate_result_free(parsed);
  mailactivesync_wbxml_document_free(response);
  mailactivesync_http_response_free(http_response);
  mailactivesync_wbxml_node_free(request);
  return r;
}

static int build_item_operations_fetch_request(const char * collection_id,
    const char * server_id, struct mailactivesync_wbxml_node ** result)
{
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_node * fetch;
  struct mailactivesync_wbxml_node * options;
  struct mailactivesync_wbxml_node * body_preference;
  int r;

  if ((collection_id == NULL) || (server_id == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  root = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_ITEM_OPERATIONS);
  fetch = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_FETCH);
  options = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_OPTIONS);
  body_preference = mailactivesync_wbxml_node_new(
      MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_BODY_PREFERENCE);
  if ((root == NULL) || (fetch == NULL) || (options == NULL) ||
      (body_preference == NULL)) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto err;
  }

  r = node_add_text(fetch, MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_STORE, "Mailbox");
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  r = node_add_text(fetch, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_COLLECTION_ID, collection_id);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  r = node_add_text(fetch, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_SERVER_ID, server_id);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  r = node_add_uint(body_preference, MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_TYPE,
      MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_MIME);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  r = node_add_uint(body_preference, MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_TRUNCATION_SIZE, 200 * 1024);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  r = mailactivesync_wbxml_node_add_child(options, body_preference);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  body_preference = NULL;
  r = mailactivesync_wbxml_node_add_child(fetch, options);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  options = NULL;
  r = mailactivesync_wbxml_node_add_child(root, fetch);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  fetch = NULL;

  * result = root;
  return MAILACTIVESYNC_NO_ERROR;

 err:
  mailactivesync_wbxml_node_free(body_preference);
  mailactivesync_wbxml_node_free(options);
  mailactivesync_wbxml_node_free(fetch);
  mailactivesync_wbxml_node_free(root);
  return r;
}

int mailactivesync_command_item_operations_fetch(mailactivesync * session,
    const char * collection_id,
    const char * server_id,
    struct mailactivesync_item ** result)
{
  struct mailactivesync_wbxml_node * request;
  struct mailactivesync_wbxml_document * response;
  struct mailactivesync_wbxml_node * response_node;
  struct mailactivesync_wbxml_node * fetch;
  struct mailactivesync_wbxml_node * properties;
  struct mailactivesync_item * item;
  int status;
  int r;

  if ((session == NULL) || (collection_id == NULL) || (server_id == NULL) ||
      (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  request = NULL;
  response = NULL;
  item = NULL;

  r = build_item_operations_fetch_request(collection_id, server_id, &request);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  r = post_wbxml_document(session, "ItemOperations", NULL, request, &response);
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
  if (mailactivesync_global_status_to_error(status) !=
      MAILACTIVESYNC_NO_ERROR) {
    r = mailactivesync_global_status_to_error(status);
    goto cleanup;
  }

  response_node = node_child(response->root, MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_RESPONSE);
  fetch = node_child(response_node, MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_FETCH);
  status = node_child_int(fetch, MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_STATUS);
  if (mailactivesync_global_status_to_error(status) !=
      MAILACTIVESYNC_NO_ERROR) {
    r = mailactivesync_global_status_to_error(status);
    goto cleanup;
  }

  item = calloc(1, sizeof(* item));
  if (item == NULL) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto cleanup;
  }
  item->server_id = strdup(server_id);
  if (item->server_id == NULL) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto cleanup;
  }

  properties = node_child(fetch, MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_PROPERTIES);
  r = parse_body(node_child(properties, MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_BODY), &item->body);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;

  if ((item->body != NULL) &&
      (item->body->type ==
          MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_MIME) &&
      (item->body->data != NULL)) {
    item->mime = malloc(item->body->data_len + 1);
    if (item->mime == NULL) {
      r = MAILACTIVESYNC_ERROR_MEMORY;
      goto cleanup;
    }
    memcpy(item->mime, item->body->data, item->body->data_len);
    item->mime[item->body->data_len] = '\0';
    item->mime_len = item->body->data_len;
  }

  * result = item;
  item = NULL;

 cleanup:
  mailactivesync_item_free(item);
  mailactivesync_wbxml_document_free(response);
  mailactivesync_wbxml_node_free(request);
  return r;
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
  const char * response_sync_key;
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
    * result = parsed;
    goto cleanup_response;
  }

  response_sync_key = node_child_text(collection, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_SYNC_KEY);
  if (response_sync_key != NULL) {
    parsed->sync_key = strdup(response_sync_key);
    if (parsed->sync_key == NULL) {
      mailactivesync_sync_result_free(parsed);
      r = MAILACTIVESYNC_ERROR_MEMORY;
      goto cleanup_response;
    }
    parsed->sync_key_from_response = 1;
  }
  parsed->status = node_child_int(collection, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_STATUS);
  parsed->more_available = node_child(collection, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_MORE_AVAILABLE) != NULL;

  r = parse_sync_commands(node_child(collection, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_COMMANDS), parsed);
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
