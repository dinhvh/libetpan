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

static int post_wbxml_document(mailactivesync * session, const char * command,
    const char * collection_id, struct mailactivesync_wbxml_node * root,
    struct mailactivesync_wbxml_document ** result)
{
  struct mailactivesync_http_response * response;
  unsigned char * encoded;
  size_t encoded_len;
  int r;

  if ((session == NULL) || (command == NULL) || (root == NULL) ||
      (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  response = NULL;
  encoded = NULL;
  encoded_len = 0;

  r = encode_document(root, &encoded, &encoded_len);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;

  r = mailactivesync_command_post(session, command, collection_id,
      encoded, encoded_len, &response);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;

  if ((response->body == NULL) || (response->body_len == 0)) {
    r = MAILACTIVESYNC_ERROR_PROTOCOL;
    goto cleanup;
  }

  r = mailactivesync_wbxml_decode(response->body, response->body_len, result);

 cleanup:
  free(encoded);
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

static int build_sync_request(struct mailactivesync_sync_request * request,
    struct mailactivesync_wbxml_node ** result)
{
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_node * collections;
  struct mailactivesync_wbxml_node * collection;
  struct mailactivesync_wbxml_node * active_collection;
  struct mailactivesync_wbxml_node * options;
  struct mailactivesync_wbxml_node * body_preference;
  int r;

  * result = NULL;
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
  r = node_add_text(active_collection, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_CLASS, "Email");
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
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

  if (request->body_preference != NULL) {
    options = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
        MAILACTIVESYNC_AIRSYNC_OPTIONS);
    body_preference = mailactivesync_wbxml_node_new(
        MAILACTIVESYNC_CP_AIRSYNCBASE,
        MAILACTIVESYNC_AIRSYNCBASE_BODY_PREFERENCE);
    if ((options == NULL) || (body_preference == NULL)) {
      mailactivesync_wbxml_node_free(options);
      mailactivesync_wbxml_node_free(body_preference);
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

int mailactivesync_command_sync(mailactivesync * session,
    struct mailactivesync_sync_request * request,
    struct mailactivesync_sync_result ** result)
{
  struct mailactivesync_wbxml_document * response_document;
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_node * collections;
  struct mailactivesync_wbxml_node * collection;
  struct mailactivesync_sync_result * parsed;
  const char * response_sync_key;
  int r;

  if ((session == NULL) || (request == NULL) ||
      (request->collection_id == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  response_document = NULL;
  root = NULL;
  r = build_sync_request(request, &root);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  r = post_wbxml_document(session, "Sync", NULL, root, &response_document);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup_root;

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
  if (collection == NULL) {
    r = MAILACTIVESYNC_ERROR_PROTOCOL;
    goto cleanup_response;
  }

  parsed = sync_result_new();
  if (parsed == NULL) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
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
 cleanup_root:
  mailactivesync_wbxml_node_free(root);
  return r;
}
