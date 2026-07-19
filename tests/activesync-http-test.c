/*
 * libEtPan! -- a mail stuff library
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include <libetpan/mailactivesync.h>
#include <libetpan/mailactivesync_codes.h>
#include <libetpan/mailactivesync_wbxml.h>

#include "../src/low-level/activesync/mailactivesync_command.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

struct fake_context {
  struct mailactivesync_http_request * last_request;
  int status_code;
  const char * protocol_versions;
  const char * commands;
  unsigned char * response_body;
  size_t response_body_len;
};

static struct mailactivesync_http_transport * fake_transport_new(
    int status_code, struct fake_context ** context_result);

static int check(int condition, const char * message)
{
  if (!condition) {
    fprintf(stderr, "%s\n", message);
    return 0;
  }

  return 1;
}

static const char * request_header_value(
    struct mailactivesync_http_request * request,
    const char * name)
{
  clistiter * cur;

  for (cur = clist_begin(request->headers); cur != NULL;
      cur = clist_next(cur)) {
    struct mailactivesync_http_header * header;

    header = clist_content(cur);
    if (strcasecmp(header->name, name) == 0)
      return header->value;
  }

  return NULL;
}

static int clone_request(struct mailactivesync_http_request * src,
    struct mailactivesync_http_request ** result)
{
  struct mailactivesync_http_request * copy;
  clistiter * cur;
  int r;

  copy = mailactivesync_http_request_new(src->method, src->url);
  if (copy == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  copy->timeout = src->timeout;
  r = mailactivesync_http_request_set_body(copy, src->body, src->body_len);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;

  for (cur = clist_begin(src->headers); cur != NULL; cur = clist_next(cur)) {
    struct mailactivesync_http_header * header;

    header = clist_content(cur);
    r = mailactivesync_http_request_add_header(copy, header->name,
        header->value);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
  }

  * result = copy;
  return MAILACTIVESYNC_NO_ERROR;

 err:
  mailactivesync_http_request_free(copy);
  return r;
}

static struct mailactivesync_wbxml_node * test_node_child(
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

static const char * test_node_child_text(
    struct mailactivesync_wbxml_node * node,
    uint8_t code_page, uint8_t token)
{
  struct mailactivesync_wbxml_node * child;

  child = test_node_child(node, code_page, token);
  if (child == NULL)
    return NULL;

  return child->text;
}

static int test_node_add_text(struct mailactivesync_wbxml_node * parent,
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

static int test_node_add_opaque(struct mailactivesync_wbxml_node * parent,
    uint8_t code_page, uint8_t token, const unsigned char * data,
    size_t data_len)
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

static int encode_test_document(struct mailactivesync_wbxml_node * root,
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

static int fake_context_set_response_body(struct fake_context * context,
    struct mailactivesync_wbxml_node * root)
{
  unsigned char * body;
  size_t body_len;
  int r;

  body = NULL;
  body_len = 0;
  r = encode_test_document(root, &body, &body_len);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  free(context->response_body);
  context->response_body = body;
  context->response_body_len = body_len;
  return MAILACTIVESYNC_NO_ERROR;
}

static int fake_context_set_raw_response_body(struct fake_context * context,
    const unsigned char * body, size_t body_len)
{
  unsigned char * copied;

  copied = NULL;
  if (body_len > 0) {
    copied = malloc(body_len);
    if (copied == NULL)
      return MAILACTIVESYNC_ERROR_MEMORY;
    memcpy(copied, body, body_len);
  }

  free(context->response_body);
  context->response_body = copied;
  context->response_body_len = body_len;
  return MAILACTIVESYNC_NO_ERROR;
}

static int setup_oauth_session(mailactivesync ** session_result,
    struct fake_context ** context_result)
{
  mailactivesync * session;
  struct mailactivesync_http_transport * transport;
  struct fake_context * context;

  if ((session_result == NULL) || (context_result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * session_result = NULL;
  * context_result = NULL;
  context = NULL;
  transport = NULL;

  session = mailactivesync_new(0, NULL);
  if (session == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  transport = fake_transport_new(200, &context);
  if (transport == NULL)
    goto err;

  if (mailactivesync_set_http_transport(session, transport) !=
      MAILACTIVESYNC_NO_ERROR)
    goto err;
  transport = NULL;

  if (mailactivesync_connect(session, "https://example.com") !=
      MAILACTIVESYNC_NO_ERROR)
    goto err;
  if (mailactivesync_login_oauth2(session, "user@example.com",
      "token-value") != MAILACTIVESYNC_NO_ERROR)
    goto err;

  * session_result = session;
  * context_result = context;
  return MAILACTIVESYNC_NO_ERROR;

 err:
  mailactivesync_http_transport_free(transport);
  mailactivesync_free(session);
  return MAILACTIVESYNC_ERROR_MEMORY;
}

static int fake_perform(struct mailactivesync_http_transport * transport,
    struct mailactivesync_http_request * request,
    struct mailactivesync_http_response ** result)
{
  struct fake_context * context;
  struct mailactivesync_http_response * response;
  int r;

  context = transport->context;
  mailactivesync_http_request_free(context->last_request);
  context->last_request = NULL;

  r = clone_request(request, &context->last_request);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  response = mailactivesync_http_response_new(context->status_code);
  if (response == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  if (context->protocol_versions != NULL) {
    r = mailactivesync_http_response_add_header(response,
        "MS-ASProtocolVersions", context->protocol_versions);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
  }
  if (context->commands != NULL) {
    r = mailactivesync_http_response_add_header(response,
        "MS-ASProtocolCommands", context->commands);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
  }
  if (context->response_body != NULL) {
    response->body = malloc(context->response_body_len);
    if (response->body == NULL)
      goto err;
    memcpy(response->body, context->response_body,
        context->response_body_len);
    response->body_len = context->response_body_len;
  }
  else if (context->response_body_len != 0) {
    response->body = malloc(1);
    if (response->body == NULL)
      goto err;
    response->body_len = 0;
  }

  * result = response;
  return MAILACTIVESYNC_NO_ERROR;

 err:
  mailactivesync_http_response_free(response);
  return r;
}

static void fake_free(struct mailactivesync_http_transport * transport)
{
  struct fake_context * context;

  context = transport->context;
  mailactivesync_http_request_free(context->last_request);
  free(context->response_body);
  free(context);
}

static struct mailactivesync_http_transport * fake_transport_new(
    int status_code, struct fake_context ** context_result)
{
  struct mailactivesync_http_transport * transport;
  struct fake_context * context;

  transport = malloc(sizeof(* transport));
  if (transport == NULL)
    return NULL;

  context = malloc(sizeof(* context));
  if (context == NULL) {
    free(transport);
    return NULL;
  }

  context->last_request = NULL;
  context->status_code = status_code;
  context->protocol_versions = "14.1, 16.0, 16.1";
  context->commands = "Sync, FolderSync, ItemOperations";
  context->response_body = NULL;
  context->response_body_len = 0;
  transport->context = context;
  transport->perform = fake_perform;
  transport->free = fake_free;

  * context_result = context;
  return transport;
}

static int test_response_header_lookup(void)
{
  struct mailactivesync_http_response * response;
  const char * value;
  int r;

  response = mailactivesync_http_response_new(200);
  if (!check(response != NULL, "response allocation failed"))
    return 0;

  r = mailactivesync_http_response_add_header(response,
      "Content-Type", "application/vnd.ms-sync.wbxml");
  if (!check(r == MAILACTIVESYNC_NO_ERROR, "add response header failed"))
    goto err;
  r = mailactivesync_http_response_add_header(response,
      "content-type", "text/plain");
  if (!check(r == MAILACTIVESYNC_NO_ERROR, "add duplicate header failed"))
    goto err;

  value = mailactivesync_http_response_header_value(response,
      "CONTENT-TYPE");
  if (!check((value != NULL) &&
      (strcmp(value, "application/vnd.ms-sync.wbxml") == 0),
      "case-insensitive first header lookup failed"))
    goto err;
  if (!check(mailactivesync_http_response_header_value(response,
      "Missing") == NULL, "missing header lookup failed"))
    goto err;

  mailactivesync_http_response_free(response);
  return 1;

 err:
  mailactivesync_http_response_free(response);
  return 0;
}

static int test_request_body_copy(void)
{
  static const unsigned char first_body[] = { 0x01, 0x02, 0x03 };
  static const unsigned char second_body[] = { 0x04, 0x05 };
  struct mailactivesync_http_request * request;
  int r;

  request = mailactivesync_http_request_new("POST",
      "https://example.com/Microsoft-Server-ActiveSync");
  if (!check(request != NULL, "request allocation failed"))
    return 0;

  r = mailactivesync_http_request_set_body(request, first_body,
      sizeof(first_body));
  if (!check(r == MAILACTIVESYNC_NO_ERROR, "set first body failed"))
    goto err;
  if (!check((request->body_len == sizeof(first_body)) &&
      (memcmp(request->body, first_body, sizeof(first_body)) == 0),
      "first body copy mismatch"))
    goto err;

  r = mailactivesync_http_request_set_body(request, second_body,
      sizeof(second_body));
  if (!check(r == MAILACTIVESYNC_NO_ERROR, "replace body failed"))
    goto err;
  if (!check((request->body_len == sizeof(second_body)) &&
      (memcmp(request->body, second_body, sizeof(second_body)) == 0),
      "second body copy mismatch"))
    goto err;

  r = mailactivesync_http_request_set_body(request, NULL, 0);
  if (!check(r == MAILACTIVESYNC_NO_ERROR, "clear body failed"))
    goto err;
  if (!check((request->body == NULL) && (request->body_len == 0),
      "clear body did not reset request body"))
    goto err;

  mailactivesync_http_request_free(request);
  return 1;

 err:
  mailactivesync_http_request_free(request);
  return 0;
}

static int test_options_success(void)
{
  mailactivesync * session;
  struct fake_context * context;
  struct mailactivesync_http_transport * transport;
  struct mailactivesync_options * options;
  const char * authorization;
  const char * first_command;
  const char * first_version;
  int r;

  options = NULL;
  context = NULL;
  session = mailactivesync_new(0, NULL);
  if (!check(session != NULL, "session allocation failed"))
    return 0;

  transport = fake_transport_new(200, &context);
  if (!check(transport != NULL, "fake transport allocation failed"))
    goto err;
  if (!check(mailactivesync_set_http_transport(session, transport) ==
      MAILACTIVESYNC_NO_ERROR, "set fake transport failed"))
    goto err;
  transport = NULL;

  if (!check(mailactivesync_set_device(session, "device 1", "libetpan") ==
      MAILACTIVESYNC_NO_ERROR, "set device failed"))
    goto err;
  if (!check(mailactivesync_connect(session,
      "https://example.com") == MAILACTIVESYNC_NO_ERROR, "connect failed"))
    goto err;
  if (!check(mailactivesync_login_oauth2(session, "user@example.com",
      "token-value") == MAILACTIVESYNC_NO_ERROR, "login failed"))
    goto err;

  r = mailactivesync_options(session, &options);
  if (!check(r == MAILACTIVESYNC_NO_ERROR, "OPTIONS failed"))
    goto err;
  if (!check(options != NULL, "OPTIONS result missing"))
    goto err;
  if (!check(clist_count(options->protocol_versions) == 3,
      "protocol version count mismatch"))
    goto err;
  if (!check(clist_count(options->commands) == 3, "command count mismatch"))
    goto err;
  first_version = clist_content(clist_begin(options->protocol_versions));
  first_command = clist_content(clist_begin(options->commands));
  if (!check((first_version != NULL) &&
      (strcmp(first_version, "14.1") == 0),
      "first protocol version mismatch"))
    goto err;
  if (!check((first_command != NULL) &&
      (strcmp(first_command, "Sync") == 0), "first command mismatch"))
    goto err;
  if (!check(context->last_request != NULL, "request capture missing"))
    goto err;
  if (!check(strcmp(context->last_request->method, "OPTIONS") == 0,
      "OPTIONS method mismatch"))
    goto err;
  if (!check(strcmp(context->last_request->url,
      "https://example.com/Microsoft-Server-ActiveSync") == 0,
      "normalized endpoint mismatch"))
    goto err;

  authorization = request_header_value(context->last_request,
      "Authorization");
  if (!check((authorization != NULL) &&
      (strcmp(authorization, "Bearer token-value") == 0),
      "bearer authorization header mismatch"))
    goto err;
  if (!check(strcmp(request_header_value(context->last_request,
      "MS-ASProtocolVersion"), "16.1") == 0,
      "protocol version header mismatch"))
    goto err;

  mailactivesync_options_free(options);
  mailactivesync_free(session);
  return 1;

 err:
  mailactivesync_options_free(options);
  mailactivesync_http_transport_free(transport);
  mailactivesync_free(session);
  return 0;
}

static int test_options_trims_empty_list_items(void)
{
  mailactivesync * session;
  struct fake_context * context;
  struct mailactivesync_http_transport * transport;
  struct mailactivesync_options * options;
  int r;

  options = NULL;
  context = NULL;
  session = mailactivesync_new(0, NULL);
  if (!check(session != NULL, "session allocation failed"))
    return 0;

  transport = fake_transport_new(200, &context);
  if (!check(transport != NULL, "fake transport allocation failed"))
    goto err;
  context->protocol_versions = " 12.1, ,16.1, ";
  context->commands = " Sync, , FolderSync ";
  if (!check(mailactivesync_set_http_transport(session, transport) ==
      MAILACTIVESYNC_NO_ERROR, "set fake transport failed"))
    goto err;
  transport = NULL;

  if (!check(mailactivesync_connect(session,
      "https://example.com") == MAILACTIVESYNC_NO_ERROR, "connect failed"))
    goto err;
  if (!check(mailactivesync_login_oauth2(session, "user@example.com",
      "token-value") == MAILACTIVESYNC_NO_ERROR, "login failed"))
    goto err;

  r = mailactivesync_options(session, &options);
  if (!check(r == MAILACTIVESYNC_NO_ERROR, "OPTIONS failed"))
    goto err;
  if (!check(clist_count(options->protocol_versions) == 2,
      "trimmed protocol version count mismatch"))
    goto err;
  if (!check(clist_count(options->commands) == 2,
      "trimmed command count mismatch"))
    goto err;

  mailactivesync_options_free(options);
  mailactivesync_free(session);
  return 1;

 err:
  mailactivesync_options_free(options);
  mailactivesync_http_transport_free(transport);
  mailactivesync_free(session);
  return 0;
}

static int test_options_missing_headers(void)
{
  mailactivesync * session;
  struct fake_context * context;
  struct mailactivesync_http_transport * transport;
  struct mailactivesync_options * options;
  int r;

  options = NULL;
  context = NULL;
  session = mailactivesync_new(0, NULL);
  if (!check(session != NULL, "session allocation failed"))
    return 0;

  transport = fake_transport_new(200, &context);
  if (!check(transport != NULL, "fake transport allocation failed"))
    goto err;
  context->protocol_versions = NULL;
  if (!check(mailactivesync_set_http_transport(session, transport) ==
      MAILACTIVESYNC_NO_ERROR, "set fake transport failed"))
    goto err;
  transport = NULL;

  if (!check(mailactivesync_connect(session,
      "https://example.com") == MAILACTIVESYNC_NO_ERROR, "connect failed"))
    goto err;
  if (!check(mailactivesync_login_oauth2(session, "user@example.com",
      "token-value") == MAILACTIVESYNC_NO_ERROR, "login failed"))
    goto err;

  r = mailactivesync_options(session, &options);
  if (!check(r == MAILACTIVESYNC_ERROR_PROTOCOL,
      "missing OPTIONS headers did not map to protocol error"))
    goto err;
  if (!check(options == NULL, "missing header OPTIONS returned result"))
    goto err;

  mailactivesync_free(session);
  return 1;

 err:
  mailactivesync_options_free(options);
  mailactivesync_http_transport_free(transport);
  mailactivesync_free(session);
  return 0;
}

static int test_options_unauthorized(void)
{
  mailactivesync * session;
  struct fake_context * context;
  struct mailactivesync_http_transport * transport;
  struct mailactivesync_options * options;
  int r;

  options = NULL;
  context = NULL;
  session = mailactivesync_new(0, NULL);
  if (!check(session != NULL, "session allocation failed"))
    return 0;

  transport = fake_transport_new(401, &context);
  if (!check(transport != NULL, "fake transport allocation failed"))
    goto err;
  if (!check(mailactivesync_set_http_transport(session, transport) ==
      MAILACTIVESYNC_NO_ERROR, "set fake transport failed"))
    goto err;
  transport = NULL;

  if (!check(mailactivesync_connect(session,
      "https://example.com/Microsoft-Server-ActiveSync") ==
      MAILACTIVESYNC_NO_ERROR, "connect failed"))
    goto err;
  if (!check(mailactivesync_login_oauth2(session, "user@example.com",
      "token-value") == MAILACTIVESYNC_NO_ERROR, "login failed"))
    goto err;

  r = mailactivesync_options(session, &options);
  if (!check(r == MAILACTIVESYNC_ERROR_UNAUTHORIZED,
      "OPTIONS 401 did not map to unauthorized"))
    goto err;
  if (!check(options == NULL, "unauthorized OPTIONS returned result"))
    goto err;

  mailactivesync_free(session);
  return 1;

 err:
  mailactivesync_options_free(options);
  mailactivesync_http_transport_free(transport);
  mailactivesync_free(session);
  return 0;
}

static int test_command_post_request(void)
{
  static const unsigned char body[] = { 0x03, 0x01, 0x6A, 0x00 };
  mailactivesync * session;
  struct fake_context * context;
  struct mailactivesync_http_transport * transport;
  struct mailactivesync_http_response * response;
  int r;

  response = NULL;
  context = NULL;
  session = mailactivesync_new(0, NULL);
  if (!check(session != NULL, "session allocation failed"))
    return 0;

  transport = fake_transport_new(200, &context);
  if (!check(transport != NULL, "fake transport allocation failed"))
    goto err;
  if (!check(mailactivesync_set_http_transport(session, transport) ==
      MAILACTIVESYNC_NO_ERROR, "set fake transport failed"))
    goto err;
  transport = NULL;

  if (!check(mailactivesync_set_device(session, "device 1", "libetpan") ==
      MAILACTIVESYNC_NO_ERROR, "set device failed"))
    goto err;
  if (!check(mailactivesync_set_policy_key(session, "policy-key-1") ==
      MAILACTIVESYNC_NO_ERROR, "set policy key failed"))
    goto err;
  if (!check(mailactivesync_connect(session,
      "https://example.com/Microsoft-Server-ActiveSync") ==
      MAILACTIVESYNC_NO_ERROR, "connect failed"))
    goto err;
  if (!check(mailactivesync_login_oauth2(session, "user@example.com",
      "token-value") == MAILACTIVESYNC_NO_ERROR, "login failed"))
    goto err;

  r = mailactivesync_command_post(session, "Sync", "5:Inbox & More",
      body, sizeof(body), &response);
  if (!check(r == MAILACTIVESYNC_NO_ERROR, "command POST failed"))
    goto err;
  if (!check(context->last_request != NULL, "POST request capture missing"))
    goto err;
  if (!check(strcmp(context->last_request->method, "POST") == 0,
      "POST method mismatch"))
    goto err;
  if (!check(strcmp(context->last_request->url,
      "https://example.com/Microsoft-Server-ActiveSync"
      "?Cmd=Sync&User=user%40example.com&DeviceId=device%201"
      "&DeviceType=libetpan&CollectionId=5%3AInbox%20%26%20More") == 0,
      "POST URL escaping mismatch"))
    goto err;
  if (!check((context->last_request->body_len == sizeof(body)) &&
      (memcmp(context->last_request->body, body, sizeof(body)) == 0),
      "POST body mismatch"))
    goto err;
  if (!check(strcmp(request_header_value(context->last_request,
      "Authorization"), "Bearer token-value") == 0,
      "POST bearer header mismatch"))
    goto err;
  if (!check(strcmp(request_header_value(context->last_request,
      "X-MS-PolicyKey"), "policy-key-1") == 0,
      "POST policy key header mismatch"))
    goto err;
  if (!check(strcmp(request_header_value(context->last_request,
      "Content-Type"), "application/vnd.ms-sync.wbxml") == 0,
      "POST content type header mismatch"))
    goto err;
  if (!check(strcmp(request_header_value(context->last_request,
      "Accept"), "application/vnd.ms-sync.wbxml") == 0,
      "POST accept header mismatch"))
    goto err;

  mailactivesync_http_response_free(response);
  mailactivesync_free(session);
  return 1;

 err:
  mailactivesync_http_response_free(response);
  mailactivesync_http_transport_free(transport);
  mailactivesync_free(session);
  return 0;
}

static int test_command_post_basic_auth(void)
{
  mailactivesync * session;
  struct fake_context * context;
  struct mailactivesync_http_transport * transport;
  struct mailactivesync_http_response * response;
  int r;

  response = NULL;
  context = NULL;
  session = mailactivesync_new(0, NULL);
  if (!check(session != NULL, "session allocation failed"))
    return 0;

  transport = fake_transport_new(200, &context);
  if (!check(transport != NULL, "fake transport allocation failed"))
    goto err;
  if (!check(mailactivesync_set_http_transport(session, transport) ==
      MAILACTIVESYNC_NO_ERROR, "set fake transport failed"))
    goto err;
  transport = NULL;

  if (!check(mailactivesync_connect(session,
      "https://example.com/") == MAILACTIVESYNC_NO_ERROR, "connect failed"))
    goto err;
  if (!check(mailactivesync_login(session, "user", "pass") ==
      MAILACTIVESYNC_NO_ERROR, "basic login failed"))
    goto err;

  r = mailactivesync_command_post(session, "Ping", NULL, NULL, 0,
      &response);
  if (!check(r == MAILACTIVESYNC_NO_ERROR, "basic auth POST failed"))
    goto err;
  if (!check(strcmp(context->last_request->url,
      "https://example.com/Microsoft-Server-ActiveSync"
      "?Cmd=Ping&User=user&DeviceId=libetpan&DeviceType=libetpan") == 0,
      "host URL normalization or default device query mismatch"))
    goto err;
  if (!check(strcmp(request_header_value(context->last_request,
      "Authorization"), "Basic dXNlcjpwYXNz") == 0,
      "basic authorization header mismatch"))
    goto err;

  mailactivesync_http_response_free(response);
  mailactivesync_free(session);
  return 1;

 err:
  mailactivesync_http_response_free(response);
  mailactivesync_http_transport_free(transport);
  mailactivesync_free(session);
  return 0;
}

static int test_folder_sync_success(void)
{
  mailactivesync * session;
  struct fake_context * context;
  struct mailactivesync_http_transport * transport;
  struct mailactivesync_folder_sync_result * result;
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_node * changes;
  struct mailactivesync_wbxml_node * folder;
  struct mailactivesync_wbxml_document * request_document;
  const char * deleted_id;
  int r;

  result = NULL;
  root = NULL;
  changes = NULL;
  folder = NULL;
  request_document = NULL;
  context = NULL;
  transport = NULL;
  session = mailactivesync_new(0, NULL);
  if (!check(session != NULL, "session allocation failed"))
    return 0;

  transport = fake_transport_new(200, &context);
  if (!check(transport != NULL, "fake transport allocation failed"))
    goto err;
  if (!check(mailactivesync_set_http_transport(session, transport) ==
      MAILACTIVESYNC_NO_ERROR, "set fake transport failed"))
    goto err;
  transport = NULL;

  root = mailactivesync_wbxml_node_new(
      MAILACTIVESYNC_CP_FOLDERHIERARCHY,
      MAILACTIVESYNC_FOLDER_FOLDER_SYNC);
  changes = mailactivesync_wbxml_node_new(
      MAILACTIVESYNC_CP_FOLDERHIERARCHY,
      MAILACTIVESYNC_FOLDER_CHANGES);
  if (!check((root != NULL) && (changes != NULL),
      "FolderSync response allocation failed"))
    goto err;
  if (!check(test_node_add_text(root, MAILACTIVESYNC_CP_FOLDERHIERARCHY,
      MAILACTIVESYNC_FOLDER_SYNC_KEY, "123") == MAILACTIVESYNC_NO_ERROR,
      "FolderSync response SyncKey add failed"))
    goto err;
  if (!check(test_node_add_text(root, MAILACTIVESYNC_CP_FOLDERHIERARCHY,
      MAILACTIVESYNC_FOLDER_STATUS, "1") == MAILACTIVESYNC_NO_ERROR,
      "FolderSync response Status add failed"))
    goto err;

  folder = mailactivesync_wbxml_node_new(
      MAILACTIVESYNC_CP_FOLDERHIERARCHY, MAILACTIVESYNC_FOLDER_ADD);
  if (!check(folder != NULL, "FolderSync add folder allocation failed"))
    goto err;
  if (!check(test_node_add_text(folder, MAILACTIVESYNC_CP_FOLDERHIERARCHY,
      MAILACTIVESYNC_FOLDER_SERVER_ID, "5") == MAILACTIVESYNC_NO_ERROR,
      "FolderSync add ServerId failed"))
    goto err;
  if (!check(test_node_add_text(folder, MAILACTIVESYNC_CP_FOLDERHIERARCHY,
      MAILACTIVESYNC_FOLDER_PARENT_ID, "0") == MAILACTIVESYNC_NO_ERROR,
      "FolderSync add ParentId failed"))
    goto err;
  if (!check(test_node_add_text(folder, MAILACTIVESYNC_CP_FOLDERHIERARCHY,
      MAILACTIVESYNC_FOLDER_DISPLAY_NAME, "Inbox") ==
      MAILACTIVESYNC_NO_ERROR, "FolderSync add DisplayName failed"))
    goto err;
  if (!check(test_node_add_text(folder, MAILACTIVESYNC_CP_FOLDERHIERARCHY,
      MAILACTIVESYNC_FOLDER_TYPE, "2") == MAILACTIVESYNC_NO_ERROR,
      "FolderSync add Type failed"))
    goto err;
  if (!check(mailactivesync_wbxml_node_add_child(changes, folder) ==
      MAILACTIVESYNC_NO_ERROR, "FolderSync add change append failed"))
    goto err;
  folder = NULL;

  folder = mailactivesync_wbxml_node_new(
      MAILACTIVESYNC_CP_FOLDERHIERARCHY, MAILACTIVESYNC_FOLDER_DELETE);
  if (!check(folder != NULL, "FolderSync delete allocation failed"))
    goto err;
  if (!check(test_node_add_text(folder, MAILACTIVESYNC_CP_FOLDERHIERARCHY,
      MAILACTIVESYNC_FOLDER_SERVER_ID, "7") == MAILACTIVESYNC_NO_ERROR,
      "FolderSync delete ServerId failed"))
    goto err;
  if (!check(mailactivesync_wbxml_node_add_child(changes, folder) ==
      MAILACTIVESYNC_NO_ERROR, "FolderSync delete append failed"))
    goto err;
  folder = NULL;

  if (!check(mailactivesync_wbxml_node_add_child(root, changes) ==
      MAILACTIVESYNC_NO_ERROR, "FolderSync changes append failed"))
    goto err;
  changes = NULL;
  if (!check(fake_context_set_response_body(context, root) ==
      MAILACTIVESYNC_NO_ERROR, "FolderSync response encode failed"))
    goto err;
  mailactivesync_wbxml_node_free(root);
  root = NULL;

  if (!check(mailactivesync_connect(session,
      "https://example.com") == MAILACTIVESYNC_NO_ERROR, "connect failed"))
    goto err;
  if (!check(mailactivesync_login_oauth2(session, "user@example.com",
      "token-value") == MAILACTIVESYNC_NO_ERROR, "login failed"))
    goto err;

  r = mailactivesync_folder_sync(session, "0", &result);
  if (!check(r == MAILACTIVESYNC_NO_ERROR, "FolderSync failed"))
    goto err;
  if (!check(strcmp(context->last_request->method, "POST") == 0,
      "FolderSync method mismatch"))
    goto err;
  if (!check(strstr(context->last_request->url, "Cmd=FolderSync") != NULL,
      "FolderSync command URL mismatch"))
    goto err;
  if (!check(mailactivesync_wbxml_decode(context->last_request->body,
      context->last_request->body_len, &request_document) ==
      MAILACTIVESYNC_NO_ERROR, "FolderSync request decode failed"))
    goto err;
  if (!check(strcmp(test_node_child_text(request_document->root,
      MAILACTIVESYNC_CP_FOLDERHIERARCHY,
      MAILACTIVESYNC_FOLDER_SYNC_KEY), "0") == 0,
      "FolderSync request SyncKey mismatch"))
    goto err;
  if (!check((result->sync_key != NULL) &&
      (strcmp(result->sync_key, "123") == 0),
      "FolderSync parsed SyncKey mismatch"))
    goto err;
  if (!check(result->status == 1, "FolderSync status mismatch"))
    goto err;
  if (!check(clist_count(result->added) == 1,
      "FolderSync add count mismatch"))
    goto err;
  folder = clist_content(clist_begin(result->added));
  if (!check((folder != NULL) &&
      (strcmp(((struct mailactivesync_folder *) folder)->server_id, "5") == 0) &&
      (strcmp(((struct mailactivesync_folder *) folder)->display_name,
          "Inbox") == 0) &&
      (((struct mailactivesync_folder *) folder)->type == 2),
      "FolderSync parsed add folder mismatch"))
    goto err;
  if (!check(clist_count(result->deleted) == 1,
      "FolderSync delete count mismatch"))
    goto err;
  deleted_id = clist_content(clist_begin(result->deleted));
  if (!check((deleted_id != NULL) && (strcmp(deleted_id, "7") == 0),
      "FolderSync parsed delete mismatch"))
    goto err;

  mailactivesync_wbxml_document_free(request_document);
  mailactivesync_folder_sync_result_free(result);
  mailactivesync_free(session);
  return 1;

 err:
  mailactivesync_wbxml_document_free(request_document);
  mailactivesync_wbxml_node_free(folder);
  mailactivesync_wbxml_node_free(changes);
  mailactivesync_wbxml_node_free(root);
  mailactivesync_folder_sync_result_free(result);
  mailactivesync_http_transport_free(transport);
  mailactivesync_free(session);
  return 0;
}

static int test_folder_sync_response_errors(void)
{
  static const unsigned char malformed[] = {
    0x03, 0x01, 0x6A, 0x00, 0x00, 0x07, 0x56
  };
  mailactivesync * session;
  struct fake_context * context;
  struct mailactivesync_folder_sync_result * result;
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_node * changes;
  struct mailactivesync_wbxml_node * folder;
  int r;

  session = NULL;
  context = NULL;
  result = NULL;
  root = NULL;
  changes = NULL;
  folder = NULL;

  if (!check(setup_oauth_session(&session, &context) ==
      MAILACTIVESYNC_NO_ERROR, "setup OAuth session failed"))
    return 0;

  r = mailactivesync_folder_sync(session, "0", &result);
  if (!check((r == MAILACTIVESYNC_ERROR_PROTOCOL) && (result == NULL),
      "FolderSync empty body did not fail as protocol error"))
    goto err;

  if (!check(fake_context_set_raw_response_body(context, malformed,
      sizeof(malformed)) == MAILACTIVESYNC_NO_ERROR,
      "set malformed FolderSync body failed"))
    goto err;
  r = mailactivesync_folder_sync(session, "0", &result);
  if (!check((r == MAILACTIVESYNC_ERROR_PARSE) && (result == NULL),
      "FolderSync malformed body did not fail as parse error"))
    goto err;

  root = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_SYNC);
  if (!check(root != NULL, "wrong-root FolderSync response allocation failed"))
    goto err;
  if (!check(fake_context_set_response_body(context, root) ==
      MAILACTIVESYNC_NO_ERROR, "wrong-root FolderSync response encode failed"))
    goto err;
  mailactivesync_wbxml_node_free(root);
  root = NULL;
  r = mailactivesync_folder_sync(session, "0", &result);
  if (!check((r == MAILACTIVESYNC_ERROR_PROTOCOL) && (result == NULL),
      "FolderSync wrong root did not fail as protocol error"))
    goto err;

  root = mailactivesync_wbxml_node_new(
      MAILACTIVESYNC_CP_FOLDERHIERARCHY,
      MAILACTIVESYNC_FOLDER_FOLDER_SYNC);
  changes = mailactivesync_wbxml_node_new(
      MAILACTIVESYNC_CP_FOLDERHIERARCHY,
      MAILACTIVESYNC_FOLDER_CHANGES);
  folder = mailactivesync_wbxml_node_new(
      MAILACTIVESYNC_CP_FOLDERHIERARCHY, MAILACTIVESYNC_FOLDER_ADD);
  if (!check((root != NULL) && (changes != NULL) && (folder != NULL),
      "FolderSync missing ServerId fixture allocation failed"))
    goto err;
  if (!check(mailactivesync_wbxml_node_add_child(changes, folder) ==
      MAILACTIVESYNC_NO_ERROR, "FolderSync missing ServerId append failed"))
    goto err;
  folder = NULL;
  if (!check(mailactivesync_wbxml_node_add_child(root, changes) ==
      MAILACTIVESYNC_NO_ERROR, "FolderSync missing ServerId changes failed"))
    goto err;
  changes = NULL;
  if (!check(fake_context_set_response_body(context, root) ==
      MAILACTIVESYNC_NO_ERROR,
      "FolderSync missing ServerId response encode failed"))
    goto err;
  mailactivesync_wbxml_node_free(root);
  root = NULL;
  r = mailactivesync_folder_sync(session, "0", &result);
  if (!check((r == MAILACTIVESYNC_ERROR_PROTOCOL) && (result == NULL),
      "FolderSync add without ServerId did not fail as protocol error"))
    goto err;

  mailactivesync_free(session);
  return 1;

 err:
  mailactivesync_wbxml_node_free(folder);
  mailactivesync_wbxml_node_free(changes);
  mailactivesync_wbxml_node_free(root);
  mailactivesync_folder_sync_result_free(result);
  mailactivesync_free(session);
  return 0;
}

static int test_sync_success(void)
{
  static const unsigned char mime[] =
      "Subject: Hello\r\nFrom: sender@example.com\r\n\r\nBody\r\n";
  mailactivesync * session;
  struct fake_context * context;
  struct mailactivesync_http_transport * transport;
  struct mailactivesync_sync_request * request;
  struct mailactivesync_sync_result * result;
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_node * collections;
  struct mailactivesync_wbxml_node * collection;
  struct mailactivesync_wbxml_node * commands;
  struct mailactivesync_wbxml_node * command;
  struct mailactivesync_wbxml_node * app_data;
  struct mailactivesync_wbxml_node * body;
  struct mailactivesync_wbxml_document * request_document;
  struct mailactivesync_wbxml_node * request_collections;
  struct mailactivesync_wbxml_node * request_collection;
  struct mailactivesync_wbxml_node * request_options;
  struct mailactivesync_wbxml_node * request_body_pref;
  struct mailactivesync_message * message;
  const char * deleted_id;
  int r;

  request = NULL;
  result = NULL;
  root = NULL;
  collections = NULL;
  collection = NULL;
  commands = NULL;
  command = NULL;
  app_data = NULL;
  body = NULL;
  request_document = NULL;
  context = NULL;
  transport = NULL;
  session = mailactivesync_new(0, NULL);
  if (!check(session != NULL, "session allocation failed"))
    return 0;

  transport = fake_transport_new(200, &context);
  if (!check(transport != NULL, "fake transport allocation failed"))
    goto err;
  if (!check(mailactivesync_set_http_transport(session, transport) ==
      MAILACTIVESYNC_NO_ERROR, "set fake transport failed"))
    goto err;
  transport = NULL;

  root = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_SYNC);
  collections = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_COLLECTIONS);
  collection = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_COLLECTION);
  commands = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_COMMANDS);
  if (!check((root != NULL) && (collections != NULL) &&
      (collection != NULL) && (commands != NULL),
      "Sync response allocation failed"))
    goto err;
  if (!check(test_node_add_text(collection, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_SYNC_KEY, "222") == MAILACTIVESYNC_NO_ERROR,
      "Sync response SyncKey add failed"))
    goto err;
  if (!check(test_node_add_text(collection, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_STATUS, "1") == MAILACTIVESYNC_NO_ERROR,
      "Sync response Status add failed"))
    goto err;
  if (!check(mailactivesync_wbxml_node_add_child(collection,
      mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
          MAILACTIVESYNC_AIRSYNC_MORE_AVAILABLE)) ==
      MAILACTIVESYNC_NO_ERROR, "Sync MoreAvailable add failed"))
    goto err;

  command = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_ADD);
  app_data = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_APPLICATION_DATA);
  body = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_BODY);
  if (!check((command != NULL) && (app_data != NULL) && (body != NULL),
      "Sync add response allocation failed"))
    goto err;
  if (!check(test_node_add_text(command, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_SERVER_ID, "msg-1") ==
      MAILACTIVESYNC_NO_ERROR, "Sync add ServerId failed"))
    goto err;
  if (!check(test_node_add_text(app_data, MAILACTIVESYNC_CP_EMAIL,
      MAILACTIVESYNC_EMAIL_SUBJECT, "Hello") == MAILACTIVESYNC_NO_ERROR,
      "Sync subject add failed"))
    goto err;
  if (!check(test_node_add_text(app_data, MAILACTIVESYNC_CP_EMAIL,
      MAILACTIVESYNC_EMAIL_FROM, "sender@example.com") ==
      MAILACTIVESYNC_NO_ERROR, "Sync from add failed"))
    goto err;
  if (!check(test_node_add_text(app_data, MAILACTIVESYNC_CP_EMAIL,
      MAILACTIVESYNC_EMAIL_TO, "user@example.com") ==
      MAILACTIVESYNC_NO_ERROR, "Sync to add failed"))
    goto err;
  if (!check(test_node_add_text(app_data, MAILACTIVESYNC_CP_EMAIL,
      MAILACTIVESYNC_EMAIL_READ, "1") == MAILACTIVESYNC_NO_ERROR,
      "Sync read add failed"))
    goto err;
  if (!check(test_node_add_text(body, MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_TYPE, "4") == MAILACTIVESYNC_NO_ERROR,
      "Sync body type add failed"))
    goto err;
  if (!check(test_node_add_text(body, MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_ESTIMATED_DATA_SIZE, "48") ==
      MAILACTIVESYNC_NO_ERROR, "Sync body estimated size add failed"))
    goto err;
  if (!check(test_node_add_opaque(body, MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_DATA, mime, sizeof(mime) - 1) ==
      MAILACTIVESYNC_NO_ERROR, "Sync body data add failed"))
    goto err;
  if (!check(mailactivesync_wbxml_node_add_child(app_data, body) ==
      MAILACTIVESYNC_NO_ERROR, "Sync body append failed"))
    goto err;
  body = NULL;
  if (!check(mailactivesync_wbxml_node_add_child(command, app_data) ==
      MAILACTIVESYNC_NO_ERROR, "Sync app data append failed"))
    goto err;
  app_data = NULL;
  if (!check(mailactivesync_wbxml_node_add_child(commands, command) ==
      MAILACTIVESYNC_NO_ERROR, "Sync add command append failed"))
    goto err;
  command = NULL;

  command = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_DELETE);
  if (!check(command != NULL, "Sync delete allocation failed"))
    goto err;
  if (!check(test_node_add_text(command, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_SERVER_ID, "msg-2") ==
      MAILACTIVESYNC_NO_ERROR, "Sync delete ServerId failed"))
    goto err;
  if (!check(mailactivesync_wbxml_node_add_child(commands, command) ==
      MAILACTIVESYNC_NO_ERROR, "Sync delete command append failed"))
    goto err;
  command = NULL;

  if (!check(mailactivesync_wbxml_node_add_child(collection, commands) ==
      MAILACTIVESYNC_NO_ERROR, "Sync commands append failed"))
    goto err;
  commands = NULL;
  if (!check(mailactivesync_wbxml_node_add_child(collections, collection) ==
      MAILACTIVESYNC_NO_ERROR, "Sync collection append failed"))
    goto err;
  collection = NULL;
  if (!check(mailactivesync_wbxml_node_add_child(root, collections) ==
      MAILACTIVESYNC_NO_ERROR, "Sync collections append failed"))
    goto err;
  collections = NULL;
  if (!check(fake_context_set_response_body(context, root) ==
      MAILACTIVESYNC_NO_ERROR, "Sync response encode failed"))
    goto err;
  mailactivesync_wbxml_node_free(root);
  root = NULL;

  if (!check(mailactivesync_connect(session,
      "https://example.com") == MAILACTIVESYNC_NO_ERROR, "connect failed"))
    goto err;
  if (!check(mailactivesync_login_oauth2(session, "user@example.com",
      "token-value") == MAILACTIVESYNC_NO_ERROR, "login failed"))
    goto err;

  request = mailactivesync_sync_request_new("5", "111");
  if (!check(request != NULL, "Sync request allocation failed"))
    goto err;
  if (!check(mailactivesync_sync_request_set_window_size(request, 25) ==
      MAILACTIVESYNC_NO_ERROR, "Sync request WindowSize set failed"))
    goto err;
  if (!check(mailactivesync_sync_request_set_mime_body_preference(request,
      204800) == MAILACTIVESYNC_NO_ERROR,
      "Sync request BodyPreference set failed"))
    goto err;

  r = mailactivesync_sync(session, request, &result);
  if (!check(r == MAILACTIVESYNC_NO_ERROR, "Sync failed"))
    goto err;
  if (!check(strstr(context->last_request->url, "Cmd=Sync") != NULL,
      "Sync command URL mismatch"))
    goto err;
  if (!check(mailactivesync_wbxml_decode(context->last_request->body,
      context->last_request->body_len, &request_document) ==
      MAILACTIVESYNC_NO_ERROR, "Sync request decode failed"))
    goto err;
  request_collections = test_node_child(request_document->root,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_COLLECTIONS);
  request_collection = test_node_child(request_collections,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_COLLECTION);
  request_options = test_node_child(request_collection,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_OPTIONS);
  request_body_pref = test_node_child(request_options,
      MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_BODY_PREFERENCE);
  if (!check(strcmp(test_node_child_text(request_collection,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_COLLECTION_ID),
      "5") == 0, "Sync request CollectionId mismatch"))
    goto err;
  if (!check(strcmp(test_node_child_text(request_collection,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_SYNC_KEY),
      "111") == 0, "Sync request SyncKey mismatch"))
    goto err;
  if (!check(strcmp(test_node_child_text(request_collection,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_WINDOW_SIZE),
      "25") == 0, "Sync request WindowSize mismatch"))
    goto err;
  if (!check(strcmp(test_node_child_text(request_options,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_MIME_SUPPORT),
      "2") == 0, "Sync request MIMESupport mismatch"))
    goto err;
  if (!check(strcmp(test_node_child_text(request_body_pref,
      MAILACTIVESYNC_CP_AIRSYNCBASE, MAILACTIVESYNC_AIRSYNCBASE_TYPE),
      "4") == 0, "Sync request body type mismatch"))
    goto err;

  if (!check((result->sync_key != NULL) &&
      (strcmp(result->sync_key, "222") == 0),
      "Sync parsed SyncKey mismatch"))
    goto err;
  if (!check((result->status == 1) && result->more_available,
      "Sync parsed status/more mismatch"))
    goto err;
  if (!check((clist_count(result->added) == 1) &&
      (clist_count(result->deleted) == 1), "Sync result counts mismatch"))
    goto err;
  message = clist_content(clist_begin(result->added));
  if (!check((message != NULL) &&
      (strcmp(message->server_id, "msg-1") == 0) &&
      (strcmp(message->subject, "Hello") == 0) &&
      (strcmp(message->from, "sender@example.com") == 0) &&
      (message->read == 1), "Sync parsed message mismatch"))
    goto err;
  if (!check((message->body != NULL) &&
      (message->body->type == MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_MIME) &&
      (message->body->data_len == sizeof(mime) - 1) &&
      (message->mime_len == sizeof(mime) - 1) &&
      (memcmp(message->mime, mime, sizeof(mime) - 1) == 0),
      "Sync parsed MIME body mismatch"))
    goto err;
  deleted_id = clist_content(clist_begin(result->deleted));
  if (!check((deleted_id != NULL) && (strcmp(deleted_id, "msg-2") == 0),
      "Sync parsed delete mismatch"))
    goto err;

  mailactivesync_wbxml_document_free(request_document);
  mailactivesync_sync_result_free(result);
  mailactivesync_sync_request_free(request);
  mailactivesync_free(session);
  return 1;

 err:
  mailactivesync_wbxml_document_free(request_document);
  mailactivesync_wbxml_node_free(body);
  mailactivesync_wbxml_node_free(app_data);
  mailactivesync_wbxml_node_free(command);
  mailactivesync_wbxml_node_free(commands);
  mailactivesync_wbxml_node_free(collection);
  mailactivesync_wbxml_node_free(collections);
  mailactivesync_wbxml_node_free(root);
  mailactivesync_sync_result_free(result);
  mailactivesync_sync_request_free(request);
  mailactivesync_http_transport_free(transport);
  mailactivesync_free(session);
  return 0;
}

static int append_sync_collection_response(
    struct mailactivesync_wbxml_node * root,
    struct mailactivesync_wbxml_node * collection)
{
  struct mailactivesync_wbxml_node * collections;
  int r;

  collections = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_COLLECTIONS);
  if (collections == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  r = mailactivesync_wbxml_node_add_child(collections, collection);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    mailactivesync_wbxml_node_free(collections);
    return r;
  }

  r = mailactivesync_wbxml_node_add_child(root, collections);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    mailactivesync_wbxml_node_free(collections);
    return r;
  }

  return MAILACTIVESYNC_NO_ERROR;
}

static int test_sync_empty_response_and_request_defaults(void)
{
  mailactivesync * session;
  struct fake_context * context;
  struct mailactivesync_sync_request * request;
  struct mailactivesync_sync_result * result;
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_node * collection;
  struct mailactivesync_wbxml_document * request_document;
  struct mailactivesync_wbxml_node * request_collections;
  struct mailactivesync_wbxml_node * request_collection;
  int r;

  session = NULL;
  context = NULL;
  request = NULL;
  result = NULL;
  root = NULL;
  collection = NULL;
  request_document = NULL;

  if (!check(setup_oauth_session(&session, &context) ==
      MAILACTIVESYNC_NO_ERROR, "setup OAuth session failed"))
    return 0;

  root = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_SYNC);
  collection = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_COLLECTION);
  if (!check((root != NULL) && (collection != NULL),
      "empty Sync response allocation failed"))
    goto err;
  if (!check(test_node_add_text(collection, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_SYNC_KEY, "333") == MAILACTIVESYNC_NO_ERROR,
      "empty Sync response SyncKey add failed"))
    goto err;
  if (!check(test_node_add_text(collection, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_STATUS, "5") == MAILACTIVESYNC_NO_ERROR,
      "empty Sync response Status add failed"))
    goto err;
  if (!check(append_sync_collection_response(root, collection) ==
      MAILACTIVESYNC_NO_ERROR, "empty Sync response collection append failed"))
    goto err;
  collection = NULL;
  if (!check(fake_context_set_response_body(context, root) ==
      MAILACTIVESYNC_NO_ERROR, "empty Sync response encode failed"))
    goto err;
  mailactivesync_wbxml_node_free(root);
  root = NULL;

  request = mailactivesync_sync_request_new("5", NULL);
  if (!check(request != NULL, "default Sync request allocation failed"))
    goto err;
  if (!check(mailactivesync_sync_request_set_get_changes(request, 0) ==
      MAILACTIVESYNC_NO_ERROR, "set GetChanges=0 failed"))
    goto err;

  r = mailactivesync_sync(session, request, &result);
  if (!check(r == MAILACTIVESYNC_NO_ERROR, "empty Sync failed"))
    goto err;
  if (!check(mailactivesync_wbxml_decode(context->last_request->body,
      context->last_request->body_len, &request_document) ==
      MAILACTIVESYNC_NO_ERROR, "empty Sync request decode failed"))
    goto err;
  request_collections = test_node_child(request_document->root,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_COLLECTIONS);
  request_collection = test_node_child(request_collections,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_COLLECTION);
  if (!check(strcmp(test_node_child_text(request_collection,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_SYNC_KEY), "0") == 0,
      "default SyncKey was not 0"))
    goto err;
  if (!check(strcmp(test_node_child_text(request_collection,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_GET_CHANGES), "0") == 0,
      "GetChanges=0 request mismatch"))
    goto err;
  if (!check(test_node_child(request_collection, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_WINDOW_SIZE) == NULL,
      "default Sync request unexpectedly included WindowSize"))
    goto err;
  if (!check(test_node_child(request_collection, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_OPTIONS) == NULL,
      "default Sync request unexpectedly included Options"))
    goto err;
  if (!check((result->sync_key != NULL) &&
      (strcmp(result->sync_key, "333") == 0) &&
      (result->status == 5) && !result->more_available &&
      (clist_count(result->added) == 0) &&
      (clist_count(result->changed) == 0) &&
      (clist_count(result->deleted) == 0),
      "empty Sync result mismatch"))
    goto err;

  mailactivesync_wbxml_document_free(request_document);
  mailactivesync_sync_result_free(result);
  mailactivesync_sync_request_free(request);
  mailactivesync_free(session);
  return 1;

 err:
  mailactivesync_wbxml_document_free(request_document);
  mailactivesync_wbxml_node_free(collection);
  mailactivesync_wbxml_node_free(root);
  mailactivesync_sync_result_free(result);
  mailactivesync_sync_request_free(request);
  mailactivesync_free(session);
  return 0;
}

static int test_sync_change_softdelete_and_html_body(void)
{
  static const char html_body[] = "<p>Hello</p>";
  mailactivesync * session;
  struct fake_context * context;
  struct mailactivesync_sync_request * request;
  struct mailactivesync_sync_result * result;
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_node * collection;
  struct mailactivesync_wbxml_node * commands;
  struct mailactivesync_wbxml_node * command;
  struct mailactivesync_wbxml_node * app_data;
  struct mailactivesync_wbxml_node * body;
  struct mailactivesync_message * message;
  const char * deleted_id;
  int r;

  session = NULL;
  context = NULL;
  request = NULL;
  result = NULL;
  root = NULL;
  collection = NULL;
  commands = NULL;
  command = NULL;
  app_data = NULL;
  body = NULL;

  if (!check(setup_oauth_session(&session, &context) ==
      MAILACTIVESYNC_NO_ERROR, "setup OAuth session failed"))
    return 0;

  root = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_SYNC);
  collection = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_COLLECTION);
  commands = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_COMMANDS);
  command = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_CHANGE);
  app_data = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_APPLICATION_DATA);
  body = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_BODY);
  if (!check((root != NULL) && (collection != NULL) &&
      (commands != NULL) && (command != NULL) &&
      (app_data != NULL) && (body != NULL),
      "Sync change response allocation failed"))
    goto err;

  if (!check(test_node_add_text(collection, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_SYNC_KEY, "444") == MAILACTIVESYNC_NO_ERROR,
      "Sync change response SyncKey add failed"))
    goto err;
  if (!check(test_node_add_text(collection, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_STATUS, "1") == MAILACTIVESYNC_NO_ERROR,
      "Sync change response Status add failed"))
    goto err;
  if (!check(test_node_add_text(command, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_SERVER_ID, "msg-change") ==
      MAILACTIVESYNC_NO_ERROR, "Sync change ServerId add failed"))
    goto err;
  if (!check(test_node_add_text(app_data, MAILACTIVESYNC_CP_EMAIL,
      MAILACTIVESYNC_EMAIL_SUBJECT, "Changed") == MAILACTIVESYNC_NO_ERROR,
      "Sync change subject add failed"))
    goto err;
  if (!check(test_node_add_text(body, MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_TYPE, "2") == MAILACTIVESYNC_NO_ERROR,
      "Sync HTML body type add failed"))
    goto err;
  if (!check(test_node_add_text(body, MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_DATA, html_body) ==
      MAILACTIVESYNC_NO_ERROR, "Sync HTML body data add failed"))
    goto err;
  if (!check(mailactivesync_wbxml_node_add_child(app_data, body) ==
      MAILACTIVESYNC_NO_ERROR, "Sync HTML body append failed"))
    goto err;
  body = NULL;
  if (!check(mailactivesync_wbxml_node_add_child(command, app_data) ==
      MAILACTIVESYNC_NO_ERROR, "Sync change app data append failed"))
    goto err;
  app_data = NULL;
  if (!check(mailactivesync_wbxml_node_add_child(commands, command) ==
      MAILACTIVESYNC_NO_ERROR, "Sync change command append failed"))
    goto err;
  command = NULL;

  command = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_SOFT_DELETE);
  if (!check(command != NULL, "Sync soft delete allocation failed"))
    goto err;
  if (!check(test_node_add_text(command, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_SERVER_ID, "msg-soft-delete") ==
      MAILACTIVESYNC_NO_ERROR, "Sync soft delete ServerId add failed"))
    goto err;
  if (!check(mailactivesync_wbxml_node_add_child(commands, command) ==
      MAILACTIVESYNC_NO_ERROR, "Sync soft delete append failed"))
    goto err;
  command = NULL;
  if (!check(mailactivesync_wbxml_node_add_child(collection, commands) ==
      MAILACTIVESYNC_NO_ERROR, "Sync change commands append failed"))
    goto err;
  commands = NULL;
  if (!check(append_sync_collection_response(root, collection) ==
      MAILACTIVESYNC_NO_ERROR, "Sync change collection append failed"))
    goto err;
  collection = NULL;
  if (!check(fake_context_set_response_body(context, root) ==
      MAILACTIVESYNC_NO_ERROR, "Sync change response encode failed"))
    goto err;
  mailactivesync_wbxml_node_free(root);
  root = NULL;

  request = mailactivesync_sync_request_new("5", "333");
  if (!check(request != NULL, "Sync change request allocation failed"))
    goto err;
  r = mailactivesync_sync(session, request, &result);
  if (!check(r == MAILACTIVESYNC_NO_ERROR, "Sync change failed"))
    goto err;
  if (!check((clist_count(result->added) == 0) &&
      (clist_count(result->changed) == 1) &&
      (clist_count(result->deleted) == 1),
      "Sync change result counts mismatch"))
    goto err;
  message = clist_content(clist_begin(result->changed));
  if (!check((message != NULL) &&
      (strcmp(message->server_id, "msg-change") == 0) &&
      (strcmp(message->subject, "Changed") == 0) &&
      (message->body != NULL) &&
      (message->body->type == MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_HTML) &&
      (strcmp(message->body->data, html_body) == 0) &&
      (message->mime == NULL) && (message->mime_len == 0),
      "Sync change HTML body parse mismatch"))
    goto err;
  deleted_id = clist_content(clist_begin(result->deleted));
  if (!check((deleted_id != NULL) &&
      (strcmp(deleted_id, "msg-soft-delete") == 0),
      "Sync soft delete parse mismatch"))
    goto err;

  mailactivesync_sync_result_free(result);
  mailactivesync_sync_request_free(request);
  mailactivesync_free(session);
  return 1;

 err:
  mailactivesync_wbxml_node_free(body);
  mailactivesync_wbxml_node_free(app_data);
  mailactivesync_wbxml_node_free(command);
  mailactivesync_wbxml_node_free(commands);
  mailactivesync_wbxml_node_free(collection);
  mailactivesync_wbxml_node_free(root);
  mailactivesync_sync_result_free(result);
  mailactivesync_sync_request_free(request);
  mailactivesync_free(session);
  return 0;
}

static int test_sync_response_errors(void)
{
  static const unsigned char malformed[] = {
    0x03, 0x01, 0x6A, 0x00, 0x00, 0x00, 0x45
  };
  mailactivesync * session;
  struct fake_context * context;
  struct mailactivesync_sync_request * request;
  struct mailactivesync_sync_result * result;
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_node * collection;
  struct mailactivesync_wbxml_node * commands;
  struct mailactivesync_wbxml_node * command;
  int r;

  session = NULL;
  context = NULL;
  request = NULL;
  result = NULL;
  root = NULL;
  collection = NULL;
  commands = NULL;
  command = NULL;

  if (!check(setup_oauth_session(&session, &context) ==
      MAILACTIVESYNC_NO_ERROR, "setup OAuth session failed"))
    return 0;
  request = mailactivesync_sync_request_new("5", "1");
  if (!check(request != NULL, "Sync error request allocation failed"))
    goto err;

  r = mailactivesync_sync(session, request, &result);
  if (!check((r == MAILACTIVESYNC_ERROR_PROTOCOL) && (result == NULL),
      "Sync empty body did not fail as protocol error"))
    goto err;

  if (!check(fake_context_set_raw_response_body(context, malformed,
      sizeof(malformed)) == MAILACTIVESYNC_NO_ERROR,
      "set malformed Sync body failed"))
    goto err;
  r = mailactivesync_sync(session, request, &result);
  if (!check((r == MAILACTIVESYNC_ERROR_PARSE) && (result == NULL),
      "Sync malformed body did not fail as parse error"))
    goto err;

  root = mailactivesync_wbxml_node_new(
      MAILACTIVESYNC_CP_FOLDERHIERARCHY,
      MAILACTIVESYNC_FOLDER_FOLDER_SYNC);
  if (!check(root != NULL, "wrong-root Sync response allocation failed"))
    goto err;
  if (!check(fake_context_set_response_body(context, root) ==
      MAILACTIVESYNC_NO_ERROR, "wrong-root Sync response encode failed"))
    goto err;
  mailactivesync_wbxml_node_free(root);
  root = NULL;
  r = mailactivesync_sync(session, request, &result);
  if (!check((r == MAILACTIVESYNC_ERROR_PROTOCOL) && (result == NULL),
      "Sync wrong root did not fail as protocol error"))
    goto err;

  root = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_SYNC);
  if (!check(root != NULL, "Sync no collection response allocation failed"))
    goto err;
  if (!check(fake_context_set_response_body(context, root) ==
      MAILACTIVESYNC_NO_ERROR, "Sync no collection response encode failed"))
    goto err;
  mailactivesync_wbxml_node_free(root);
  root = NULL;
  r = mailactivesync_sync(session, request, &result);
  if (!check((r == MAILACTIVESYNC_ERROR_PROTOCOL) && (result == NULL),
      "Sync response without collection did not fail as protocol error"))
    goto err;

  root = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_SYNC);
  collection = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_COLLECTION);
  commands = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_COMMANDS);
  command = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_ADD);
  if (!check((root != NULL) && (collection != NULL) &&
      (commands != NULL) && (command != NULL),
      "Sync missing ServerId fixture allocation failed"))
    goto err;
  if (!check(mailactivesync_wbxml_node_add_child(commands, command) ==
      MAILACTIVESYNC_NO_ERROR, "Sync missing ServerId command append failed"))
    goto err;
  command = NULL;
  if (!check(mailactivesync_wbxml_node_add_child(collection, commands) ==
      MAILACTIVESYNC_NO_ERROR, "Sync missing ServerId commands append failed"))
    goto err;
  commands = NULL;
  if (!check(append_sync_collection_response(root, collection) ==
      MAILACTIVESYNC_NO_ERROR,
      "Sync missing ServerId collection append failed"))
    goto err;
  collection = NULL;
  if (!check(fake_context_set_response_body(context, root) ==
      MAILACTIVESYNC_NO_ERROR,
      "Sync missing ServerId response encode failed"))
    goto err;
  mailactivesync_wbxml_node_free(root);
  root = NULL;
  r = mailactivesync_sync(session, request, &result);
  if (!check((r == MAILACTIVESYNC_ERROR_PROTOCOL) && (result == NULL),
      "Sync add without ServerId did not fail as protocol error"))
    goto err;

  mailactivesync_sync_request_free(request);
  mailactivesync_free(session);
  return 1;

 err:
  mailactivesync_wbxml_node_free(command);
  mailactivesync_wbxml_node_free(commands);
  mailactivesync_wbxml_node_free(collection);
  mailactivesync_wbxml_node_free(root);
  mailactivesync_sync_result_free(result);
  mailactivesync_sync_request_free(request);
  mailactivesync_free(session);
  return 0;
}

int main(void)
{
  if (!test_response_header_lookup())
    return 1;
  if (!test_request_body_copy())
    return 1;
  if (!test_options_success())
    return 1;
  if (!test_options_trims_empty_list_items())
    return 1;
  if (!test_options_missing_headers())
    return 1;
  if (!test_options_unauthorized())
    return 1;
  if (!test_command_post_request())
    return 1;
  if (!test_command_post_basic_auth())
    return 1;
  if (!test_folder_sync_success())
    return 1;
  if (!test_folder_sync_response_errors())
    return 1;
  if (!test_sync_success())
    return 1;
  if (!test_sync_empty_response_and_request_defaults())
    return 1;
  if (!test_sync_change_softdelete_and_html_body())
    return 1;
  if (!test_sync_response_errors())
    return 1;

  return 0;
}
