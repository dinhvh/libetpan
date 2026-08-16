/*
 * libEtPan! -- a mail stuff library
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include <libetpan/mailactivesync.h>
#include "../../src/low-level/activesync/mailactivesync_codes.h"
#include "../../src/low-level/activesync/mailactivesync_wbxml.h"

#include "../../src/low-level/activesync/mailactivesync_command.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "activesync_tests.h"

static const unsigned char test_conversation_id[] = { 0x01, 0x02, 0x03, 0x04 };
static const unsigned char test_conversation_index[] = { 0x05, 0x06, 0x07 };

struct fake_context {
  struct mailactivesync_http_request * first_request;
  struct mailactivesync_http_request * last_request;
  int request_count;
  int status_code;
  const char * protocol_versions;
  const char * commands;
  const char * ms_location;
  const char * www_authenticate;
  unsigned char * response_body;
  size_t response_body_len;
  unsigned char * next_response_body;
  size_t next_response_body_len;
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

static int str_equal(const char * left, const char * right)
{
  return (left != NULL) && (right != NULL) && (strcmp(left, right) == 0);
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

static struct mailactivesync_wbxml_node * test_node_child_at(
    struct mailactivesync_wbxml_node * node,
    uint8_t code_page, uint8_t token, unsigned int index)
{
  clistiter * cur;
  unsigned int current_index;

  if ((node == NULL) || (node->children == NULL))
    return NULL;

  current_index = 0;
  for (cur = clist_begin(node->children); cur != NULL; cur = clist_next(cur)) {
    struct mailactivesync_wbxml_node * child;

    child = clist_content(cur);
    if ((child->code_page == code_page) && (child->token == token)) {
      if (current_index == index)
        return child;
      current_index ++;
    }
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

static int test_node_add_empty(struct mailactivesync_wbxml_node * parent,
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

static int fake_context_set_next_response_body(struct fake_context * context,
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

  free(context->next_response_body);
  context->next_response_body = body;
  context->next_response_body_len = body_len;
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

  session = mailactivesync_new();
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
  if (context->request_count == 0) {
    mailactivesync_http_request_free(context->first_request);
    context->first_request = NULL;
    r = clone_request(request, &context->first_request);
    if (r != MAILACTIVESYNC_NO_ERROR)
      return r;
  }

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
  if (context->ms_location != NULL) {
    r = mailactivesync_http_response_add_header(response, "X-MS-Location",
        context->ms_location);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
  }
  if (context->www_authenticate != NULL) {
    r = mailactivesync_http_response_add_header(response,
        "WWW-Authenticate", context->www_authenticate);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
  }
  if ((context->request_count > 0) &&
      (context->next_response_body != NULL)) {
    response->body = malloc(context->next_response_body_len);
    if (response->body == NULL)
      goto err;
    memcpy(response->body, context->next_response_body,
        context->next_response_body_len);
    response->body_len = context->next_response_body_len;
  }
  else if (context->response_body != NULL) {
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

  context->request_count ++;
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
  mailactivesync_http_request_free(context->first_request);
  mailactivesync_http_request_free(context->last_request);
  free(context->response_body);
  free(context->next_response_body);
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

  context->first_request = NULL;
  context->last_request = NULL;
  context->request_count = 0;
  context->status_code = status_code;
  context->protocol_versions = "14.1, 16.0, 16.1";
  context->commands =
      "Sync, FolderSync, FolderCreate, FolderUpdate, FolderDelete, "
      "Provision, Settings, GetItemEstimate, ItemOperations, Search, Find, "
      "ResolveRecipients, ValidateCert, MoveItems, Ping, SendMail, "
      "SmartReply, SmartForward";
  context->ms_location = NULL;
  context->www_authenticate = NULL;
  context->response_body = NULL;
  context->response_body_len = 0;
  context->next_response_body = NULL;
  context->next_response_body_len = 0;
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
  const char * best_version;
  const char * first_command;
  const char * first_version;
  const char * preferred_versions[] = { "16.1", "16.0", "14.1", NULL };
  int r;

  best_version = NULL;
  options = NULL;
  context = NULL;
  session = mailactivesync_new();
  if (!check(session != NULL, "session allocation failed"))
    return 0;

  transport = fake_transport_new(200, &context);
  if (!check(transport != NULL, "fake transport allocation failed"))
    goto err;
  context->commands = "Sync, FolderSync, ItemOperations";
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
  if (!check(mailactivesync_options_supports_command(options, "Sync"),
      "OPTIONS command helper missed Sync"))
    goto err;
  if (!check(!mailactivesync_options_supports_command(options, "Find"),
      "OPTIONS command helper found absent Find"))
    goto err;
  if (!check(mailactivesync_options_supports_protocol_version(options, "14.1"),
      "OPTIONS version helper missed 14.1"))
    goto err;
  if (!check(!mailactivesync_options_supports_protocol_version(options, "12.0"),
      "OPTIONS version helper found absent 12.0"))
    goto err;
  if (!check(mailactivesync_options_best_protocol_version(options,
      preferred_versions, &best_version) == MAILACTIVESYNC_NO_ERROR,
      "OPTIONS best protocol version failed"))
    goto err;
  if (!check(best_version == preferred_versions[0],
      "OPTIONS best protocol version mismatch"))
    goto err;
  preferred_versions[0] = "12.0";
  preferred_versions[1] = "12.1";
  preferred_versions[2] = NULL;
  if (!check(mailactivesync_options_best_protocol_version(options,
      preferred_versions, &best_version) == MAILACTIVESYNC_ERROR_NOT_IMPLEMENTED,
      "OPTIONS absent best protocol version mismatch"))
    goto err;
  if (!check(best_version == NULL,
      "OPTIONS absent best protocol version did not clear result"))
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
  session = mailactivesync_new();
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
  session = mailactivesync_new();
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
  session = mailactivesync_new();
  if (!check(session != NULL, "session allocation failed"))
    return 0;

  transport = fake_transport_new(401, &context);
  if (!check(transport != NULL, "fake transport allocation failed"))
    goto err;
  context->www_authenticate =
      "Bearer error=\"invalid_token\", authorization_uri=\"https://login.example.com/oauth2/authorize\"";
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
  if (!check(str_equal(mailactivesync_get_last_authenticate_header(session),
      context->www_authenticate),
      "unauthorized OPTIONS did not store WWW-Authenticate"))
    goto err;

  mailactivesync_free(session);
  return 1;

 err:
  mailactivesync_options_free(options);
  mailactivesync_http_transport_free(transport);
  mailactivesync_free(session);
  return 0;
}

static int test_options_status_mapping(void)
{
  static const struct {
    int status_code;
    const char * ms_location;
    int expected_error;
    const char * message;
  } cases[] = {
    { 403, NULL, MAILACTIVESYNC_ERROR_UNAUTHORIZED,
      "OPTIONS 403 did not map to unauthorized" },
    { 449, NULL, MAILACTIVESYNC_ERROR_PROVISION_REQUIRED,
      "OPTIONS 449 did not map to provision required" },
    { 451, "https://redirect.example.com/Microsoft-Server-ActiveSync",
      MAILACTIVESYNC_ERROR_REDIRECT,
      "OPTIONS 451 with X-MS-Location did not map to redirect" },
    { 451, NULL, MAILACTIVESYNC_ERROR_HTTP,
      "OPTIONS 451 without X-MS-Location did not map to HTTP error" },
  };
  size_t i;

  for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i ++) {
    mailactivesync * session;
    struct fake_context * context;
    struct mailactivesync_http_transport * transport;
    struct mailactivesync_options * options;
    int r;

    options = NULL;
    context = NULL;
    session = mailactivesync_new();
    if (!check(session != NULL, "session allocation failed"))
      return 0;

    transport = fake_transport_new(cases[i].status_code, &context);
    if (!check(transport != NULL, "fake transport allocation failed"))
      goto err;
    context->ms_location = cases[i].ms_location;
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
    if (!check(r == cases[i].expected_error, cases[i].message))
      goto err;
    if ((r == MAILACTIVESYNC_ERROR_REDIRECT) &&
        !check((mailactivesync_get_last_redirect_url(session) != NULL) &&
            (strcmp(mailactivesync_get_last_redirect_url(session),
                cases[i].ms_location) == 0),
            "OPTIONS 451 did not store redirect URL"))
      goto err;
    if ((r != MAILACTIVESYNC_ERROR_REDIRECT) &&
        !check(mailactivesync_get_last_redirect_url(session) == NULL,
            "non-redirect OPTIONS retained redirect URL"))
      goto err;
    if (!check(options == NULL, "failed OPTIONS returned result"))
      goto err;

    mailactivesync_free(session);
    continue;

   err:
    mailactivesync_options_free(options);
    mailactivesync_http_transport_free(transport);
    mailactivesync_free(session);
    return 0;
  }

  return 1;
}

static int test_options_advertised_command_enforcement(void)
{
  mailactivesync * session;
  struct fake_context * context;
  struct mailactivesync_options * options;
  struct mailactivesync_sync_request * request;
  struct mailactivesync_sync_result * result;
  int r;

  session = NULL;
  context = NULL;
  options = NULL;
  request = NULL;
  result = NULL;

  if (!check(setup_oauth_session(&session, &context) ==
      MAILACTIVESYNC_NO_ERROR, "OAuth session setup failed"))
    return 0;

  context->commands = "FolderSync, ItemOperations";
  r = mailactivesync_options(session, &options);
  if (!check(r == MAILACTIVESYNC_NO_ERROR, "OPTIONS failed"))
    goto err;
  if (!check(context->request_count == 1, "OPTIONS request count mismatch"))
    goto err;

  request = mailactivesync_sync_request_new("5", "1");
  if (!check(request != NULL, "Sync request allocation failed"))
    goto err;

  r = mailactivesync_sync(session, request, &result);
  if (!check(r == MAILACTIVESYNC_ERROR_NOT_IMPLEMENTED,
      "non-advertised Sync was not rejected"))
    goto err;
  if (!check(result == NULL, "non-advertised Sync returned result"))
    goto err;
  if (!check(context->request_count == 1,
      "non-advertised Sync sent HTTP request"))
    goto err;

  mailactivesync_sync_request_free(request);
  mailactivesync_options_free(options);
  mailactivesync_free(session);
  return 1;

 err:
  mailactivesync_sync_result_free(result);
  mailactivesync_sync_request_free(request);
  mailactivesync_options_free(options);
  mailactivesync_free(session);
  return 0;
}

static int test_oauth2_token_replacement(void)
{
  mailactivesync * session;
  struct fake_context * context;
  struct mailactivesync_options * options;
  const char * authorization;
  int r;

  session = NULL;
  context = NULL;
  options = NULL;

  if (!check(setup_oauth_session(&session, &context) ==
      MAILACTIVESYNC_NO_ERROR, "OAuth session setup failed"))
    return 0;

  if (!check(mailactivesync_set_oauth2_token(session, "fresh-token") ==
      MAILACTIVESYNC_NO_ERROR, "OAuth token replacement failed"))
    goto err;

  r = mailactivesync_options(session, &options);
  if (!check(r == MAILACTIVESYNC_NO_ERROR, "OPTIONS after token update failed"))
    goto err;

  authorization = request_header_value(context->last_request,
      "Authorization");
  if (!check((authorization != NULL) &&
      (strcmp(authorization, "Bearer fresh-token") == 0),
      "updated bearer authorization header mismatch"))
    goto err;

  mailactivesync_options_free(options);
  mailactivesync_free(session);
  return 1;

 err:
  mailactivesync_options_free(options);
  mailactivesync_free(session);
  return 0;
}

static int test_active_sync_status_mapping(void)
{
  struct mailactivesync_provision_result provision_result;
  struct mailactivesync_settings_result settings_result;

  if (!check(mailactivesync_global_status_to_error(1) ==
      MAILACTIVESYNC_NO_ERROR, "global status 1 did not map to no error"))
    return 0;
  if (!check(mailactivesync_global_status_to_error(110) ==
      MAILACTIVESYNC_ERROR_SERVER_BUSY,
      "global status 110 did not map to server busy"))
    return 0;
  if (!check(mailactivesync_global_status_to_error(141) ==
      MAILACTIVESYNC_ERROR_PROVISION_REQUIRED,
      "global status 141 did not map to provision required"))
    return 0;
  if (!check(mailactivesync_global_status_to_error(126) ==
      MAILACTIVESYNC_ERROR_CLIENT_DENIED,
      "global status 126 did not map to client denied"))
    return 0;
  if (!check(mailactivesync_global_status_to_error(111) ==
      MAILACTIVESYNC_ERROR_SERVER_BUSY,
      "global status 111 did not map to server busy"))
    return 0;
  if (!check(mailactivesync_global_status_to_error(137) ==
      MAILACTIVESYNC_ERROR_NOT_IMPLEMENTED,
      "global status 137 did not map to not implemented"))
    return 0;
  if (!check(mailactivesync_global_status_to_error(140) ==
      MAILACTIVESYNC_ERROR_PROVISION_REQUIRED,
      "global status 140 did not map to provision required"))
    return 0;
  if (!check(mailactivesync_global_status_to_error(165) ==
      MAILACTIVESYNC_ERROR_PROVISION_REQUIRED,
      "global status 165 did not map to provision required"))
    return 0;
  if (!check(mailactivesync_global_status_to_error(167) ==
      MAILACTIVESYNC_ERROR_CLIENT_DENIED,
      "global status 167 did not map to client denied"))
    return 0;
  if (!check(mailactivesync_sync_status_to_error(3) ==
      MAILACTIVESYNC_ERROR_FOLDER_RESYNC_REQUIRED,
      "Sync status 3 did not map to folder resync required"))
    return 0;
  if (!check(mailactivesync_sync_status_to_error(12) ==
      MAILACTIVESYNC_ERROR_ACCOUNT_RESYNC_REQUIRED,
      "Sync status 12 did not map to account resync required"))
    return 0;
  if (!check(mailactivesync_folder_sync_status_to_error(9) ==
      MAILACTIVESYNC_ERROR_ACCOUNT_RESYNC_REQUIRED,
      "FolderSync status 9 did not map to account resync required"))
    return 0;
  if (!check(mailactivesync_sync_status_to_error(4) ==
      MAILACTIVESYNC_ERROR_PROTOCOL,
      "Sync status 4 did not map to protocol error"))
    return 0;
  if (!check(mailactivesync_move_items_status_to_error(3) ==
      MAILACTIVESYNC_NO_ERROR,
      "MoveItems status 3 did not map to no error"))
    return 0;
  if (!check(mailactivesync_move_items_status_to_error(1) ==
      MAILACTIVESYNC_ERROR_FOLDER_RESYNC_REQUIRED,
      "MoveItems status 1 did not map to folder resync required"))
    return 0;
  if (!check(mailactivesync_get_item_estimate_status_to_error(2) ==
      MAILACTIVESYNC_ERROR_FOLDER_RESYNC_REQUIRED,
      "GetItemEstimate status 2 did not map to folder resync required"))
    return 0;
  if (!check(mailactivesync_get_item_estimate_status_to_error(3) ==
      MAILACTIVESYNC_ERROR_INVALID_SYNC_KEY,
      "GetItemEstimate status 3 did not map to invalid sync key"))
    return 0;
  if (!check(mailactivesync_get_item_estimate_status_to_error(4) ==
      MAILACTIVESYNC_ERROR_INVALID_SYNC_KEY,
      "GetItemEstimate status 4 did not map to invalid sync key"))
    return 0;
  if (!check(mailactivesync_move_items_status_to_error(7) ==
      MAILACTIVESYNC_ERROR_SERVER_BUSY,
      "MoveItems status 7 did not map to server busy"))
    return 0;
  if (!check(mailactivesync_item_operations_status_to_error(17) ==
      MAILACTIVESYNC_NO_ERROR,
      "ItemOperations status 17 did not map to no error"))
    return 0;
  if (!check(mailactivesync_item_operations_status_to_error(12) ==
      MAILACTIVESYNC_ERROR_SERVER_BUSY,
      "ItemOperations status 12 did not map to server busy"))
    return 0;
  if (!check(mailactivesync_item_operations_status_to_error(5) ==
      MAILACTIVESYNC_ERROR_CLIENT_DENIED,
      "ItemOperations status 5 did not map to client denied"))
    return 0;
  if (!check(mailactivesync_item_operations_status_to_error(15) ==
      MAILACTIVESYNC_ERROR_PROTOCOL,
      "ItemOperations status 15 did not map to protocol error"))
    return 0;
  if (!check(mailactivesync_resolve_recipients_response_status_to_error(3) ==
      MAILACTIVESYNC_NO_ERROR,
      "ResolveRecipients response status 3 did not map to no error"))
    return 0;
  if (!check(mailactivesync_resolve_recipients_response_status_to_error(4) ==
      MAILACTIVESYNC_ERROR_PROTOCOL,
      "ResolveRecipients response status 4 did not map to protocol error"))
    return 0;
  if (!check(
      mailactivesync_resolve_recipients_certificates_status_to_error(7) ==
      MAILACTIVESYNC_NO_ERROR,
      "ResolveRecipients certificate status 7 did not map to no error"))
    return 0;
  if (!check(
      mailactivesync_resolve_recipients_certificates_status_to_error(8) ==
      MAILACTIVESYNC_ERROR_PROTOCOL,
      "ResolveRecipients certificate status 8 did not map to protocol error"))
    return 0;
  if (!check(mailactivesync_validate_cert_status_to_error(17) ==
      MAILACTIVESYNC_ERROR_PROTOCOL,
      "ValidateCert command status 17 did not map to protocol error"))
    return 0;
  if (!check(mailactivesync_validate_cert_certificate_status_to_error(14) ==
      MAILACTIVESYNC_ERROR_SERVER_BUSY,
      "ValidateCert certificate status 14 did not map to server busy"))
    return 0;
  if (!check(mailactivesync_validate_cert_certificate_status_to_error(17) ==
      MAILACTIVESYNC_ERROR_PROTOCOL,
      "ValidateCert certificate status 17 did not map to protocol error"))
    return 0;
  if (!check(mailactivesync_provision_status_to_error(3) ==
      MAILACTIVESYNC_ERROR_SERVER_BUSY,
      "Provision status 3 did not map to server busy"))
    return 0;
  if (!check(mailactivesync_provision_policy_status_to_error(2) ==
      MAILACTIVESYNC_NO_ERROR,
      "Provision policy status 2 did not map to no error"))
    return 0;
  if (!check(mailactivesync_provision_policy_status_to_error(5) ==
      MAILACTIVESYNC_ERROR_PROVISION_REQUIRED,
      "Provision policy status 5 did not map to provision required"))
    return 0;
  memset(&provision_result, 0, sizeof(provision_result));
  provision_result.status = 1;
  provision_result.policy_status = 5;
  if (!check(mailactivesync_provision_result_status_to_error(
      &provision_result) == MAILACTIVESYNC_ERROR_PROVISION_REQUIRED,
      "Provision result status did not preserve policy failure"))
    return 0;
  if (!check(mailactivesync_provision_result_status_to_error(NULL) ==
      MAILACTIVESYNC_ERROR_BAD_STATE,
      "Provision result NULL status did not fail as bad state"))
    return 0;
  if (!check(mailactivesync_settings_status_to_error(3) ==
      MAILACTIVESYNC_ERROR_CLIENT_DENIED,
      "Settings status 3 did not map to client denied"))
    return 0;
  if (!check(mailactivesync_settings_status_to_error(4) ==
      MAILACTIVESYNC_ERROR_SERVER_BUSY,
      "Settings status 4 did not map to server busy"))
    return 0;
  memset(&settings_result, 0, sizeof(settings_result));
  settings_result.status = 1;
  settings_result.device_information_status = 1;
  settings_result.user_information_status = 4;
  if (!check(mailactivesync_settings_result_status_to_error(
      &settings_result) == MAILACTIVESYNC_ERROR_SERVER_BUSY,
      "Settings result status did not preserve user information failure"))
    return 0;
  if (!check(mailactivesync_settings_result_status_to_error(NULL) ==
      MAILACTIVESYNC_ERROR_BAD_STATE,
      "Settings result NULL status did not fail as bad state"))
    return 0;

  return 1;
}

static int test_version_specific_email_restrictions(void)
{
  mailactivesync * session;
  struct fake_context * context;
  struct mailactivesync_sync_request * sync_request;
  struct mailactivesync_sync_result * sync_result;
  struct mailactivesync_item * item;
  struct mailactivesync_mail_search_request search_request;
  struct mailactivesync_mail_search_result * search_result;
  struct mailactivesync_mail_find_request find_request;
  struct mailactivesync_mail_find_result * find_result;
  struct mailactivesync_draft draft;
  int initial_request_count;
  int r;

  session = NULL;
  context = NULL;
  sync_request = NULL;
  sync_result = NULL;
  item = NULL;
  search_result = NULL;
  find_result = NULL;

  if (!check(setup_oauth_session(&session, &context) ==
      MAILACTIVESYNC_NO_ERROR, "setup OAuth session failed"))
    return 0;
  initial_request_count = context->request_count;

  if (!check(mailactivesync_set_protocol_version(session, "12.0") ==
      MAILACTIVESYNC_NO_ERROR, "set ActiveSync 12.0 failed"))
    goto err;
  sync_request = mailactivesync_sync_request_new("inbox", "1");
  if (!check(sync_request != NULL,
      "version restriction Wait request allocation failed"))
    goto err;
  if (!check(mailactivesync_sync_request_set_wait(sync_request, 10) ==
      MAILACTIVESYNC_NO_ERROR, "version restriction Wait set failed"))
    goto err;
  r = mailactivesync_sync(session, sync_request, &sync_result);
  if (!check((r == MAILACTIVESYNC_ERROR_NOT_IMPLEMENTED) &&
      (sync_result == NULL) &&
      (context->request_count == initial_request_count),
      "Sync Wait protocol restriction sent request or returned result"))
    goto err;
  mailactivesync_sync_request_free(sync_request);
  sync_request = NULL;

  if (!check(mailactivesync_set_protocol_version(session, "12.1") ==
      MAILACTIVESYNC_NO_ERROR, "set ActiveSync 12.1 failed"))
    goto err;
  sync_request = mailactivesync_sync_request_new("inbox", "1");
  if (!check(sync_request != NULL,
      "version restriction ConversationMode request allocation failed"))
    goto err;
  if (!check(mailactivesync_sync_request_set_conversation_mode(sync_request,
      1) == MAILACTIVESYNC_NO_ERROR,
      "version restriction ConversationMode set failed"))
    goto err;
  r = mailactivesync_sync(session, sync_request, &sync_result);
  if (!check((r == MAILACTIVESYNC_ERROR_NOT_IMPLEMENTED) &&
      (sync_result == NULL) &&
      (context->request_count == initial_request_count),
      "Sync ConversationMode protocol restriction sent request or result"))
    goto err;
  mailactivesync_sync_request_free(sync_request);
  sync_request = NULL;

  if (!check(mailactivesync_set_protocol_version(session, "14.0") ==
      MAILACTIVESYNC_NO_ERROR, "set ActiveSync 14.0 failed"))
    goto err;
  r = mailactivesync_item_operations_fetch_body_part(session, "inbox",
      "msg-1", MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_HTML, 0, &item);
  if (!check((r == MAILACTIVESYNC_ERROR_NOT_IMPLEMENTED) &&
      (item == NULL) && (context->request_count == initial_request_count),
      "BodyPartPreference protocol restriction sent request or item"))
    goto err;

  if (!check(mailactivesync_set_protocol_version(session, "2.5") ==
      MAILACTIVESYNC_NO_ERROR, "set ActiveSync 2.5 failed"))
    goto err;
  memset(&search_request, 0, sizeof(search_request));
  search_request.collection_id = "inbox";
  search_request.free_text = "needle";
  r = mailactivesync_mail_search(session, &search_request, &search_result);
  if (!check((r == MAILACTIVESYNC_ERROR_NOT_IMPLEMENTED) &&
      (search_result == NULL) &&
      (context->request_count == initial_request_count),
      "Search protocol restriction sent request or result"))
    goto err;

  if (!check(mailactivesync_set_protocol_version(session, "16.0") ==
      MAILACTIVESYNC_NO_ERROR, "set ActiveSync 16.0 failed"))
    goto err;
  memset(&find_request, 0, sizeof(find_request));
  find_request.search_id = "01234567-89ab-cdef-0123-456789abcdef";
  find_request.collection_id = "inbox";
  find_request.query = "needle";
  r = mailactivesync_mail_find(session, &find_request, &find_result);
  if (!check((r == MAILACTIVESYNC_ERROR_NOT_IMPLEMENTED) &&
      (find_result == NULL) &&
      (context->request_count == initial_request_count),
      "Find protocol restriction sent request or result"))
    goto err;

  if (!check(mailactivesync_set_protocol_version(session, "14.1") ==
      MAILACTIVESYNC_NO_ERROR, "set ActiveSync 14.1 failed"))
    goto err;
  memset(&draft, 0, sizeof(draft));
  draft.to = "to@example.com";
  draft.subject = "Draft";
  draft.body_type = MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_PLAIN_TEXT;
  draft.body = "body";
  r = mailactivesync_add_draft(session, "drafts", "1", "client-1", &draft,
      &sync_result);
  if (!check((r == MAILACTIVESYNC_ERROR_NOT_IMPLEMENTED) &&
      (sync_result == NULL) &&
      (context->request_count == initial_request_count),
      "Draft Add protocol restriction sent request or result"))
    goto err;

  if (!check(mailactivesync_set_protocol_version(session, "16.1") ==
      MAILACTIVESYNC_NO_ERROR, "set ActiveSync 16.1 failed"))
    goto err;
  memset(&draft, 0, sizeof(draft));
  draft.subject = "Invalid MIME draft subject";
  draft.body_type = MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_MIME;
  draft.body = "Subject: MIME draft\r\n\r\nbody";
  r = mailactivesync_update_draft(session, "drafts", "1", "server-1",
      &draft, &sync_result);
  if (!check((r == MAILACTIVESYNC_ERROR_BAD_STATE) &&
      (sync_result == NULL) &&
      (context->request_count == initial_request_count),
      "MIME draft field restriction sent request or result"))
    goto err;

  mailactivesync_free(session);
  return 1;

 err:
  mailactivesync_item_free(item);
  mailactivesync_sync_result_free(sync_result);
  mailactivesync_sync_request_free(sync_request);
  mailactivesync_mail_search_result_free(search_result);
  mailactivesync_mail_find_result_free(find_result);
  mailactivesync_free(session);
  return 0;
}

static struct mailactivesync_wbxml_node * provision_response_new(
    const char * status, const char * policy_status, const char * policy_key)
{
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_node * policies;
  struct mailactivesync_wbxml_node * policy;

  root = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_PROVISION,
      MAILACTIVESYNC_PROVISION_PROVISION);
  policies = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_PROVISION,
      MAILACTIVESYNC_PROVISION_POLICIES);
  policy = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_PROVISION,
      MAILACTIVESYNC_PROVISION_POLICY);
  if ((root == NULL) || (policies == NULL) || (policy == NULL))
    goto err;
  if (test_node_add_text(root, MAILACTIVESYNC_CP_PROVISION,
      MAILACTIVESYNC_PROVISION_STATUS, status) != MAILACTIVESYNC_NO_ERROR)
    goto err;
  if (test_node_add_text(policy, MAILACTIVESYNC_CP_PROVISION,
      MAILACTIVESYNC_PROVISION_STATUS,
      policy_status) != MAILACTIVESYNC_NO_ERROR)
    goto err;
  if ((policy_key != NULL) && (test_node_add_text(policy,
      MAILACTIVESYNC_CP_PROVISION, MAILACTIVESYNC_PROVISION_POLICY_KEY,
      policy_key) != MAILACTIVESYNC_NO_ERROR))
    goto err;
  if (mailactivesync_wbxml_node_add_child(policies, policy) !=
      MAILACTIVESYNC_NO_ERROR)
    goto err;
  policy = NULL;
  if (mailactivesync_wbxml_node_add_child(root, policies) !=
      MAILACTIVESYNC_NO_ERROR)
    goto err;
  policies = NULL;

  return root;

 err:
  mailactivesync_wbxml_node_free(policy);
  mailactivesync_wbxml_node_free(policies);
  mailactivesync_wbxml_node_free(root);
  return NULL;
}

static int test_provision_success(void)
{
  mailactivesync * session;
  struct fake_context * context;
  struct mailactivesync_provision_result * result;
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_document * request_document;
  struct mailactivesync_wbxml_node * policies;
  struct mailactivesync_wbxml_node * policy;
  struct mailactivesync_options * options;
  int r;

  session = NULL;
  context = NULL;
  result = NULL;
  root = NULL;
  request_document = NULL;
  options = NULL;

  if (!check(setup_oauth_session(&session, &context) ==
      MAILACTIVESYNC_NO_ERROR, "setup OAuth session failed"))
    return 0;

  root = provision_response_new("1", "1", "12345");
  if (!check(root != NULL, "Provision initial response allocation failed"))
    goto err;
  if (!check(fake_context_set_response_body(context, root) ==
      MAILACTIVESYNC_NO_ERROR, "Provision initial response encode failed"))
    goto err;
  mailactivesync_wbxml_node_free(root);
  root = NULL;

  root = provision_response_new("1", "1", "67890");
  if (!check(root != NULL, "Provision ACK response allocation failed"))
    goto err;
  if (!check(fake_context_set_next_response_body(context, root) ==
      MAILACTIVESYNC_NO_ERROR, "Provision ACK response encode failed"))
    goto err;
  mailactivesync_wbxml_node_free(root);
  root = NULL;

  r = mailactivesync_provision(session, &result);
  if (!check(r == MAILACTIVESYNC_NO_ERROR, "Provision command failed"))
    goto err;
  if (!check((result != NULL) && (result->status == 1) &&
      (result->policy_status == 1) && (result->policy_key != NULL) &&
      (strcmp(result->policy_key, "67890") == 0),
      "Provision result mismatch"))
    goto err;
  if (!check(context->request_count == 2,
      "Provision did not perform two requests"))
    goto err;

  if (!check(mailactivesync_wbxml_decode(context->first_request->body,
      context->first_request->body_len, &request_document) ==
      MAILACTIVESYNC_NO_ERROR, "Provision initial request decode failed"))
    goto err;
  policies = test_node_child(request_document->root,
      MAILACTIVESYNC_CP_PROVISION, MAILACTIVESYNC_PROVISION_POLICIES);
  policy = test_node_child(policies, MAILACTIVESYNC_CP_PROVISION,
      MAILACTIVESYNC_PROVISION_POLICY);
  if (!check(strcmp(test_node_child_text(policy, MAILACTIVESYNC_CP_PROVISION,
      MAILACTIVESYNC_PROVISION_POLICY_TYPE),
      "MS-EAS-Provisioning-WBXML") == 0,
      "Provision initial PolicyType mismatch"))
    goto err;
  if (!check(test_node_child(policy, MAILACTIVESYNC_CP_PROVISION,
      MAILACTIVESYNC_PROVISION_POLICY_KEY) == NULL,
      "Provision initial request unexpectedly included PolicyKey"))
    goto err;
  mailactivesync_wbxml_document_free(request_document);
  request_document = NULL;

  if (!check(mailactivesync_wbxml_decode(context->last_request->body,
      context->last_request->body_len, &request_document) ==
      MAILACTIVESYNC_NO_ERROR, "Provision ACK request decode failed"))
    goto err;
  policies = test_node_child(request_document->root,
      MAILACTIVESYNC_CP_PROVISION, MAILACTIVESYNC_PROVISION_POLICIES);
  policy = test_node_child(policies, MAILACTIVESYNC_CP_PROVISION,
      MAILACTIVESYNC_PROVISION_POLICY);
  if (!check(strcmp(test_node_child_text(policy, MAILACTIVESYNC_CP_PROVISION,
      MAILACTIVESYNC_PROVISION_POLICY_KEY), "12345") == 0,
      "Provision ACK PolicyKey mismatch"))
    goto err;
  if (!check(strcmp(test_node_child_text(policy, MAILACTIVESYNC_CP_PROVISION,
      MAILACTIVESYNC_PROVISION_STATUS), "1") == 0,
      "Provision ACK Status mismatch"))
    goto err;

  mailactivesync_options_free(options);
  mailactivesync_wbxml_document_free(request_document);
  mailactivesync_provision_result_free(result);
  result = NULL;
  r = mailactivesync_options(session, &options);
  if (!check(r == MAILACTIVESYNC_NO_ERROR,
      "OPTIONS after Provision failed"))
    goto err;
  if (!check(strcmp(request_header_value(context->last_request,
      "X-MS-PolicyKey"), "67890") == 0,
      "Provision did not store policy key for later requests"))
    goto err;

  mailactivesync_options_free(options);
  mailactivesync_free(session);
  return 1;

 err:
  mailactivesync_options_free(options);
  mailactivesync_wbxml_document_free(request_document);
  mailactivesync_wbxml_node_free(root);
  mailactivesync_provision_result_free(result);
  mailactivesync_free(session);
  return 0;
}

static int test_settings_device_information_success(void)
{
  mailactivesync * session;
  struct fake_context * context;
  struct mailactivesync_device_information device_information;
  struct mailactivesync_settings_result * result;
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_node * device_node;
  struct mailactivesync_wbxml_node * set_node;
  struct mailactivesync_wbxml_node * user_node;
  struct mailactivesync_wbxml_node * get_node;
  struct mailactivesync_wbxml_node * email_addresses_node;
  struct mailactivesync_wbxml_node * accounts_node;
  struct mailactivesync_wbxml_node * account_node;
  struct mailactivesync_wbxml_node * request_device_node;
  struct mailactivesync_wbxml_node * request_user_node;
  struct mailactivesync_wbxml_node * request_get_node;
  struct mailactivesync_wbxml_document * request_document;
  struct mailactivesync_settings_account * account;
  int r;

  session = NULL;
  context = NULL;
  result = NULL;
  root = NULL;
  user_node = NULL;
  get_node = NULL;
  email_addresses_node = NULL;
  accounts_node = NULL;
  account_node = NULL;
  request_device_node = NULL;
  request_user_node = NULL;
  request_get_node = NULL;
  request_document = NULL;
  account = NULL;

  if (!check(setup_oauth_session(&session, &context) ==
      MAILACTIVESYNC_NO_ERROR, "setup OAuth session failed"))
    return 0;

  root = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_SETTINGS,
      MAILACTIVESYNC_SETTINGS_SETTINGS);
  device_node = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_SETTINGS,
      MAILACTIVESYNC_SETTINGS_DEVICE_INFORMATION);
  if (!check((root != NULL) && (device_node != NULL),
      "Settings response allocation failed"))
    goto err;
  if (!check(test_node_add_text(root, MAILACTIVESYNC_CP_SETTINGS,
      MAILACTIVESYNC_SETTINGS_STATUS, "1") == MAILACTIVESYNC_NO_ERROR,
      "Settings response Status add failed"))
    goto err;
  if (!check(test_node_add_text(device_node, MAILACTIVESYNC_CP_SETTINGS,
      MAILACTIVESYNC_SETTINGS_STATUS, "1") == MAILACTIVESYNC_NO_ERROR,
      "Settings DeviceInformation Status add failed"))
    goto err;
  if (!check(mailactivesync_wbxml_node_add_child(root, device_node) ==
      MAILACTIVESYNC_NO_ERROR,
      "Settings DeviceInformation response append failed"))
    goto err;
  device_node = NULL;
  if (!check(fake_context_set_response_body(context, root) ==
      MAILACTIVESYNC_NO_ERROR, "Settings response encode failed"))
    goto err;
  mailactivesync_wbxml_node_free(root);
  root = NULL;

  memset(&device_information, 0, sizeof(device_information));
  device_information.model = "libetpan";
  device_information.imei = "device-1";
  device_information.friendly_name = "Friendly libetpan";
  device_information.os = "Linux";
  device_information.os_language = "en-US";
  device_information.phone_number = "";
  device_information.user_agent = "libEtPan ActiveSync";
  device_information.mobile_operator = "";

  r = mailactivesync_settings_set_device_information(session,
      &device_information, &result);
  if (!check(r == MAILACTIVESYNC_NO_ERROR,
      "Settings DeviceInformation command failed"))
    goto err;
  if (!check((result != NULL) && (result->status == 1) &&
      (result->device_information_status == 1),
      "Settings DeviceInformation result mismatch"))
    goto err;
  if (!check(mailactivesync_wbxml_decode(context->last_request->body,
      context->last_request->body_len, &request_document) ==
      MAILACTIVESYNC_NO_ERROR, "Settings request decode failed"))
    goto err;
  request_device_node = test_node_child(request_document->root,
      MAILACTIVESYNC_CP_SETTINGS,
      MAILACTIVESYNC_SETTINGS_DEVICE_INFORMATION);
  set_node = test_node_child(request_device_node, MAILACTIVESYNC_CP_SETTINGS,
      MAILACTIVESYNC_SETTINGS_SET);
  if (!check(strcmp(test_node_child_text(set_node,
      MAILACTIVESYNC_CP_SETTINGS, MAILACTIVESYNC_SETTINGS_MODEL),
      "libetpan") == 0, "Settings Model mismatch"))
    goto err;
  if (!check(strcmp(test_node_child_text(set_node,
      MAILACTIVESYNC_CP_SETTINGS, MAILACTIVESYNC_SETTINGS_IMEI),
      "device-1") == 0, "Settings IMEI mismatch"))
    goto err;
  if (!check(strcmp(test_node_child_text(set_node,
      MAILACTIVESYNC_CP_SETTINGS, MAILACTIVESYNC_SETTINGS_FRIENDLY_NAME),
      "Friendly libetpan") == 0, "Settings FriendlyName mismatch"))
    goto err;
  if (!check(strcmp(test_node_child_text(set_node,
      MAILACTIVESYNC_CP_SETTINGS, MAILACTIVESYNC_SETTINGS_USER_AGENT),
      "libEtPan ActiveSync") == 0, "Settings UserAgent mismatch"))
    goto err;

  mailactivesync_wbxml_document_free(request_document);
  request_document = NULL;
  mailactivesync_settings_result_free(result);
  result = NULL;

  root = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_SETTINGS,
      MAILACTIVESYNC_SETTINGS_SETTINGS);
  user_node = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_SETTINGS,
      MAILACTIVESYNC_SETTINGS_USER_INFORMATION);
  get_node = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_SETTINGS,
      MAILACTIVESYNC_SETTINGS_GET);
  email_addresses_node = mailactivesync_wbxml_node_new(
      MAILACTIVESYNC_CP_SETTINGS, MAILACTIVESYNC_SETTINGS_EMAIL_ADDRESSES);
  accounts_node = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_SETTINGS,
      MAILACTIVESYNC_SETTINGS_ACCOUNTS);
  account_node = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_SETTINGS,
      MAILACTIVESYNC_SETTINGS_ACCOUNT);
  if (!check((root != NULL) && (user_node != NULL) && (get_node != NULL) &&
      (email_addresses_node != NULL) && (accounts_node != NULL) &&
      (account_node != NULL), "Settings Get response allocation failed"))
    goto err;
  if (!check(test_node_add_text(root, MAILACTIVESYNC_CP_SETTINGS,
      MAILACTIVESYNC_SETTINGS_STATUS, "1") == MAILACTIVESYNC_NO_ERROR,
      "Settings Get response Status add failed"))
    goto err;
  if (!check(test_node_add_text(user_node, MAILACTIVESYNC_CP_SETTINGS,
      MAILACTIVESYNC_SETTINGS_STATUS, "1") == MAILACTIVESYNC_NO_ERROR,
      "Settings UserInformation Status add failed"))
    goto err;
  if (!check(test_node_add_text(email_addresses_node,
      MAILACTIVESYNC_CP_SETTINGS, MAILACTIVESYNC_SETTINGS_SMTP_ADDRESS,
      "user@example.com") == MAILACTIVESYNC_NO_ERROR,
      "Settings SMTPAddress add failed"))
    goto err;
  if (!check(test_node_add_text(email_addresses_node,
      MAILACTIVESYNC_CP_SETTINGS,
      MAILACTIVESYNC_SETTINGS_PRIMARY_SMTP_ADDRESS,
      "primary@example.com") == MAILACTIVESYNC_NO_ERROR,
      "Settings PrimarySmtpAddress add failed"))
    goto err;
  if (!check(test_node_add_text(account_node, MAILACTIVESYNC_CP_SETTINGS,
      MAILACTIVESYNC_SETTINGS_ACCOUNT_ID, "account-1") ==
      MAILACTIVESYNC_NO_ERROR, "Settings AccountId add failed"))
    goto err;
  if (!check(test_node_add_text(account_node, MAILACTIVESYNC_CP_SETTINGS,
      MAILACTIVESYNC_SETTINGS_ACCOUNT_NAME, "Work") ==
      MAILACTIVESYNC_NO_ERROR, "Settings AccountName add failed"))
    goto err;
  if (!check(test_node_add_text(account_node, MAILACTIVESYNC_CP_SETTINGS,
      MAILACTIVESYNC_SETTINGS_USER_DISPLAY_NAME, "Example User") ==
      MAILACTIVESYNC_NO_ERROR, "Settings UserDisplayName add failed"))
    goto err;
  if (!check(test_node_add_text(account_node, MAILACTIVESYNC_CP_SETTINGS,
      MAILACTIVESYNC_SETTINGS_SEND_DISABLED, "0") ==
      MAILACTIVESYNC_NO_ERROR, "Settings SendDisabled add failed"))
    goto err;
  if (!check(mailactivesync_wbxml_node_add_child(get_node,
      email_addresses_node) == MAILACTIVESYNC_NO_ERROR,
      "Settings EmailAddresses append failed"))
    goto err;
  email_addresses_node = NULL;
  if (!check(mailactivesync_wbxml_node_add_child(accounts_node,
      account_node) == MAILACTIVESYNC_NO_ERROR,
      "Settings Account append failed"))
    goto err;
  account_node = NULL;
  if (!check(mailactivesync_wbxml_node_add_child(get_node,
      accounts_node) == MAILACTIVESYNC_NO_ERROR,
      "Settings Accounts append failed"))
    goto err;
  accounts_node = NULL;
  if (!check(mailactivesync_wbxml_node_add_child(user_node, get_node) ==
      MAILACTIVESYNC_NO_ERROR, "Settings UserInformation Get append failed"))
    goto err;
  get_node = NULL;
  if (!check(mailactivesync_wbxml_node_add_child(root, user_node) ==
      MAILACTIVESYNC_NO_ERROR,
      "Settings UserInformation response append failed"))
    goto err;
  user_node = NULL;
  if (!check(fake_context_set_response_body(context, root) ==
      MAILACTIVESYNC_NO_ERROR, "Settings Get response encode failed"))
    goto err;
  mailactivesync_wbxml_node_free(root);
  root = NULL;

  r = mailactivesync_settings_get_user_information(session, &result);
  if (!check(r == MAILACTIVESYNC_NO_ERROR,
      "Settings UserInformation command failed"))
    goto err;
  if (!check((result != NULL) && (result->status == 1) &&
      (result->user_information_status == 1) &&
      str_equal(result->primary_smtp_address, "primary@example.com") &&
      (result->smtp_addresses != NULL) &&
      (clist_count(result->smtp_addresses) == 1) &&
      str_equal(clist_content(clist_begin(result->smtp_addresses)),
          "user@example.com") &&
      (result->accounts != NULL) && (clist_count(result->accounts) == 1),
      "Settings UserInformation result mismatch"))
    goto err;
  account = clist_content(clist_begin(result->accounts));
  if (!check((account != NULL) &&
      str_equal(account->account_id, "account-1") &&
      str_equal(account->account_name, "Work") &&
      str_equal(account->user_display_name, "Example User") &&
      (account->send_disabled == 0),
      "Settings account result mismatch"))
    goto err;
  if (!check(mailactivesync_wbxml_decode(context->last_request->body,
      context->last_request->body_len, &request_document) ==
      MAILACTIVESYNC_NO_ERROR, "Settings Get request decode failed"))
    goto err;
  request_user_node = test_node_child(request_document->root,
      MAILACTIVESYNC_CP_SETTINGS,
      MAILACTIVESYNC_SETTINGS_USER_INFORMATION);
  request_get_node = test_node_child(request_user_node,
      MAILACTIVESYNC_CP_SETTINGS,
      MAILACTIVESYNC_SETTINGS_GET);
  if (!check((request_user_node != NULL) && (request_get_node != NULL),
      "Settings UserInformation Get request shape mismatch"))
    goto err;

  mailactivesync_wbxml_document_free(request_document);
  mailactivesync_settings_result_free(result);
  mailactivesync_free(session);
  return 1;

 err:
  mailactivesync_wbxml_document_free(request_document);
  mailactivesync_wbxml_node_free(account_node);
  mailactivesync_wbxml_node_free(accounts_node);
  mailactivesync_wbxml_node_free(email_addresses_node);
  mailactivesync_wbxml_node_free(get_node);
  mailactivesync_wbxml_node_free(user_node);
  mailactivesync_wbxml_node_free(device_node);
  mailactivesync_wbxml_node_free(root);
  mailactivesync_settings_result_free(result);
  mailactivesync_free(session);
  return 0;
}

static int test_get_item_estimate_success_and_empty_response(void)
{
  mailactivesync * session;
  struct fake_context * context;
  struct mailactivesync_get_item_estimate_result * result;
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_node * response;
  struct mailactivesync_wbxml_node * collection;
  struct mailactivesync_wbxml_document * request_document;
  struct mailactivesync_wbxml_node * request_collections;
  struct mailactivesync_wbxml_node * request_collection;
  int r;

  session = NULL;
  context = NULL;
  result = NULL;
  root = NULL;
  response = NULL;
  collection = NULL;
  request_document = NULL;

  if (!check(setup_oauth_session(&session, &context) ==
      MAILACTIVESYNC_NO_ERROR, "setup OAuth session failed"))
    return 0;

  root = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_GETITEMESTIMATE,
      MAILACTIVESYNC_GETITEMESTIMATE_GET_ITEM_ESTIMATE);
  response = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_GETITEMESTIMATE,
      MAILACTIVESYNC_GETITEMESTIMATE_RESPONSE);
  collection = mailactivesync_wbxml_node_new(
      MAILACTIVESYNC_CP_GETITEMESTIMATE,
      MAILACTIVESYNC_GETITEMESTIMATE_COLLECTION);
  if (!check((root != NULL) && (response != NULL) && (collection != NULL),
      "GetItemEstimate response allocation failed"))
    goto err;
  if (!check(test_node_add_text(root, MAILACTIVESYNC_CP_GETITEMESTIMATE,
      MAILACTIVESYNC_GETITEMESTIMATE_STATUS, "1") ==
      MAILACTIVESYNC_NO_ERROR, "GetItemEstimate Status add failed"))
    goto err;
  if (!check(test_node_add_text(collection,
      MAILACTIVESYNC_CP_GETITEMESTIMATE,
      MAILACTIVESYNC_GETITEMESTIMATE_STATUS, "1") ==
      MAILACTIVESYNC_NO_ERROR,
      "GetItemEstimate collection Status add failed"))
    goto err;
  if (!check(test_node_add_text(collection,
      MAILACTIVESYNC_CP_GETITEMESTIMATE,
      MAILACTIVESYNC_GETITEMESTIMATE_ESTIMATE, "42") ==
      MAILACTIVESYNC_NO_ERROR, "GetItemEstimate Estimate add failed"))
    goto err;
  if (!check(mailactivesync_wbxml_node_add_child(response, collection) ==
      MAILACTIVESYNC_NO_ERROR,
      "GetItemEstimate collection append failed"))
    goto err;
  collection = NULL;
  if (!check(mailactivesync_wbxml_node_add_child(root, response) ==
      MAILACTIVESYNC_NO_ERROR, "GetItemEstimate response append failed"))
    goto err;
  response = NULL;
  if (!check(fake_context_set_response_body(context, root) ==
      MAILACTIVESYNC_NO_ERROR, "GetItemEstimate response encode failed"))
    goto err;
  mailactivesync_wbxml_node_free(root);
  root = NULL;

  r = mailactivesync_get_item_estimate(session, "7", "123", &result);
  if (!check(r == MAILACTIVESYNC_NO_ERROR, "GetItemEstimate command failed"))
    goto err;
  if (!check((result != NULL) && (result->status == 1) &&
      (result->collection_status == 1) && (result->estimate == 42) &&
      !result->empty_response, "GetItemEstimate result mismatch"))
    goto err;
  if (!check(mailactivesync_wbxml_decode(context->last_request->body,
      context->last_request->body_len, &request_document) ==
      MAILACTIVESYNC_NO_ERROR, "GetItemEstimate request decode failed"))
    goto err;
  request_collections = test_node_child(request_document->root,
      MAILACTIVESYNC_CP_GETITEMESTIMATE,
      MAILACTIVESYNC_GETITEMESTIMATE_COLLECTIONS);
  request_collection = test_node_child(request_collections,
      MAILACTIVESYNC_CP_GETITEMESTIMATE,
      MAILACTIVESYNC_GETITEMESTIMATE_COLLECTION);
  if (!check(strcmp(test_node_child_text(request_collection,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_SYNC_KEY),
      "123") == 0, "GetItemEstimate request SyncKey mismatch"))
    goto err;
  if (!check(strcmp(test_node_child_text(request_collection,
      MAILACTIVESYNC_CP_GETITEMESTIMATE,
      MAILACTIVESYNC_GETITEMESTIMATE_COLLECTION_ID),
      "7") == 0, "GetItemEstimate request CollectionId mismatch"))
    goto err;
  mailactivesync_wbxml_document_free(request_document);
  request_document = NULL;
  mailactivesync_get_item_estimate_result_free(result);
  result = NULL;

  if (!check(fake_context_set_raw_response_body(context, NULL, 0) ==
      MAILACTIVESYNC_NO_ERROR, "GetItemEstimate empty response set failed"))
    goto err;
  r = mailactivesync_get_item_estimate(session, "7", "123", &result);
  if (!check(r == MAILACTIVESYNC_NO_ERROR,
      "GetItemEstimate empty response failed"))
    goto err;
  if (!check((result != NULL) && (result->status == 1) &&
      (result->collection_status == 1) && (result->estimate == 0) &&
      result->empty_response,
      "GetItemEstimate empty response result mismatch"))
    goto err;

  mailactivesync_get_item_estimate_result_free(result);
  mailactivesync_free(session);
  return 1;

 err:
  mailactivesync_wbxml_document_free(request_document);
  mailactivesync_wbxml_node_free(collection);
  mailactivesync_wbxml_node_free(response);
  mailactivesync_wbxml_node_free(root);
  mailactivesync_get_item_estimate_result_free(result);
  mailactivesync_free(session);
  return 0;
}

static int test_get_item_estimate_multi_success(void)
{
  mailactivesync * session;
  struct fake_context * context;
  struct mailactivesync_get_item_estimate_collection_request request_1;
  struct mailactivesync_get_item_estimate_collection_request request_2;
  struct mailactivesync_get_item_estimate_result * result;
  struct mailactivesync_get_item_estimate_collection * parsed_collection;
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_node * response;
  struct mailactivesync_wbxml_node * collection;
  struct mailactivesync_wbxml_document * request_document;
  struct mailactivesync_wbxml_node * request_collections;
  struct mailactivesync_wbxml_node * request_collection_1;
  struct mailactivesync_wbxml_node * request_collection_2;
  struct mailactivesync_wbxml_node * request_options;
  clist * requests;
  int r;

  session = NULL;
  context = NULL;
  result = NULL;
  root = NULL;
  response = NULL;
  collection = NULL;
  request_document = NULL;
  requests = NULL;
  memset(&request_1, 0, sizeof(request_1));
  memset(&request_2, 0, sizeof(request_2));

  if (!check(setup_oauth_session(&session, &context) ==
      MAILACTIVESYNC_NO_ERROR, "setup OAuth session failed"))
    return 0;

  root = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_GETITEMESTIMATE,
      MAILACTIVESYNC_GETITEMESTIMATE_GET_ITEM_ESTIMATE);
  if (!check(root != NULL, "multi GetItemEstimate root allocation failed"))
    goto err;
  if (!check(test_node_add_text(root, MAILACTIVESYNC_CP_GETITEMESTIMATE,
      MAILACTIVESYNC_GETITEMESTIMATE_STATUS, "1") ==
      MAILACTIVESYNC_NO_ERROR, "multi GetItemEstimate Status add failed"))
    goto err;

  response = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_GETITEMESTIMATE,
      MAILACTIVESYNC_GETITEMESTIMATE_RESPONSE);
  collection = mailactivesync_wbxml_node_new(
      MAILACTIVESYNC_CP_GETITEMESTIMATE,
      MAILACTIVESYNC_GETITEMESTIMATE_COLLECTION);
  if (!check((response != NULL) && (collection != NULL),
      "multi GetItemEstimate first response allocation failed"))
    goto err;
  if (!check(test_node_add_text(response, MAILACTIVESYNC_CP_GETITEMESTIMATE,
      MAILACTIVESYNC_GETITEMESTIMATE_STATUS, "1") ==
      MAILACTIVESYNC_NO_ERROR,
      "multi GetItemEstimate first response Status add failed"))
    goto err;
  if (!check(test_node_add_text(collection,
      MAILACTIVESYNC_CP_GETITEMESTIMATE,
      MAILACTIVESYNC_GETITEMESTIMATE_COLLECTION_ID, "inbox") ==
      MAILACTIVESYNC_NO_ERROR,
      "multi GetItemEstimate first CollectionId add failed"))
    goto err;
  if (!check(test_node_add_text(collection,
      MAILACTIVESYNC_CP_GETITEMESTIMATE,
      MAILACTIVESYNC_GETITEMESTIMATE_CLASS, "Email") ==
      MAILACTIVESYNC_NO_ERROR,
      "multi GetItemEstimate first Class add failed"))
    goto err;
  if (!check(test_node_add_text(collection,
      MAILACTIVESYNC_CP_GETITEMESTIMATE,
      MAILACTIVESYNC_GETITEMESTIMATE_ESTIMATE, "12") ==
      MAILACTIVESYNC_NO_ERROR,
      "multi GetItemEstimate first Estimate add failed"))
    goto err;
  if (!check(mailactivesync_wbxml_node_add_child(response, collection) ==
      MAILACTIVESYNC_NO_ERROR,
      "multi GetItemEstimate first collection append failed"))
    goto err;
  collection = NULL;
  if (!check(mailactivesync_wbxml_node_add_child(root, response) ==
      MAILACTIVESYNC_NO_ERROR,
      "multi GetItemEstimate first response append failed"))
    goto err;
  response = NULL;

  response = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_GETITEMESTIMATE,
      MAILACTIVESYNC_GETITEMESTIMATE_RESPONSE);
  collection = mailactivesync_wbxml_node_new(
      MAILACTIVESYNC_CP_GETITEMESTIMATE,
      MAILACTIVESYNC_GETITEMESTIMATE_COLLECTION);
  if (!check((response != NULL) && (collection != NULL),
      "multi GetItemEstimate second response allocation failed"))
    goto err;
  if (!check(test_node_add_text(response, MAILACTIVESYNC_CP_GETITEMESTIMATE,
      MAILACTIVESYNC_GETITEMESTIMATE_STATUS, "3") ==
      MAILACTIVESYNC_NO_ERROR,
      "multi GetItemEstimate second response Status add failed"))
    goto err;
  if (!check(test_node_add_text(collection,
      MAILACTIVESYNC_CP_GETITEMESTIMATE,
      MAILACTIVESYNC_GETITEMESTIMATE_COLLECTION_ID, "sent") ==
      MAILACTIVESYNC_NO_ERROR,
      "multi GetItemEstimate second CollectionId add failed"))
    goto err;
  if (!check(test_node_add_text(collection,
      MAILACTIVESYNC_CP_GETITEMESTIMATE,
      MAILACTIVESYNC_GETITEMESTIMATE_CLASS, "Email") ==
      MAILACTIVESYNC_NO_ERROR,
      "multi GetItemEstimate second Class add failed"))
    goto err;
  if (!check(test_node_add_text(collection,
      MAILACTIVESYNC_CP_GETITEMESTIMATE,
      MAILACTIVESYNC_GETITEMESTIMATE_ESTIMATE, "0") ==
      MAILACTIVESYNC_NO_ERROR,
      "multi GetItemEstimate second Estimate add failed"))
    goto err;
  if (!check(mailactivesync_wbxml_node_add_child(response, collection) ==
      MAILACTIVESYNC_NO_ERROR,
      "multi GetItemEstimate second collection append failed"))
    goto err;
  collection = NULL;
  if (!check(mailactivesync_wbxml_node_add_child(root, response) ==
      MAILACTIVESYNC_NO_ERROR,
      "multi GetItemEstimate second response append failed"))
    goto err;
  response = NULL;
  if (!check(fake_context_set_response_body(context, root) ==
      MAILACTIVESYNC_NO_ERROR,
      "multi GetItemEstimate response encode failed"))
    goto err;
  mailactivesync_wbxml_node_free(root);
  root = NULL;

  request_1.collection_id = "inbox";
  request_1.sync_key = "111";
  request_1.collection_class = "Email";
  request_1.has_filter_type = 1;
  request_1.filter_type = 2;
  request_2.collection_id = "sent";
  request_2.sync_key = "222";
  request_2.collection_class = "Email";
  request_2.has_conversation_mode = 1;
  request_2.conversation_mode = 1;
  requests = clist_new();
  if (!check(requests != NULL,
      "multi GetItemEstimate request list allocation failed"))
    goto err;
  if (!check((clist_append(requests, &request_1) == 0) &&
      (clist_append(requests, &request_2) == 0),
      "multi GetItemEstimate request list append failed"))
    goto err;

  r = mailactivesync_get_item_estimate_multi(session, requests, &result);
  if (!check(r == MAILACTIVESYNC_NO_ERROR,
      "multi GetItemEstimate command failed"))
    goto err;
  if (!check((result != NULL) && (result->status == 1) &&
      (result->collection_status == 1) && (result->estimate == 12) &&
      (result->collections != NULL) &&
      (clist_count(result->collections) == 2),
      "multi GetItemEstimate result summary mismatch"))
    goto err;
  parsed_collection = clist_content(clist_begin(result->collections));
  if (!check((parsed_collection != NULL) &&
      str_equal(parsed_collection->collection_id, "inbox") &&
      str_equal(parsed_collection->collection_class, "Email") &&
      (parsed_collection->status == 1) &&
      (parsed_collection->estimate == 12),
      "multi GetItemEstimate first parsed collection mismatch"))
    goto err;
  parsed_collection = mailactivesync_get_item_estimate_result_collection(
      result, "inbox");
  if (!check((parsed_collection != NULL) &&
      (parsed_collection->estimate == 12) &&
      (mailactivesync_get_item_estimate_result_collection_status_to_error(
          result, "inbox") == MAILACTIVESYNC_NO_ERROR),
      "multi GetItemEstimate first collection helper mismatch"))
    goto err;
  parsed_collection = clist_content(clist_next(
      clist_begin(result->collections)));
  if (!check((parsed_collection != NULL) &&
      str_equal(parsed_collection->collection_id, "sent") &&
      str_equal(parsed_collection->collection_class, "Email") &&
      (parsed_collection->status == 3) &&
      (parsed_collection->estimate == 0),
      "multi GetItemEstimate second parsed collection mismatch"))
    goto err;
  parsed_collection = mailactivesync_get_item_estimate_result_collection(
      result, "sent");
  if (!check((parsed_collection != NULL) &&
      (mailactivesync_get_item_estimate_result_collection_status_to_error(
          result, "sent") == MAILACTIVESYNC_ERROR_INVALID_SYNC_KEY),
      "multi GetItemEstimate second collection status helper mismatch"))
    goto err;
  if (!check((mailactivesync_get_item_estimate_result_collection(result,
      "missing") == NULL) &&
      (mailactivesync_get_item_estimate_result_collection_status_to_error(
      result, "missing") == MAILACTIVESYNC_ERROR_PROTOCOL),
      "multi GetItemEstimate missing collection helper mismatch"))
    goto err;

  if (!check(mailactivesync_wbxml_decode(context->last_request->body,
      context->last_request->body_len, &request_document) ==
      MAILACTIVESYNC_NO_ERROR, "multi GetItemEstimate request decode failed"))
    goto err;
  request_collections = test_node_child(request_document->root,
      MAILACTIVESYNC_CP_GETITEMESTIMATE,
      MAILACTIVESYNC_GETITEMESTIMATE_COLLECTIONS);
  request_collection_1 = test_node_child_at(request_collections,
      MAILACTIVESYNC_CP_GETITEMESTIMATE,
      MAILACTIVESYNC_GETITEMESTIMATE_COLLECTION, 0);
  request_collection_2 = test_node_child_at(request_collections,
      MAILACTIVESYNC_CP_GETITEMESTIMATE,
      MAILACTIVESYNC_GETITEMESTIMATE_COLLECTION, 1);
  if (!check((request_collection_1 != NULL) &&
      (request_collection_2 != NULL) &&
      (test_node_child_at(request_collections,
          MAILACTIVESYNC_CP_GETITEMESTIMATE,
          MAILACTIVESYNC_GETITEMESTIMATE_COLLECTION, 2) == NULL),
      "multi GetItemEstimate request collection count mismatch"))
    goto err;
  if (!check(str_equal(test_node_child_text(request_collection_1,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_SYNC_KEY), "111") &&
      str_equal(test_node_child_text(request_collection_1,
          MAILACTIVESYNC_CP_GETITEMESTIMATE,
          MAILACTIVESYNC_GETITEMESTIMATE_COLLECTION_ID), "inbox"),
      "multi GetItemEstimate first request ids mismatch"))
    goto err;
  request_options = test_node_child(request_collection_1,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_OPTIONS);
  if (!check(str_equal(test_node_child_text(request_options,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_CLASS), "Email") &&
      str_equal(test_node_child_text(request_options,
          MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_FILTER_TYPE),
          "2"),
      "multi GetItemEstimate first request options mismatch"))
    goto err;
  if (!check(str_equal(test_node_child_text(request_collection_2,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_SYNC_KEY), "222") &&
      str_equal(test_node_child_text(request_collection_2,
          MAILACTIVESYNC_CP_GETITEMESTIMATE,
          MAILACTIVESYNC_GETITEMESTIMATE_COLLECTION_ID), "sent") &&
      str_equal(test_node_child_text(request_collection_2,
          MAILACTIVESYNC_CP_AIRSYNC,
          MAILACTIVESYNC_AIRSYNC_CONVERSATION_MODE), "1"),
      "multi GetItemEstimate second request mismatch"))
    goto err;

  mailactivesync_wbxml_document_free(request_document);
  mailactivesync_get_item_estimate_result_free(result);
  clist_free(requests);
  mailactivesync_free(session);
  return 1;

 err:
  mailactivesync_wbxml_document_free(request_document);
  mailactivesync_wbxml_node_free(collection);
  mailactivesync_wbxml_node_free(response);
  mailactivesync_wbxml_node_free(root);
  mailactivesync_get_item_estimate_result_free(result);
  clist_free(requests);
  mailactivesync_free(session);
  return 0;
}

static int test_item_operations_fetch_success(void)
{
  static const unsigned char mime[] =
      "Subject: Fetch\r\nFrom: sender@example.com\r\n\r\nFetched body\r\n";
  mailactivesync * session;
  struct fake_context * context;
  struct mailactivesync_item * item;
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_node * response;
  struct mailactivesync_wbxml_node * fetch;
  struct mailactivesync_wbxml_node * properties;
  struct mailactivesync_wbxml_node * body;
  struct mailactivesync_wbxml_document * request_document;
  struct mailactivesync_wbxml_node * request_fetch;
  struct mailactivesync_wbxml_node * request_options;
  struct mailactivesync_wbxml_node * request_body_pref;
  int r;

  session = NULL;
  context = NULL;
  item = NULL;
  root = NULL;
  response = NULL;
  fetch = NULL;
  properties = NULL;
  body = NULL;
  request_document = NULL;

  if (!check(setup_oauth_session(&session, &context) ==
      MAILACTIVESYNC_NO_ERROR, "setup OAuth session failed"))
    return 0;

  root = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_ITEM_OPERATIONS);
  response = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_RESPONSE);
  fetch = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_FETCH);
  properties = mailactivesync_wbxml_node_new(
      MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_PROPERTIES);
  body = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_BODY);
  if (!check((root != NULL) && (response != NULL) && (fetch != NULL) &&
      (properties != NULL) && (body != NULL),
      "ItemOperations response allocation failed"))
    goto err;
  if (!check(test_node_add_text(root, MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_STATUS, "1") == MAILACTIVESYNC_NO_ERROR,
      "ItemOperations Status add failed"))
    goto err;
  if (!check(test_node_add_text(fetch, MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_STATUS, "1") == MAILACTIVESYNC_NO_ERROR,
      "ItemOperations Fetch Status add failed"))
    goto err;
  if (!check(test_node_add_text(body, MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_TYPE, "4") == MAILACTIVESYNC_NO_ERROR,
      "ItemOperations body type add failed"))
    goto err;
  if (!check(test_node_add_opaque(body, MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_DATA, mime, sizeof(mime) - 1) ==
      MAILACTIVESYNC_NO_ERROR, "ItemOperations body data add failed"))
    goto err;
  if (!check(mailactivesync_wbxml_node_add_child(properties, body) ==
      MAILACTIVESYNC_NO_ERROR, "ItemOperations body append failed"))
    goto err;
  body = NULL;
  if (!check(mailactivesync_wbxml_node_add_child(fetch, properties) ==
      MAILACTIVESYNC_NO_ERROR, "ItemOperations properties append failed"))
    goto err;
  properties = NULL;
  if (!check(mailactivesync_wbxml_node_add_child(response, fetch) ==
      MAILACTIVESYNC_NO_ERROR, "ItemOperations fetch append failed"))
    goto err;
  fetch = NULL;
  if (!check(mailactivesync_wbxml_node_add_child(root, response) ==
      MAILACTIVESYNC_NO_ERROR, "ItemOperations response append failed"))
    goto err;
  response = NULL;
  if (!check(fake_context_set_response_body(context, root) ==
      MAILACTIVESYNC_NO_ERROR, "ItemOperations response encode failed"))
    goto err;
  mailactivesync_wbxml_node_free(root);
  root = NULL;

  r = mailactivesync_item_operations_fetch(session, "7", "msg-1", &item);
  if (!check(r == MAILACTIVESYNC_NO_ERROR,
      "ItemOperations Fetch command failed"))
    goto err;
  if (!check((item != NULL) && (item->server_id != NULL) &&
      (strcmp(item->server_id, "msg-1") == 0) &&
      (item->body != NULL) &&
      (item->body->type == MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_MIME) &&
      (item->mime_len == sizeof(mime) - 1) &&
      (memcmp(item->mime, mime, sizeof(mime) - 1) == 0),
      "ItemOperations Fetch item mismatch"))
    goto err;
  if (!check(!mailactivesync_item_needs_body_fetch(item) &&
      !mailactivesync_airsyncbase_body_needs_fetch(item->body),
      "ItemOperations full Fetch unexpectedly needed body refetch"))
    goto err;

  if (!check(mailactivesync_wbxml_decode(context->last_request->body,
      context->last_request->body_len, &request_document) ==
      MAILACTIVESYNC_NO_ERROR, "ItemOperations request decode failed"))
    goto err;
  request_fetch = test_node_child(request_document->root,
      MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_FETCH);
  if (!check(strcmp(test_node_child_text(request_fetch,
      MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_STORE), "Mailbox") == 0,
      "ItemOperations request Store mismatch"))
    goto err;
  if (!check(strcmp(test_node_child_text(request_fetch,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_COLLECTION_ID),
      "7") == 0, "ItemOperations request CollectionId mismatch"))
    goto err;
  if (!check(strcmp(test_node_child_text(request_fetch,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_SERVER_ID),
      "msg-1") == 0, "ItemOperations request ServerId mismatch"))
    goto err;
  request_options = test_node_child(request_fetch,
      MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_OPTIONS);
  request_body_pref = test_node_child(request_options,
      MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_BODY_PREFERENCE);
  if (!check(strcmp(test_node_child_text(request_body_pref,
      MAILACTIVESYNC_CP_AIRSYNCBASE, MAILACTIVESYNC_AIRSYNCBASE_TYPE),
      "4") == 0, "ItemOperations request body type mismatch"))
    goto err;

  mailactivesync_wbxml_document_free(request_document);
  request_document = NULL;
  mailactivesync_item_free(item);
  item = NULL;

  root = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_ITEM_OPERATIONS);
  response = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_RESPONSE);
  fetch = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_FETCH);
  if (!check((root != NULL) && (response != NULL) && (fetch != NULL),
      "ItemOperations error response allocation failed"))
    goto err;
  if (!check(test_node_add_text(root, MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_STATUS, "1") == MAILACTIVESYNC_NO_ERROR,
      "ItemOperations error root Status add failed"))
    goto err;
  if (!check(test_node_add_text(fetch, MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_STATUS, "15") == MAILACTIVESYNC_NO_ERROR,
      "ItemOperations error Fetch Status add failed"))
    goto err;
  if (!check(mailactivesync_wbxml_node_add_child(response, fetch) ==
      MAILACTIVESYNC_NO_ERROR, "ItemOperations error fetch append failed"))
    goto err;
  fetch = NULL;
  if (!check(mailactivesync_wbxml_node_add_child(root, response) ==
      MAILACTIVESYNC_NO_ERROR, "ItemOperations error response append failed"))
    goto err;
  response = NULL;
  if (!check(fake_context_set_response_body(context, root) ==
      MAILACTIVESYNC_NO_ERROR, "ItemOperations error response encode failed"))
    goto err;
  mailactivesync_wbxml_node_free(root);
  root = NULL;

  r = mailactivesync_item_operations_fetch(session, "7", "msg-1", &item);
  if (!check((r == MAILACTIVESYNC_ERROR_PROTOCOL) && (item != NULL) &&
      (item->status == 15) && str_equal(item->collection_id, "7") &&
      str_equal(item->server_id, "msg-1"),
      "ItemOperations Fetch error item status was not preserved"))
    goto err;
  mailactivesync_item_free(item);
  item = NULL;

  mailactivesync_free(session);
  return 1;

 err:
  mailactivesync_wbxml_document_free(request_document);
  mailactivesync_wbxml_node_free(body);
  mailactivesync_wbxml_node_free(properties);
  mailactivesync_wbxml_node_free(fetch);
  mailactivesync_wbxml_node_free(response);
  mailactivesync_wbxml_node_free(root);
  mailactivesync_item_free(item);
  mailactivesync_free(session);
  return 0;
}

static int test_item_operations_fetch_multi_success(void)
{
  static const unsigned char mime[] =
      "Subject: Multi Fetch\r\n\r\nFirst body\r\n";
  static const unsigned char body_part_data[] = "Second body part";
  mailactivesync * session;
  struct fake_context * context;
  struct mailactivesync_item_operations_fetch_request request_items[2];
  struct mailactivesync_item_operations_fetch_result * result;
  struct mailactivesync_item * item;
  struct mailactivesync_body_part * parsed_body_part;
  clist * requests;
  clistiter * cur;
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_node * response;
  struct mailactivesync_wbxml_node * fetch;
  struct mailactivesync_wbxml_node * properties;
  struct mailactivesync_wbxml_node * body;
  struct mailactivesync_wbxml_node * body_part_node;
  struct mailactivesync_wbxml_document * request_document;
  struct mailactivesync_wbxml_node * request_fetch;
  struct mailactivesync_wbxml_node * request_options;
  struct mailactivesync_wbxml_node * request_preference;
  int r;

  session = NULL;
  context = NULL;
  result = NULL;
  requests = NULL;
  root = NULL;
  response = NULL;
  fetch = NULL;
  properties = NULL;
  body = NULL;
  body_part_node = NULL;
  request_document = NULL;

  if (!check(setup_oauth_session(&session, &context) ==
      MAILACTIVESYNC_NO_ERROR, "setup OAuth session failed"))
    return 0;

  root = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_ITEM_OPERATIONS);
  response = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_RESPONSE);
  if (!check((root != NULL) && (response != NULL),
      "ItemOperations multi response allocation failed"))
    goto err;
  if (!check(test_node_add_text(root, MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_STATUS, "1") == MAILACTIVESYNC_NO_ERROR,
      "ItemOperations multi Status add failed"))
    goto err;

  fetch = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_FETCH);
  properties = mailactivesync_wbxml_node_new(
      MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_PROPERTIES);
  body = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_BODY);
  if (!check((fetch != NULL) && (properties != NULL) && (body != NULL),
      "ItemOperations multi first fetch allocation failed"))
    goto err;
  if (!check(test_node_add_text(fetch, MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_STATUS, "1") == MAILACTIVESYNC_NO_ERROR,
      "ItemOperations multi first status add failed"))
    goto err;
  if (!check(test_node_add_text(body, MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_TYPE, "4") == MAILACTIVESYNC_NO_ERROR,
      "ItemOperations multi first body type add failed"))
    goto err;
  if (!check(test_node_add_opaque(body, MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_DATA, mime, sizeof(mime) - 1) ==
      MAILACTIVESYNC_NO_ERROR,
      "ItemOperations multi first body data add failed"))
    goto err;
  if (!check(mailactivesync_wbxml_node_add_child(properties, body) ==
      MAILACTIVESYNC_NO_ERROR,
      "ItemOperations multi first body append failed"))
    goto err;
  body = NULL;
  if (!check(mailactivesync_wbxml_node_add_child(fetch, properties) ==
      MAILACTIVESYNC_NO_ERROR,
      "ItemOperations multi first properties append failed"))
    goto err;
  properties = NULL;
  if (!check(mailactivesync_wbxml_node_add_child(response, fetch) ==
      MAILACTIVESYNC_NO_ERROR,
      "ItemOperations multi first fetch append failed"))
    goto err;
  fetch = NULL;

  fetch = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_FETCH);
  properties = mailactivesync_wbxml_node_new(
      MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_PROPERTIES);
  body_part_node = mailactivesync_wbxml_node_new(
      MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_BODY_PART);
  if (!check((fetch != NULL) && (properties != NULL) &&
      (body_part_node != NULL),
      "ItemOperations multi second fetch allocation failed"))
    goto err;
  if (!check(test_node_add_text(fetch, MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_STATUS, "1") == MAILACTIVESYNC_NO_ERROR,
      "ItemOperations multi second status add failed"))
    goto err;
  if (!check(test_node_add_text(body_part_node,
      MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_STATUS, "1") == MAILACTIVESYNC_NO_ERROR,
      "ItemOperations multi body part status add failed"))
    goto err;
  if (!check(test_node_add_text(body_part_node,
      MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_TYPE, "2") == MAILACTIVESYNC_NO_ERROR,
      "ItemOperations multi body part type add failed"))
    goto err;
  if (!check(test_node_add_opaque(body_part_node,
      MAILACTIVESYNC_CP_AIRSYNCBASE, MAILACTIVESYNC_AIRSYNCBASE_DATA,
      body_part_data,
      sizeof(body_part_data) - 1) == MAILACTIVESYNC_NO_ERROR,
      "ItemOperations multi body part data add failed"))
    goto err;
  if (!check(mailactivesync_wbxml_node_add_child(properties,
      body_part_node) ==
      MAILACTIVESYNC_NO_ERROR,
      "ItemOperations multi body part append failed"))
    goto err;
  body_part_node = NULL;
  if (!check(mailactivesync_wbxml_node_add_child(fetch, properties) ==
      MAILACTIVESYNC_NO_ERROR,
      "ItemOperations multi second properties append failed"))
    goto err;
  properties = NULL;
  if (!check(mailactivesync_wbxml_node_add_child(response, fetch) ==
      MAILACTIVESYNC_NO_ERROR,
      "ItemOperations multi second fetch append failed"))
    goto err;
  fetch = NULL;

  if (!check(mailactivesync_wbxml_node_add_child(root, response) ==
      MAILACTIVESYNC_NO_ERROR,
      "ItemOperations multi response append failed"))
    goto err;
  response = NULL;
  if (!check(fake_context_set_response_body(context, root) ==
      MAILACTIVESYNC_NO_ERROR,
      "ItemOperations multi response encode failed"))
    goto err;
  mailactivesync_wbxml_node_free(root);
  root = NULL;

  memset(request_items, 0, sizeof(request_items));
  request_items[0].collection_id = "7";
  request_items[0].server_id = "msg-1";
  request_items[0].truncation_size = 111;
  request_items[1].collection_id = "8";
  request_items[1].server_id = "msg-2";
  request_items[1].body_part_type = MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_HTML;
  request_items[1].truncation_size = 222;
  requests = clist_new();
  if (!check(requests != NULL, "ItemOperations multi request list failed"))
    goto err;
  if (!check(clist_append(requests, &request_items[0]) == 0,
      "ItemOperations multi first request append failed"))
    goto err;
  if (!check(clist_append(requests, &request_items[1]) == 0,
      "ItemOperations multi second request append failed"))
    goto err;

  r = mailactivesync_item_operations_fetch_multi(session, requests, &result);
  if (!check(r == MAILACTIVESYNC_NO_ERROR,
      "ItemOperations multi Fetch command failed"))
    goto err;
  if (!check((result != NULL) && (result->status == 1) &&
      (clist_count(result->items) == 2),
      "ItemOperations multi result summary mismatch"))
    goto err;
  cur = clist_begin(result->items);
  item = clist_content(cur);
  if (!check((item != NULL) && (item->status == 1) &&
      str_equal(item->collection_id, "7") &&
      str_equal(item->server_id, "msg-1") &&
      (item->mime_len == sizeof(mime) - 1) &&
      (memcmp(item->mime, mime, sizeof(mime) - 1) == 0),
      "ItemOperations multi first parsed item mismatch"))
    goto err;
  cur = clist_next(cur);
  item = clist_content(cur);
  parsed_body_part = clist_content(clist_begin(item->body_parts));
  if (!check((item != NULL) && (item->status == 1) &&
      str_equal(item->collection_id, "8") &&
      str_equal(item->server_id, "msg-2") &&
      (parsed_body_part != NULL) && (parsed_body_part->type == 2) &&
      (parsed_body_part->data_len == sizeof(body_part_data) - 1) &&
      (memcmp(parsed_body_part->data, body_part_data,
          sizeof(body_part_data) - 1) == 0),
      "ItemOperations multi second parsed item mismatch"))
    goto err;

  if (!check(mailactivesync_wbxml_decode(context->last_request->body,
      context->last_request->body_len, &request_document) ==
      MAILACTIVESYNC_NO_ERROR,
      "ItemOperations multi request decode failed"))
    goto err;
  request_fetch = test_node_child_at(request_document->root,
      MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_FETCH, 0);
  if (!check(str_equal(test_node_child_text(request_fetch,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_COLLECTION_ID),
      "7") && str_equal(test_node_child_text(request_fetch,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_SERVER_ID),
      "msg-1"), "ItemOperations multi first request identity mismatch"))
    goto err;
  request_options = test_node_child(request_fetch,
      MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_OPTIONS);
  request_preference = test_node_child(request_options,
      MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_BODY_PREFERENCE);
  if (!check(str_equal(test_node_child_text(request_preference,
      MAILACTIVESYNC_CP_AIRSYNCBASE, MAILACTIVESYNC_AIRSYNCBASE_TYPE),
      "4") && str_equal(test_node_child_text(request_preference,
      MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_TRUNCATION_SIZE), "111"),
      "ItemOperations multi first request preference mismatch"))
    goto err;
  request_fetch = test_node_child_at(request_document->root,
      MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_FETCH, 1);
  if (!check(str_equal(test_node_child_text(request_fetch,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_COLLECTION_ID),
      "8") && str_equal(test_node_child_text(request_fetch,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_SERVER_ID),
      "msg-2"), "ItemOperations multi second request identity mismatch"))
    goto err;
  request_options = test_node_child(request_fetch,
      MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_OPTIONS);
  request_preference = test_node_child(request_options,
      MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_BODY_PART_PREFERENCE);
  if (!check(str_equal(test_node_child_text(request_preference,
      MAILACTIVESYNC_CP_AIRSYNCBASE, MAILACTIVESYNC_AIRSYNCBASE_TYPE),
      "2") && str_equal(test_node_child_text(request_preference,
      MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_TRUNCATION_SIZE), "222"),
      "ItemOperations multi second request preference mismatch"))
    goto err;

  mailactivesync_wbxml_document_free(request_document);
  mailactivesync_item_operations_fetch_result_free(result);
  clist_free(requests);
  mailactivesync_free(session);
  return 1;

 err:
  mailactivesync_wbxml_document_free(request_document);
  mailactivesync_wbxml_node_free(body_part_node);
  mailactivesync_wbxml_node_free(body);
  mailactivesync_wbxml_node_free(properties);
  mailactivesync_wbxml_node_free(fetch);
  mailactivesync_wbxml_node_free(response);
  mailactivesync_wbxml_node_free(root);
  mailactivesync_item_operations_fetch_result_free(result);
  clist_free(requests);
  mailactivesync_free(session);
  return 0;
}

static int test_item_operations_fetch_body_part_success(void)
{
  static const unsigned char body_part_data[] =
      "<html><body>Body part</body></html>";
  mailactivesync * session;
  struct fake_context * context;
  struct mailactivesync_item * item;
  struct mailactivesync_body_part * body_part;
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_node * response;
  struct mailactivesync_wbxml_node * fetch;
  struct mailactivesync_wbxml_node * properties;
  struct mailactivesync_wbxml_node * response_body_part;
  struct mailactivesync_wbxml_document * request_document;
  struct mailactivesync_wbxml_node * request_fetch;
  struct mailactivesync_wbxml_node * request_options;
  struct mailactivesync_wbxml_node * request_body_part_pref;
  int r;

  session = NULL;
  context = NULL;
  item = NULL;
  body_part = NULL;
  root = NULL;
  response = NULL;
  fetch = NULL;
  properties = NULL;
  response_body_part = NULL;
  request_document = NULL;

  if (!check(setup_oauth_session(&session, &context) ==
      MAILACTIVESYNC_NO_ERROR, "setup OAuth session failed"))
    return 0;

  root = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_ITEM_OPERATIONS);
  response = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_RESPONSE);
  fetch = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_FETCH);
  properties = mailactivesync_wbxml_node_new(
      MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_PROPERTIES);
  response_body_part = mailactivesync_wbxml_node_new(
      MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_BODY_PART);
  if (!check((root != NULL) && (response != NULL) && (fetch != NULL) &&
      (properties != NULL) && (response_body_part != NULL),
      "ItemOperations BodyPart response allocation failed"))
    goto err;
  if (!check(test_node_add_text(root, MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_STATUS, "1") == MAILACTIVESYNC_NO_ERROR,
      "ItemOperations BodyPart Status add failed"))
    goto err;
  if (!check(test_node_add_text(fetch, MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_STATUS, "1") == MAILACTIVESYNC_NO_ERROR,
      "ItemOperations BodyPart Fetch Status add failed"))
    goto err;
  if (!check(test_node_add_text(response_body_part,
      MAILACTIVESYNC_CP_AIRSYNCBASE, MAILACTIVESYNC_AIRSYNCBASE_STATUS,
      "1") == MAILACTIVESYNC_NO_ERROR,
      "ItemOperations BodyPart nested status add failed"))
    goto err;
  if (!check(test_node_add_text(response_body_part,
      MAILACTIVESYNC_CP_AIRSYNCBASE, MAILACTIVESYNC_AIRSYNCBASE_TYPE,
      "2") == MAILACTIVESYNC_NO_ERROR,
      "ItemOperations BodyPart type add failed"))
    goto err;
  if (!check(test_node_add_text(response_body_part,
      MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_ESTIMATED_DATA_SIZE, "64") ==
      MAILACTIVESYNC_NO_ERROR,
      "ItemOperations BodyPart estimated size add failed"))
    goto err;
  if (!check(test_node_add_text(response_body_part,
      MAILACTIVESYNC_CP_AIRSYNCBASE, MAILACTIVESYNC_AIRSYNCBASE_TRUNCATED,
      "1") == MAILACTIVESYNC_NO_ERROR,
      "ItemOperations BodyPart truncated add failed"))
    goto err;
  if (!check(test_node_add_opaque(response_body_part,
      MAILACTIVESYNC_CP_AIRSYNCBASE, MAILACTIVESYNC_AIRSYNCBASE_DATA,
      body_part_data, sizeof(body_part_data) - 1) ==
      MAILACTIVESYNC_NO_ERROR,
      "ItemOperations BodyPart data add failed"))
    goto err;
  if (!check(test_node_add_text(response_body_part,
      MAILACTIVESYNC_CP_AIRSYNCBASE, MAILACTIVESYNC_AIRSYNCBASE_PREVIEW,
      "Body part") == MAILACTIVESYNC_NO_ERROR,
      "ItemOperations BodyPart preview add failed"))
    goto err;
  if (!check(mailactivesync_wbxml_node_add_child(properties,
      response_body_part) == MAILACTIVESYNC_NO_ERROR,
      "ItemOperations BodyPart append failed"))
    goto err;
  response_body_part = NULL;
  if (!check(mailactivesync_wbxml_node_add_child(fetch, properties) ==
      MAILACTIVESYNC_NO_ERROR,
      "ItemOperations BodyPart properties append failed"))
    goto err;
  properties = NULL;
  if (!check(mailactivesync_wbxml_node_add_child(response, fetch) ==
      MAILACTIVESYNC_NO_ERROR,
      "ItemOperations BodyPart fetch append failed"))
    goto err;
  fetch = NULL;
  if (!check(mailactivesync_wbxml_node_add_child(root, response) ==
      MAILACTIVESYNC_NO_ERROR,
      "ItemOperations BodyPart response append failed"))
    goto err;
  response = NULL;
  if (!check(fake_context_set_response_body(context, root) ==
      MAILACTIVESYNC_NO_ERROR,
      "ItemOperations BodyPart response encode failed"))
    goto err;
  mailactivesync_wbxml_node_free(root);
  root = NULL;

  r = mailactivesync_item_operations_fetch_body_part(session, "7", "msg-1",
      MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_HTML, 4096, &item);
  if (!check(r == MAILACTIVESYNC_NO_ERROR,
      "ItemOperations BodyPart Fetch command failed"))
    goto err;
  if (!check((item != NULL) && str_equal(item->server_id, "msg-1") &&
      (item->body_parts != NULL) && (clist_count(item->body_parts) == 1),
      "ItemOperations BodyPart item mismatch"))
    goto err;
  body_part = clist_content(clist_begin(item->body_parts));
  if (!check((body_part != NULL) && (body_part->status == 1) &&
      (body_part->type == MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_HTML) &&
      (body_part->estimated_data_size == 64) &&
      (body_part->truncated == 1) &&
      str_equal(body_part->preview, "Body part") &&
      (body_part->data_len == sizeof(body_part_data) - 1) &&
      (memcmp(body_part->data, body_part_data,
          sizeof(body_part_data) - 1) == 0),
      "ItemOperations BodyPart parsed data mismatch"))
    goto err;
  if (!check(mailactivesync_body_part_needs_fetch(body_part) &&
      mailactivesync_item_needs_body_fetch(item),
      "ItemOperations truncated BodyPart did not need body refetch"))
    goto err;

  if (!check(mailactivesync_wbxml_decode(context->last_request->body,
      context->last_request->body_len, &request_document) ==
      MAILACTIVESYNC_NO_ERROR,
      "ItemOperations BodyPart request decode failed"))
    goto err;
  request_fetch = test_node_child(request_document->root,
      MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_FETCH);
  request_options = test_node_child(request_fetch,
      MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_OPTIONS);
  request_body_part_pref = test_node_child(request_options,
      MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_BODY_PART_PREFERENCE);
  if (!check(request_body_part_pref != NULL,
      "ItemOperations request BodyPartPreference missing"))
    goto err;
  if (!check(test_node_child(request_options, MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_BODY_PREFERENCE) == NULL,
      "ItemOperations request unexpectedly included BodyPreference"))
    goto err;
  if (!check(strcmp(test_node_child_text(request_body_part_pref,
      MAILACTIVESYNC_CP_AIRSYNCBASE, MAILACTIVESYNC_AIRSYNCBASE_TYPE),
      "2") == 0,
      "ItemOperations request BodyPartPreference type mismatch"))
    goto err;
  if (!check(strcmp(test_node_child_text(request_body_part_pref,
      MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_TRUNCATION_SIZE), "4096") == 0,
      "ItemOperations request BodyPartPreference truncation mismatch"))
    goto err;

  mailactivesync_wbxml_document_free(request_document);
  request_document = NULL;
  mailactivesync_item_free(item);
  item = NULL;

  root = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_ITEM_OPERATIONS);
  response = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_RESPONSE);
  fetch = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_FETCH);
  if (!check((root != NULL) && (response != NULL) && (fetch != NULL),
      "ItemOperations BodyPart error response allocation failed"))
    goto err;
  if (!check(test_node_add_text(root, MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_STATUS, "1") == MAILACTIVESYNC_NO_ERROR,
      "ItemOperations BodyPart error root Status add failed"))
    goto err;
  if (!check(test_node_add_text(fetch, MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_STATUS, "15") == MAILACTIVESYNC_NO_ERROR,
      "ItemOperations BodyPart error Fetch Status add failed"))
    goto err;
  if (!check(mailactivesync_wbxml_node_add_child(response, fetch) ==
      MAILACTIVESYNC_NO_ERROR,
      "ItemOperations BodyPart error fetch append failed"))
    goto err;
  fetch = NULL;
  if (!check(mailactivesync_wbxml_node_add_child(root, response) ==
      MAILACTIVESYNC_NO_ERROR,
      "ItemOperations BodyPart error response append failed"))
    goto err;
  response = NULL;
  if (!check(fake_context_set_response_body(context, root) ==
      MAILACTIVESYNC_NO_ERROR,
      "ItemOperations BodyPart error response encode failed"))
    goto err;
  mailactivesync_wbxml_node_free(root);
  root = NULL;

  r = mailactivesync_item_operations_fetch_body_part(session, "7", "msg-1",
      MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_HTML, 4096, &item);
  if (!check((r == MAILACTIVESYNC_ERROR_PROTOCOL) && (item != NULL) &&
      (item->status == 15) && str_equal(item->collection_id, "7") &&
      str_equal(item->server_id, "msg-1"),
      "ItemOperations BodyPart error item status was not preserved"))
    goto err;
  mailactivesync_item_free(item);
  item = NULL;

  if (!check(mailactivesync_set_protocol_version(session, "14.0") ==
      MAILACTIVESYNC_NO_ERROR,
      "ItemOperations BodyPart protocol setup failed"))
    goto err;
  r = mailactivesync_item_operations_fetch_body_part(session, "7", "msg-1",
      MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_HTML, 4096, &item);
  if (!check((r == MAILACTIVESYNC_ERROR_NOT_IMPLEMENTED) && (item == NULL),
      "ItemOperations BodyPart unsupported protocol did not fail"))
    goto err;

  mailactivesync_free(session);
  return 1;

 err:
  mailactivesync_wbxml_document_free(request_document);
  mailactivesync_wbxml_node_free(response_body_part);
  mailactivesync_wbxml_node_free(properties);
  mailactivesync_wbxml_node_free(fetch);
  mailactivesync_wbxml_node_free(response);
  mailactivesync_wbxml_node_free(root);
  mailactivesync_item_free(item);
  mailactivesync_free(session);
  return 0;
}

static struct mailactivesync_wbxml_node *
item_operations_attachment_response_new(
    const char * status,
    const char * file_reference,
    const char * range,
    const char * total,
    const unsigned char * data,
    size_t data_len)
{
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_node * response;
  struct mailactivesync_wbxml_node * fetch;
  struct mailactivesync_wbxml_node * properties;

  root = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_ITEM_OPERATIONS);
  response = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_RESPONSE);
  fetch = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_FETCH);
  properties = mailactivesync_wbxml_node_new(
      MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_PROPERTIES);
  if ((root == NULL) || (response == NULL) || (fetch == NULL) ||
      (properties == NULL))
    goto err;

  if ((test_node_add_text(root, MAILACTIVESYNC_CP_ITEMOPERATIONS,
          MAILACTIVESYNC_ITEMOPERATIONS_STATUS, "1") !=
          MAILACTIVESYNC_NO_ERROR) ||
      (test_node_add_text(fetch, MAILACTIVESYNC_CP_ITEMOPERATIONS,
          MAILACTIVESYNC_ITEMOPERATIONS_STATUS, status) !=
          MAILACTIVESYNC_NO_ERROR))
    goto err;
  if ((file_reference != NULL) &&
      (test_node_add_text(fetch, MAILACTIVESYNC_CP_AIRSYNCBASE,
          MAILACTIVESYNC_AIRSYNCBASE_FILE_REFERENCE, file_reference) !=
          MAILACTIVESYNC_NO_ERROR))
    goto err;
  if ((range != NULL) &&
      (test_node_add_text(properties, MAILACTIVESYNC_CP_ITEMOPERATIONS,
          MAILACTIVESYNC_ITEMOPERATIONS_RANGE, range) !=
          MAILACTIVESYNC_NO_ERROR))
    goto err;
  if ((total != NULL) &&
      (test_node_add_text(properties, MAILACTIVESYNC_CP_ITEMOPERATIONS,
          MAILACTIVESYNC_ITEMOPERATIONS_TOTAL, total) !=
          MAILACTIVESYNC_NO_ERROR))
    goto err;
  if ((data != NULL) &&
      (test_node_add_opaque(properties, MAILACTIVESYNC_CP_ITEMOPERATIONS,
          MAILACTIVESYNC_ITEMOPERATIONS_DATA, data, data_len) !=
          MAILACTIVESYNC_NO_ERROR))
    goto err;
  if (mailactivesync_wbxml_node_add_child(fetch, properties) !=
      MAILACTIVESYNC_NO_ERROR)
    goto err;
  properties = NULL;
  if (mailactivesync_wbxml_node_add_child(response, fetch) !=
      MAILACTIVESYNC_NO_ERROR)
    goto err;
  fetch = NULL;
  if (mailactivesync_wbxml_node_add_child(root, response) !=
      MAILACTIVESYNC_NO_ERROR)
    goto err;
  response = NULL;

  return root;

 err:
  mailactivesync_wbxml_node_free(properties);
  mailactivesync_wbxml_node_free(fetch);
  mailactivesync_wbxml_node_free(response);
  mailactivesync_wbxml_node_free(root);
  return NULL;
}

static int test_item_operations_fetch_attachment_success_and_status(void)
{
  static const unsigned char data[] = { 0x25, 0x50, 0x44, 0x46, 0x0A };
  mailactivesync * session;
  struct fake_context * context;
  struct mailactivesync_attachment_data * attachment_data;
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_document * request_document;
  struct mailactivesync_wbxml_node * request_fetch;
  struct mailactivesync_wbxml_node * request_options;
  int r;

  session = NULL;
  context = NULL;
  attachment_data = NULL;
  root = NULL;
  request_document = NULL;

  if (!check(setup_oauth_session(&session, &context) ==
      MAILACTIVESYNC_NO_ERROR, "setup OAuth session failed"))
    return 0;

  root = item_operations_attachment_response_new("1", "file-ref-1",
      "0-4", "5", data, sizeof(data));
  if (!check(root != NULL,
      "ItemOperations attachment response allocation failed"))
    goto err;
  if (!check(fake_context_set_response_body(context, root) ==
      MAILACTIVESYNC_NO_ERROR,
      "ItemOperations attachment response encode failed"))
    goto err;
  mailactivesync_wbxml_node_free(root);
  root = NULL;

  r = mailactivesync_item_operations_fetch_attachment(session,
      "file-ref-1", "0-4", &attachment_data);
  if (!check(r == MAILACTIVESYNC_NO_ERROR,
      "ItemOperations attachment Fetch command failed"))
    goto err;
  if (!check(strstr(context->last_request->url,
      "Cmd=ItemOperations") != NULL,
      "ItemOperations attachment command URL mismatch"))
    goto err;
  if (!check(mailactivesync_wbxml_decode(context->last_request->body,
      context->last_request->body_len, &request_document) ==
      MAILACTIVESYNC_NO_ERROR,
      "ItemOperations attachment request decode failed"))
    goto err;
  request_fetch = test_node_child(request_document->root,
      MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_FETCH);
  if (!check(str_equal(test_node_child_text(request_fetch,
      MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_STORE), "Mailbox") &&
      str_equal(test_node_child_text(request_fetch,
          MAILACTIVESYNC_CP_AIRSYNCBASE,
          MAILACTIVESYNC_AIRSYNCBASE_FILE_REFERENCE), "file-ref-1") &&
      (test_node_child(request_fetch, MAILACTIVESYNC_CP_AIRSYNC,
          MAILACTIVESYNC_AIRSYNC_COLLECTION_ID) == NULL) &&
      (test_node_child(request_fetch, MAILACTIVESYNC_CP_AIRSYNC,
          MAILACTIVESYNC_AIRSYNC_SERVER_ID) == NULL),
      "ItemOperations attachment request identity mismatch"))
    goto err;
  request_options = test_node_child(request_fetch,
      MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_OPTIONS);
  if (!check(str_equal(test_node_child_text(request_options,
      MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_RANGE), "0-4"),
      "ItemOperations attachment range request mismatch"))
    goto err;
  if (!check((attachment_data != NULL) &&
      (attachment_data->status == 1) &&
      str_equal(attachment_data->file_reference, "file-ref-1") &&
      str_equal(attachment_data->range, "0-4") &&
      (attachment_data->total == 5) &&
      (attachment_data->data_len == sizeof(data)) &&
      (memcmp(attachment_data->data, data, sizeof(data)) == 0),
      "ItemOperations attachment result mismatch"))
    goto err;
  mailactivesync_wbxml_document_free(request_document);
  request_document = NULL;
  mailactivesync_attachment_data_free(attachment_data);
  attachment_data = NULL;

  root = item_operations_attachment_response_new("15", NULL, NULL, NULL,
      NULL, 0);
  if (!check(root != NULL,
      "ItemOperations attachment error response allocation failed"))
    goto err;
  if (!check(fake_context_set_response_body(context, root) ==
      MAILACTIVESYNC_NO_ERROR,
      "ItemOperations attachment error response encode failed"))
    goto err;
  mailactivesync_wbxml_node_free(root);
  root = NULL;
  r = mailactivesync_item_operations_fetch_attachment(session,
      "file-ref-1", NULL, &attachment_data);
  if (!check((r == MAILACTIVESYNC_ERROR_PROTOCOL) &&
      (attachment_data != NULL) && (attachment_data->status == 15) &&
      str_equal(attachment_data->file_reference, "file-ref-1"),
      "ItemOperations attachment status mapping mismatch"))
    goto err;

  mailactivesync_attachment_data_free(attachment_data);
  mailactivesync_free(session);
  return 1;

 err:
  mailactivesync_wbxml_document_free(request_document);
  mailactivesync_wbxml_node_free(root);
  mailactivesync_attachment_data_free(attachment_data);
  mailactivesync_free(session);
  return 0;
}

static struct mailactivesync_wbxml_node * mail_search_response_new(
    const char * status,
    int include_item)
{
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_node * response;
  struct mailactivesync_wbxml_node * store;
  struct mailactivesync_wbxml_node * result;
  struct mailactivesync_wbxml_node * properties;
  struct mailactivesync_wbxml_node * flag;
  int r;

  root = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_SEARCH,
      MAILACTIVESYNC_SEARCH_SEARCH);
  response = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_SEARCH,
      MAILACTIVESYNC_SEARCH_RESPONSE);
  store = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_SEARCH,
      MAILACTIVESYNC_SEARCH_STORE);
  result = NULL;
  properties = NULL;
  flag = NULL;
  if ((root == NULL) || (response == NULL) || (store == NULL))
    goto err;

  r = test_node_add_text(store, MAILACTIVESYNC_CP_SEARCH,
      MAILACTIVESYNC_SEARCH_STATUS, status);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;

  if (include_item) {
    result = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_SEARCH,
        MAILACTIVESYNC_SEARCH_RESULT);
    properties = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_SEARCH,
        MAILACTIVESYNC_SEARCH_PROPERTIES);
    if ((result == NULL) || (properties == NULL))
      goto err;
    if ((test_node_add_text(result, MAILACTIVESYNC_CP_AIRSYNC,
          MAILACTIVESYNC_AIRSYNC_CLASS, "Email") !=
          MAILACTIVESYNC_NO_ERROR) ||
        (test_node_add_text(result, MAILACTIVESYNC_CP_AIRSYNC,
          MAILACTIVESYNC_AIRSYNC_COLLECTION_ID, "inbox") !=
          MAILACTIVESYNC_NO_ERROR) ||
        (test_node_add_text(result, MAILACTIVESYNC_CP_AIRSYNC,
          MAILACTIVESYNC_AIRSYNC_SERVER_ID, "msg-1") !=
          MAILACTIVESYNC_NO_ERROR) ||
        (test_node_add_text(result, MAILACTIVESYNC_CP_SEARCH,
          MAILACTIVESYNC_SEARCH_LONG_ID, "long-1") !=
          MAILACTIVESYNC_NO_ERROR) ||
        (test_node_add_text(properties, MAILACTIVESYNC_CP_EMAIL,
          MAILACTIVESYNC_EMAIL_SUBJECT, "Found subject") !=
          MAILACTIVESYNC_NO_ERROR) ||
        (test_node_add_text(properties, MAILACTIVESYNC_CP_EMAIL,
          MAILACTIVESYNC_EMAIL_FROM, "sender@example.com") !=
          MAILACTIVESYNC_NO_ERROR) ||
        (test_node_add_text(properties, MAILACTIVESYNC_CP_EMAIL,
          MAILACTIVESYNC_EMAIL_DISPLAY_TO, "User One; User Two") !=
          MAILACTIVESYNC_NO_ERROR) ||
        (test_node_add_text(properties, MAILACTIVESYNC_CP_EMAIL,
          MAILACTIVESYNC_EMAIL_IMPORTANCE, "2") !=
          MAILACTIVESYNC_NO_ERROR) ||
        (test_node_add_text(properties, MAILACTIVESYNC_CP_EMAIL,
          MAILACTIVESYNC_EMAIL_THREAD_TOPIC, "Found thread") !=
          MAILACTIVESYNC_NO_ERROR) ||
        (test_node_add_text(properties, MAILACTIVESYNC_CP_EMAIL,
          MAILACTIVESYNC_EMAIL_INTERNET_CPID, "65001") !=
          MAILACTIVESYNC_NO_ERROR) ||
        (test_node_add_text(properties, MAILACTIVESYNC_CP_EMAIL,
          MAILACTIVESYNC_EMAIL_CONTENT_CLASS,
          "urn:content-classes:message") != MAILACTIVESYNC_NO_ERROR) ||
        (test_node_add_text(properties, MAILACTIVESYNC_CP_EMAIL,
          MAILACTIVESYNC_EMAIL_BODY, "Legacy body preview") !=
          MAILACTIVESYNC_NO_ERROR) ||
        (test_node_add_text(properties, MAILACTIVESYNC_CP_EMAIL,
          MAILACTIVESYNC_EMAIL_BODY_SIZE, "120") !=
          MAILACTIVESYNC_NO_ERROR) ||
        (test_node_add_text(properties, MAILACTIVESYNC_CP_EMAIL,
          MAILACTIVESYNC_EMAIL_BODY_TRUNCATED, "1") !=
          MAILACTIVESYNC_NO_ERROR) ||
        (test_node_add_text(properties, MAILACTIVESYNC_CP_EMAIL,
          MAILACTIVESYNC_EMAIL_MIME_DATA, "MIME preview") !=
          MAILACTIVESYNC_NO_ERROR) ||
        (test_node_add_text(properties, MAILACTIVESYNC_CP_EMAIL,
          MAILACTIVESYNC_EMAIL_MIME_SIZE, "240") !=
          MAILACTIVESYNC_NO_ERROR) ||
        (test_node_add_text(properties, MAILACTIVESYNC_CP_EMAIL,
          MAILACTIVESYNC_EMAIL_MIME_TRUNCATED, "1") !=
          MAILACTIVESYNC_NO_ERROR) ||
        (test_node_add_opaque(properties, MAILACTIVESYNC_CP_EMAIL2,
          MAILACTIVESYNC_EMAIL2_CONVERSATION_ID, test_conversation_id,
          sizeof(test_conversation_id)) != MAILACTIVESYNC_NO_ERROR) ||
        (test_node_add_opaque(properties, MAILACTIVESYNC_CP_EMAIL2,
          MAILACTIVESYNC_EMAIL2_CONVERSATION_INDEX, test_conversation_index,
          sizeof(test_conversation_index)) != MAILACTIVESYNC_NO_ERROR) ||
        (test_node_add_text(properties, MAILACTIVESYNC_CP_EMAIL,
          MAILACTIVESYNC_EMAIL_CATEGORIES, NULL) !=
          MAILACTIVESYNC_NO_ERROR) ||
        (test_node_add_text(properties, MAILACTIVESYNC_CP_EMAIL,
          MAILACTIVESYNC_EMAIL_READ, "0") !=
          MAILACTIVESYNC_NO_ERROR))
      goto err;
    flag = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_EMAIL,
        MAILACTIVESYNC_EMAIL_FLAG);
    if (flag == NULL)
      goto err;
    if ((test_node_add_text(flag, MAILACTIVESYNC_CP_EMAIL,
          MAILACTIVESYNC_EMAIL_STATUS, "2") != MAILACTIVESYNC_NO_ERROR) ||
        (test_node_add_text(flag, MAILACTIVESYNC_CP_EMAIL,
          MAILACTIVESYNC_EMAIL_FLAG_TYPE, "Flag for follow up") !=
          MAILACTIVESYNC_NO_ERROR) ||
        (test_node_add_text(flag, MAILACTIVESYNC_CP_EMAIL,
          MAILACTIVESYNC_EMAIL_COMPLETE_TIME, "2026-08-02T10:11:12.000Z") !=
          MAILACTIVESYNC_NO_ERROR))
      goto err;
    if (mailactivesync_wbxml_node_add_child(properties, flag) !=
        MAILACTIVESYNC_NO_ERROR)
      goto err;
    flag = NULL;
    if ((test_node_add_text(test_node_child(properties,
          MAILACTIVESYNC_CP_EMAIL, MAILACTIVESYNC_EMAIL_CATEGORIES),
          MAILACTIVESYNC_CP_EMAIL, MAILACTIVESYNC_EMAIL_CATEGORY, "Blue") !=
          MAILACTIVESYNC_NO_ERROR) ||
        (test_node_add_text(test_node_child(properties,
          MAILACTIVESYNC_CP_EMAIL, MAILACTIVESYNC_EMAIL_CATEGORIES),
          MAILACTIVESYNC_CP_EMAIL, MAILACTIVESYNC_EMAIL_CATEGORY, "Project") !=
          MAILACTIVESYNC_NO_ERROR))
      goto err;
    if (mailactivesync_wbxml_node_add_child(result, properties) !=
        MAILACTIVESYNC_NO_ERROR)
      goto err;
    properties = NULL;
    if (mailactivesync_wbxml_node_add_child(store, result) !=
        MAILACTIVESYNC_NO_ERROR)
      goto err;
    result = NULL;
    if ((test_node_add_text(store, MAILACTIVESYNC_CP_SEARCH,
          MAILACTIVESYNC_SEARCH_RANGE, "0-0") !=
          MAILACTIVESYNC_NO_ERROR) ||
        (test_node_add_text(store, MAILACTIVESYNC_CP_SEARCH,
          MAILACTIVESYNC_SEARCH_TOTAL, "1") !=
          MAILACTIVESYNC_NO_ERROR))
      goto err;
  }

  if (mailactivesync_wbxml_node_add_child(response, store) !=
      MAILACTIVESYNC_NO_ERROR)
    goto err;
  store = NULL;
  if (mailactivesync_wbxml_node_add_child(root, response) !=
      MAILACTIVESYNC_NO_ERROR)
    goto err;
  response = NULL;

  return root;

 err:
  mailactivesync_wbxml_node_free(flag);
  mailactivesync_wbxml_node_free(properties);
  mailactivesync_wbxml_node_free(result);
  mailactivesync_wbxml_node_free(store);
  mailactivesync_wbxml_node_free(response);
  mailactivesync_wbxml_node_free(root);
  return NULL;
}

static int test_mail_search_success_and_status(void)
{
  mailactivesync * session;
  struct fake_context * context;
  struct mailactivesync_mail_search_request request;
  struct mailactivesync_mail_search_result * result;
  struct mailactivesync_mail_search_item * item;
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_node * store;
  struct mailactivesync_wbxml_node * query;
  struct mailactivesync_wbxml_node * and_node;
  struct mailactivesync_wbxml_node * options;
  struct mailactivesync_wbxml_document * request_document;
  int request_count;
  int r;

  session = NULL;
  context = NULL;
  result = NULL;
  root = NULL;
  request_document = NULL;

  if (!check(setup_oauth_session(&session, &context) ==
      MAILACTIVESYNC_NO_ERROR, "setup OAuth session failed"))
    return 0;

  root = mail_search_response_new("1", 1);
  if (!check(root != NULL, "Search response allocation failed"))
    goto err;
  if (!check(fake_context_set_response_body(context, root) ==
      MAILACTIVESYNC_NO_ERROR, "Search response encode failed"))
    goto err;
  mailactivesync_wbxml_node_free(root);
  root = NULL;

  request.collection_id = "inbox";
  request.free_text = "Sales Totals";
  request.range_start = 0;
  request.range_end = 4;
  request.rebuild_results = 1;
  r = mailactivesync_mail_search(session, &request, &result);
  if (!check(r == MAILACTIVESYNC_NO_ERROR, "Search command failed"))
    goto err;
  if (!check(strstr(context->last_request->url, "Cmd=Search") != NULL,
      "Search command URL mismatch"))
    goto err;
  if (!check(mailactivesync_wbxml_decode(context->last_request->body,
      context->last_request->body_len, &request_document) ==
      MAILACTIVESYNC_NO_ERROR, "Search request decode failed"))
    goto err;
  store = test_node_child(request_document->root, MAILACTIVESYNC_CP_SEARCH,
      MAILACTIVESYNC_SEARCH_STORE);
  query = test_node_child(store, MAILACTIVESYNC_CP_SEARCH,
      MAILACTIVESYNC_SEARCH_QUERY);
  and_node = test_node_child(query, MAILACTIVESYNC_CP_SEARCH,
      MAILACTIVESYNC_SEARCH_AND);
  options = test_node_child(store, MAILACTIVESYNC_CP_SEARCH,
      MAILACTIVESYNC_SEARCH_OPTIONS);
  if (!check((request_document->root != NULL) &&
      (request_document->root->code_page == MAILACTIVESYNC_CP_SEARCH) &&
      (request_document->root->token == MAILACTIVESYNC_SEARCH_SEARCH) &&
      str_equal(test_node_child_text(store, MAILACTIVESYNC_CP_SEARCH,
          MAILACTIVESYNC_SEARCH_NAME), "Mailbox") &&
      str_equal(test_node_child_text(and_node, MAILACTIVESYNC_CP_AIRSYNC,
          MAILACTIVESYNC_AIRSYNC_CLASS), "Email") &&
      str_equal(test_node_child_text(and_node, MAILACTIVESYNC_CP_AIRSYNC,
          MAILACTIVESYNC_AIRSYNC_COLLECTION_ID), "inbox") &&
      str_equal(test_node_child_text(and_node, MAILACTIVESYNC_CP_SEARCH,
          MAILACTIVESYNC_SEARCH_FREE_TEXT), "Sales Totals") &&
      (test_node_child(options, MAILACTIVESYNC_CP_SEARCH,
          MAILACTIVESYNC_SEARCH_REBUILD_RESULTS) != NULL) &&
      str_equal(test_node_child_text(options, MAILACTIVESYNC_CP_SEARCH,
          MAILACTIVESYNC_SEARCH_RANGE), "0-4"),
      "Search request shape mismatch"))
    goto err;
  if (!check((result != NULL) && (result->status == 1) &&
      str_equal(result->range, "0-0") && (result->total == 1) &&
      (clist_count(result->items) == 1),
      "Search result summary mismatch"))
    goto err;
  item = clist_content(clist_begin(result->items));
  if (!check((item != NULL) && str_equal(item->collection_id, "inbox") &&
      str_equal(item->server_id, "msg-1") &&
      str_equal(item->long_id, "long-1") &&
      (item->message != NULL) &&
      str_equal(item->message->server_id, "msg-1") &&
      str_equal(item->message->subject, "Found subject") &&
      str_equal(item->message->from, "sender@example.com") &&
      str_equal(item->message->display_to, "User One; User Two") &&
      (item->message->importance == 2) &&
      str_equal(item->message->thread_topic, "Found thread") &&
      str_equal(item->message->internet_cpid, "65001") &&
      str_equal(item->message->content_class, "urn:content-classes:message") &&
      (item->message->conversation_id_len == sizeof(test_conversation_id)) &&
      (memcmp(item->message->conversation_id, test_conversation_id,
          sizeof(test_conversation_id)) == 0) &&
      (item->message->conversation_index_len ==
          sizeof(test_conversation_index)) &&
      (memcmp(item->message->conversation_index, test_conversation_index,
          sizeof(test_conversation_index)) == 0) &&
      (item->message->body != NULL) &&
      (item->message->body->type ==
          MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_PLAIN_TEXT) &&
      str_equal(item->message->body->data, "Legacy body preview") &&
      (item->message->body->estimated_data_size == 120) &&
      (item->message->body->truncated == 1) &&
      str_equal(item->message->mime, "MIME preview") &&
      (item->message->mime_size == 240) &&
      (item->message->mime_truncated == 1) &&
      (item->message->categories != NULL) &&
      (clist_count(item->message->categories) == 2) &&
      str_equal(clist_content(clist_begin(item->message->categories)),
          "Blue") &&
      (item->message->read == 0), "Search result item mismatch"))
    goto err;
  if (!check((item->message->flagged == 1) &&
      (item->message->flag_status == 2) &&
      str_equal(item->message->flag_type, "Flag for follow up") &&
      str_equal(item->message->flag_complete_time,
          "2026-08-02T10:11:12.000Z"),
      "Search result flag metadata mismatch"))
    goto err;
  mailactivesync_wbxml_document_free(request_document);
  request_document = NULL;
  mailactivesync_mail_search_result_free(result);
  result = NULL;

  root = mail_search_response_new("2", 0);
  if (!check(root != NULL, "Search error response allocation failed"))
    goto err;
  if (!check(fake_context_set_response_body(context, root) ==
      MAILACTIVESYNC_NO_ERROR, "Search error response encode failed"))
    goto err;
  mailactivesync_wbxml_node_free(root);
  root = NULL;
  r = mailactivesync_mail_search(session, &request, &result);
  if (!check((r == MAILACTIVESYNC_ERROR_PROTOCOL) && (result != NULL) &&
      (result->status == 2) && (clist_count(result->items) == 0),
      "Search status mapping mismatch"))
    goto err;
  mailactivesync_mail_search_result_free(result);
  result = NULL;

  request_count = context->request_count;
  if (!check(mailactivesync_set_protocol_version(session, "2.5") ==
      MAILACTIVESYNC_NO_ERROR, "Search protocol set failed"))
    goto err;
  r = mailactivesync_mail_search(session, &request, &result);
  if (!check((r == MAILACTIVESYNC_ERROR_NOT_IMPLEMENTED) &&
      (result == NULL) && (context->request_count == request_count),
      "Search protocol guard mismatch"))
    goto err;

  mailactivesync_free(session);
  return 1;

 err:
  mailactivesync_wbxml_document_free(request_document);
  mailactivesync_wbxml_node_free(root);
  mailactivesync_mail_search_result_free(result);
  mailactivesync_free(session);
  return 0;
}

static struct mailactivesync_wbxml_node * mail_find_response_new(
    const char * status,
    const char * response_status,
    int include_item)
{
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_node * response;
  struct mailactivesync_wbxml_node * result;
  struct mailactivesync_wbxml_node * properties;
  int r;

  root = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_FIND,
      MAILACTIVESYNC_FIND_FIND);
  response = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_FIND,
      MAILACTIVESYNC_FIND_RESPONSE);
  result = NULL;
  properties = NULL;
  if ((root == NULL) || (response == NULL))
    goto err;

  r = test_node_add_text(root, MAILACTIVESYNC_CP_FIND,
      MAILACTIVESYNC_FIND_STATUS, status);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  r = test_node_add_text(response, MAILACTIVESYNC_CP_ITEMOPERATIONS,
      MAILACTIVESYNC_ITEMOPERATIONS_STORE, "Mailbox");
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  r = test_node_add_text(response, MAILACTIVESYNC_CP_FIND,
      MAILACTIVESYNC_FIND_STATUS, response_status);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;

  if (include_item) {
    result = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_FIND,
        MAILACTIVESYNC_FIND_RESULT);
    properties = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_FIND,
        MAILACTIVESYNC_FIND_PROPERTIES);
    if ((result == NULL) || (properties == NULL))
      goto err;
    if ((test_node_add_text(result, MAILACTIVESYNC_CP_AIRSYNC,
          MAILACTIVESYNC_AIRSYNC_CLASS, "Email") !=
          MAILACTIVESYNC_NO_ERROR) ||
        (test_node_add_text(result, MAILACTIVESYNC_CP_AIRSYNC,
          MAILACTIVESYNC_AIRSYNC_COLLECTION_ID, "inbox") !=
          MAILACTIVESYNC_NO_ERROR) ||
        (test_node_add_text(result, MAILACTIVESYNC_CP_AIRSYNC,
          MAILACTIVESYNC_AIRSYNC_SERVER_ID, "msg-1") !=
          MAILACTIVESYNC_NO_ERROR) ||
        (test_node_add_text(properties, MAILACTIVESYNC_CP_EMAIL,
          MAILACTIVESYNC_EMAIL_SUBJECT, "Find subject") !=
          MAILACTIVESYNC_NO_ERROR) ||
        (test_node_add_text(properties, MAILACTIVESYNC_CP_EMAIL,
          MAILACTIVESYNC_EMAIL_FROM, "sender@example.com") !=
          MAILACTIVESYNC_NO_ERROR) ||
        (test_node_add_text(properties, MAILACTIVESYNC_CP_EMAIL,
          MAILACTIVESYNC_EMAIL_DISPLAY_TO, "User One") !=
          MAILACTIVESYNC_NO_ERROR) ||
        (test_node_add_text(properties, MAILACTIVESYNC_CP_EMAIL,
          MAILACTIVESYNC_EMAIL_IMPORTANCE, "1") !=
          MAILACTIVESYNC_NO_ERROR) ||
        (test_node_add_text(properties, MAILACTIVESYNC_CP_FIND,
          MAILACTIVESYNC_FIND_DISPLAY_CC, "Cc User") !=
          MAILACTIVESYNC_NO_ERROR) ||
        (test_node_add_text(properties, MAILACTIVESYNC_CP_FIND,
          MAILACTIVESYNC_FIND_DISPLAY_BCC, "Bcc User") !=
          MAILACTIVESYNC_NO_ERROR) ||
        (test_node_add_text(properties, MAILACTIVESYNC_CP_FIND,
          MAILACTIVESYNC_FIND_PREVIEW, "Preview text") !=
          MAILACTIVESYNC_NO_ERROR) ||
        (test_node_add_text(properties, MAILACTIVESYNC_CP_FIND,
          MAILACTIVESYNC_FIND_HAS_ATTACHMENTS, "1") !=
          MAILACTIVESYNC_NO_ERROR) ||
        (test_node_add_text(properties, MAILACTIVESYNC_CP_EMAIL,
          MAILACTIVESYNC_EMAIL_READ, "1") !=
          MAILACTIVESYNC_NO_ERROR))
      goto err;
    if (mailactivesync_wbxml_node_add_child(result, properties) !=
        MAILACTIVESYNC_NO_ERROR)
      goto err;
    properties = NULL;
    if (mailactivesync_wbxml_node_add_child(response, result) !=
        MAILACTIVESYNC_NO_ERROR)
      goto err;
    result = NULL;
    if ((test_node_add_text(response, MAILACTIVESYNC_CP_FIND,
          MAILACTIVESYNC_FIND_RANGE, "0-0") !=
          MAILACTIVESYNC_NO_ERROR) ||
        (test_node_add_text(response, MAILACTIVESYNC_CP_FIND,
          MAILACTIVESYNC_FIND_TOTAL, "1") !=
          MAILACTIVESYNC_NO_ERROR))
      goto err;
  }

  if (mailactivesync_wbxml_node_add_child(root, response) !=
      MAILACTIVESYNC_NO_ERROR)
    goto err;
  response = NULL;

  return root;

 err:
  mailactivesync_wbxml_node_free(properties);
  mailactivesync_wbxml_node_free(result);
  mailactivesync_wbxml_node_free(response);
  mailactivesync_wbxml_node_free(root);
  return NULL;
}

static int test_mail_find_success_and_status(void)
{
  mailactivesync * session;
  struct fake_context * context;
  struct mailactivesync_mail_find_request request;
  struct mailactivesync_mail_find_result * result;
  struct mailactivesync_mail_find_item * item;
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_node * execute_search;
  struct mailactivesync_wbxml_node * criterion;
  struct mailactivesync_wbxml_node * query;
  struct mailactivesync_wbxml_node * options;
  struct mailactivesync_wbxml_document * request_document;
  int request_count;
  int r;

  session = NULL;
  context = NULL;
  result = NULL;
  root = NULL;
  request_document = NULL;

  if (!check(setup_oauth_session(&session, &context) ==
      MAILACTIVESYNC_NO_ERROR, "setup OAuth session failed"))
    return 0;

  root = mail_find_response_new("1", "1", 1);
  if (!check(root != NULL, "Find response allocation failed"))
    goto err;
  if (!check(fake_context_set_response_body(context, root) ==
      MAILACTIVESYNC_NO_ERROR, "Find response encode failed"))
    goto err;
  mailactivesync_wbxml_node_free(root);
  root = NULL;

  request.search_id = "01234567-89ab-cdef-0123-456789abcdef";
  request.collection_id = "inbox";
  request.query = "from:sender subject:Find";
  request.range_start = 0;
  request.range_end = 9;
  request.deep_traversal = 1;
  r = mailactivesync_mail_find(session, &request, &result);
  if (!check(r == MAILACTIVESYNC_NO_ERROR, "Find command failed"))
    goto err;
  if (!check(strstr(context->last_request->url, "Cmd=Find") != NULL,
      "Find command URL mismatch"))
    goto err;
  if (!check(mailactivesync_wbxml_decode(context->last_request->body,
      context->last_request->body_len, &request_document) ==
      MAILACTIVESYNC_NO_ERROR, "Find request decode failed"))
    goto err;
  execute_search = test_node_child(request_document->root,
      MAILACTIVESYNC_CP_FIND, MAILACTIVESYNC_FIND_EXECUTE_SEARCH);
  criterion = test_node_child(execute_search, MAILACTIVESYNC_CP_FIND,
      MAILACTIVESYNC_FIND_MAILBOX_SEARCH_CRITERION);
  query = test_node_child(criterion, MAILACTIVESYNC_CP_FIND,
      MAILACTIVESYNC_FIND_QUERY);
  options = test_node_child(criterion, MAILACTIVESYNC_CP_FIND,
      MAILACTIVESYNC_FIND_OPTIONS);
  if (!check((request_document->root != NULL) &&
      (request_document->root->code_page == MAILACTIVESYNC_CP_FIND) &&
      (request_document->root->token == MAILACTIVESYNC_FIND_FIND) &&
      str_equal(test_node_child_text(request_document->root,
          MAILACTIVESYNC_CP_FIND, MAILACTIVESYNC_FIND_SEARCH_ID),
          "01234567-89ab-cdef-0123-456789abcdef") &&
      str_equal(test_node_child_text(query, MAILACTIVESYNC_CP_AIRSYNC,
          MAILACTIVESYNC_AIRSYNC_CLASS), "Email") &&
      str_equal(test_node_child_text(query, MAILACTIVESYNC_CP_AIRSYNC,
          MAILACTIVESYNC_AIRSYNC_COLLECTION_ID), "inbox") &&
      str_equal(test_node_child_text(query, MAILACTIVESYNC_CP_FIND,
          MAILACTIVESYNC_FIND_FREE_TEXT), "from:sender subject:Find") &&
      str_equal(test_node_child_text(options, MAILACTIVESYNC_CP_FIND,
          MAILACTIVESYNC_FIND_RANGE), "0-9") &&
      (test_node_child(options, MAILACTIVESYNC_CP_FIND,
          MAILACTIVESYNC_FIND_DEEP_TRAVERSAL) != NULL),
      "Find request shape mismatch"))
    goto err;
  if (!check((result != NULL) && (result->status == 1) &&
      (result->response_status == 1) && str_equal(result->store, "Mailbox") &&
      str_equal(result->range, "0-0") && (result->total == 1) &&
      (clist_count(result->items) == 1),
      "Find result summary mismatch"))
    goto err;
  item = clist_content(clist_begin(result->items));
  if (!check((item != NULL) && str_equal(item->collection_class, "Email") &&
      str_equal(item->collection_id, "inbox") &&
      str_equal(item->server_id, "msg-1") &&
      str_equal(item->preview, "Preview text") &&
      str_equal(item->display_cc, "Cc User") &&
      str_equal(item->display_bcc, "Bcc User") &&
      (item->has_attachments == 1) &&
      (item->message != NULL) &&
      str_equal(item->message->server_id, "msg-1") &&
      str_equal(item->message->subject, "Find subject") &&
      str_equal(item->message->from, "sender@example.com") &&
      str_equal(item->message->display_to, "User One") &&
      (item->message->importance == 1) &&
      (item->message->read == 1), "Find result item mismatch"))
    goto err;
  mailactivesync_wbxml_document_free(request_document);
  request_document = NULL;
  mailactivesync_mail_find_result_free(result);
  result = NULL;

  root = mail_find_response_new("1", "2", 0);
  if (!check(root != NULL, "Find error response allocation failed"))
    goto err;
  if (!check(fake_context_set_response_body(context, root) ==
      MAILACTIVESYNC_NO_ERROR, "Find error response encode failed"))
    goto err;
  mailactivesync_wbxml_node_free(root);
  root = NULL;
  r = mailactivesync_mail_find(session, &request, &result);
  if (!check((r == MAILACTIVESYNC_ERROR_PROTOCOL) && (result != NULL) &&
      (result->status == 1) && (result->response_status == 2) &&
      (clist_count(result->items) == 0), "Find status mapping mismatch"))
    goto err;
  mailactivesync_mail_find_result_free(result);
  result = NULL;

  request_count = context->request_count;
  if (!check(mailactivesync_set_protocol_version(session, "16.0") ==
      MAILACTIVESYNC_NO_ERROR, "Find protocol set failed"))
    goto err;
  r = mailactivesync_mail_find(session, &request, &result);
  if (!check((r == MAILACTIVESYNC_ERROR_NOT_IMPLEMENTED) &&
      (result == NULL) && (context->request_count == request_count),
      "Find protocol guard mismatch"))
    goto err;

  mailactivesync_free(session);
  return 1;

 err:
  mailactivesync_wbxml_document_free(request_document);
  mailactivesync_wbxml_node_free(root);
  mailactivesync_mail_find_result_free(result);
  mailactivesync_free(session);
  return 0;
}

static struct mailactivesync_wbxml_node * validate_cert_response_new(
    const char * command_status,
    const char * first_status,
    const char * second_status)
{
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_node * certificate;

  root = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_VALIDATECERT,
      MAILACTIVESYNC_VALIDATECERT_VALIDATE_CERT);
  if (root == NULL)
    return NULL;
  certificate = NULL;

  if (test_node_add_text(root, MAILACTIVESYNC_CP_VALIDATECERT,
      MAILACTIVESYNC_VALIDATECERT_STATUS, command_status) !=
      MAILACTIVESYNC_NO_ERROR)
    goto err;

  if (first_status != NULL) {
    certificate = mailactivesync_wbxml_node_new(
        MAILACTIVESYNC_CP_VALIDATECERT,
        MAILACTIVESYNC_VALIDATECERT_CERTIFICATE);
    if (certificate == NULL)
      goto err;
    if (test_node_add_text(certificate, MAILACTIVESYNC_CP_VALIDATECERT,
        MAILACTIVESYNC_VALIDATECERT_STATUS, first_status) !=
        MAILACTIVESYNC_NO_ERROR)
      goto err;
    if ((second_status != NULL) &&
        (test_node_add_text(certificate, MAILACTIVESYNC_CP_VALIDATECERT,
            MAILACTIVESYNC_VALIDATECERT_STATUS, second_status) !=
            MAILACTIVESYNC_NO_ERROR))
      goto err;
    if (mailactivesync_wbxml_node_add_child(root, certificate) !=
        MAILACTIVESYNC_NO_ERROR)
      goto err;
    certificate = NULL;
  }

  return root;

 err:
  mailactivesync_wbxml_node_free(certificate);
  mailactivesync_wbxml_node_free(root);
  return NULL;
}

static int test_validate_cert_success_and_status(void)
{
  mailactivesync * session;
  struct fake_context * context;
  struct mailactivesync_validate_cert_request request;
  struct mailactivesync_validate_cert_result * result;
  struct mailactivesync_validate_cert_certificate * certificate_result;
  struct mailactivesync_wbxml_document * request_document;
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_node * certificates;
  struct mailactivesync_wbxml_node * certificate;
  struct mailactivesync_wbxml_node * chain;
  clist * cert_list;
  clist * chain_list;
  int * status;
  int r;

  session = NULL;
  context = NULL;
  result = NULL;
  request_document = NULL;
  root = NULL;
  cert_list = NULL;
  chain_list = NULL;

  if (!check(setup_oauth_session(&session, &context) ==
      MAILACTIVESYNC_NO_ERROR, "setup OAuth session failed"))
    return 0;

  cert_list = clist_new();
  chain_list = clist_new();
  if (!check((cert_list != NULL) && (chain_list != NULL),
      "ValidateCert list allocation failed"))
    goto err;
  if (!check((clist_append(cert_list, "BASE64CERT1") == 0) &&
      (clist_append(cert_list, "BASE64CERT2") == 0) &&
      (clist_append(chain_list, "BASE64CHAIN1") == 0),
      "ValidateCert list append failed"))
    goto err;

  root = validate_cert_response_new("1", "1", NULL);
  if (!check(root != NULL, "ValidateCert response allocation failed"))
    goto err;
  if (!check(fake_context_set_response_body(context, root) ==
      MAILACTIVESYNC_NO_ERROR, "ValidateCert response encode failed"))
    goto err;
  mailactivesync_wbxml_node_free(root);
  root = NULL;

  memset(&request, 0, sizeof(request));
  request.certificates = cert_list;
  request.certificate_chain = chain_list;
  request.check_crl = 1;
  r = mailactivesync_validate_cert(session, &request, &result);
  if (!check(r == MAILACTIVESYNC_NO_ERROR, "ValidateCert command failed"))
    goto err;
  if (!check(strstr(context->last_request->url, "Cmd=ValidateCert") !=
      NULL, "ValidateCert command URL mismatch"))
    goto err;
  if (!check(mailactivesync_wbxml_decode(context->last_request->body,
      context->last_request->body_len, &request_document) ==
      MAILACTIVESYNC_NO_ERROR, "ValidateCert request decode failed"))
    goto err;
  if (!check((request_document->root != NULL) &&
      (request_document->root->code_page == MAILACTIVESYNC_CP_VALIDATECERT) &&
      (request_document->root->token ==
          MAILACTIVESYNC_VALIDATECERT_VALIDATE_CERT),
      "ValidateCert request root mismatch"))
    goto err;
  chain = test_node_child(request_document->root,
      MAILACTIVESYNC_CP_VALIDATECERT,
      MAILACTIVESYNC_VALIDATECERT_CERTIFICATE_CHAIN);
  certificates = test_node_child(request_document->root,
      MAILACTIVESYNC_CP_VALIDATECERT,
      MAILACTIVESYNC_VALIDATECERT_CERTIFICATES);
  certificate = test_node_child(certificates, MAILACTIVESYNC_CP_VALIDATECERT,
      MAILACTIVESYNC_VALIDATECERT_CERTIFICATE);
  if (!check((chain != NULL) && (clist_count(chain->children) == 1) &&
      (certificates != NULL) && (clist_count(certificates->children) == 2) &&
      str_equal(certificate != NULL ? certificate->text : NULL,
          "BASE64CERT1") &&
      str_equal(test_node_child_text(request_document->root,
          MAILACTIVESYNC_CP_VALIDATECERT,
          MAILACTIVESYNC_VALIDATECERT_CHECK_CRL), "1"),
      "ValidateCert request body mismatch"))
    goto err;
  if (!check((result != NULL) && (result->status == 1) &&
      (clist_count(result->certificates) == 1),
      "ValidateCert result summary mismatch"))
    goto err;
  certificate_result = clist_content(clist_begin(result->certificates));
  status = clist_content(clist_begin(certificate_result->statuses));
  if (!check((certificate_result != NULL) &&
      (clist_count(certificate_result->statuses) == 1) &&
      (status != NULL) && (* status == 1),
      "ValidateCert certificate status mismatch"))
    goto err;
  mailactivesync_wbxml_document_free(request_document);
  request_document = NULL;
  mailactivesync_validate_cert_result_free(result);
  result = NULL;

  root = validate_cert_response_new("1", "14", NULL);
  if (!check(root != NULL, "ValidateCert error response allocation failed"))
    goto err;
  if (!check(fake_context_set_response_body(context, root) ==
      MAILACTIVESYNC_NO_ERROR, "ValidateCert error response encode failed"))
    goto err;
  mailactivesync_wbxml_node_free(root);
  root = NULL;
  r = mailactivesync_validate_cert(session, &request, &result);
  if (!check((r == MAILACTIVESYNC_ERROR_SERVER_BUSY) &&
      (result != NULL) && (result->status == 1) &&
      (clist_count(result->certificates) == 1),
      "ValidateCert status mapping mismatch"))
    goto err;
  mailactivesync_validate_cert_result_free(result);
  result = NULL;

  mailactivesync_free(session);
  clist_free(cert_list);
  clist_free(chain_list);
  return 1;

 err:
  mailactivesync_wbxml_document_free(request_document);
  mailactivesync_wbxml_node_free(root);
  mailactivesync_validate_cert_result_free(result);
  mailactivesync_free(session);
  clist_free(cert_list);
  clist_free(chain_list);
  return 0;
}

static struct mailactivesync_wbxml_node * resolve_recipient_node_new(
    const char * type,
    const char * display_name,
    const char * email_address)
{
  struct mailactivesync_wbxml_node * recipient;

  recipient = mailactivesync_wbxml_node_new(
      MAILACTIVESYNC_CP_RESOLVERECIPIENTS,
      MAILACTIVESYNC_RESOLVERECIPIENTS_RECIPIENT);
  if (recipient == NULL)
    return NULL;

  if ((test_node_add_text(recipient, MAILACTIVESYNC_CP_RESOLVERECIPIENTS,
          MAILACTIVESYNC_RESOLVERECIPIENTS_TYPE, type) !=
          MAILACTIVESYNC_NO_ERROR) ||
      (test_node_add_text(recipient, MAILACTIVESYNC_CP_RESOLVERECIPIENTS,
          MAILACTIVESYNC_RESOLVERECIPIENTS_DISPLAY_NAME, display_name) !=
          MAILACTIVESYNC_NO_ERROR) ||
      (test_node_add_text(recipient, MAILACTIVESYNC_CP_RESOLVERECIPIENTS,
          MAILACTIVESYNC_RESOLVERECIPIENTS_EMAIL_ADDRESS, email_address) !=
          MAILACTIVESYNC_NO_ERROR)) {
    mailactivesync_wbxml_node_free(recipient);
    return NULL;
  }

  return recipient;
}

static struct mailactivesync_wbxml_node * resolve_rich_recipient_node_new(void)
{
  struct mailactivesync_wbxml_node * recipient;
  struct mailactivesync_wbxml_node * certificates;

  recipient = resolve_recipient_node_new("1", "Jane Example",
      "jane@example.com");
  certificates = NULL;
  if (recipient == NULL)
    return NULL;

  certificates = mailactivesync_wbxml_node_new(
      MAILACTIVESYNC_CP_RESOLVERECIPIENTS,
      MAILACTIVESYNC_RESOLVERECIPIENTS_CERTIFICATES);
  if (certificates == NULL)
    goto err;
  if ((test_node_add_text(certificates, MAILACTIVESYNC_CP_RESOLVERECIPIENTS,
          MAILACTIVESYNC_RESOLVERECIPIENTS_STATUS, "1") !=
          MAILACTIVESYNC_NO_ERROR) ||
      (test_node_add_text(certificates, MAILACTIVESYNC_CP_RESOLVERECIPIENTS,
          MAILACTIVESYNC_RESOLVERECIPIENTS_CERTIFICATE_COUNT, "1") !=
          MAILACTIVESYNC_NO_ERROR) ||
      (test_node_add_text(certificates, MAILACTIVESYNC_CP_RESOLVERECIPIENTS,
          MAILACTIVESYNC_RESOLVERECIPIENTS_CERTIFICATE, "cert-base64") !=
          MAILACTIVESYNC_NO_ERROR) ||
      (test_node_add_text(certificates, MAILACTIVESYNC_CP_RESOLVERECIPIENTS,
          MAILACTIVESYNC_RESOLVERECIPIENTS_MINI_CERTIFICATE, "mini-base64") !=
          MAILACTIVESYNC_NO_ERROR))
    goto err;
  if (mailactivesync_wbxml_node_add_child(recipient, certificates) !=
      MAILACTIVESYNC_NO_ERROR)
    goto err;
  certificates = NULL;

  return recipient;

 err:
  mailactivesync_wbxml_node_free(certificates);
  mailactivesync_wbxml_node_free(recipient);
  return NULL;
}

static struct mailactivesync_wbxml_node *
resolve_status_only_recipient_node_new(void)
{
  struct mailactivesync_wbxml_node * recipient;
  struct mailactivesync_wbxml_node * certificates;

  recipient = resolve_recipient_node_new("1", "No Cert",
      "nocert@example.com");
  certificates = NULL;
  if (recipient == NULL)
    return NULL;

  certificates = mailactivesync_wbxml_node_new(
      MAILACTIVESYNC_CP_RESOLVERECIPIENTS,
      MAILACTIVESYNC_RESOLVERECIPIENTS_CERTIFICATES);
  if (certificates == NULL)
    goto err;
  if (test_node_add_text(certificates, MAILACTIVESYNC_CP_RESOLVERECIPIENTS,
      MAILACTIVESYNC_RESOLVERECIPIENTS_STATUS, "7") !=
      MAILACTIVESYNC_NO_ERROR)
    goto err;
  if (mailactivesync_wbxml_node_add_child(recipient, certificates) !=
      MAILACTIVESYNC_NO_ERROR)
    goto err;
  certificates = NULL;

  return recipient;

 err:
  mailactivesync_wbxml_node_free(certificates);
  mailactivesync_wbxml_node_free(recipient);
  return NULL;
}

static struct mailactivesync_wbxml_node * resolve_response_node_new(
    const char * to,
    const char * status,
    const char * recipient_count,
    struct mailactivesync_wbxml_node * recipient)
{
  struct mailactivesync_wbxml_node * response;

  response = mailactivesync_wbxml_node_new(
      MAILACTIVESYNC_CP_RESOLVERECIPIENTS,
      MAILACTIVESYNC_RESOLVERECIPIENTS_RESPONSE);
  if (response == NULL)
    return NULL;

  if ((test_node_add_text(response, MAILACTIVESYNC_CP_RESOLVERECIPIENTS,
          MAILACTIVESYNC_RESOLVERECIPIENTS_TO, to) !=
          MAILACTIVESYNC_NO_ERROR) ||
      (test_node_add_text(response, MAILACTIVESYNC_CP_RESOLVERECIPIENTS,
          MAILACTIVESYNC_RESOLVERECIPIENTS_STATUS, status) !=
          MAILACTIVESYNC_NO_ERROR) ||
      ((recipient_count != NULL) &&
          (test_node_add_text(response, MAILACTIVESYNC_CP_RESOLVERECIPIENTS,
              MAILACTIVESYNC_RESOLVERECIPIENTS_RECIPIENT_COUNT,
              recipient_count) != MAILACTIVESYNC_NO_ERROR)))
    goto err;

  if (recipient != NULL) {
    if (mailactivesync_wbxml_node_add_child(response, recipient) !=
        MAILACTIVESYNC_NO_ERROR)
      goto err;
  }

  return response;

 err:
  mailactivesync_wbxml_node_free(response);
  return NULL;
}

static struct mailactivesync_wbxml_node * resolve_recipients_response_new(
    const char * status,
    int include_responses)
{
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_node * response;
  struct mailactivesync_wbxml_node * recipient;

  root = mailactivesync_wbxml_node_new(
      MAILACTIVESYNC_CP_RESOLVERECIPIENTS,
      MAILACTIVESYNC_RESOLVERECIPIENTS_RESOLVE_RECIPIENTS);
  response = NULL;
  recipient = NULL;
  if (root == NULL)
    return NULL;

  if (test_node_add_text(root, MAILACTIVESYNC_CP_RESOLVERECIPIENTS,
      MAILACTIVESYNC_RESOLVERECIPIENTS_STATUS, status) !=
      MAILACTIVESYNC_NO_ERROR)
    goto err;

  if (include_responses) {
    recipient = resolve_rich_recipient_node_new();
    if (recipient == NULL)
      goto err;
    response = resolve_response_node_new("jane", "1", "1", recipient);
    if (response == NULL)
      goto err;
    recipient = NULL;
    if (mailactivesync_wbxml_node_add_child(root, response) !=
        MAILACTIVESYNC_NO_ERROR)
      goto err;
    response = NULL;

    recipient = resolve_recipient_node_new("1", "John Example",
        "john@example.com");
    if (recipient == NULL)
      goto err;
    response = resolve_response_node_new("jo", "2", "2", recipient);
    if (response == NULL)
      goto err;
    recipient = NULL;
    if (mailactivesync_wbxml_node_add_child(root, response) !=
        MAILACTIVESYNC_NO_ERROR)
      goto err;
    response = NULL;

    recipient = resolve_status_only_recipient_node_new();
    if (recipient == NULL)
      goto err;
    response = resolve_response_node_new("nocert", "1", "1", recipient);
    if (response == NULL)
      goto err;
    recipient = NULL;
    if (mailactivesync_wbxml_node_add_child(root, response) !=
        MAILACTIVESYNC_NO_ERROR)
      goto err;
    response = NULL;
  }

  return root;

 err:
  mailactivesync_wbxml_node_free(recipient);
  mailactivesync_wbxml_node_free(response);
  mailactivesync_wbxml_node_free(root);
  return NULL;
}

static int test_resolve_recipients_success_and_status(void)
{
  mailactivesync * session;
  struct fake_context * context;
  struct mailactivesync_resolve_recipients_request request;
  struct mailactivesync_resolve_recipients_result * result;
  struct mailactivesync_resolve_recipients_response * response;
  struct mailactivesync_resolved_recipient * recipient;
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_node * options;
  struct mailactivesync_wbxml_node * unresolved_response;
  struct mailactivesync_wbxml_document * request_document;
  clist * recipients;
  clistiter * cur;
  const char * second_to;
  int to_count;
  int r;

  session = NULL;
  context = NULL;
  result = NULL;
  root = NULL;
  options = NULL;
  unresolved_response = NULL;
  request_document = NULL;
  recipients = NULL;
  second_to = NULL;

  if (!check(setup_oauth_session(&session, &context) ==
      MAILACTIVESYNC_NO_ERROR, "setup OAuth session failed"))
    return 0;
  recipients = clist_new();
  if (!check(recipients != NULL, "ResolveRecipients list allocation failed"))
    goto err;
  if ((clist_append(recipients, "jane") < 0) ||
      (clist_append(recipients, "jo") < 0))
    goto err;

  root = resolve_recipients_response_new("1", 1);
  if (!check(root != NULL, "ResolveRecipients response allocation failed"))
    goto err;
  if (!check(fake_context_set_response_body(context, root) ==
      MAILACTIVESYNC_NO_ERROR, "ResolveRecipients response encode failed"))
    goto err;
  mailactivesync_wbxml_node_free(root);
  root = NULL;

  memset(&request, 0, sizeof(request));
  request.recipients = recipients;
  request.max_ambiguous_recipients = 2;
  request.certificate_retrieval = 2;
  request.max_certificates = 3;
  r = mailactivesync_resolve_recipients_ext(session, &request, &result);
  if (!check(r == MAILACTIVESYNC_NO_ERROR,
      "ResolveRecipients command failed"))
    goto err;
  if (!check(strstr(context->last_request->url,
      "Cmd=ResolveRecipients") != NULL,
      "ResolveRecipients command URL mismatch"))
    goto err;
  if (!check(mailactivesync_wbxml_decode(context->last_request->body,
      context->last_request->body_len, &request_document) ==
      MAILACTIVESYNC_NO_ERROR, "ResolveRecipients request decode failed"))
    goto err;
  if (!check((request_document->root != NULL) &&
      (request_document->root->code_page ==
          MAILACTIVESYNC_CP_RESOLVERECIPIENTS) &&
      (request_document->root->token ==
          MAILACTIVESYNC_RESOLVERECIPIENTS_RESOLVE_RECIPIENTS),
      "ResolveRecipients request root mismatch"))
    goto err;
  to_count = 0;
  for (cur = clist_begin(request_document->root->children); cur != NULL;
      cur = clist_next(cur)) {
    struct mailactivesync_wbxml_node * child;

    child = clist_content(cur);
    if ((child->code_page == MAILACTIVESYNC_CP_RESOLVERECIPIENTS) &&
        (child->token == MAILACTIVESYNC_RESOLVERECIPIENTS_TO)) {
      to_count ++;
      if (to_count == 2)
        second_to = child->text;
    }
  }
  if (!check((to_count == 2) &&
      str_equal(test_node_child_text(request_document->root,
          MAILACTIVESYNC_CP_RESOLVERECIPIENTS,
          MAILACTIVESYNC_RESOLVERECIPIENTS_TO), "jane") &&
      str_equal(second_to, "jo"), "ResolveRecipients To list mismatch"))
    goto err;
  options = test_node_child(request_document->root,
      MAILACTIVESYNC_CP_RESOLVERECIPIENTS,
      MAILACTIVESYNC_RESOLVERECIPIENTS_OPTIONS);
  if (!check(str_equal(test_node_child_text(options,
      MAILACTIVESYNC_CP_RESOLVERECIPIENTS,
      MAILACTIVESYNC_RESOLVERECIPIENTS_MAX_AMBIGUOUS_RECIPIENTS), "2"),
      "ResolveRecipients MaxAmbiguousRecipients mismatch"))
    goto err;
  if (!check(str_equal(test_node_child_text(options,
      MAILACTIVESYNC_CP_RESOLVERECIPIENTS,
      MAILACTIVESYNC_RESOLVERECIPIENTS_CERTIFICATE_RETRIEVAL), "2") &&
      str_equal(test_node_child_text(options,
          MAILACTIVESYNC_CP_RESOLVERECIPIENTS,
          MAILACTIVESYNC_RESOLVERECIPIENTS_MAX_CERTIFICATES), "3"),
      "ResolveRecipients certificate option mismatch"))
    goto err;

  if (!check((result != NULL) && (result->status == 1) &&
      (clist_count(result->responses) == 3),
      "ResolveRecipients result summary mismatch"))
    goto err;
  cur = clist_begin(result->responses);
  response = clist_content(cur);
  if (!check((response != NULL) && str_equal(response->to, "jane") &&
      (response->status == 1) && (response->recipient_count == 1) &&
      (clist_count(response->recipients) == 1),
      "ResolveRecipients resolved response mismatch"))
    goto err;
  recipient = clist_content(clist_begin(response->recipients));
  if (!check((recipient != NULL) && (recipient->type == 1) &&
      str_equal(recipient->display_name, "Jane Example") &&
      str_equal(recipient->email_address, "jane@example.com") &&
      (recipient->certificates_status == 1) &&
      (recipient->certificate_count == 1) &&
      (recipient->certificates != NULL) &&
      (clist_count(recipient->certificates) == 1) &&
      str_equal(clist_content(clist_begin(recipient->certificates)),
          "cert-base64") &&
      str_equal(recipient->mini_certificate, "mini-base64"),
      "ResolveRecipients resolved recipient mismatch"))
    goto err;
  response = clist_content(clist_next(cur));
  if (!check((response != NULL) && str_equal(response->to, "jo") &&
      (response->status == 2) && (response->recipient_count == 2) &&
      (clist_count(response->recipients) == 1),
      "ResolveRecipients ambiguous response mismatch"))
    goto err;
  cur = clist_next(cur);
  response = clist_content(clist_next(cur));
  if (!check((response != NULL) && str_equal(response->to, "nocert") &&
      (response->status == 1) && (response->recipient_count == 1) &&
      (clist_count(response->recipients) == 1),
      "ResolveRecipients status-only response mismatch"))
    goto err;
  recipient = clist_content(clist_begin(response->recipients));
  if (!check((recipient != NULL) &&
      (recipient->certificates_status == 7) &&
      (recipient->certificates != NULL) &&
      (clist_count(recipient->certificates) == 0),
      "ResolveRecipients status-only recipient mismatch"))
    goto err;

  mailactivesync_wbxml_document_free(request_document);
  request_document = NULL;
  mailactivesync_resolve_recipients_result_free(result);
  result = NULL;

  root = resolve_recipients_response_new("6", 0);
  if (!check(root != NULL,
      "ResolveRecipients server-busy response allocation failed"))
    goto err;
  if (!check(fake_context_set_response_body(context, root) ==
      MAILACTIVESYNC_NO_ERROR,
      "ResolveRecipients server-busy response encode failed"))
    goto err;
  mailactivesync_wbxml_node_free(root);
  root = NULL;
  r = mailactivesync_resolve_recipients(session, recipients, 0, &result);
  if (!check((r == MAILACTIVESYNC_ERROR_SERVER_BUSY) &&
      (result != NULL) && (result->status == 6),
      "ResolveRecipients status mapping mismatch"))
    goto err;
  mailactivesync_resolve_recipients_result_free(result);
  result = NULL;

  root = mailactivesync_wbxml_node_new(
      MAILACTIVESYNC_CP_RESOLVERECIPIENTS,
      MAILACTIVESYNC_RESOLVERECIPIENTS_RESOLVE_RECIPIENTS);
  if (!check(root != NULL,
      "ResolveRecipients unresolved response root allocation failed"))
    goto err;
  unresolved_response = resolve_response_node_new("nobody", "4", "0", NULL);
  if ((test_node_add_text(root, MAILACTIVESYNC_CP_RESOLVERECIPIENTS,
          MAILACTIVESYNC_RESOLVERECIPIENTS_STATUS, "1") !=
          MAILACTIVESYNC_NO_ERROR) ||
      (unresolved_response == NULL) ||
      (mailactivesync_wbxml_node_add_child(root, unresolved_response) !=
          MAILACTIVESYNC_NO_ERROR))
    goto err;
  unresolved_response = NULL;
  if (!check(fake_context_set_response_body(context, root) ==
      MAILACTIVESYNC_NO_ERROR,
      "ResolveRecipients unresolved response encode failed"))
    goto err;
  mailactivesync_wbxml_node_free(root);
  root = NULL;
  r = mailactivesync_resolve_recipients(session, recipients, 0, &result);
  if (!check((r == MAILACTIVESYNC_ERROR_PROTOCOL) &&
      (result != NULL) && (result->status == 1) &&
      (clist_count(result->responses) == 1),
      "ResolveRecipients response status mapping mismatch"))
    goto err;
  response = clist_content(clist_begin(result->responses));
  if (!check((response != NULL) && str_equal(response->to, "nobody") &&
      (response->status == 4) && (response->recipient_count == 0),
      "ResolveRecipients unresolved response mismatch"))
    goto err;

  clist_free(recipients);
  mailactivesync_resolve_recipients_result_free(result);
  mailactivesync_free(session);
  return 1;

 err:
  clist_free(recipients);
  mailactivesync_wbxml_document_free(request_document);
  mailactivesync_wbxml_node_free(unresolved_response);
  mailactivesync_wbxml_node_free(root);
  mailactivesync_resolve_recipients_result_free(result);
  mailactivesync_free(session);
  return 0;
}

static int test_outlook_anchor_mailbox_cookie(void)
{
  mailactivesync * session;
  struct fake_context * context;
  struct mailactivesync_http_transport * transport;
  struct mailactivesync_options * options;
  const char * cookie;
  int r;

  options = NULL;
  context = NULL;
  session = mailactivesync_new();
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
      "https://eas.outlook.com/Microsoft-Server-ActiveSync") ==
      MAILACTIVESYNC_NO_ERROR, "connect failed"))
    goto err;
  if (!check(mailactivesync_login_oauth2(session, "user+test@example.com",
      "token-value") == MAILACTIVESYNC_NO_ERROR, "login failed"))
    goto err;

  r = mailactivesync_options(session, &options);
  if (!check(r == MAILACTIVESYNC_NO_ERROR, "OPTIONS failed"))
    goto err;

  cookie = request_header_value(context->last_request, "Cookie");
  if (!check((cookie != NULL) &&
      (strcmp(cookie, "DefaultAnchorMailbox=user%2Btest@example.com") == 0),
      "Outlook anchor mailbox cookie missing or malformed"))
    goto err;
  if (!check(request_header_value(context->last_request,
      "X-AnchorMailbox") == NULL,
      "X-AnchorMailbox should not be sent by default"))
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

static int test_anchor_mailbox_cookie_not_sent_to_office365(void)
{
  mailactivesync * session;
  struct fake_context * context;
  struct mailactivesync_http_transport * transport;
  struct mailactivesync_options * options;
  int r;

  options = NULL;
  context = NULL;
  session = mailactivesync_new();
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
      "https://outlook.office365.com/Microsoft-Server-ActiveSync") ==
      MAILACTIVESYNC_NO_ERROR, "connect failed"))
    goto err;
  if (!check(mailactivesync_login_oauth2(session, "user@example.com",
      "token-value") == MAILACTIVESYNC_NO_ERROR, "login failed"))
    goto err;

  r = mailactivesync_options(session, &options);
  if (!check(r == MAILACTIVESYNC_NO_ERROR, "OPTIONS failed"))
    goto err;
  if (!check(request_header_value(context->last_request, "Cookie") == NULL,
      "anchor mailbox cookie should not be sent to Office 365 host"))
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
  session = mailactivesync_new();
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
  session = mailactivesync_new();
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
  session = mailactivesync_new();
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

static struct mailactivesync_wbxml_node * folder_mutation_response_new(
    uint8_t root_token,
    const char * status,
    const char * sync_key,
    const char * server_id)
{
  struct mailactivesync_wbxml_node * root;

  root = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_FOLDERHIERARCHY,
      root_token);
  if (root == NULL)
    return NULL;

  if ((test_node_add_text(root, MAILACTIVESYNC_CP_FOLDERHIERARCHY,
          MAILACTIVESYNC_FOLDER_STATUS, status) !=
          MAILACTIVESYNC_NO_ERROR) ||
      ((sync_key != NULL) &&
          (test_node_add_text(root, MAILACTIVESYNC_CP_FOLDERHIERARCHY,
              MAILACTIVESYNC_FOLDER_SYNC_KEY, sync_key) !=
              MAILACTIVESYNC_NO_ERROR)) ||
      ((server_id != NULL) &&
          (test_node_add_text(root, MAILACTIVESYNC_CP_FOLDERHIERARCHY,
              MAILACTIVESYNC_FOLDER_SERVER_ID, server_id) !=
              MAILACTIVESYNC_NO_ERROR))) {
    mailactivesync_wbxml_node_free(root);
    return NULL;
  }

  return root;
}

static int test_folder_mutation_success_and_status(void)
{
  mailactivesync * session;
  struct fake_context * context;
  struct mailactivesync_folder_mutation_result * result;
  struct mailactivesync_folder_sync_result * sync_result;
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_node * changes;
  struct mailactivesync_wbxml_node * folder;
  struct mailactivesync_wbxml_document * request_document;
  int r;

  session = NULL;
  context = NULL;
  result = NULL;
  sync_result = NULL;
  root = NULL;
  changes = NULL;
  folder = NULL;
  request_document = NULL;

  if (!check(setup_oauth_session(&session, &context) ==
      MAILACTIVESYNC_NO_ERROR, "setup OAuth session failed"))
    return 0;

  root = folder_mutation_response_new(MAILACTIVESYNC_FOLDER_FOLDER_CREATE,
      "1", "2", "folder-1");
  if (!check(root != NULL, "FolderCreate response allocation failed"))
    goto err;
  if (!check(fake_context_set_response_body(context, root) ==
      MAILACTIVESYNC_NO_ERROR, "FolderCreate response encode failed"))
    goto err;
  root = NULL;

  r = mailactivesync_folder_create(session, "1", "parent-1",
      "Test Folder", 12, &result);
  if (!check(r == MAILACTIVESYNC_NO_ERROR, "FolderCreate failed"))
    goto err;
  if (!check(strstr(context->last_request->url, "Cmd=FolderCreate") != NULL,
      "FolderCreate command URL mismatch"))
    goto err;
  if (!check(mailactivesync_wbxml_decode(context->last_request->body,
      context->last_request->body_len, &request_document) ==
      MAILACTIVESYNC_NO_ERROR, "FolderCreate request decode failed"))
    goto err;
  if (!check((request_document->root != NULL) &&
      (request_document->root->code_page ==
          MAILACTIVESYNC_CP_FOLDERHIERARCHY) &&
      (request_document->root->token == MAILACTIVESYNC_FOLDER_FOLDER_CREATE) &&
      str_equal(test_node_child_text(request_document->root,
          MAILACTIVESYNC_CP_FOLDERHIERARCHY,
          MAILACTIVESYNC_FOLDER_SYNC_KEY), "1") &&
      str_equal(test_node_child_text(request_document->root,
          MAILACTIVESYNC_CP_FOLDERHIERARCHY,
          MAILACTIVESYNC_FOLDER_PARENT_ID), "parent-1") &&
      str_equal(test_node_child_text(request_document->root,
          MAILACTIVESYNC_CP_FOLDERHIERARCHY,
          MAILACTIVESYNC_FOLDER_DISPLAY_NAME), "Test Folder") &&
      str_equal(test_node_child_text(request_document->root,
          MAILACTIVESYNC_CP_FOLDERHIERARCHY,
          MAILACTIVESYNC_FOLDER_TYPE), "12"),
      "FolderCreate request mismatch"))
    goto err;
  if (!check((result->status == 1) &&
      str_equal(result->sync_key, "2") &&
      str_equal(result->server_id, "folder-1"),
      "FolderCreate result mismatch"))
    goto err;
  mailactivesync_wbxml_document_free(request_document);
  request_document = NULL;
  mailactivesync_folder_mutation_result_free(result);
  result = NULL;

  root = folder_mutation_response_new(MAILACTIVESYNC_FOLDER_FOLDER_UPDATE,
      "1", "3", NULL);
  if (!check(root != NULL, "FolderUpdate response allocation failed"))
    goto err;
  if (!check(fake_context_set_response_body(context, root) ==
      MAILACTIVESYNC_NO_ERROR, "FolderUpdate response encode failed"))
    goto err;
  root = NULL;

  r = mailactivesync_folder_update(session, "2", "folder-1",
      "parent-2", "Renamed Folder", &result);
  if (!check(r == MAILACTIVESYNC_NO_ERROR, "FolderUpdate failed"))
    goto err;
  if (!check(strstr(context->last_request->url, "Cmd=FolderUpdate") != NULL,
      "FolderUpdate command URL mismatch"))
    goto err;
  if (!check(mailactivesync_wbxml_decode(context->last_request->body,
      context->last_request->body_len, &request_document) ==
      MAILACTIVESYNC_NO_ERROR, "FolderUpdate request decode failed"))
    goto err;
  if (!check((request_document->root != NULL) &&
      (request_document->root->token == MAILACTIVESYNC_FOLDER_FOLDER_UPDATE) &&
      str_equal(test_node_child_text(request_document->root,
          MAILACTIVESYNC_CP_FOLDERHIERARCHY,
          MAILACTIVESYNC_FOLDER_SYNC_KEY), "2") &&
      str_equal(test_node_child_text(request_document->root,
          MAILACTIVESYNC_CP_FOLDERHIERARCHY,
          MAILACTIVESYNC_FOLDER_SERVER_ID), "folder-1") &&
      str_equal(test_node_child_text(request_document->root,
          MAILACTIVESYNC_CP_FOLDERHIERARCHY,
          MAILACTIVESYNC_FOLDER_PARENT_ID), "parent-2") &&
      str_equal(test_node_child_text(request_document->root,
          MAILACTIVESYNC_CP_FOLDERHIERARCHY,
          MAILACTIVESYNC_FOLDER_DISPLAY_NAME), "Renamed Folder") &&
      str_equal(result->sync_key, "3"),
      "FolderUpdate request/result mismatch"))
    goto err;
  mailactivesync_wbxml_document_free(request_document);
  request_document = NULL;
  mailactivesync_folder_mutation_result_free(result);
  result = NULL;

  root = folder_mutation_response_new(MAILACTIVESYNC_FOLDER_FOLDER_DELETE,
      "1", "4", NULL);
  if (!check(root != NULL, "FolderDelete response allocation failed"))
    goto err;
  if (!check(fake_context_set_response_body(context, root) ==
      MAILACTIVESYNC_NO_ERROR, "FolderDelete response encode failed"))
    goto err;
  root = NULL;

  r = mailactivesync_folder_delete(session, "3", "folder-1", &result);
  if (!check(r == MAILACTIVESYNC_NO_ERROR, "FolderDelete failed"))
    goto err;
  if (!check(strstr(context->last_request->url, "Cmd=FolderDelete") != NULL,
      "FolderDelete command URL mismatch"))
    goto err;
  if (!check(mailactivesync_wbxml_decode(context->last_request->body,
      context->last_request->body_len, &request_document) ==
      MAILACTIVESYNC_NO_ERROR, "FolderDelete request decode failed"))
    goto err;
  if (!check((request_document->root != NULL) &&
      (request_document->root->token == MAILACTIVESYNC_FOLDER_FOLDER_DELETE) &&
      str_equal(test_node_child_text(request_document->root,
          MAILACTIVESYNC_CP_FOLDERHIERARCHY,
          MAILACTIVESYNC_FOLDER_SYNC_KEY), "3") &&
      str_equal(test_node_child_text(request_document->root,
          MAILACTIVESYNC_CP_FOLDERHIERARCHY,
          MAILACTIVESYNC_FOLDER_SERVER_ID), "folder-1") &&
      str_equal(result->sync_key, "4"),
      "FolderDelete request/result mismatch"))
    goto err;
  mailactivesync_wbxml_document_free(request_document);
  request_document = NULL;
  mailactivesync_folder_mutation_result_free(result);
  result = NULL;

  root = folder_mutation_response_new(MAILACTIVESYNC_FOLDER_FOLDER_CREATE,
      "9", NULL, NULL);
  if (!check(root != NULL, "FolderCreate invalid-key response allocation failed"))
    goto err;
  if (!check(fake_context_set_response_body(context, root) ==
      MAILACTIVESYNC_NO_ERROR, "FolderCreate invalid-key response encode failed"))
    goto err;
  root = NULL;
  r = mailactivesync_folder_create(session, "bad", "parent-1",
      "Duplicate", 12, &result);
  if (!check((r == MAILACTIVESYNC_ERROR_INVALID_SYNC_KEY) &&
      (result != NULL) && (result->status == 9) &&
      (result->sync_key == NULL),
      "FolderCreate invalid-key status mapping mismatch"))
    goto err;
  if (!check(mailactivesync_folder_mutation_result_needs_resync(result),
      "FolderCreate invalid-key result did not require resync"))
    goto err;
  mailactivesync_folder_mutation_result_free(result);
  result = NULL;

  root = mailactivesync_wbxml_node_new(
      MAILACTIVESYNC_CP_FOLDERHIERARCHY,
      MAILACTIVESYNC_FOLDER_FOLDER_SYNC);
  changes = mailactivesync_wbxml_node_new(
      MAILACTIVESYNC_CP_FOLDERHIERARCHY,
      MAILACTIVESYNC_FOLDER_CHANGES);
  folder = mailactivesync_wbxml_node_new(
      MAILACTIVESYNC_CP_FOLDERHIERARCHY, MAILACTIVESYNC_FOLDER_ADD);
  if (!check((root != NULL) && (changes != NULL) && (folder != NULL),
      "Folder resync fixture allocation failed"))
    goto err;
  if (!check(test_node_add_text(root, MAILACTIVESYNC_CP_FOLDERHIERARCHY,
      MAILACTIVESYNC_FOLDER_SYNC_KEY, "resync-key") ==
      MAILACTIVESYNC_NO_ERROR, "Folder resync SyncKey add failed"))
    goto err;
  if (!check(test_node_add_text(root, MAILACTIVESYNC_CP_FOLDERHIERARCHY,
      MAILACTIVESYNC_FOLDER_STATUS, "1") == MAILACTIVESYNC_NO_ERROR,
      "Folder resync Status add failed"))
    goto err;
  if (!check(test_node_add_text(folder, MAILACTIVESYNC_CP_FOLDERHIERARCHY,
      MAILACTIVESYNC_FOLDER_SERVER_ID, "inbox") == MAILACTIVESYNC_NO_ERROR,
      "Folder resync ServerId add failed"))
    goto err;
  if (!check(test_node_add_text(folder, MAILACTIVESYNC_CP_FOLDERHIERARCHY,
      MAILACTIVESYNC_FOLDER_PARENT_ID, "0") == MAILACTIVESYNC_NO_ERROR,
      "Folder resync ParentId add failed"))
    goto err;
  if (!check(test_node_add_text(folder, MAILACTIVESYNC_CP_FOLDERHIERARCHY,
      MAILACTIVESYNC_FOLDER_DISPLAY_NAME, "Inbox") ==
      MAILACTIVESYNC_NO_ERROR, "Folder resync DisplayName add failed"))
    goto err;
  if (!check(test_node_add_text(folder, MAILACTIVESYNC_CP_FOLDERHIERARCHY,
      MAILACTIVESYNC_FOLDER_TYPE, "2") == MAILACTIVESYNC_NO_ERROR,
      "Folder resync Type add failed"))
    goto err;
  if (!check(mailactivesync_wbxml_node_add_child(changes, folder) ==
      MAILACTIVESYNC_NO_ERROR, "Folder resync add append failed"))
    goto err;
  folder = NULL;
  if (!check(mailactivesync_wbxml_node_add_child(root, changes) ==
      MAILACTIVESYNC_NO_ERROR, "Folder resync changes append failed"))
    goto err;
  changes = NULL;
  if (!check(fake_context_set_response_body(context, root) ==
      MAILACTIVESYNC_NO_ERROR, "Folder resync response encode failed"))
    goto err;
  mailactivesync_wbxml_node_free(root);
  root = NULL;

  r = mailactivesync_folder_resync(session, &sync_result);
  if (!check(r == MAILACTIVESYNC_NO_ERROR, "Folder resync command failed"))
    goto err;
  if (!check(strstr(context->last_request->url, "Cmd=FolderSync") != NULL,
      "Folder resync command URL mismatch"))
    goto err;
  if (!check(mailactivesync_wbxml_decode(context->last_request->body,
      context->last_request->body_len, &request_document) ==
      MAILACTIVESYNC_NO_ERROR, "Folder resync request decode failed"))
    goto err;
  if (!check(str_equal(test_node_child_text(request_document->root,
      MAILACTIVESYNC_CP_FOLDERHIERARCHY,
      MAILACTIVESYNC_FOLDER_SYNC_KEY), "0"),
      "Folder resync did not request hierarchy SyncKey 0"))
    goto err;
  if (!check((sync_result != NULL) &&
      str_equal(sync_result->sync_key, "resync-key") &&
      (sync_result->status == 1) && (clist_count(sync_result->added) == 1),
      "Folder resync result mismatch"))
    goto err;

  mailactivesync_wbxml_document_free(request_document);
  mailactivesync_folder_sync_result_free(sync_result);
  mailactivesync_free(session);
  return 1;

 err:
  mailactivesync_wbxml_document_free(request_document);
  mailactivesync_wbxml_node_free(folder);
  mailactivesync_wbxml_node_free(changes);
  mailactivesync_wbxml_node_free(root);
  mailactivesync_folder_sync_result_free(sync_result);
  mailactivesync_folder_mutation_result_free(result);
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
  static const unsigned char conversation_id[] = { 0x10, 0x20, 0x30 };
  static const unsigned char conversation_index[] =
      { 0x40, 0x50, 0x60, 0x70 };
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
  session = mailactivesync_new();
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
      MAILACTIVESYNC_EMAIL_DISPLAY_TO, "User One") ==
      MAILACTIVESYNC_NO_ERROR, "Sync display-to add failed"))
    goto err;
  if (!check(test_node_add_text(app_data, MAILACTIVESYNC_CP_EMAIL,
      MAILACTIVESYNC_EMAIL_IMPORTANCE, "1") ==
      MAILACTIVESYNC_NO_ERROR, "Sync importance add failed"))
    goto err;
  if (!check(test_node_add_text(app_data, MAILACTIVESYNC_CP_EMAIL,
      MAILACTIVESYNC_EMAIL_THREAD_TOPIC, "Hello thread") ==
      MAILACTIVESYNC_NO_ERROR, "Sync thread topic add failed"))
    goto err;
  if (!check(test_node_add_text(app_data, MAILACTIVESYNC_CP_EMAIL,
      MAILACTIVESYNC_EMAIL_INTERNET_CPID, "65001") ==
      MAILACTIVESYNC_NO_ERROR, "Sync internet cpid add failed"))
    goto err;
  if (!check(test_node_add_text(app_data, MAILACTIVESYNC_CP_EMAIL,
      MAILACTIVESYNC_EMAIL_CONTENT_CLASS, "urn:content-classes:message") ==
      MAILACTIVESYNC_NO_ERROR, "Sync content class add failed"))
    goto err;
  if (!check(test_node_add_opaque(app_data, MAILACTIVESYNC_CP_EMAIL2,
      MAILACTIVESYNC_EMAIL2_CONVERSATION_ID, conversation_id,
      sizeof(conversation_id)) == MAILACTIVESYNC_NO_ERROR,
      "Sync conversation id add failed"))
    goto err;
  if (!check(test_node_add_opaque(app_data, MAILACTIVESYNC_CP_EMAIL2,
      MAILACTIVESYNC_EMAIL2_CONVERSATION_INDEX, conversation_index,
      sizeof(conversation_index)) == MAILACTIVESYNC_NO_ERROR,
      "Sync conversation index add failed"))
    goto err;
  if (!check(test_node_add_text(app_data, MAILACTIVESYNC_CP_EMAIL,
      MAILACTIVESYNC_EMAIL_CATEGORIES, NULL) == MAILACTIVESYNC_NO_ERROR,
      "Sync categories add failed"))
    goto err;
  if (!check((test_node_add_text(test_node_child(app_data,
      MAILACTIVESYNC_CP_EMAIL, MAILACTIVESYNC_EMAIL_CATEGORIES),
      MAILACTIVESYNC_CP_EMAIL, MAILACTIVESYNC_EMAIL_CATEGORY, "Red") ==
      MAILACTIVESYNC_NO_ERROR) &&
      (test_node_add_text(test_node_child(app_data, MAILACTIVESYNC_CP_EMAIL,
      MAILACTIVESYNC_EMAIL_CATEGORIES), MAILACTIVESYNC_CP_EMAIL,
      MAILACTIVESYNC_EMAIL_CATEGORY, "Follow up") ==
      MAILACTIVESYNC_NO_ERROR), "Sync category values add failed"))
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
  if (!check(mailactivesync_sync_request_set_conversation_mode(request, 1) ==
      MAILACTIVESYNC_NO_ERROR, "Sync request ConversationMode set failed"))
    goto err;
  if (!check(mailactivesync_sync_request_set_wait(request, 10) ==
      MAILACTIVESYNC_NO_ERROR, "Sync request Wait set failed"))
    goto err;
  if (!check(mailactivesync_sync_request_set_collection_class(request,
      "Email") == MAILACTIVESYNC_NO_ERROR,
      "Sync request collection class set failed"))
    goto err;
  if (!check(mailactivesync_sync_request_set_deletes_as_moves(request, 0) ==
      MAILACTIVESYNC_NO_ERROR, "Sync request DeletesAsMoves set failed"))
    goto err;
  if (!check(mailactivesync_sync_request_set_filter_type(request, 2) ==
      MAILACTIVESYNC_NO_ERROR, "Sync request FilterType set failed"))
    goto err;
  if (!check(mailactivesync_sync_request_set_conflict(request, 1) ==
      MAILACTIVESYNC_NO_ERROR, "Sync request Conflict set failed"))
    goto err;
  if (!check(mailactivesync_sync_request_set_rights_management_support(request,
      1) == MAILACTIVESYNC_NO_ERROR,
      "Sync request RightsManagementSupport set failed"))
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
  if (!check(test_node_child(request_collection, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_CLASS) == NULL,
      "Sync request unexpectedly included collection-level Class"))
    goto err;
  if (!check(strcmp(test_node_child_text(request_collection,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_DELETES_AS_MOVES),
      "0") == 0, "Sync request DeletesAsMoves mismatch"))
    goto err;
  if (!check(strcmp(test_node_child_text(request_collection,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_WINDOW_SIZE),
      "25") == 0, "Sync request WindowSize mismatch"))
    goto err;
  if (!check(strcmp(test_node_child_text(request_collection,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_CONVERSATION_MODE),
      "1") == 0, "Sync request ConversationMode mismatch"))
    goto err;
  if (!check(strcmp(test_node_child_text(request_document->root,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_WAIT),
      "10") == 0, "Sync request Wait mismatch"))
    goto err;
  if (!check(test_node_child(request_document->root,
      MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_HEARTBEAT_INTERVAL) == NULL,
      "Sync request unexpectedly included HeartbeatInterval"))
    goto err;
  if (!check(strcmp(test_node_child_text(request_options,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_CLASS),
      "Email") == 0, "Sync request Options Class mismatch"))
    goto err;
  if (!check(strcmp(test_node_child_text(request_options,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_FILTER_TYPE),
      "2") == 0, "Sync request FilterType mismatch"))
    goto err;
  if (!check(strcmp(test_node_child_text(request_options,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_CONFLICT),
      "1") == 0, "Sync request Conflict mismatch"))
    goto err;
  if (!check(strcmp(test_node_child_text(request_options,
      MAILACTIVESYNC_CP_RIGHTSMANAGEMENT,
      MAILACTIVESYNC_RIGHTSMANAGEMENT_RIGHTS_MANAGEMENT_SUPPORT),
      "1") == 0, "Sync request RightsManagementSupport mismatch"))
    goto err;
  if (!check(strcmp(test_node_child_text(request_options,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_MIME_SUPPORT),
      "2") == 0, "Sync request MIMESupport mismatch"))
    goto err;
  if (!check(strcmp(test_node_child_text(request_body_pref,
      MAILACTIVESYNC_CP_AIRSYNCBASE, MAILACTIVESYNC_AIRSYNCBASE_TYPE),
      "4") == 0, "Sync request body type mismatch"))
    goto err;
  if (!check(strcmp(test_node_child_text(request_body_pref,
      MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_TRUNCATION_SIZE), "204800") == 0,
      "Sync request body truncation mismatch"))
    goto err;

  if (!check((result->sync_key != NULL) &&
      (strcmp(result->sync_key, "222") == 0),
      "Sync parsed SyncKey mismatch"))
    goto err;
  if (!check((result->status == 1) && result->more_available &&
      !result->empty_response && result->sync_key_from_response,
      "Sync parsed status/more/flag mismatch"))
    goto err;
  if (!check((clist_count(result->added) == 1) &&
      (clist_count(result->deleted) == 1), "Sync result counts mismatch"))
    goto err;
  message = clist_content(clist_begin(result->added));
  if (!check((message != NULL) &&
      (strcmp(message->server_id, "msg-1") == 0) &&
      (strcmp(message->subject, "Hello") == 0) &&
      (strcmp(message->from, "sender@example.com") == 0) &&
      (strcmp(message->display_to, "User One") == 0) &&
      (message->importance == 1) &&
      (strcmp(message->thread_topic, "Hello thread") == 0) &&
      str_equal(message->internet_cpid, "65001") &&
      str_equal(message->content_class, "urn:content-classes:message") &&
      (message->conversation_id_len == sizeof(conversation_id)) &&
      (memcmp(message->conversation_id, conversation_id,
          sizeof(conversation_id)) == 0) &&
      (message->conversation_index_len == sizeof(conversation_index)) &&
      (memcmp(message->conversation_index, conversation_index,
          sizeof(conversation_index)) == 0) &&
      (message->categories != NULL) &&
      (clist_count(message->categories) == 2) &&
      str_equal(clist_content(clist_begin(message->categories)), "Red") &&
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

static int test_sync_multi_collection_success(void)
{
  mailactivesync * session;
  struct fake_context * context;
  struct mailactivesync_sync_request * request_1;
  struct mailactivesync_sync_request * request_2;
  struct mailactivesync_sync_result * result;
  struct mailactivesync_sync_result * collection_result;
  struct mailactivesync_message * message;
  clist * requests;
  clistiter * cur;
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_node * collections;
  struct mailactivesync_wbxml_node * collection;
  struct mailactivesync_wbxml_node * commands;
  struct mailactivesync_wbxml_node * command;
  struct mailactivesync_wbxml_node * app_data;
  struct mailactivesync_wbxml_document * request_document;
  struct mailactivesync_wbxml_node * request_collections;
  struct mailactivesync_wbxml_node * request_collection;
  struct mailactivesync_wbxml_node * request_options;
  int r;

  session = NULL;
  context = NULL;
  request_1 = NULL;
  request_2 = NULL;
  result = NULL;
  requests = NULL;
  root = NULL;
  collections = NULL;
  collection = NULL;
  commands = NULL;
  command = NULL;
  app_data = NULL;
  request_document = NULL;

  if (!check(setup_oauth_session(&session, &context) ==
      MAILACTIVESYNC_NO_ERROR, "setup OAuth session failed"))
    return 0;

  root = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_SYNC);
  collections = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_COLLECTIONS);
  if (!check((root != NULL) && (collections != NULL),
      "Sync multi response allocation failed"))
    goto err;

  collection = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_COLLECTION);
  commands = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_COMMANDS);
  command = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_ADD);
  app_data = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_APPLICATION_DATA);
  if (!check((collection != NULL) && (commands != NULL) &&
      (command != NULL) && (app_data != NULL),
      "Sync multi first collection allocation failed"))
    goto err;
  if (!check(test_node_add_text(collection, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_COLLECTION_ID, "inbox") ==
      MAILACTIVESYNC_NO_ERROR,
      "Sync multi first CollectionId add failed"))
    goto err;
  if (!check(test_node_add_text(collection, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_SYNC_KEY, "222") == MAILACTIVESYNC_NO_ERROR,
      "Sync multi first SyncKey add failed"))
    goto err;
  if (!check(test_node_add_text(collection, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_STATUS, "1") == MAILACTIVESYNC_NO_ERROR,
      "Sync multi first Status add failed"))
    goto err;
  if (!check(test_node_add_text(command, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_SERVER_ID, "msg-1") ==
      MAILACTIVESYNC_NO_ERROR,
      "Sync multi first ServerId add failed"))
    goto err;
  if (!check(test_node_add_text(app_data, MAILACTIVESYNC_CP_EMAIL,
      MAILACTIVESYNC_EMAIL_SUBJECT, "Inbox subject") ==
      MAILACTIVESYNC_NO_ERROR,
      "Sync multi first Subject add failed"))
    goto err;
  if (!check(mailactivesync_wbxml_node_add_child(command, app_data) ==
      MAILACTIVESYNC_NO_ERROR, "Sync multi first app data append failed"))
    goto err;
  app_data = NULL;
  if (!check(mailactivesync_wbxml_node_add_child(commands, command) ==
      MAILACTIVESYNC_NO_ERROR, "Sync multi first command append failed"))
    goto err;
  command = NULL;
  if (!check(mailactivesync_wbxml_node_add_child(collection, commands) ==
      MAILACTIVESYNC_NO_ERROR, "Sync multi first commands append failed"))
    goto err;
  commands = NULL;
  if (!check(mailactivesync_wbxml_node_add_child(collections, collection) ==
      MAILACTIVESYNC_NO_ERROR, "Sync multi first collection append failed"))
    goto err;
  collection = NULL;

  collection = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_COLLECTION);
  commands = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_COMMANDS);
  command = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_DELETE);
  if (!check((collection != NULL) && (commands != NULL) &&
      (command != NULL), "Sync multi second collection allocation failed"))
    goto err;
  if (!check(test_node_add_text(collection, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_COLLECTION_ID, "archive") ==
      MAILACTIVESYNC_NO_ERROR,
      "Sync multi second CollectionId add failed"))
    goto err;
  if (!check(test_node_add_text(collection, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_SYNC_KEY, "333") == MAILACTIVESYNC_NO_ERROR,
      "Sync multi second SyncKey add failed"))
    goto err;
  if (!check(test_node_add_text(collection, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_STATUS, "1") == MAILACTIVESYNC_NO_ERROR,
      "Sync multi second Status add failed"))
    goto err;
  if (!check(test_node_add_empty(collection, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_MORE_AVAILABLE) == MAILACTIVESYNC_NO_ERROR,
      "Sync multi second MoreAvailable add failed"))
    goto err;
  if (!check(test_node_add_text(command, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_SERVER_ID, "msg-2") ==
      MAILACTIVESYNC_NO_ERROR,
      "Sync multi second ServerId add failed"))
    goto err;
  if (!check(mailactivesync_wbxml_node_add_child(commands, command) ==
      MAILACTIVESYNC_NO_ERROR, "Sync multi second command append failed"))
    goto err;
  command = NULL;
  if (!check(mailactivesync_wbxml_node_add_child(collection, commands) ==
      MAILACTIVESYNC_NO_ERROR, "Sync multi second commands append failed"))
    goto err;
  commands = NULL;
  if (!check(mailactivesync_wbxml_node_add_child(collections, collection) ==
      MAILACTIVESYNC_NO_ERROR, "Sync multi second collection append failed"))
    goto err;
  collection = NULL;

  if (!check(mailactivesync_wbxml_node_add_child(root, collections) ==
      MAILACTIVESYNC_NO_ERROR, "Sync multi collections append failed"))
    goto err;
  collections = NULL;
  if (!check(fake_context_set_response_body(context, root) ==
      MAILACTIVESYNC_NO_ERROR, "Sync multi response encode failed"))
    goto err;
  mailactivesync_wbxml_node_free(root);
  root = NULL;

  request_1 = mailactivesync_sync_request_new("inbox", "111");
  request_2 = mailactivesync_sync_request_new("archive", "222");
  requests = clist_new();
  if (!check((request_1 != NULL) && (request_2 != NULL) &&
      (requests != NULL), "Sync multi request allocation failed"))
    goto err;
  if (!check(mailactivesync_sync_request_set_wait(request_1, 10) ==
      MAILACTIVESYNC_NO_ERROR, "Sync multi Wait set failed"))
    goto err;
  if (!check(mailactivesync_sync_request_set_collection_class(request_1,
      "Email") == MAILACTIVESYNC_NO_ERROR,
      "Sync multi first Class set failed"))
    goto err;
  if (!check(mailactivesync_sync_request_set_filter_type(request_2, 2) ==
      MAILACTIVESYNC_NO_ERROR, "Sync multi second FilterType set failed"))
    goto err;
  if (!check(clist_append(requests, request_1) == 0,
      "Sync multi first request append failed"))
    goto err;
  if (!check(clist_append(requests, request_2) == 0,
      "Sync multi second request append failed"))
    goto err;

  r = mailactivesync_sync_multi(session, requests, &result);
  if (!check(r == MAILACTIVESYNC_NO_ERROR, "Sync multi command failed"))
    goto err;
  if (!check(mailactivesync_wbxml_decode(context->last_request->body,
      context->last_request->body_len, &request_document) ==
      MAILACTIVESYNC_NO_ERROR, "Sync multi request decode failed"))
    goto err;
  request_collections = test_node_child(request_document->root,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_COLLECTIONS);
  request_collection = test_node_child_at(request_collections,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_COLLECTION, 0);
  request_options = test_node_child(request_collection,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_OPTIONS);
  if (!check(str_equal(test_node_child_text(request_collection,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_COLLECTION_ID),
      "inbox") && str_equal(test_node_child_text(request_collection,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_SYNC_KEY), "111") &&
      str_equal(test_node_child_text(request_options,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_CLASS), "Email"),
      "Sync multi first request collection mismatch"))
    goto err;
  request_collection = test_node_child_at(request_collections,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_COLLECTION, 1);
  request_options = test_node_child(request_collection,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_OPTIONS);
  if (!check(str_equal(test_node_child_text(request_collection,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_COLLECTION_ID),
      "archive") && str_equal(test_node_child_text(request_options,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_FILTER_TYPE), "2") &&
      str_equal(test_node_child_text(request_document->root,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_WAIT), "10"),
      "Sync multi second request/top-level mismatch"))
    goto err;

  if (!check((result != NULL) && (result->collections != NULL) &&
      (clist_count(result->collections) == 2),
      "Sync multi result collection count mismatch"))
    goto err;
  cur = clist_begin(result->collections);
  collection_result = clist_content(cur);
  message = clist_content(clist_begin(collection_result->added));
  if (!check((collection_result != NULL) &&
      str_equal(collection_result->collection_id, "inbox") &&
      str_equal(collection_result->sync_key, "222") &&
      (collection_result->status == 1) &&
      (message != NULL) && str_equal(message->server_id, "msg-1") &&
      str_equal(message->subject, "Inbox subject"),
      "Sync multi first parsed collection mismatch"))
    goto err;
  collection_result = mailactivesync_sync_result_collection(result, "inbox");
  if (!check((collection_result != NULL) &&
      str_equal(collection_result->collection_id, "inbox") &&
      (mailactivesync_sync_result_collection_status_to_error(result,
          "inbox") == MAILACTIVESYNC_NO_ERROR),
      "Sync multi first collection helper mismatch"))
    goto err;
  cur = clist_next(cur);
  collection_result = clist_content(cur);
  if (!check((collection_result != NULL) &&
      str_equal(collection_result->collection_id, "archive") &&
      str_equal(collection_result->sync_key, "333") &&
      (collection_result->status == 1) &&
      collection_result->more_available &&
      (clist_count(collection_result->deleted) == 1) &&
      str_equal(clist_content(clist_begin(collection_result->deleted)),
          "msg-2"),
      "Sync multi second parsed collection mismatch"))
    goto err;
  collection_result = mailactivesync_sync_result_collection(result, "archive");
  if (!check((collection_result != NULL) &&
      str_equal(collection_result->collection_id, "archive") &&
      (mailactivesync_sync_result_collection_status_to_error(result,
          "archive") == MAILACTIVESYNC_NO_ERROR),
      "Sync multi second collection helper mismatch"))
    goto err;
  collection_result->status = 3;
  if (!check(mailactivesync_sync_result_collection_status_to_error(result,
      "archive") == MAILACTIVESYNC_ERROR_FOLDER_RESYNC_REQUIRED,
      "Sync multi collection status helper did not map status 3"))
    goto err;
  if (!check((mailactivesync_sync_result_collection(result, "missing") ==
      NULL) && (mailactivesync_sync_result_collection_status_to_error(result,
      "missing") == MAILACTIVESYNC_ERROR_PROTOCOL),
      "Sync multi missing collection helper mismatch"))
    goto err;

  mailactivesync_wbxml_document_free(request_document);
  mailactivesync_sync_result_free(result);
  clist_free(requests);
  mailactivesync_sync_request_free(request_1);
  mailactivesync_sync_request_free(request_2);
  mailactivesync_free(session);
  return 1;

 err:
  mailactivesync_wbxml_document_free(request_document);
  mailactivesync_wbxml_node_free(app_data);
  mailactivesync_wbxml_node_free(command);
  mailactivesync_wbxml_node_free(commands);
  mailactivesync_wbxml_node_free(collection);
  mailactivesync_wbxml_node_free(collections);
  mailactivesync_wbxml_node_free(root);
  mailactivesync_sync_result_free(result);
  clist_free(requests);
  mailactivesync_sync_request_free(request_1);
  mailactivesync_sync_request_free(request_2);
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
  struct mailactivesync_wbxml_node * request_options;
  struct mailactivesync_wbxml_node * request_body_pref;
  struct mailactivesync_wbxml_node * request_body_pref_2;
  int r;

  session = NULL;
  context = NULL;
  request = NULL;
  result = NULL;
  root = NULL;
  collection = NULL;
  request_document = NULL;
  request_body_pref_2 = NULL;

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
  if (!check(mailactivesync_sync_request_set_body_preference(request,
      MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_PLAIN_TEXT, 1024) ==
      MAILACTIVESYNC_NO_ERROR, "set plain BodyPreference failed"))
    goto err;
  if (!check(mailactivesync_sync_request_set_body_preference_all_or_none(
      request, 1) == MAILACTIVESYNC_NO_ERROR,
      "set plain BodyPreference AllOrNone failed"))
    goto err;
  if (!check(mailactivesync_sync_request_add_body_preference(request,
      MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_MIME, 4096) ==
      MAILACTIVESYNC_NO_ERROR, "append MIME BodyPreference failed"))
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
  request_options = test_node_child(request_collection,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_OPTIONS);
  request_body_pref = test_node_child(request_options,
      MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_BODY_PREFERENCE);
  request_body_pref_2 = test_node_child_at(request_options,
      MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_BODY_PREFERENCE, 1);
  if (!check(request_body_pref_2 != NULL,
      "second Sync request BodyPreference missing"))
    goto err;
  if (!check(test_node_child_at(request_options,
      MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_BODY_PREFERENCE, 2) == NULL,
      "Sync request included too many BodyPreference nodes"))
    goto err;
  if (!check(strcmp(test_node_child_text(request_body_pref,
      MAILACTIVESYNC_CP_AIRSYNCBASE, MAILACTIVESYNC_AIRSYNCBASE_TYPE),
      "1") == 0, "plain Sync request body type mismatch"))
    goto err;
  if (!check(strcmp(test_node_child_text(request_body_pref,
      MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_TRUNCATION_SIZE), "1024") == 0,
      "plain Sync request body truncation mismatch"))
    goto err;
  if (!check(strcmp(test_node_child_text(request_body_pref,
      MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_ALL_OR_NONE), "1") == 0,
      "plain Sync request body AllOrNone mismatch"))
    goto err;
  if (!check(strcmp(test_node_child_text(request_body_pref_2,
      MAILACTIVESYNC_CP_AIRSYNCBASE, MAILACTIVESYNC_AIRSYNCBASE_TYPE),
      "4") == 0, "MIME Sync request body type mismatch"))
    goto err;
  if (!check(strcmp(test_node_child_text(request_body_pref_2,
      MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_TRUNCATION_SIZE), "4096") == 0,
      "MIME Sync request body truncation mismatch"))
    goto err;
  if (!check(test_node_child(request_body_pref_2,
      MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_ALL_OR_NONE) == NULL,
      "MIME Sync request unexpectedly included AllOrNone"))
    goto err;
  if (!check((result->sync_key != NULL) &&
      (strcmp(result->sync_key, "333") == 0) &&
      (result->status == 5) && !result->more_available &&
      !result->empty_response && result->sync_key_from_response &&
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

static int test_sync_activesync_25_collection_class_shape(void)
{
  mailactivesync * session;
  struct fake_context * context;
  struct mailactivesync_sync_request * request;
  struct mailactivesync_sync_result * result;
  struct mailactivesync_wbxml_document * request_document;
  struct mailactivesync_wbxml_node * request_collections;
  struct mailactivesync_wbxml_node * request_collection;
  struct mailactivesync_wbxml_node * request_options;
  int r;

  session = NULL;
  context = NULL;
  request = NULL;
  result = NULL;
  request_document = NULL;

  if (!check(setup_oauth_session(&session, &context) ==
      MAILACTIVESYNC_NO_ERROR, "setup OAuth session failed"))
    return 0;
  if (!check(mailactivesync_set_protocol_version(session, "2.5") ==
      MAILACTIVESYNC_NO_ERROR, "set ActiveSync 2.5 failed"))
    goto err;

  request = mailactivesync_sync_request_new("5", "111");
  if (!check(request != NULL, "Sync 2.5 request allocation failed"))
    goto err;
  if (!check(mailactivesync_sync_request_set_collection_class(request,
      "Email") == MAILACTIVESYNC_NO_ERROR,
      "Sync 2.5 request collection class set failed"))
    goto err;

  r = mailactivesync_sync(session, request, &result);
  if (!check(r == MAILACTIVESYNC_NO_ERROR, "Sync 2.5 empty response failed"))
    goto err;
  if (!check(mailactivesync_wbxml_decode(context->last_request->body,
      context->last_request->body_len, &request_document) ==
      MAILACTIVESYNC_NO_ERROR, "Sync 2.5 request decode failed"))
    goto err;

  request_collections = test_node_child(request_document->root,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_COLLECTIONS);
  request_collection = test_node_child(request_collections,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_COLLECTION);
  request_options = test_node_child(request_collection,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_OPTIONS);
  if (!check(strcmp(test_node_child_text(request_collection,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_CLASS),
      "Email") == 0, "Sync 2.5 collection-level Class mismatch"))
    goto err;
  if (!check((request_options == NULL) ||
      (test_node_child(request_options, MAILACTIVESYNC_CP_AIRSYNC,
          MAILACTIVESYNC_AIRSYNC_CLASS) == NULL),
      "Sync 2.5 unexpectedly included Options/Class"))
    goto err;

  mailactivesync_wbxml_document_free(request_document);
  mailactivesync_sync_result_free(result);
  mailactivesync_sync_request_free(request);
  mailactivesync_free(session);
  return 1;

 err:
  mailactivesync_wbxml_document_free(request_document);
  mailactivesync_sync_result_free(result);
  mailactivesync_sync_request_free(request);
  mailactivesync_free(session);
  return 0;
}

static int test_sync_wait_and_heartbeat_request_shape(void)
{
  mailactivesync * session;
  struct fake_context * context;
  struct mailactivesync_sync_request * request;
  struct mailactivesync_sync_result * result;
  struct mailactivesync_wbxml_document * request_document;
  int r;

  session = NULL;
  context = NULL;
  request = NULL;
  result = NULL;
  request_document = NULL;

  if (!check(setup_oauth_session(&session, &context) ==
      MAILACTIVESYNC_NO_ERROR, "setup OAuth session failed"))
    return 0;

  request = mailactivesync_sync_request_new("5", "111");
  if (!check(request != NULL, "Sync Heartbeat request allocation failed"))
    goto err;
  if (!check(mailactivesync_sync_request_set_wait(request, 10) ==
      MAILACTIVESYNC_NO_ERROR, "Sync Wait set failed"))
    goto err;
  if (!check(mailactivesync_sync_request_set_heartbeat_interval(request,
      120) == MAILACTIVESYNC_NO_ERROR, "Sync HeartbeatInterval set failed"))
    goto err;

  r = mailactivesync_sync(session, request, &result);
  if (!check(r == MAILACTIVESYNC_NO_ERROR,
      "Sync Heartbeat empty response failed"))
    goto err;
  if (!check(mailactivesync_wbxml_decode(context->last_request->body,
      context->last_request->body_len, &request_document) ==
      MAILACTIVESYNC_NO_ERROR, "Sync Heartbeat request decode failed"))
    goto err;
  if (!check(test_node_child(request_document->root,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_WAIT) == NULL,
      "Sync Heartbeat request unexpectedly included Wait"))
    goto err;
  if (!check(strcmp(test_node_child_text(request_document->root,
      MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_HEARTBEAT_INTERVAL), "120") == 0,
      "Sync HeartbeatInterval mismatch"))
    goto err;

  mailactivesync_wbxml_document_free(request_document);
  request_document = NULL;
  mailactivesync_sync_result_free(result);
  result = NULL;
  mailactivesync_sync_request_free(request);
  request = NULL;

  request = mailactivesync_sync_request_new("5", "111");
  if (!check(request != NULL, "Sync 12.0 Wait request allocation failed"))
    goto err;
  if (!check(mailactivesync_set_protocol_version(session, "12.0") ==
      MAILACTIVESYNC_NO_ERROR, "set ActiveSync 12.0 failed"))
    goto err;
  if (!check(mailactivesync_sync_request_set_wait(request, 10) ==
      MAILACTIVESYNC_NO_ERROR, "Sync 12.0 Wait set failed"))
    goto err;
  r = mailactivesync_sync(session, request, &result);
  if (!check(r == MAILACTIVESYNC_ERROR_NOT_IMPLEMENTED,
      "Sync 12.0 Wait did not fail as not implemented"))
    goto err;

  if (!check(mailactivesync_sync_request_set_wait(request, 60) ==
      MAILACTIVESYNC_ERROR_BAD_STATE,
      "Sync invalid Wait did not fail as bad state"))
    goto err;
  if (!check(mailactivesync_sync_request_set_heartbeat_interval(request,
      59) == MAILACTIVESYNC_ERROR_BAD_STATE,
      "Sync invalid HeartbeatInterval did not fail as bad state"))
    goto err;

  mailactivesync_sync_request_free(request);
  mailactivesync_free(session);
  return 1;

 err:
  mailactivesync_wbxml_document_free(request_document);
  mailactivesync_sync_result_free(result);
  mailactivesync_sync_request_free(request);
  mailactivesync_free(session);
  return 0;
}

static int test_sync_conversation_mode_request_shape(void)
{
  mailactivesync * session;
  struct fake_context * context;
  struct mailactivesync_sync_request * request;
  struct mailactivesync_sync_result * result;
  struct mailactivesync_wbxml_document * request_document;
  struct mailactivesync_wbxml_node * request_collections;
  struct mailactivesync_wbxml_node * request_collection;
  int request_count;
  int r;

  session = NULL;
  context = NULL;
  request = NULL;
  result = NULL;
  request_document = NULL;

  if (!check(setup_oauth_session(&session, &context) ==
      MAILACTIVESYNC_NO_ERROR, "setup OAuth session failed"))
    return 0;

  request = mailactivesync_sync_request_new("5", "111");
  if (!check(request != NULL,
      "Sync ConversationMode request allocation failed"))
    goto err;
  if (!check(mailactivesync_sync_request_set_conversation_mode(request, 0) ==
      MAILACTIVESYNC_NO_ERROR, "Sync ConversationMode set failed"))
    goto err;

  r = mailactivesync_sync(session, request, &result);
  if (!check(r == MAILACTIVESYNC_NO_ERROR,
      "Sync ConversationMode empty response failed"))
    goto err;
  if (!check(mailactivesync_wbxml_decode(context->last_request->body,
      context->last_request->body_len, &request_document) ==
      MAILACTIVESYNC_NO_ERROR, "Sync ConversationMode request decode failed"))
    goto err;
  request_collections = test_node_child(request_document->root,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_COLLECTIONS);
  request_collection = test_node_child(request_collections,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_COLLECTION);
  if (!check(strcmp(test_node_child_text(request_collection,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_CONVERSATION_MODE),
      "0") == 0, "Sync ConversationMode disabled mismatch"))
    goto err;

  mailactivesync_wbxml_document_free(request_document);
  request_document = NULL;
  mailactivesync_sync_result_free(result);
  result = NULL;
  mailactivesync_sync_request_free(request);
  request = NULL;

  request = mailactivesync_sync_request_new("5", "111");
  if (!check(request != NULL,
      "Sync legacy ConversationMode request allocation failed"))
    goto err;
  if (!check(mailactivesync_set_protocol_version(session, "12.1") ==
      MAILACTIVESYNC_NO_ERROR, "set ActiveSync 12.1 failed"))
    goto err;
  if (!check(mailactivesync_sync_request_set_conversation_mode(request, 1) ==
      MAILACTIVESYNC_NO_ERROR, "Sync legacy ConversationMode set failed"))
    goto err;
  request_count = context->request_count;
  r = mailactivesync_sync(session, request, &result);
  if (!check(r == MAILACTIVESYNC_ERROR_NOT_IMPLEMENTED,
      "Sync 12.1 ConversationMode did not fail as not implemented"))
    goto err;
  if (!check(context->request_count == request_count,
      "Sync 12.1 ConversationMode sent an HTTP request"))
    goto err;

  mailactivesync_sync_request_free(request);
  mailactivesync_free(session);
  return 1;

 err:
  mailactivesync_wbxml_document_free(request_document);
  mailactivesync_sync_result_free(result);
  mailactivesync_sync_request_free(request);
  mailactivesync_free(session);
  return 0;
}

static int test_sync_top_level_status_response(void)
{
  mailactivesync * session;
  struct fake_context * context;
  struct mailactivesync_sync_request * request;
  struct mailactivesync_sync_result * result;
  struct mailactivesync_wbxml_node * root;
  int r;

  session = NULL;
  context = NULL;
  request = NULL;
  result = NULL;
  root = NULL;

  if (!check(setup_oauth_session(&session, &context) ==
      MAILACTIVESYNC_NO_ERROR, "setup OAuth session failed"))
    return 0;

  root = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_SYNC);
  if (!check(root != NULL, "top-level Sync response allocation failed"))
    goto err;
  if (!check(test_node_add_text(root, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_STATUS, "4") == MAILACTIVESYNC_NO_ERROR,
      "top-level Sync response Status add failed"))
    goto err;
  if (!check(test_node_add_text(root, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_LIMIT, "60") == MAILACTIVESYNC_NO_ERROR,
      "top-level Sync response Limit add failed"))
    goto err;
  if (!check(fake_context_set_response_body(context, root) ==
      MAILACTIVESYNC_NO_ERROR, "top-level Sync response encode failed"))
    goto err;
  mailactivesync_wbxml_node_free(root);
  root = NULL;

  request = mailactivesync_sync_request_new("5", "0");
  if (!check(request != NULL, "top-level Sync request allocation failed"))
    goto err;

  r = mailactivesync_sync(session, request, &result);
  if (!check(r == MAILACTIVESYNC_NO_ERROR,
      "top-level Sync status response failed"))
    goto err;
  if (!check((result != NULL) && (result->status == 4) &&
      (result->limit == 60) && (result->sync_key == NULL),
      "top-level Sync status result mismatch"))
    goto err;

  mailactivesync_sync_result_free(result);
  mailactivesync_sync_request_free(request);
  mailactivesync_free(session);
  return 1;

 err:
  mailactivesync_wbxml_node_free(root);
  mailactivesync_sync_result_free(result);
  mailactivesync_sync_request_free(request);
  mailactivesync_free(session);
  return 0;
}

static int test_sync_empty_http_response_is_no_change(void)
{
  mailactivesync * session;
  struct fake_context * context;
  struct mailactivesync_sync_request * request;
  struct mailactivesync_sync_result * result;
  int r;

  session = NULL;
  context = NULL;
  request = NULL;
  result = NULL;

  if (!check(setup_oauth_session(&session, &context) ==
      MAILACTIVESYNC_NO_ERROR, "setup OAuth session failed"))
    return 0;

  request = mailactivesync_sync_request_new("5", "1732367094");
  if (!check(request != NULL, "empty HTTP Sync request allocation failed"))
    goto err;

  r = mailactivesync_sync(session, request, &result);
  if (!check(r == MAILACTIVESYNC_NO_ERROR,
      "empty HTTP Sync response failed"))
    goto err;
  if (!check((result != NULL) && (result->status == 1) &&
      (result->sync_key != NULL) &&
      (strcmp(result->sync_key, "1732367094") == 0) &&
      result->empty_response && !result->sync_key_from_response &&
      (clist_count(result->added) == 0) &&
      (clist_count(result->changed) == 0) &&
      (clist_count(result->deleted) == 0),
      "empty HTTP Sync result mismatch"))
    goto err;

  mailactivesync_sync_result_free(result);
  mailactivesync_sync_request_free(request);
  mailactivesync_free(session);
  return 1;

 err:
  mailactivesync_sync_result_free(result);
  mailactivesync_sync_request_free(request);
  mailactivesync_free(session);
  return 0;
}

static int test_sync_change_softdelete_and_html_body(void)
{
  static const char html_body[] = "<p>Hello</p>";
  static const char plain_body[] = "Plain hello";
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
  struct mailactivesync_wbxml_node * attachments;
  struct mailactivesync_wbxml_node * attachment;
  struct mailactivesync_message * message;
  struct mailactivesync_attachment * parsed_attachment;
  clistiter * cur;
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
  attachments = NULL;
  attachment = NULL;

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
      MAILACTIVESYNC_AIRSYNCBASE_ESTIMATED_DATA_SIZE, "1234") ==
      MAILACTIVESYNC_NO_ERROR, "Sync HTML estimated size add failed"))
    goto err;
  if (!check(test_node_add_text(body, MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_TRUNCATED, "1") ==
      MAILACTIVESYNC_NO_ERROR, "Sync HTML truncated add failed"))
    goto err;
  if (!check(test_node_add_text(body, MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_NATIVE_BODY_TYPE, "1") ==
      MAILACTIVESYNC_NO_ERROR, "Sync HTML native body type add failed"))
    goto err;
  if (!check(test_node_add_text(body, MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_CONTENT_TYPE, "text/html") ==
      MAILACTIVESYNC_NO_ERROR, "Sync HTML content type add failed"))
    goto err;
  if (!check(test_node_add_text(body, MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_PREVIEW, "Hello") ==
      MAILACTIVESYNC_NO_ERROR, "Sync HTML preview add failed"))
    goto err;
  attachments = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_ATTACHMENTS);
  attachment = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_ATTACHMENT);
  if (!check((attachments != NULL) && (attachment != NULL),
      "Sync HTML attachment allocation failed"))
    goto err;
  if (!check(test_node_add_text(attachment, MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_DISPLAY_NAME, "report.pdf") ==
      MAILACTIVESYNC_NO_ERROR, "Sync HTML attachment display name add failed"))
    goto err;
  if (!check(test_node_add_text(attachment, MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_FILE_REFERENCE, "file-ref-1") ==
      MAILACTIVESYNC_NO_ERROR, "Sync HTML attachment file ref add failed"))
    goto err;
  if (!check(test_node_add_text(attachment, MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_METHOD, "1") ==
      MAILACTIVESYNC_NO_ERROR, "Sync HTML attachment method add failed"))
    goto err;
  if (!check(test_node_add_text(attachment, MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_CONTENT_ID, "cid-1") ==
      MAILACTIVESYNC_NO_ERROR, "Sync HTML attachment content id add failed"))
    goto err;
  if (!check(test_node_add_text(attachment, MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_CONTENT_LOCATION, "report.pdf") ==
      MAILACTIVESYNC_NO_ERROR,
      "Sync HTML attachment content location add failed"))
    goto err;
  if (!check(test_node_add_text(attachment, MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_IS_INLINE, "0") ==
      MAILACTIVESYNC_NO_ERROR, "Sync HTML attachment inline add failed"))
    goto err;
  if (!check(test_node_add_text(attachment, MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_CONTENT_TYPE, "application/pdf") ==
      MAILACTIVESYNC_NO_ERROR, "Sync HTML attachment content type add failed"))
    goto err;
  if (!check(test_node_add_text(attachment, MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_ESTIMATED_DATA_SIZE, "4567") ==
      MAILACTIVESYNC_NO_ERROR, "Sync HTML attachment size add failed"))
    goto err;
  if (!check(mailactivesync_wbxml_node_add_child(attachments, attachment) ==
      MAILACTIVESYNC_NO_ERROR, "Sync HTML attachment append failed"))
    goto err;
  attachment = NULL;
  if (!check(mailactivesync_wbxml_node_add_child(app_data, attachments) ==
      MAILACTIVESYNC_NO_ERROR, "Sync HTML attachments append failed"))
    goto err;
  attachments = NULL;
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
      MAILACTIVESYNC_AIRSYNC_CHANGE);
  app_data = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_APPLICATION_DATA);
  body = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_BODY);
  if (!check((command != NULL) && (app_data != NULL) && (body != NULL),
      "Sync plain change response allocation failed"))
    goto err;
  if (!check(test_node_add_text(command, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_SERVER_ID, "msg-plain-change") ==
      MAILACTIVESYNC_NO_ERROR, "Sync plain change ServerId add failed"))
    goto err;
  if (!check(test_node_add_text(app_data, MAILACTIVESYNC_CP_EMAIL,
      MAILACTIVESYNC_EMAIL_SUBJECT, "Plain Changed") ==
      MAILACTIVESYNC_NO_ERROR, "Sync plain subject add failed"))
    goto err;
  if (!check(test_node_add_text(body, MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_TYPE, "1") == MAILACTIVESYNC_NO_ERROR,
      "Sync plain body type add failed"))
    goto err;
  if (!check(test_node_add_text(body, MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_CONTENT_TYPE, "text/plain") ==
      MAILACTIVESYNC_NO_ERROR, "Sync plain content type add failed"))
    goto err;
  if (!check(test_node_add_text(body, MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_DATA, plain_body) ==
      MAILACTIVESYNC_NO_ERROR, "Sync plain body data add failed"))
    goto err;
  if (!check(mailactivesync_wbxml_node_add_child(app_data, body) ==
      MAILACTIVESYNC_NO_ERROR, "Sync plain body append failed"))
    goto err;
  body = NULL;
  if (!check(mailactivesync_wbxml_node_add_child(command, app_data) ==
      MAILACTIVESYNC_NO_ERROR, "Sync plain app data append failed"))
    goto err;
  app_data = NULL;
  if (!check(mailactivesync_wbxml_node_add_child(commands, command) ==
      MAILACTIVESYNC_NO_ERROR, "Sync plain change command append failed"))
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
      (clist_count(result->changed) == 2) &&
      (clist_count(result->deleted) == 1),
      "Sync change result counts mismatch"))
    goto err;
  cur = clist_begin(result->changed);
  message = clist_content(cur);
  if (!check((message != NULL) &&
      (strcmp(message->server_id, "msg-change") == 0) &&
      (strcmp(message->subject, "Changed") == 0) &&
      (message->body != NULL) &&
      (message->body->type == MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_HTML) &&
      (message->body->estimated_data_size == 1234) &&
      (message->body->truncated == 1) &&
      (message->body->native_body_type == 1) &&
      (strcmp(message->body->content_type, "text/html") == 0) &&
      (strcmp(message->body->preview, "Hello") == 0) &&
      (message->body->attachments != NULL) &&
      (clist_count(message->body->attachments) == 1) &&
      (strcmp(message->body->data, html_body) == 0) &&
      (message->mime == NULL) && (message->mime_len == 0),
      "Sync change HTML body parse mismatch"))
    goto err;
  if (!check(mailactivesync_airsyncbase_body_needs_fetch(message->body) &&
      mailactivesync_message_needs_body_fetch(message),
      "Sync truncated HTML body did not need body refetch"))
    goto err;
  parsed_attachment = clist_content(clist_begin(message->body->attachments));
  if (!check((parsed_attachment != NULL) &&
      str_equal(parsed_attachment->display_name, "report.pdf") &&
      str_equal(parsed_attachment->file_reference, "file-ref-1") &&
      (parsed_attachment->method == 1) &&
      str_equal(parsed_attachment->content_id, "cid-1") &&
      str_equal(parsed_attachment->content_location, "report.pdf") &&
      (parsed_attachment->is_inline == 0) &&
      str_equal(parsed_attachment->content_type, "application/pdf") &&
      (parsed_attachment->estimated_data_size == 4567),
      "Sync change attachment metadata parse mismatch"))
    goto err;
  cur = clist_next(cur);
  message = clist_content(cur);
  if (!check((message != NULL) &&
      (strcmp(message->server_id, "msg-plain-change") == 0) &&
      (strcmp(message->subject, "Plain Changed") == 0) &&
      (message->body != NULL) &&
      (message->body->type ==
          MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_PLAIN_TEXT) &&
      (strcmp(message->body->content_type, "text/plain") == 0) &&
      (strcmp(message->body->data, plain_body) == 0) &&
      (message->mime == NULL) && (message->mime_len == 0),
      "Sync change plain body parse mismatch"))
    goto err;
  if (!check(!mailactivesync_airsyncbase_body_needs_fetch(message->body) &&
      !mailactivesync_message_needs_body_fetch(message),
      "Sync full plain body unexpectedly needed body refetch"))
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
  mailactivesync_wbxml_node_free(attachment);
  mailactivesync_wbxml_node_free(attachments);
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
  static const unsigned char html_body[] =
      "<html><body>not wbxml</body></html>";
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

  if (!check(fake_context_set_raw_response_body(context, malformed,
      sizeof(malformed)) == MAILACTIVESYNC_NO_ERROR,
      "set malformed Sync body failed"))
    goto err;
  r = mailactivesync_sync(session, request, &result);
  if (!check((r == MAILACTIVESYNC_ERROR_PARSE) && (result == NULL),
      "Sync malformed body did not fail as parse error"))
    goto err;

  if (!check(fake_context_set_raw_response_body(context, html_body,
      sizeof(html_body) - 1) == MAILACTIVESYNC_NO_ERROR,
      "set non-WBXML Sync body failed"))
    goto err;
  r = mailactivesync_sync(session, request, &result);
  if (!check((r == MAILACTIVESYNC_ERROR_RESPONSE_NOT_WBXML) &&
      (result == NULL),
      "Sync non-WBXML body did not fail as response-not-WBXML"))
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

static struct mailactivesync_wbxml_node * sync_success_response_new(
    const char * sync_key)
{
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_node * collection;

  root = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_SYNC);
  collection = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_COLLECTION);
  if ((root == NULL) || (collection == NULL))
    goto err;

  if (test_node_add_text(collection, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_SYNC_KEY, sync_key) !=
      MAILACTIVESYNC_NO_ERROR)
    goto err;
  if (test_node_add_text(collection, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_STATUS, "1") != MAILACTIVESYNC_NO_ERROR)
    goto err;
  if (append_sync_collection_response(root, collection) !=
      MAILACTIVESYNC_NO_ERROR)
    goto err;

  return root;

 err:
  mailactivesync_wbxml_node_free(collection);
  mailactivesync_wbxml_node_free(root);
  return NULL;
}

static struct mailactivesync_wbxml_node * sync_add_response_new(
    const char * sync_key,
    const char * client_id,
    const char * server_id)
{
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_node * collection;
  struct mailactivesync_wbxml_node * responses;
  struct mailactivesync_wbxml_node * response;

  root = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_SYNC);
  collection = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_COLLECTION);
  responses = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_RESPONSES);
  response = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_ADD);
  if ((root == NULL) || (collection == NULL) || (responses == NULL) ||
      (response == NULL))
    goto err;

  if ((test_node_add_text(collection, MAILACTIVESYNC_CP_AIRSYNC,
          MAILACTIVESYNC_AIRSYNC_SYNC_KEY, sync_key) !=
          MAILACTIVESYNC_NO_ERROR) ||
      (test_node_add_text(collection, MAILACTIVESYNC_CP_AIRSYNC,
          MAILACTIVESYNC_AIRSYNC_STATUS, "1") !=
          MAILACTIVESYNC_NO_ERROR) ||
      (test_node_add_text(response, MAILACTIVESYNC_CP_AIRSYNC,
          MAILACTIVESYNC_AIRSYNC_CLIENT_ID, client_id) !=
          MAILACTIVESYNC_NO_ERROR) ||
      (test_node_add_text(response, MAILACTIVESYNC_CP_AIRSYNC,
          MAILACTIVESYNC_AIRSYNC_SERVER_ID, server_id) !=
          MAILACTIVESYNC_NO_ERROR) ||
      (test_node_add_text(response, MAILACTIVESYNC_CP_AIRSYNC,
          MAILACTIVESYNC_AIRSYNC_STATUS, "1") !=
          MAILACTIVESYNC_NO_ERROR) ||
      (mailactivesync_wbxml_node_add_child(responses, response) !=
          MAILACTIVESYNC_NO_ERROR))
    goto err;
  response = NULL;

  if (mailactivesync_wbxml_node_add_child(collection, responses) !=
      MAILACTIVESYNC_NO_ERROR)
    goto err;
  responses = NULL;
  if (append_sync_collection_response(root, collection) !=
      MAILACTIVESYNC_NO_ERROR)
    goto err;

  return root;

 err:
  mailactivesync_wbxml_node_free(response);
  mailactivesync_wbxml_node_free(responses);
  mailactivesync_wbxml_node_free(collection);
  mailactivesync_wbxml_node_free(root);
  return NULL;
}

static int test_sync_client_command_request_and_responses(void)
{
  mailactivesync * session;
  struct fake_context * context;
  struct mailactivesync_sync_request * request;
  struct mailactivesync_sync_command * command;
  struct mailactivesync_sync_result * result;
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_node * collection;
  struct mailactivesync_wbxml_node * responses;
  struct mailactivesync_wbxml_node * response;
  struct mailactivesync_wbxml_node * response_app_data;
  struct mailactivesync_wbxml_document * request_document;
  struct mailactivesync_wbxml_node * request_collection;
  struct mailactivesync_wbxml_node * request_supported;
  struct mailactivesync_wbxml_node * request_commands;
  struct mailactivesync_wbxml_node * request_command;
  struct mailactivesync_wbxml_node * request_app_data;
  struct mailactivesync_sync_command_response * parsed_response;
  clistiter * cur;
  int r;

  session = NULL;
  context = NULL;
  request = NULL;
  command = NULL;
  result = NULL;
  root = NULL;
  collection = NULL;
  responses = NULL;
  response = NULL;
  response_app_data = NULL;
  request_document = NULL;

  if (!check(setup_oauth_session(&session, &context) ==
      MAILACTIVESYNC_NO_ERROR, "setup OAuth session failed"))
    return 0;

  root = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_SYNC);
  collection = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_COLLECTION);
  responses = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_RESPONSES);
  if (!check((root != NULL) && (collection != NULL) && (responses != NULL),
      "Sync command response fixture allocation failed"))
    goto err;
  if (!check(test_node_add_text(collection, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_SYNC_KEY, "222") == MAILACTIVESYNC_NO_ERROR,
      "Sync command response SyncKey add failed"))
    goto err;
  if (!check(test_node_add_text(collection, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_STATUS, "1") == MAILACTIVESYNC_NO_ERROR,
      "Sync command response Status add failed"))
    goto err;

  response = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_ADD);
  if (!check(response != NULL, "Sync Add response allocation failed"))
    goto err;
  if (!check(test_node_add_text(response, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_CLIENT_ID, "client-1") ==
      MAILACTIVESYNC_NO_ERROR, "Sync Add response ClientId add failed"))
    goto err;
  if (!check(test_node_add_text(response, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_SERVER_ID, "server-1") ==
      MAILACTIVESYNC_NO_ERROR, "Sync Add response ServerId add failed"))
    goto err;
  if (!check(test_node_add_text(response, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_CLASS, "Email") == MAILACTIVESYNC_NO_ERROR,
      "Sync Add response Class add failed"))
    goto err;
  if (!check(test_node_add_text(response, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_STATUS, "1") == MAILACTIVESYNC_NO_ERROR,
      "Sync Add response Status add failed"))
    goto err;
  if (!check(mailactivesync_wbxml_node_add_child(responses, response) ==
      MAILACTIVESYNC_NO_ERROR, "Sync Add response append failed"))
    goto err;
  response = NULL;

  response = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_CHANGE);
  if (!check(response != NULL, "Sync Change response allocation failed"))
    goto err;
  if (!check(test_node_add_text(response, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_SERVER_ID, "server-2") ==
      MAILACTIVESYNC_NO_ERROR, "Sync Change response ServerId add failed"))
    goto err;
  if (!check(test_node_add_text(response, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_STATUS, "6") == MAILACTIVESYNC_NO_ERROR,
      "Sync Change response Status add failed"))
    goto err;
  if (!check(mailactivesync_wbxml_node_add_child(responses, response) ==
      MAILACTIVESYNC_NO_ERROR, "Sync Change response append failed"))
    goto err;
  response = NULL;

  response = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_FETCH);
  response_app_data = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_APPLICATION_DATA);
  if (!check((response != NULL) && (response_app_data != NULL),
      "Sync Fetch response allocation failed"))
    goto err;
  if (!check(test_node_add_text(response, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_SERVER_ID, "server-4") ==
      MAILACTIVESYNC_NO_ERROR, "Sync Fetch response ServerId add failed"))
    goto err;
  if (!check(test_node_add_text(response, MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_STATUS, "1") == MAILACTIVESYNC_NO_ERROR,
      "Sync Fetch response Status add failed"))
    goto err;
  if (!check(test_node_add_text(response_app_data, MAILACTIVESYNC_CP_EMAIL,
      MAILACTIVESYNC_EMAIL_SUBJECT, "Fetched subject") ==
      MAILACTIVESYNC_NO_ERROR, "Sync Fetch response Subject add failed"))
    goto err;
  if (!check(test_node_add_text(response_app_data, MAILACTIVESYNC_CP_EMAIL,
      MAILACTIVESYNC_EMAIL_READ, "1") == MAILACTIVESYNC_NO_ERROR,
      "Sync Fetch response Read add failed"))
    goto err;
  if (!check(mailactivesync_wbxml_node_add_child(response,
      response_app_data) == MAILACTIVESYNC_NO_ERROR,
      "Sync Fetch response ApplicationData append failed"))
    goto err;
  response_app_data = NULL;
  if (!check(mailactivesync_wbxml_node_add_child(responses, response) ==
      MAILACTIVESYNC_NO_ERROR, "Sync Fetch response append failed"))
    goto err;
  response = NULL;

  if (!check(mailactivesync_wbxml_node_add_child(collection, responses) ==
      MAILACTIVESYNC_NO_ERROR, "Sync Responses append failed"))
    goto err;
  responses = NULL;
  if (!check(append_sync_collection_response(root, collection) ==
      MAILACTIVESYNC_NO_ERROR, "Sync command response collection append failed"))
    goto err;
  collection = NULL;
  if (!check(fake_context_set_response_body(context, root) ==
      MAILACTIVESYNC_NO_ERROR, "Sync command response encode failed"))
    goto err;
  mailactivesync_wbxml_node_free(root);
  root = NULL;

  request = mailactivesync_sync_request_new("5", "111");
  if (!check(request != NULL, "Sync command request allocation failed"))
    goto err;
  if (!check(mailactivesync_sync_request_add_supported_property(request,
      MAILACTIVESYNC_CP_EMAIL, MAILACTIVESYNC_EMAIL_READ) ==
      MAILACTIVESYNC_NO_ERROR, "Sync Supported email property add failed"))
    goto err;
  if (!check(mailactivesync_sync_request_add_supported_property(request,
      MAILACTIVESYNC_CP_AIRSYNCBASE, MAILACTIVESYNC_AIRSYNCBASE_BODY) ==
      MAILACTIVESYNC_NO_ERROR, "Sync Supported body property add failed"))
    goto err;
  command = mailactivesync_sync_command_add_new("client-1", "Email");
  if (!check(command != NULL, "Sync Add command allocation failed"))
    goto err;
  if (!check(mailactivesync_sync_command_add_application_data_text(command,
      MAILACTIVESYNC_CP_EMAIL, MAILACTIVESYNC_EMAIL_READ, "1") ==
      MAILACTIVESYNC_NO_ERROR, "Sync Add app data add failed"))
    goto err;
  if (!check(mailactivesync_sync_request_add_command(request, command) ==
      MAILACTIVESYNC_NO_ERROR, "Sync Add command append failed"))
    goto err;
  command = NULL;

  command = mailactivesync_sync_command_change_new("server-2");
  if (!check(command != NULL, "Sync Change command allocation failed"))
    goto err;
  if (!check(mailactivesync_sync_command_add_application_data_text(command,
      MAILACTIVESYNC_CP_EMAIL, MAILACTIVESYNC_EMAIL_READ, "0") ==
      MAILACTIVESYNC_NO_ERROR, "Sync Change app data add failed"))
    goto err;
  if (!check(mailactivesync_sync_request_add_command(request, command) ==
      MAILACTIVESYNC_NO_ERROR, "Sync Change command append failed"))
    goto err;
  command = NULL;

  command = mailactivesync_sync_command_delete_new("server-3");
  if (!check(command != NULL, "Sync Delete command allocation failed"))
    goto err;
  if (!check(mailactivesync_sync_request_add_command(request, command) ==
      MAILACTIVESYNC_NO_ERROR, "Sync Delete command append failed"))
    goto err;
  command = NULL;

  command = mailactivesync_sync_command_fetch_new("server-4");
  if (!check(command != NULL, "Sync Fetch command allocation failed"))
    goto err;
  if (!check(mailactivesync_sync_request_add_command(request, command) ==
      MAILACTIVESYNC_NO_ERROR, "Sync Fetch command append failed"))
    goto err;
  command = NULL;

  r = mailactivesync_sync(session, request, &result);
  if (!check(r == MAILACTIVESYNC_NO_ERROR, "Sync with client commands failed"))
    goto err;
  if (!check(mailactivesync_wbxml_decode(context->last_request->body,
      context->last_request->body_len, &request_document) ==
      MAILACTIVESYNC_NO_ERROR, "Sync client command request decode failed"))
    goto err;
  request_collection = test_node_child(test_node_child(request_document->root,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_COLLECTIONS),
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_COLLECTION);
  request_supported = test_node_child(request_collection,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_SUPPORTED);
  if (!check((request_supported != NULL) &&
      (clist_count(request_supported->children) == 2) &&
      (test_node_child(request_supported, MAILACTIVESYNC_CP_EMAIL,
          MAILACTIVESYNC_EMAIL_READ) != NULL) &&
      (test_node_child(request_supported, MAILACTIVESYNC_CP_AIRSYNCBASE,
          MAILACTIVESYNC_AIRSYNCBASE_BODY) != NULL),
      "Sync Supported request mismatch"))
    goto err;
  request_commands = test_node_child(request_collection,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_COMMANDS);
  if (!check((request_commands != NULL) &&
      (clist_count(request_commands->children) == 4),
      "Sync client command request count mismatch"))
    goto err;
  cur = clist_begin(request_commands->children);
  request_command = clist_content(cur);
  request_app_data = test_node_child(request_command,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_APPLICATION_DATA);
  if (!check((request_command->token == MAILACTIVESYNC_AIRSYNC_ADD) &&
      str_equal(test_node_child_text(request_command,
          MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_CLIENT_ID),
          "client-1") &&
      str_equal(test_node_child_text(request_command,
          MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_CLASS), "Email") &&
      str_equal(test_node_child_text(request_app_data,
          MAILACTIVESYNC_CP_EMAIL, MAILACTIVESYNC_EMAIL_READ), "1"),
      "Sync Add command request mismatch"))
    goto err;
  cur = clist_next(cur);
  request_command = clist_content(cur);
  request_app_data = test_node_child(request_command,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_APPLICATION_DATA);
  if (!check((request_command->token == MAILACTIVESYNC_AIRSYNC_CHANGE) &&
      str_equal(test_node_child_text(request_command,
          MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_SERVER_ID),
          "server-2") &&
      str_equal(test_node_child_text(request_app_data,
          MAILACTIVESYNC_CP_EMAIL, MAILACTIVESYNC_EMAIL_READ), "0"),
      "Sync Change command request mismatch"))
    goto err;
  cur = clist_next(cur);
  request_command = clist_content(cur);
  if (!check((request_command->token == MAILACTIVESYNC_AIRSYNC_DELETE) &&
      str_equal(test_node_child_text(request_command,
          MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_SERVER_ID),
          "server-3") &&
      (test_node_child(request_command, MAILACTIVESYNC_CP_AIRSYNC,
          MAILACTIVESYNC_AIRSYNC_APPLICATION_DATA) == NULL),
      "Sync Delete command request mismatch"))
    goto err;
  cur = clist_next(cur);
  request_command = clist_content(cur);
  if (!check((request_command->token == MAILACTIVESYNC_AIRSYNC_FETCH) &&
      str_equal(test_node_child_text(request_command,
          MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_SERVER_ID),
          "server-4"),
      "Sync Fetch command request mismatch"))
    goto err;

  if (!check(clist_count(result->command_responses) == 3,
      "Sync command response count mismatch"))
    goto err;
  parsed_response = clist_content(clist_begin(result->command_responses));
  if (!check((parsed_response->type == MAILACTIVESYNC_SYNC_COMMAND_ADD) &&
      (parsed_response->status == 1) &&
      str_equal(parsed_response->client_id, "client-1") &&
      str_equal(parsed_response->server_id, "server-1") &&
      str_equal(parsed_response->collection_class, "Email"),
      "Sync parsed Add response mismatch"))
    goto err;
  parsed_response = mailactivesync_sync_result_command_response(result,
      MAILACTIVESYNC_SYNC_COMMAND_ADD, "client-1", NULL);
  if (!check((parsed_response != NULL) &&
      str_equal(parsed_response->server_id, "server-1") &&
      (mailactivesync_sync_result_command_response_status_to_error(result,
          MAILACTIVESYNC_SYNC_COMMAND_ADD, "client-1", NULL) ==
          MAILACTIVESYNC_NO_ERROR),
      "Sync Add response helper mismatch"))
    goto err;
  parsed_response = clist_content(clist_next(
      clist_begin(result->command_responses)));
  if (!check((parsed_response->type == MAILACTIVESYNC_SYNC_COMMAND_CHANGE) &&
      (parsed_response->status == 6) &&
      str_equal(parsed_response->server_id, "server-2"),
      "Sync parsed Change response mismatch"))
    goto err;
  parsed_response = mailactivesync_sync_result_command_response(result,
      MAILACTIVESYNC_SYNC_COMMAND_CHANGE, NULL, "server-2");
  if (!check((parsed_response != NULL) &&
      (parsed_response->status == 6) &&
      (mailactivesync_sync_result_command_response_status_to_error(result,
          MAILACTIVESYNC_SYNC_COMMAND_CHANGE, NULL, "server-2") ==
          MAILACTIVESYNC_ERROR_PROTOCOL),
      "Sync Change response helper mismatch"))
    goto err;
  parsed_response = clist_content(clist_next(clist_next(
      clist_begin(result->command_responses))));
  if (!check((parsed_response->type == MAILACTIVESYNC_SYNC_COMMAND_FETCH) &&
      (parsed_response->status == 1) &&
      str_equal(parsed_response->server_id, "server-4") &&
      (parsed_response->message != NULL) &&
      str_equal(parsed_response->message->server_id, "server-4") &&
      str_equal(parsed_response->message->subject, "Fetched subject") &&
      (parsed_response->message->read == 1),
      "Sync parsed Fetch response ApplicationData mismatch"))
    goto err;
  parsed_response = mailactivesync_sync_result_command_response(result,
      MAILACTIVESYNC_SYNC_COMMAND_FETCH, NULL, "server-4");
  if (!check((parsed_response != NULL) &&
      (parsed_response->message != NULL) &&
      str_equal(parsed_response->message->subject, "Fetched subject"),
      "Sync Fetch response helper mismatch"))
    goto err;
  if (!check((mailactivesync_sync_result_command_response(result,
      MAILACTIVESYNC_SYNC_COMMAND_DELETE, NULL, "server-3") == NULL) &&
      (mailactivesync_sync_result_command_response_status_to_error(result,
      MAILACTIVESYNC_SYNC_COMMAND_DELETE, NULL, "server-3") ==
      MAILACTIVESYNC_ERROR_PROTOCOL),
      "Sync missing Delete response helper mismatch"))
    goto err;

  mailactivesync_wbxml_document_free(request_document);
  mailactivesync_sync_result_free(result);
  mailactivesync_sync_request_free(request);
  mailactivesync_free(session);
  return 1;

 err:
  mailactivesync_wbxml_document_free(request_document);
  mailactivesync_wbxml_node_free(response_app_data);
  mailactivesync_wbxml_node_free(response);
  mailactivesync_wbxml_node_free(responses);
  mailactivesync_wbxml_node_free(collection);
  mailactivesync_wbxml_node_free(root);
  mailactivesync_sync_command_free(command);
  mailactivesync_sync_result_free(result);
  mailactivesync_sync_request_free(request);
  mailactivesync_free(session);
  return 0;
}

static int test_sync_mail_mutation_helpers(void)
{
  mailactivesync * session;
  struct fake_context * context;
  struct mailactivesync_sync_result * result;
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_document * request_document;
  struct mailactivesync_wbxml_node * request_collection;
  struct mailactivesync_wbxml_node * request_commands;
  struct mailactivesync_wbxml_node * request_command;
  struct mailactivesync_wbxml_node * request_app_data;
  struct mailactivesync_wbxml_node * flag_node;
  int r;

  session = NULL;
  context = NULL;
  result = NULL;
  root = NULL;
  request_document = NULL;

  if (!check(setup_oauth_session(&session, &context) ==
      MAILACTIVESYNC_NO_ERROR, "setup OAuth session failed"))
    return 0;

  root = sync_success_response_new("222");
  if (!check(root != NULL, "helper Sync response allocation failed"))
    goto err;
  if (!check(fake_context_set_response_body(context, root) ==
      MAILACTIVESYNC_NO_ERROR, "helper Sync response encode failed"))
    goto err;
  mailactivesync_wbxml_node_free(root);
  root = NULL;

  r = mailactivesync_mark_read(session, "5", "111", "server-1", 1, &result);
  if (!check(r == MAILACTIVESYNC_NO_ERROR, "mark read helper failed"))
    goto err;
  if (!check(mailactivesync_wbxml_decode(context->last_request->body,
      context->last_request->body_len, &request_document) ==
      MAILACTIVESYNC_NO_ERROR, "mark read request decode failed"))
    goto err;
  request_collection = test_node_child(test_node_child(request_document->root,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_COLLECTIONS),
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_COLLECTION);
  request_commands = test_node_child(request_collection,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_COMMANDS);
  request_command = clist_content(clist_begin(request_commands->children));
  request_app_data = test_node_child(request_command,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_APPLICATION_DATA);
  if (!check((request_command->token == MAILACTIVESYNC_AIRSYNC_CHANGE) &&
      str_equal(test_node_child_text(request_command,
          MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_SERVER_ID),
          "server-1") &&
      str_equal(test_node_child_text(request_app_data,
          MAILACTIVESYNC_CP_EMAIL, MAILACTIVESYNC_EMAIL_READ), "1"),
      "mark read helper request mismatch"))
    goto err;
  mailactivesync_wbxml_document_free(request_document);
  request_document = NULL;
  mailactivesync_sync_result_free(result);
  result = NULL;

  r = mailactivesync_set_flagged(session, "5", "222", "server-1", 1,
      &result);
  if (!check(r == MAILACTIVESYNC_NO_ERROR, "set flagged helper failed"))
    goto err;
  if (!check(mailactivesync_wbxml_decode(context->last_request->body,
      context->last_request->body_len, &request_document) ==
      MAILACTIVESYNC_NO_ERROR, "set flagged request decode failed"))
    goto err;
  request_collection = test_node_child(test_node_child(request_document->root,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_COLLECTIONS),
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_COLLECTION);
  request_commands = test_node_child(request_collection,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_COMMANDS);
  request_command = clist_content(clist_begin(request_commands->children));
  request_app_data = test_node_child(request_command,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_APPLICATION_DATA);
  flag_node = test_node_child(request_app_data, MAILACTIVESYNC_CP_EMAIL,
      MAILACTIVESYNC_EMAIL_FLAG);
  if (!check((request_command->token == MAILACTIVESYNC_AIRSYNC_CHANGE) &&
      str_equal(test_node_child_text(flag_node, MAILACTIVESYNC_CP_EMAIL,
          MAILACTIVESYNC_EMAIL_STATUS), "2") &&
      str_equal(test_node_child_text(flag_node, MAILACTIVESYNC_CP_EMAIL,
          MAILACTIVESYNC_EMAIL_FLAG_TYPE), "Flag for follow up"),
      "set flagged helper request mismatch"))
    goto err;
  mailactivesync_wbxml_document_free(request_document);
  request_document = NULL;
  mailactivesync_sync_result_free(result);
  result = NULL;

  r = mailactivesync_delete_message(session, "5", "222", "server-1", 0,
      &result);
  if (!check(r == MAILACTIVESYNC_NO_ERROR, "delete helper failed"))
    goto err;
  if (!check(mailactivesync_wbxml_decode(context->last_request->body,
      context->last_request->body_len, &request_document) ==
      MAILACTIVESYNC_NO_ERROR, "delete request decode failed"))
    goto err;
  request_collection = test_node_child(test_node_child(request_document->root,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_COLLECTIONS),
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_COLLECTION);
  request_commands = test_node_child(request_collection,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_COMMANDS);
  request_command = clist_content(clist_begin(request_commands->children));
  if (!check((request_command->token == MAILACTIVESYNC_AIRSYNC_DELETE) &&
      str_equal(test_node_child_text(request_collection,
          MAILACTIVESYNC_CP_AIRSYNC,
          MAILACTIVESYNC_AIRSYNC_DELETES_AS_MOVES), "0") &&
      str_equal(test_node_child_text(request_command,
          MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_SERVER_ID),
          "server-1"),
      "delete helper request mismatch"))
    goto err;

  mailactivesync_wbxml_document_free(request_document);
  mailactivesync_sync_result_free(result);
  mailactivesync_free(session);
  return 1;

 err:
  mailactivesync_wbxml_document_free(request_document);
  mailactivesync_wbxml_node_free(root);
  mailactivesync_sync_result_free(result);
  mailactivesync_free(session);
  return 0;
}

static int test_sync_draft_helpers(void)
{
  mailactivesync * session;
  struct fake_context * context;
  struct mailactivesync_draft draft;
  struct mailactivesync_draft_attachment attachment;
  struct mailactivesync_sync_result * result;
  struct mailactivesync_sync_command_response * parsed_response;
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_document * request_document;
  struct mailactivesync_wbxml_node * request_collection;
  struct mailactivesync_wbxml_node * request_commands;
  struct mailactivesync_wbxml_node * request_command;
  struct mailactivesync_wbxml_node * request_app_data;
  struct mailactivesync_wbxml_node * body_node;
  struct mailactivesync_wbxml_node * attachments_node;
  struct mailactivesync_wbxml_node * attachment_add_node;
  struct mailactivesync_wbxml_node * attachment_content_node;
  clist * draft_attachments;
  const unsigned char attachment_content[] = { 'h', 'e', 'l', 'l', 'o' };
  int request_count;
  int r;

  session = NULL;
  context = NULL;
  result = NULL;
  root = NULL;
  request_document = NULL;
  draft_attachments = NULL;

  if (!check(setup_oauth_session(&session, &context) ==
      MAILACTIVESYNC_NO_ERROR, "setup OAuth session failed"))
    return 0;

  draft_attachments = clist_new();
  if (!check(draft_attachments != NULL,
      "draft attachment list allocation failed"))
    goto err;
  memset(&attachment, 0, sizeof(attachment));
  attachment.client_id = "attach-client-1";
  attachment.method = 1;
  attachment.content_type = "text/plain";
  attachment.content = attachment_content;
  attachment.content_len = sizeof(attachment_content);
  attachment.display_name = "hello.txt";
  attachment.content_id = "cid-hello";
  attachment.is_inline = 1;
  if (!check(clist_append(draft_attachments, &attachment) == 0,
      "draft attachment append failed"))
    goto err;

  memset(&draft, 0, sizeof(draft));
  draft.to = "to@example.com";
  draft.cc = "cc@example.com";
  draft.bcc = "bcc@example.com";
  draft.reply_to = "reply@example.com";
  draft.subject = "Draft subject";
  draft.has_importance = 1;
  draft.importance = 2;
  draft.has_read = 1;
  draft.read = 1;
  draft.body_type = MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_HTML;
  draft.body = "<p>draft</p>";
  draft.attachments = draft_attachments;

  root = sync_add_response_new("222", "draft-client-1", "draft-server-1");
  if (!check(root != NULL, "draft Add response allocation failed"))
    goto err;
  if (!check(fake_context_set_response_body(context, root) ==
      MAILACTIVESYNC_NO_ERROR, "draft Add response encode failed"))
    goto err;
  mailactivesync_wbxml_node_free(root);
  root = NULL;

  r = mailactivesync_add_draft(session, "drafts", "111",
      "draft-client-1", &draft, &result);
  if (!check(r == MAILACTIVESYNC_NO_ERROR, "add draft helper failed"))
    goto err;
  if (!check(mailactivesync_wbxml_decode(context->last_request->body,
      context->last_request->body_len, &request_document) ==
      MAILACTIVESYNC_NO_ERROR, "add draft request decode failed"))
    goto err;

  request_collection = test_node_child(test_node_child(request_document->root,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_COLLECTIONS),
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_COLLECTION);
  request_commands = test_node_child(request_collection,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_COMMANDS);
  request_command = clist_content(clist_begin(request_commands->children));
  request_app_data = test_node_child(request_command,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_APPLICATION_DATA);
  body_node = test_node_child(request_app_data, MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_BODY);
  attachments_node = test_node_child(request_app_data,
      MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_ATTACHMENTS);
  attachment_add_node = test_node_child(attachments_node,
      MAILACTIVESYNC_CP_AIRSYNCBASE, MAILACTIVESYNC_AIRSYNCBASE_ADD);
  attachment_content_node = test_node_child(attachment_add_node,
      MAILACTIVESYNC_CP_AIRSYNCBASE, MAILACTIVESYNC_AIRSYNCBASE_CONTENT);
  if (!check((request_command->token == MAILACTIVESYNC_AIRSYNC_ADD) &&
      str_equal(test_node_child_text(request_command,
          MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_CLIENT_ID),
          "draft-client-1") &&
      str_equal(test_node_child_text(request_command,
          MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_CLASS), "Email") &&
      str_equal(test_node_child_text(request_app_data,
          MAILACTIVESYNC_CP_EMAIL, MAILACTIVESYNC_EMAIL_TO),
          "to@example.com") &&
      str_equal(test_node_child_text(request_app_data,
          MAILACTIVESYNC_CP_EMAIL, MAILACTIVESYNC_EMAIL_CC),
          "cc@example.com") &&
      str_equal(test_node_child_text(request_app_data,
          MAILACTIVESYNC_CP_EMAIL2, MAILACTIVESYNC_EMAIL2_BCC),
          "bcc@example.com") &&
      str_equal(test_node_child_text(request_app_data,
          MAILACTIVESYNC_CP_EMAIL, MAILACTIVESYNC_EMAIL_REPLY_TO),
          "reply@example.com") &&
      str_equal(test_node_child_text(request_app_data,
          MAILACTIVESYNC_CP_EMAIL, MAILACTIVESYNC_EMAIL_SUBJECT),
          "Draft subject") &&
      str_equal(test_node_child_text(request_app_data,
          MAILACTIVESYNC_CP_EMAIL, MAILACTIVESYNC_EMAIL_IMPORTANCE), "2") &&
      str_equal(test_node_child_text(request_app_data,
          MAILACTIVESYNC_CP_EMAIL, MAILACTIVESYNC_EMAIL_READ), "1") &&
      str_equal(test_node_child_text(body_node, MAILACTIVESYNC_CP_AIRSYNCBASE,
          MAILACTIVESYNC_AIRSYNCBASE_TYPE), "2") &&
      str_equal(test_node_child_text(body_node, MAILACTIVESYNC_CP_AIRSYNCBASE,
          MAILACTIVESYNC_AIRSYNCBASE_DATA), "<p>draft</p>") &&
      (attachments_node != NULL) &&
      (clist_count(attachments_node->children) == 1) &&
      str_equal(test_node_child_text(attachment_add_node,
          MAILACTIVESYNC_CP_AIRSYNCBASE,
          MAILACTIVESYNC_AIRSYNCBASE_CLIENT_ID), "attach-client-1") &&
      str_equal(test_node_child_text(attachment_add_node,
          MAILACTIVESYNC_CP_AIRSYNCBASE,
          MAILACTIVESYNC_AIRSYNCBASE_METHOD), "1") &&
      str_equal(test_node_child_text(attachment_add_node,
          MAILACTIVESYNC_CP_AIRSYNCBASE,
          MAILACTIVESYNC_AIRSYNCBASE_CONTENT_TYPE), "text/plain") &&
      str_equal(test_node_child_text(attachment_add_node,
          MAILACTIVESYNC_CP_AIRSYNCBASE,
          MAILACTIVESYNC_AIRSYNCBASE_DISPLAY_NAME), "hello.txt") &&
      str_equal(test_node_child_text(attachment_add_node,
          MAILACTIVESYNC_CP_AIRSYNCBASE,
          MAILACTIVESYNC_AIRSYNCBASE_CONTENT_ID), "cid-hello") &&
      (test_node_child(attachment_add_node, MAILACTIVESYNC_CP_AIRSYNCBASE,
          MAILACTIVESYNC_AIRSYNCBASE_IS_INLINE) != NULL) &&
      (attachment_content_node != NULL) &&
      (attachment_content_node->opaque_len == sizeof(attachment_content)) &&
      (memcmp(attachment_content_node->opaque, attachment_content,
          sizeof(attachment_content)) == 0),
      "add draft helper request mismatch"))
    goto err;
  parsed_response = clist_content(clist_begin(result->command_responses));
  if (!check((parsed_response != NULL) &&
      (parsed_response->type == MAILACTIVESYNC_SYNC_COMMAND_ADD) &&
      str_equal(parsed_response->client_id, "draft-client-1") &&
      str_equal(parsed_response->server_id, "draft-server-1"),
      "add draft parsed response mismatch"))
    goto err;

  mailactivesync_wbxml_document_free(request_document);
  request_document = NULL;
  mailactivesync_sync_result_free(result);
  result = NULL;

  root = sync_success_response_new("333");
  if (!check(root != NULL, "draft Change response allocation failed"))
    goto err;
  if (!check(fake_context_set_response_body(context, root) ==
      MAILACTIVESYNC_NO_ERROR, "draft Change response encode failed"))
    goto err;
  mailactivesync_wbxml_node_free(root);
  root = NULL;

  draft.to = NULL;
  draft.cc = NULL;
  draft.bcc = NULL;
  draft.reply_to = NULL;
  draft.subject = "Updated draft subject";
  draft.has_importance = 0;
  draft.has_read = 0;
  draft.body_type = MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_PLAIN_TEXT;
  draft.body = "plain draft";

  r = mailactivesync_update_draft(session, "drafts", "222",
      "draft-server-1", &draft, &result);
  if (!check(r == MAILACTIVESYNC_NO_ERROR, "update draft helper failed"))
    goto err;
  if (!check(mailactivesync_wbxml_decode(context->last_request->body,
      context->last_request->body_len, &request_document) ==
      MAILACTIVESYNC_NO_ERROR, "update draft request decode failed"))
    goto err;

  request_collection = test_node_child(test_node_child(request_document->root,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_COLLECTIONS),
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_COLLECTION);
  request_commands = test_node_child(request_collection,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_COMMANDS);
  request_command = clist_content(clist_begin(request_commands->children));
  request_app_data = test_node_child(request_command,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_APPLICATION_DATA);
  body_node = test_node_child(request_app_data, MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_BODY);
  attachments_node = test_node_child(request_app_data,
      MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_ATTACHMENTS);
  attachment_add_node = test_node_child(attachments_node,
      MAILACTIVESYNC_CP_AIRSYNCBASE, MAILACTIVESYNC_AIRSYNCBASE_ADD);
  attachment_content_node = test_node_child(attachment_add_node,
      MAILACTIVESYNC_CP_AIRSYNCBASE, MAILACTIVESYNC_AIRSYNCBASE_CONTENT);
  if (!check((request_command->token == MAILACTIVESYNC_AIRSYNC_CHANGE) &&
      str_equal(test_node_child_text(request_command,
          MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_SERVER_ID),
          "draft-server-1") &&
      str_equal(test_node_child_text(request_app_data,
          MAILACTIVESYNC_CP_EMAIL, MAILACTIVESYNC_EMAIL_SUBJECT),
          "Updated draft subject") &&
      str_equal(test_node_child_text(body_node, MAILACTIVESYNC_CP_AIRSYNCBASE,
          MAILACTIVESYNC_AIRSYNCBASE_TYPE), "1") &&
      str_equal(test_node_child_text(body_node, MAILACTIVESYNC_CP_AIRSYNCBASE,
          MAILACTIVESYNC_AIRSYNCBASE_DATA), "plain draft") &&
      (attachments_node != NULL) &&
      (clist_count(attachments_node->children) == 1) &&
      str_equal(test_node_child_text(attachment_add_node,
          MAILACTIVESYNC_CP_AIRSYNCBASE,
          MAILACTIVESYNC_AIRSYNCBASE_CLIENT_ID), "attach-client-1") &&
      (attachment_content_node != NULL) &&
      (attachment_content_node->opaque_len == sizeof(attachment_content)) &&
      (memcmp(attachment_content_node->opaque, attachment_content,
          sizeof(attachment_content)) == 0),
      "update draft helper request mismatch"))
    goto err;
  mailactivesync_wbxml_document_free(request_document);
  request_document = NULL;
  mailactivesync_sync_result_free(result);
  result = NULL;

  request_count = context->request_count;
  if (!check(mailactivesync_set_protocol_version(session, "14.1") ==
      MAILACTIVESYNC_NO_ERROR, "draft protocol setup failed"))
    goto err;
  r = mailactivesync_add_draft(session, "drafts", "333",
      "draft-client-2", &draft, &result);
  if (!check((r == MAILACTIVESYNC_ERROR_NOT_IMPLEMENTED) &&
      (result == NULL) && (context->request_count == request_count),
      "draft helper did not reject protocol 14.1 before request"))
    goto err;
  if (!check(mailactivesync_set_protocol_version(session, "16.1") ==
      MAILACTIVESYNC_NO_ERROR, "draft protocol restore failed"))
    goto err;

  root = sync_success_response_new("444");
  if (!check(root != NULL, "MIME draft Change response allocation failed"))
    goto err;
  if (!check(fake_context_set_response_body(context, root) ==
      MAILACTIVESYNC_NO_ERROR, "MIME draft Change response encode failed"))
    goto err;
  mailactivesync_wbxml_node_free(root);
  root = NULL;

  draft.subject = NULL;
  draft.body_type = MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_MIME;
  draft.body = "Subject: MIME draft\r\n\r\nbody";
  r = mailactivesync_update_draft(session, "drafts", "333",
      "draft-server-1", &draft, &result);
  if (!check(r == MAILACTIVESYNC_NO_ERROR,
      "MIME draft update helper failed"))
    goto err;
  if (!check(mailactivesync_wbxml_decode(context->last_request->body,
      context->last_request->body_len, &request_document) ==
      MAILACTIVESYNC_NO_ERROR, "MIME draft request decode failed"))
    goto err;
  request_collection = test_node_child(test_node_child(request_document->root,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_COLLECTIONS),
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_COLLECTION);
  request_commands = test_node_child(request_collection,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_COMMANDS);
  request_command = clist_content(clist_begin(request_commands->children));
  request_app_data = test_node_child(request_command,
      MAILACTIVESYNC_CP_AIRSYNC, MAILACTIVESYNC_AIRSYNC_APPLICATION_DATA);
  body_node = test_node_child(request_app_data, MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_BODY);
  if (!check((request_command->token == MAILACTIVESYNC_AIRSYNC_CHANGE) &&
      str_equal(test_node_child_text(body_node, MAILACTIVESYNC_CP_AIRSYNCBASE,
          MAILACTIVESYNC_AIRSYNCBASE_TYPE), "4") &&
      str_equal(test_node_child_text(body_node, MAILACTIVESYNC_CP_AIRSYNCBASE,
          MAILACTIVESYNC_AIRSYNCBASE_DATA),
          "Subject: MIME draft\r\n\r\nbody") &&
      (test_node_child_text(request_app_data, MAILACTIVESYNC_CP_EMAIL,
          MAILACTIVESYNC_EMAIL_SUBJECT) == NULL),
      "MIME draft helper request mismatch"))
    goto err;
  mailactivesync_wbxml_document_free(request_document);
  request_document = NULL;
  mailactivesync_sync_result_free(result);
  result = NULL;

  request_count = context->request_count;
  draft.subject = "Invalid MIME draft subject";
  r = mailactivesync_update_draft(session, "drafts", "333",
      "draft-server-1", &draft, &result);
  if (!check((r == MAILACTIVESYNC_ERROR_BAD_STATE) &&
      (result == NULL) && (context->request_count == request_count),
      "draft helper did not reject invalid MIME draft fields before request"))
    goto err;

  mailactivesync_free(session);
  clist_free(draft_attachments);
  return 1;

 err:
  mailactivesync_wbxml_document_free(request_document);
  mailactivesync_wbxml_node_free(root);
  mailactivesync_sync_result_free(result);
  mailactivesync_free(session);
  clist_free(draft_attachments);
  return 0;
}

static int test_move_items_success(void)
{
  mailactivesync * session;
  struct fake_context * context;
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_node * response;
  struct mailactivesync_wbxml_document * request_document;
  struct mailactivesync_wbxml_node * request_move_items;
  struct mailactivesync_wbxml_node * request_move;
  struct mailactivesync_move first_move;
  struct mailactivesync_move second_move;
  struct mailactivesync_move_items_result * result;
  struct mailactivesync_move_response * first_response;
  struct mailactivesync_move_response * second_response;
  clist * moves;
  clistiter * cur;
  int r;

  session = NULL;
  context = NULL;
  root = NULL;
  response = NULL;
  request_document = NULL;
  result = NULL;
  moves = NULL;

  if (!check(setup_oauth_session(&session, &context) ==
      MAILACTIVESYNC_NO_ERROR, "setup OAuth session failed"))
    return 0;

  root = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_MOVE,
      MAILACTIVESYNC_MOVE_MOVE_ITEMS);
  if (!check(root != NULL, "MoveItems response root allocation failed"))
    goto err;
  if (test_node_add_text(root, MAILACTIVESYNC_CP_MOVE,
      MAILACTIVESYNC_MOVE_STATUS, "3") != MAILACTIVESYNC_NO_ERROR)
    goto err;

  response = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_MOVE,
      MAILACTIVESYNC_MOVE_RESPONSE);
  if (!check(response != NULL, "MoveItems response allocation failed"))
    goto err;
  if ((test_node_add_text(response, MAILACTIVESYNC_CP_MOVE,
          MAILACTIVESYNC_MOVE_SRCMSGID, "msg-1") !=
          MAILACTIVESYNC_NO_ERROR) ||
      (test_node_add_text(response, MAILACTIVESYNC_CP_MOVE,
          MAILACTIVESYNC_MOVE_STATUS, "3") != MAILACTIVESYNC_NO_ERROR) ||
      (test_node_add_text(response, MAILACTIVESYNC_CP_MOVE,
          MAILACTIVESYNC_MOVE_DSTMSGID, "moved-1") !=
          MAILACTIVESYNC_NO_ERROR) ||
      (mailactivesync_wbxml_node_add_child(root, response) !=
          MAILACTIVESYNC_NO_ERROR))
    goto err;
  response = NULL;

  response = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_MOVE,
      MAILACTIVESYNC_MOVE_RESPONSE);
  if (!check(response != NULL, "MoveItems second response allocation failed"))
    goto err;
  if ((test_node_add_text(response, MAILACTIVESYNC_CP_MOVE,
          MAILACTIVESYNC_MOVE_SRCMSGID, "msg-2") !=
          MAILACTIVESYNC_NO_ERROR) ||
      (test_node_add_text(response, MAILACTIVESYNC_CP_MOVE,
          MAILACTIVESYNC_MOVE_STATUS, "7") != MAILACTIVESYNC_NO_ERROR) ||
      (mailactivesync_wbxml_node_add_child(root, response) !=
          MAILACTIVESYNC_NO_ERROR))
    goto err;
  response = NULL;

  if (!check(fake_context_set_response_body(context, root) ==
      MAILACTIVESYNC_NO_ERROR, "MoveItems response encoding failed"))
    goto err;
  root = NULL;

  moves = clist_new();
  if (!check(moves != NULL, "MoveItems move list allocation failed"))
    goto err;
  first_move.src_msg_id = "msg-1";
  first_move.src_folder_id = "inbox";
  first_move.dst_folder_id = "archive";
  second_move.src_msg_id = "msg-2";
  second_move.src_folder_id = "inbox";
  second_move.dst_folder_id = "archive";
  if ((clist_append(moves, &first_move) < 0) ||
      (clist_append(moves, &second_move) < 0))
    goto err;

  r = mailactivesync_move_items(session, moves, &result);
  if (!check((r == MAILACTIVESYNC_ERROR_SERVER_BUSY) && (result != NULL),
      "MoveItems status error was not preserved"))
    goto err;
  if (!check(strstr(context->last_request->url, "Cmd=MoveItems") != NULL,
      "MoveItems request command missing"))
    goto err;
  if (!check(mailactivesync_wbxml_decode(context->last_request->body,
      context->last_request->body_len, &request_document) ==
      MAILACTIVESYNC_NO_ERROR, "MoveItems request decode failed"))
    goto err;

  request_move_items = request_document->root;
  if (!check((request_move_items != NULL) &&
      (request_move_items->code_page == MAILACTIVESYNC_CP_MOVE) &&
      (request_move_items->token == MAILACTIVESYNC_MOVE_MOVE_ITEMS) &&
      (clist_count(request_move_items->children) == 2),
      "MoveItems request root mismatch"))
    goto err;

  cur = clist_begin(request_move_items->children);
  request_move = clist_content(cur);
  if (!check((request_move->code_page == MAILACTIVESYNC_CP_MOVE) &&
      (request_move->token == MAILACTIVESYNC_MOVE_MOVE) &&
      str_equal(test_node_child_text(request_move, MAILACTIVESYNC_CP_MOVE,
          MAILACTIVESYNC_MOVE_SRCMSGID), "msg-1") &&
      str_equal(test_node_child_text(request_move, MAILACTIVESYNC_CP_MOVE,
          MAILACTIVESYNC_MOVE_SRCFLDID), "inbox") &&
      str_equal(test_node_child_text(request_move, MAILACTIVESYNC_CP_MOVE,
          MAILACTIVESYNC_MOVE_DSTFLDID), "archive"),
      "MoveItems first request item mismatch"))
    goto err;

  request_move = clist_content(clist_next(cur));
  if (!check((request_move->code_page == MAILACTIVESYNC_CP_MOVE) &&
      (request_move->token == MAILACTIVESYNC_MOVE_MOVE) &&
      str_equal(test_node_child_text(request_move, MAILACTIVESYNC_CP_MOVE,
          MAILACTIVESYNC_MOVE_SRCMSGID), "msg-2") &&
      str_equal(test_node_child_text(request_move, MAILACTIVESYNC_CP_MOVE,
          MAILACTIVESYNC_MOVE_SRCFLDID), "inbox") &&
      str_equal(test_node_child_text(request_move, MAILACTIVESYNC_CP_MOVE,
          MAILACTIVESYNC_MOVE_DSTFLDID), "archive"),
      "MoveItems second request item mismatch"))
    goto err;

  if (!check((result->status == 7) && (clist_count(result->responses) == 2),
      "MoveItems parsed result summary mismatch"))
    goto err;
  cur = clist_begin(result->responses);
  first_response = clist_content(cur);
  second_response = clist_content(clist_next(cur));
  if (!check((first_response->status == 3) &&
      str_equal(first_response->src_msg_id, "msg-1") &&
      str_equal(first_response->src_folder_id, "inbox") &&
      str_equal(first_response->dst_folder_id, "archive") &&
      str_equal(first_response->dst_msg_id, "moved-1") &&
      (second_response->status == 7) &&
      str_equal(second_response->src_msg_id, "msg-2") &&
      str_equal(second_response->src_folder_id, "inbox") &&
      str_equal(second_response->dst_folder_id, "archive") &&
      (second_response->dst_msg_id == NULL),
      "MoveItems parsed response mismatch"))
    goto err;

  clist_free(moves);
  mailactivesync_wbxml_document_free(request_document);
  mailactivesync_move_items_result_free(result);
  mailactivesync_free(session);
  return 1;

 err:
  clist_free(moves);
  mailactivesync_wbxml_document_free(request_document);
  mailactivesync_wbxml_node_free(response);
  mailactivesync_wbxml_node_free(root);
  mailactivesync_move_items_result_free(result);
  mailactivesync_free(session);
  return 0;
}

static struct mailactivesync_wbxml_node * ping_response_new(
    const char * status,
    const char * heartbeat_interval,
    const char * max_folders,
    const char * changed_collection_id)
{
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_node * folders;
  struct mailactivesync_wbxml_node * folder;
  int r;

  root = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_PING,
      MAILACTIVESYNC_PING_PING);
  if (root == NULL)
    return NULL;

  r = test_node_add_text(root, MAILACTIVESYNC_CP_PING,
      MAILACTIVESYNC_PING_STATUS, status);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  if (heartbeat_interval != NULL) {
    r = test_node_add_text(root, MAILACTIVESYNC_CP_PING,
        MAILACTIVESYNC_PING_HEARTBEAT_INTERVAL, heartbeat_interval);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
  }
  if (max_folders != NULL) {
    r = test_node_add_text(root, MAILACTIVESYNC_CP_PING,
        MAILACTIVESYNC_PING_MAX_FOLDERS, max_folders);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
  }
  if (changed_collection_id != NULL) {
    folders = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_PING,
        MAILACTIVESYNC_PING_FOLDERS);
    if (folders == NULL)
      goto err;
    folder = mailactivesync_wbxml_node_new_text(MAILACTIVESYNC_CP_PING,
        MAILACTIVESYNC_PING_FOLDER, changed_collection_id);
    if (folder == NULL) {
      mailactivesync_wbxml_node_free(folders);
      goto err;
    }
    if (mailactivesync_wbxml_node_add_child(folders, folder) !=
        MAILACTIVESYNC_NO_ERROR) {
      mailactivesync_wbxml_node_free(folder);
      mailactivesync_wbxml_node_free(folders);
      goto err;
    }
    if (mailactivesync_wbxml_node_add_child(root, folders) !=
        MAILACTIVESYNC_NO_ERROR) {
      mailactivesync_wbxml_node_free(folders);
      goto err;
    }
  }

  return root;

 err:
  mailactivesync_wbxml_node_free(root);
  return NULL;
}

static int test_ping_success_and_statuses(void)
{
  mailactivesync * session;
  struct fake_context * context;
  struct mailactivesync_ping_request request;
  struct mailactivesync_ping_result * result;
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_document * request_document;
  struct mailactivesync_wbxml_node * request_folders;
  struct mailactivesync_wbxml_node * request_folder;
  clist * collection_ids;
  clistiter * cur;
  int r;

  session = NULL;
  context = NULL;
  result = NULL;
  root = NULL;
  request_document = NULL;
  collection_ids = NULL;

  if (!check(setup_oauth_session(&session, &context) ==
      MAILACTIVESYNC_NO_ERROR, "setup OAuth session failed"))
    return 0;

  collection_ids = clist_new();
  if (!check(collection_ids != NULL, "Ping collection list allocation failed"))
    goto err;
  if ((clist_append(collection_ids, "inbox") < 0) ||
      (clist_append(collection_ids, "sent") < 0))
    goto err;

  root = ping_response_new("2", NULL, NULL, "inbox");
  if (!check(root != NULL, "Ping changed response allocation failed"))
    goto err;
  if (!check(fake_context_set_response_body(context, root) ==
      MAILACTIVESYNC_NO_ERROR, "Ping changed response encoding failed"))
    goto err;
  root = NULL;

  memset(&request, 0, sizeof(request));
  request.heartbeat_interval = 45;
  request.collection_ids = collection_ids;
  r = mailactivesync_ping(session, &request, &result);
  if (!check(r == MAILACTIVESYNC_NO_ERROR, "Ping changed command failed"))
    goto err;
  if (!check((strstr(context->last_request->url, "Cmd=Ping") != NULL) &&
      (context->last_request->timeout == 75),
      "Ping changed request metadata mismatch"))
    goto err;
  if (!check(mailactivesync_wbxml_decode(context->last_request->body,
      context->last_request->body_len, &request_document) ==
      MAILACTIVESYNC_NO_ERROR, "Ping changed request decode failed"))
    goto err;
  if (!check((request_document->root != NULL) &&
      (request_document->root->code_page == MAILACTIVESYNC_CP_PING) &&
      (request_document->root->token == MAILACTIVESYNC_PING_PING) &&
      str_equal(test_node_child_text(request_document->root,
          MAILACTIVESYNC_CP_PING,
          MAILACTIVESYNC_PING_HEARTBEAT_INTERVAL), "45"),
      "Ping changed request root mismatch"))
    goto err;
  request_folders = test_node_child(request_document->root,
      MAILACTIVESYNC_CP_PING, MAILACTIVESYNC_PING_FOLDERS);
  if (!check((request_folders != NULL) &&
      (clist_count(request_folders->children) == 2),
      "Ping changed request folders mismatch"))
    goto err;
  cur = clist_begin(request_folders->children);
  request_folder = clist_content(cur);
  if (!check(str_equal(test_node_child_text(request_folder,
          MAILACTIVESYNC_CP_PING, MAILACTIVESYNC_PING_ID), "inbox") &&
      str_equal(test_node_child_text(request_folder, MAILACTIVESYNC_CP_PING,
          MAILACTIVESYNC_PING_CLASS), "Email"),
      "Ping changed first folder mismatch"))
    goto err;
  request_folder = clist_content(clist_next(cur));
  if (!check(str_equal(test_node_child_text(request_folder,
          MAILACTIVESYNC_CP_PING, MAILACTIVESYNC_PING_ID), "sent") &&
      str_equal(test_node_child_text(request_folder, MAILACTIVESYNC_CP_PING,
          MAILACTIVESYNC_PING_CLASS), "Email"),
      "Ping changed second folder mismatch"))
    goto err;
  if (!check((result->status == 2) &&
      (result->heartbeat_interval == 0) &&
      (result->max_folders == 0) &&
      (clist_count(result->changed_collection_ids) == 1) &&
      str_equal(clist_content(clist_begin(result->changed_collection_ids)),
          "inbox"),
      "Ping changed result mismatch"))
    goto err;
  if (!check(mailactivesync_ping_status_to_error(result->status) ==
      MAILACTIVESYNC_NO_ERROR, "Ping changed status mapping mismatch"))
    goto err;
  mailactivesync_wbxml_document_free(request_document);
  request_document = NULL;
  mailactivesync_ping_result_free(result);
  result = NULL;

  root = ping_response_new("5", "60", NULL, NULL);
  if (!check(root != NULL, "Ping heartbeat response allocation failed"))
    goto err;
  if (!check(fake_context_set_response_body(context, root) ==
      MAILACTIVESYNC_NO_ERROR, "Ping heartbeat response encoding failed"))
    goto err;
  root = NULL;
  memset(&request, 0, sizeof(request));
  request.heartbeat_interval = 999;
  r = mailactivesync_ping(session, &request, &result);
  if (!check((r == MAILACTIVESYNC_NO_ERROR) &&
      (result->status == 5) &&
      (result->heartbeat_interval == 60) &&
      (mailactivesync_ping_status_to_error(result->status) ==
          MAILACTIVESYNC_NO_ERROR),
      "Ping heartbeat correction mismatch"))
    goto err;
  mailactivesync_ping_result_free(result);
  result = NULL;

  root = ping_response_new("6", NULL, "1", NULL);
  if (!check(root != NULL, "Ping max folders response allocation failed"))
    goto err;
  if (!check(fake_context_set_response_body(context, root) ==
      MAILACTIVESYNC_NO_ERROR, "Ping max folders response encoding failed"))
    goto err;
  root = NULL;
  memset(&request, 0, sizeof(request));
  request.heartbeat_interval = 45;
  request.collection_ids = collection_ids;
  r = mailactivesync_ping(session, &request, &result);
  if (!check((r == MAILACTIVESYNC_NO_ERROR) &&
      (result->status == 6) &&
      (result->max_folders == 1) &&
      (mailactivesync_ping_status_to_error(result->status) ==
          MAILACTIVESYNC_NO_ERROR),
      "Ping max folders correction mismatch"))
    goto err;
  mailactivesync_ping_result_free(result);
  result = NULL;

  root = ping_response_new("1", NULL, NULL, NULL);
  if (!check(root != NULL, "Ping idle response allocation failed"))
    goto err;
  if (!check(fake_context_set_response_body(context, root) ==
      MAILACTIVESYNC_NO_ERROR, "Ping idle response encoding failed"))
    goto err;
  root = NULL;
  memset(&request, 0, sizeof(request));
  r = mailactivesync_ping(session, &request, &result);
  if (!check((r == MAILACTIVESYNC_NO_ERROR) &&
      (result->status == 1) &&
      (context->last_request->body_len == 0),
      "Ping cached empty request mismatch"))
    goto err;

  clist_free(collection_ids);
  mailactivesync_wbxml_document_free(request_document);
  mailactivesync_ping_result_free(result);
  mailactivesync_free(session);
  return 1;

 err:
  clist_free(collection_ids);
  mailactivesync_wbxml_document_free(request_document);
  mailactivesync_wbxml_node_free(root);
  mailactivesync_ping_result_free(result);
  mailactivesync_free(session);
  return 0;
}

static struct mailactivesync_wbxml_node * composemail_response_new(
    uint8_t root_token,
    const char * status)
{
  struct mailactivesync_wbxml_node * root;

  root = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_COMPOSEMAIL,
      root_token);
  if (root == NULL)
    return NULL;

  if ((status != NULL) &&
      (test_node_add_text(root, MAILACTIVESYNC_CP_COMPOSEMAIL,
          MAILACTIVESYNC_COMPOSEMAIL_STATUS, status) !=
          MAILACTIVESYNC_NO_ERROR)) {
    mailactivesync_wbxml_node_free(root);
    return NULL;
  }

  return root;
}

static int test_composemail_send_mail_success_and_error(void)
{
  mailactivesync * session;
  struct fake_context * context;
  struct mailactivesync_composemail_request request;
  struct mailactivesync_wbxml_document * request_document;
  struct mailactivesync_wbxml_node * request_root;
  struct mailactivesync_wbxml_node * mime;
  struct mailactivesync_wbxml_node * root;
  int request_count;
  const char message[] =
      "To: user@example.com\r\n"
      "Subject: Test\r\n"
      "\r\n"
      "Body";
  int r;

  session = NULL;
  context = NULL;
  request_document = NULL;
  root = NULL;

  if (!check(setup_oauth_session(&session, &context) ==
      MAILACTIVESYNC_NO_ERROR, "setup OAuth session failed"))
    return 0;

  r = mailactivesync_send_mail(session, message, sizeof(message) - 1, 1);
  if (!check(r == MAILACTIVESYNC_NO_ERROR, "SendMail command failed"))
    goto err;
  if (!check(strstr(context->last_request->url, "Cmd=SendMail") != NULL,
      "SendMail request command missing"))
    goto err;
  if (!check(mailactivesync_wbxml_decode(context->last_request->body,
      context->last_request->body_len, &request_document) ==
      MAILACTIVESYNC_NO_ERROR, "SendMail request decode failed"))
    goto err;

  request_root = request_document->root;
  mime = test_node_child(request_root, MAILACTIVESYNC_CP_COMPOSEMAIL,
      MAILACTIVESYNC_COMPOSEMAIL_MIME);
  if (!check((request_root != NULL) &&
      (request_root->code_page == MAILACTIVESYNC_CP_COMPOSEMAIL) &&
      (request_root->token == MAILACTIVESYNC_COMPOSEMAIL_SEND_MAIL) &&
      (test_node_child_text(request_root, MAILACTIVESYNC_CP_COMPOSEMAIL,
          MAILACTIVESYNC_COMPOSEMAIL_CLIENT_ID) != NULL) &&
      (test_node_child(request_root, MAILACTIVESYNC_CP_COMPOSEMAIL,
          MAILACTIVESYNC_COMPOSEMAIL_SAVE_IN_SENT_ITEMS) != NULL) &&
      (mime != NULL) &&
      (mime->opaque_len == sizeof(message) - 1) &&
      (memcmp(mime->opaque, message, sizeof(message) - 1) == 0),
      "SendMail request body mismatch"))
    goto err;
  mailactivesync_wbxml_document_free(request_document);
  request_document = NULL;

  memset(&request, 0, sizeof(request));
  request.client_id = "send-client-1";
  request.mime_message = message;
  request.mime_message_len = sizeof(message) - 1;
  r = mailactivesync_send_mail_ext(session, &request);
  if (!check(r == MAILACTIVESYNC_NO_ERROR,
      "SendMail extended ClientId command failed"))
    goto err;
  if (!check(mailactivesync_wbxml_decode(context->last_request->body,
      context->last_request->body_len, &request_document) ==
      MAILACTIVESYNC_NO_ERROR, "SendMail extended ClientId decode failed"))
    goto err;
  request_root = request_document->root;
  if (!check(str_equal(test_node_child_text(request_root,
      MAILACTIVESYNC_CP_COMPOSEMAIL,
      MAILACTIVESYNC_COMPOSEMAIL_CLIENT_ID), "send-client-1"),
      "SendMail extended ClientId mismatch"))
    goto err;
  mailactivesync_wbxml_document_free(request_document);
  request_document = NULL;

  memset(&request, 0, sizeof(request));
  request.client_id =
      "12345678901234567890123456789012345678901";
  request.mime_message = message;
  request.mime_message_len = sizeof(message) - 1;
  request_count = context->request_count;
  r = mailactivesync_send_mail_ext(session, &request);
  if (!check((r == MAILACTIVESYNC_ERROR_BAD_STATE) &&
      (context->request_count == request_count),
      "SendMail overlong ClientId rejection mismatch"))
    goto err;

  root = composemail_response_new(MAILACTIVESYNC_COMPOSEMAIL_SEND_MAIL,
      "110");
  if (!check(root != NULL, "SendMail error response allocation failed"))
    goto err;
  if (!check(fake_context_set_response_body(context, root) ==
      MAILACTIVESYNC_NO_ERROR, "SendMail error response encoding failed"))
    goto err;
  root = NULL;
  r = mailactivesync_send_mail(session, message, sizeof(message) - 1, 0);
  if (!check(r == MAILACTIVESYNC_ERROR_SERVER_BUSY,
      "SendMail error status mapping mismatch"))
    goto err;

  request_count = context->request_count;
  if (!check(mailactivesync_set_protocol_version(session, "12.1") ==
      MAILACTIVESYNC_NO_ERROR, "SendMail legacy protocol setup failed"))
    goto err;
  if (!check(fake_context_set_raw_response_body(context, NULL, 0) ==
      MAILACTIVESYNC_NO_ERROR, "SendMail legacy empty response setup failed"))
    goto err;
  r = mailactivesync_send_mail(session, message, sizeof(message) - 1, 0);
  if (!check((r == MAILACTIVESYNC_NO_ERROR) &&
      (context->request_count == request_count + 1) &&
      str_equal(request_header_value(context->last_request,
          "MS-ASProtocolVersion"), "12.1") &&
      str_equal(request_header_value(context->last_request, "Content-Type"),
          "message/rfc822") &&
      (request_header_value(context->last_request, "Accept") == NULL) &&
      (context->last_request->body_len == sizeof(message) - 1) &&
      (memcmp(context->last_request->body, message, sizeof(message) - 1) == 0),
      "SendMail legacy raw MIME request mismatch"))
    goto err;

  mailactivesync_wbxml_document_free(request_document);
  mailactivesync_wbxml_node_free(root);
  mailactivesync_free(session);
  return 1;

 err:
  mailactivesync_wbxml_document_free(request_document);
  mailactivesync_wbxml_node_free(root);
  mailactivesync_free(session);
  return 0;
}

static int test_composemail_smart_reply_forward_success(void)
{
  mailactivesync * session;
  struct fake_context * context;
  struct mailactivesync_composemail_request request;
  struct mailactivesync_wbxml_document * request_document;
  struct mailactivesync_wbxml_node * request_root;
  struct mailactivesync_wbxml_node * source;
  struct mailactivesync_wbxml_node * mime;
  const char message[] =
      "To: user@example.com\r\n"
      "Subject: Reply\r\n"
      "\r\n"
      "Body";
  int r;

  session = NULL;
  context = NULL;
  request_document = NULL;

  if (!check(setup_oauth_session(&session, &context) ==
      MAILACTIVESYNC_NO_ERROR, "setup OAuth session failed"))
    return 0;

  r = mailactivesync_smart_reply(session, "inbox", "server-1", message,
      sizeof(message) - 1, 1);
  if (!check(r == MAILACTIVESYNC_NO_ERROR, "SmartReply command failed"))
    goto err;
  if (!check(strstr(context->last_request->url, "Cmd=SmartReply") != NULL,
      "SmartReply request command missing"))
    goto err;
  if (!check(mailactivesync_wbxml_decode(context->last_request->body,
      context->last_request->body_len, &request_document) ==
      MAILACTIVESYNC_NO_ERROR, "SmartReply request decode failed"))
    goto err;
  request_root = request_document->root;
  source = test_node_child(request_root, MAILACTIVESYNC_CP_COMPOSEMAIL,
      MAILACTIVESYNC_COMPOSEMAIL_SOURCE);
  mime = test_node_child(request_root, MAILACTIVESYNC_CP_COMPOSEMAIL,
      MAILACTIVESYNC_COMPOSEMAIL_MIME);
  if (!check((request_root != NULL) &&
      (request_root->code_page == MAILACTIVESYNC_CP_COMPOSEMAIL) &&
      (request_root->token == MAILACTIVESYNC_COMPOSEMAIL_SMART_REPLY) &&
      str_equal(test_node_child_text(source, MAILACTIVESYNC_CP_COMPOSEMAIL,
          MAILACTIVESYNC_COMPOSEMAIL_FOLDER_ID), "inbox") &&
      str_equal(test_node_child_text(source, MAILACTIVESYNC_CP_COMPOSEMAIL,
          MAILACTIVESYNC_COMPOSEMAIL_ITEM_ID), "server-1") &&
      (test_node_child(request_root, MAILACTIVESYNC_CP_COMPOSEMAIL,
          MAILACTIVESYNC_COMPOSEMAIL_SAVE_IN_SENT_ITEMS) != NULL) &&
      (mime != NULL) &&
      (mime->opaque_len == sizeof(message) - 1) &&
      (memcmp(mime->opaque, message, sizeof(message) - 1) == 0),
      "SmartReply request body mismatch"))
    goto err;
  mailactivesync_wbxml_document_free(request_document);
  request_document = NULL;

  memset(&request, 0, sizeof(request));
  request.client_id = "reply-client-1";
  request.long_id = "search-long-id";
  request.mime_message = message;
  request.mime_message_len = sizeof(message) - 1;
  request.replace_mime = 1;
  r = mailactivesync_smart_reply_ext(session, &request);
  if (!check(r == MAILACTIVESYNC_NO_ERROR,
      "SmartReply extended command failed"))
    goto err;
  if (!check(mailactivesync_wbxml_decode(context->last_request->body,
      context->last_request->body_len, &request_document) ==
      MAILACTIVESYNC_NO_ERROR, "SmartReply extended request decode failed"))
    goto err;
  request_root = request_document->root;
  source = test_node_child(request_root, MAILACTIVESYNC_CP_COMPOSEMAIL,
      MAILACTIVESYNC_COMPOSEMAIL_SOURCE);
  if (!check((request_root != NULL) &&
      (request_root->token == MAILACTIVESYNC_COMPOSEMAIL_SMART_REPLY) &&
      str_equal(test_node_child_text(request_root,
          MAILACTIVESYNC_CP_COMPOSEMAIL,
          MAILACTIVESYNC_COMPOSEMAIL_CLIENT_ID), "reply-client-1") &&
      str_equal(test_node_child_text(source, MAILACTIVESYNC_CP_COMPOSEMAIL,
          MAILACTIVESYNC_COMPOSEMAIL_LONG_ID), "search-long-id") &&
      (test_node_child(source, MAILACTIVESYNC_CP_COMPOSEMAIL,
          MAILACTIVESYNC_COMPOSEMAIL_FOLDER_ID) == NULL) &&
      (test_node_child(source, MAILACTIVESYNC_CP_COMPOSEMAIL,
          MAILACTIVESYNC_COMPOSEMAIL_ITEM_ID) == NULL) &&
      (test_node_child(request_root, MAILACTIVESYNC_CP_COMPOSEMAIL,
          MAILACTIVESYNC_COMPOSEMAIL_REPLACE_MIME) != NULL),
      "SmartReply extended source/options mismatch"))
    goto err;
  mailactivesync_wbxml_document_free(request_document);
  request_document = NULL;

  r = mailactivesync_smart_forward(session, "inbox", "server-1", message,
      sizeof(message) - 1, 0);
  if (!check(r == MAILACTIVESYNC_NO_ERROR, "SmartForward command failed"))
    goto err;
  if (!check(strstr(context->last_request->url, "Cmd=SmartForward") != NULL,
      "SmartForward request command missing"))
    goto err;
  if (!check(mailactivesync_wbxml_decode(context->last_request->body,
      context->last_request->body_len, &request_document) ==
      MAILACTIVESYNC_NO_ERROR, "SmartForward request decode failed"))
    goto err;
  request_root = request_document->root;
  source = test_node_child(request_root, MAILACTIVESYNC_CP_COMPOSEMAIL,
      MAILACTIVESYNC_COMPOSEMAIL_SOURCE);
  mime = test_node_child(request_root, MAILACTIVESYNC_CP_COMPOSEMAIL,
      MAILACTIVESYNC_COMPOSEMAIL_MIME);
  if (!check((request_root != NULL) &&
      (request_root->code_page == MAILACTIVESYNC_CP_COMPOSEMAIL) &&
      (request_root->token == MAILACTIVESYNC_COMPOSEMAIL_SMART_FORWARD) &&
      str_equal(test_node_child_text(source, MAILACTIVESYNC_CP_COMPOSEMAIL,
          MAILACTIVESYNC_COMPOSEMAIL_FOLDER_ID), "inbox") &&
      str_equal(test_node_child_text(source, MAILACTIVESYNC_CP_COMPOSEMAIL,
          MAILACTIVESYNC_COMPOSEMAIL_ITEM_ID), "server-1") &&
      (test_node_child(request_root, MAILACTIVESYNC_CP_COMPOSEMAIL,
          MAILACTIVESYNC_COMPOSEMAIL_SAVE_IN_SENT_ITEMS) == NULL) &&
      (mime != NULL) &&
      (mime->opaque_len == sizeof(message) - 1) &&
      (memcmp(mime->opaque, message, sizeof(message) - 1) == 0),
      "SmartForward request body mismatch"))
    goto err;
  mailactivesync_wbxml_document_free(request_document);
  request_document = NULL;

  memset(&request, 0, sizeof(request));
  request.client_id = "forward-client-1";
  request.collection_id = "archive";
  request.server_id = "message-2";
  request.instance_id = "2026-08-02T10:00:00.000Z";
  request.mime_message = message;
  request.mime_message_len = sizeof(message) - 1;
  request.save_in_sent = 1;
  request.replace_mime = 1;
  r = mailactivesync_smart_forward_ext(session, &request);
  if (!check(r == MAILACTIVESYNC_NO_ERROR,
      "SmartForward extended command failed"))
    goto err;
  if (!check(mailactivesync_wbxml_decode(context->last_request->body,
      context->last_request->body_len, &request_document) ==
      MAILACTIVESYNC_NO_ERROR, "SmartForward extended request decode failed"))
    goto err;
  request_root = request_document->root;
  source = test_node_child(request_root, MAILACTIVESYNC_CP_COMPOSEMAIL,
      MAILACTIVESYNC_COMPOSEMAIL_SOURCE);
  if (!check((request_root != NULL) &&
      (request_root->token == MAILACTIVESYNC_COMPOSEMAIL_SMART_FORWARD) &&
      str_equal(test_node_child_text(request_root,
          MAILACTIVESYNC_CP_COMPOSEMAIL,
          MAILACTIVESYNC_COMPOSEMAIL_CLIENT_ID), "forward-client-1") &&
      str_equal(test_node_child_text(source, MAILACTIVESYNC_CP_COMPOSEMAIL,
          MAILACTIVESYNC_COMPOSEMAIL_FOLDER_ID), "archive") &&
      str_equal(test_node_child_text(source, MAILACTIVESYNC_CP_COMPOSEMAIL,
          MAILACTIVESYNC_COMPOSEMAIL_ITEM_ID), "message-2") &&
      str_equal(test_node_child_text(source, MAILACTIVESYNC_CP_COMPOSEMAIL,
          MAILACTIVESYNC_COMPOSEMAIL_INSTANCE_ID),
          "2026-08-02T10:00:00.000Z") &&
      (test_node_child(request_root, MAILACTIVESYNC_CP_COMPOSEMAIL,
          MAILACTIVESYNC_COMPOSEMAIL_SAVE_IN_SENT_ITEMS) != NULL) &&
      (test_node_child(request_root, MAILACTIVESYNC_CP_COMPOSEMAIL,
          MAILACTIVESYNC_COMPOSEMAIL_REPLACE_MIME) != NULL),
      "SmartForward extended source/options mismatch"))
    goto err;
  mailactivesync_wbxml_document_free(request_document);
  request_document = NULL;

  if (!check(mailactivesync_set_protocol_version(session, "12.1") ==
      MAILACTIVESYNC_NO_ERROR, "SmartReply legacy protocol setup failed"))
    goto err;
  memset(&request, 0, sizeof(request));
  request.collection_id = "inbox";
  request.server_id = "server-1";
  request.mime_message = message;
  request.mime_message_len = sizeof(message) - 1;
  request.save_in_sent = 1;
  r = mailactivesync_smart_reply_ext(session, &request);
  if (!check((r == MAILACTIVESYNC_NO_ERROR) &&
      (strstr(context->last_request->url, "Cmd=SmartReply") != NULL) &&
      (strstr(context->last_request->url, "CollectionId=inbox") != NULL) &&
      (strstr(context->last_request->url, "ItemId=server-1") != NULL) &&
      (strstr(context->last_request->url, "Options=1") != NULL) &&
      str_equal(request_header_value(context->last_request, "Content-Type"),
          "message/rfc822") &&
      (context->last_request->body_len == sizeof(message) - 1) &&
      (memcmp(context->last_request->body, message, sizeof(message) - 1) == 0),
      "SmartReply legacy raw MIME request mismatch"))
    goto err;

  memset(&request, 0, sizeof(request));
  request.long_id = "search-long-id";
  request.instance_id = "2026-08-02T10:00:00.000Z";
  request.mime_message = message;
  request.mime_message_len = sizeof(message) - 1;
  r = mailactivesync_smart_forward_ext(session, &request);
  if (!check((r == MAILACTIVESYNC_NO_ERROR) &&
      (strstr(context->last_request->url, "Cmd=SmartForward") != NULL) &&
      (strstr(context->last_request->url, "LongId=search-long-id") != NULL) &&
      (strstr(context->last_request->url,
          "Occurrence=2026-08-02T10%3A00%3A00.000Z") != NULL) &&
      str_equal(request_header_value(context->last_request, "Content-Type"),
          "message/rfc822") &&
      (context->last_request->body_len == sizeof(message) - 1) &&
      (memcmp(context->last_request->body, message, sizeof(message) - 1) == 0),
      "SmartForward legacy raw MIME request mismatch"))
    goto err;

  memset(&request, 0, sizeof(request));
  request.collection_id = "inbox";
  request.server_id = "server-1";
  request.long_id = "search-long-id";
  request.mime_message = message;
  request.mime_message_len = sizeof(message) - 1;
  r = mailactivesync_smart_reply_ext(session, &request);
  if (!check(r == MAILACTIVESYNC_ERROR_BAD_STATE,
      "SmartReply mixed source ids did not fail as bad state"))
    goto err;

  mailactivesync_wbxml_document_free(request_document);
  mailactivesync_free(session);
  return 1;

 err:
  mailactivesync_wbxml_document_free(request_document);
  mailactivesync_free(session);
  return 0;
}

static int test_active_sync_public_bad_state(void)
{
  mailactivesync * session;
  struct mailactivesync_draft draft;
  struct mailactivesync_composemail_request compose_request;
  struct mailactivesync_mail_search_request search_request;
  struct mailactivesync_mail_search_result * search_result;
  struct mailactivesync_mail_find_request find_request;
  struct mailactivesync_mail_find_result * find_result;
  struct mailactivesync_resolve_recipients_result * resolve_result;
  struct mailactivesync_validate_cert_request validate_request;
  struct mailactivesync_validate_cert_result * validate_result;
  struct mailactivesync_ping_request ping_request;
  struct mailactivesync_ping_result * ping_result;
  struct mailactivesync_sync_request * sync_request;
  struct mailactivesync_sync_result sync_result;
  struct mailactivesync_item * item_result;
  const char * preferred_versions[] = { "16.1", NULL };
  const char * best_version;
  clist * moves;

  session = NULL;
  search_result = NULL;
  find_result = NULL;
  resolve_result = NULL;
  validate_result = NULL;
  ping_result = NULL;
  sync_request = NULL;
  item_result = NULL;
  best_version = NULL;
  moves = NULL;

  if (!check(!mailactivesync_options_supports_command(NULL, "Sync"),
      "OPTIONS NULL command helper did not fail"))
    return 0;
  if (!check(!mailactivesync_options_supports_command(NULL, NULL),
      "OPTIONS NULL command helper NULL value did not fail"))
    return 0;
  if (!check(!mailactivesync_options_supports_protocol_version(NULL, "16.1"),
      "OPTIONS NULL version helper did not fail"))
    return 0;
  if (!check(!mailactivesync_options_supports_protocol_version(NULL, NULL),
      "OPTIONS NULL version helper NULL value did not fail"))
    return 0;
  if (!check(mailactivesync_options_best_protocol_version(NULL,
      preferred_versions, &best_version) == MAILACTIVESYNC_ERROR_BAD_STATE,
      "OPTIONS best version NULL options did not fail as bad state"))
    return 0;
  if (!check(mailactivesync_options_best_protocol_version(NULL,
      preferred_versions, NULL) == MAILACTIVESYNC_ERROR_BAD_STATE,
      "OPTIONS best version NULL result did not fail as bad state"))
    return 0;

  if (!check(mailactivesync_send_mail(NULL, "msg", 3, 1) ==
      MAILACTIVESYNC_ERROR_BAD_STATE,
      "SendMail NULL session did not fail as bad state"))
    return 0;
  if (!check(mailactivesync_send_mail(session, NULL, 0, 1) ==
      MAILACTIVESYNC_ERROR_BAD_STATE,
      "SendMail NULL message did not fail as bad state"))
    return 0;
  if (!check(mailactivesync_smart_reply(NULL, "5", "server-1", "msg", 3,
      1) == MAILACTIVESYNC_ERROR_BAD_STATE,
      "SmartReply NULL session did not fail as bad state"))
    return 0;
  if (!check(mailactivesync_smart_forward(NULL, "5", "server-1", "msg", 3,
      1) == MAILACTIVESYNC_ERROR_BAD_STATE,
      "SmartForward NULL session did not fail as bad state"))
    return 0;
  memset(&compose_request, 0, sizeof(compose_request));
  compose_request.mime_message = "msg";
  compose_request.mime_message_len = 3;
  compose_request.client_id = "";
  if (!check(mailactivesync_send_mail_ext(session, &compose_request) ==
      MAILACTIVESYNC_ERROR_BAD_STATE,
      "SendMail extended empty ClientId did not fail as bad state"))
    return 0;
  compose_request.client_id = NULL;
  compose_request.collection_id = "inbox";
  if (!check(mailactivesync_send_mail_ext(session, &compose_request) ==
      MAILACTIVESYNC_ERROR_BAD_STATE,
      "SendMail extended source did not fail as bad state"))
    return 0;
  compose_request.server_id = "server-1";
  compose_request.long_id = "search-long-id";
  if (!check(mailactivesync_smart_forward_ext(session, &compose_request) ==
      MAILACTIVESYNC_ERROR_BAD_STATE,
      "SmartForward extended mixed source did not fail as bad state"))
    return 0;
  if (!check(mailactivesync_folder_create(NULL, "1", "0", "Folder", 12,
      NULL) == MAILACTIVESYNC_ERROR_BAD_STATE,
      "FolderCreate NULL arguments did not fail as bad state"))
    return 0;
  if (!check(mailactivesync_folder_update(NULL, "1", "folder-1", "0",
      "Folder", NULL) == MAILACTIVESYNC_ERROR_BAD_STATE,
      "FolderUpdate NULL arguments did not fail as bad state"))
    return 0;
  if (!check(mailactivesync_folder_delete(NULL, "1", "folder-1", NULL) ==
      MAILACTIVESYNC_ERROR_BAD_STATE,
      "FolderDelete NULL arguments did not fail as bad state"))
    return 0;
  if (!check(mailactivesync_folder_resync(NULL, NULL) ==
      MAILACTIVESYNC_ERROR_BAD_STATE,
      "Folder resync NULL arguments did not fail as bad state"))
    return 0;
  if (!check(!mailactivesync_folder_mutation_result_needs_resync(NULL),
      "Folder mutation NULL result unexpectedly required resync"))
    return 0;
  if (!check(mailactivesync_get_item_estimate_multi(NULL, NULL, NULL) ==
      MAILACTIVESYNC_ERROR_BAD_STATE,
      "GetItemEstimate multi NULL arguments did not fail as bad state"))
    return 0;
  if (!check(mailactivesync_settings_get_user_information(NULL, NULL) ==
      MAILACTIVESYNC_ERROR_BAD_STATE,
      "Settings UserInformation NULL arguments did not fail as bad state"))
    return 0;
  if (!check(mailactivesync_get_item_estimate_result_collection(NULL,
      "inbox") == NULL,
      "GetItemEstimate collection NULL result did not return NULL"))
    return 0;
  if (!check(mailactivesync_get_item_estimate_result_collection_status_to_error(
      NULL, "inbox") == MAILACTIVESYNC_ERROR_BAD_STATE,
      "GetItemEstimate collection status NULL result did not fail"))
    return 0;
  if (!check(mailactivesync_item_operations_fetch_attachment(NULL,
      "file-ref-1", NULL, NULL) == MAILACTIVESYNC_ERROR_BAD_STATE,
      "ItemOperations attachment NULL arguments did not fail as bad state"))
    return 0;
  if (!check(mailactivesync_item_operations_fetch_body_part(NULL, "inbox",
      "server-1", MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_HTML, 0, NULL) ==
      MAILACTIVESYNC_ERROR_BAD_STATE,
      "ItemOperations BodyPart NULL arguments did not fail as bad state"))
    return 0;
  if (!check(mailactivesync_item_operations_fetch_body_part(session, "inbox",
      "server-1", 0, 0, &item_result) == MAILACTIVESYNC_ERROR_BAD_STATE,
      "ItemOperations BodyPart invalid body type did not fail as bad state"))
    return 0;
  if (!check(mailactivesync_item_operations_fetch_multi(NULL, NULL, NULL) ==
      MAILACTIVESYNC_ERROR_BAD_STATE,
      "ItemOperations multi Fetch NULL arguments did not fail as bad state"))
    return 0;
  if (!check(!mailactivesync_airsyncbase_body_needs_fetch(NULL) &&
      !mailactivesync_body_part_needs_fetch(NULL) &&
      !mailactivesync_message_needs_body_fetch(NULL) &&
      !mailactivesync_item_needs_body_fetch(NULL),
      "Body fetch helper NULL arguments unexpectedly needed fetch"))
    return 0;
  memset(&search_request, 0, sizeof(search_request));
  search_request.collection_id = "inbox";
  search_request.free_text = "needle";
  if (!check(mailactivesync_mail_search(NULL, &search_request, NULL) ==
      MAILACTIVESYNC_ERROR_BAD_STATE,
      "Search NULL arguments did not fail as bad state"))
    return 0;
  search_request.range_start = 5;
  search_request.range_end = 4;
  if (!check(mailactivesync_mail_search(session, &search_request,
      &search_result) ==
      MAILACTIVESYNC_ERROR_BAD_STATE,
      "Search invalid range did not fail as bad state"))
    return 0;
  memset(&find_request, 0, sizeof(find_request));
  find_request.search_id = "01234567-89ab-cdef-0123-456789abcdef";
  find_request.collection_id = "inbox";
  find_request.query = "needle";
  if (!check(mailactivesync_mail_find(NULL, &find_request, NULL) ==
      MAILACTIVESYNC_ERROR_BAD_STATE,
      "Find NULL arguments did not fail as bad state"))
    return 0;
  find_request.range_start = 5;
  find_request.range_end = 4;
  if (!check(mailactivesync_mail_find(session, &find_request,
      &find_result) == MAILACTIVESYNC_ERROR_BAD_STATE,
      "Find invalid range did not fail as bad state"))
    return 0;
  if (!check(mailactivesync_resolve_recipients(NULL, moves, 0,
      &resolve_result) == MAILACTIVESYNC_ERROR_BAD_STATE,
      "ResolveRecipients NULL arguments did not fail as bad state"))
    return 0;
  memset(&validate_request, 0, sizeof(validate_request));
  validate_request.certificates = moves;
  if (!check(mailactivesync_validate_cert(NULL, &validate_request,
      &validate_result) == MAILACTIVESYNC_ERROR_BAD_STATE,
      "ValidateCert NULL session did not fail as bad state"))
    return 0;
  validate_request.certificates = NULL;
  if (!check(mailactivesync_validate_cert(session, &validate_request,
      &validate_result) == MAILACTIVESYNC_ERROR_BAD_STATE,
      "ValidateCert empty request did not fail as bad state"))
    return 0;
  if (!check(mailactivesync_move_items(NULL, moves, NULL) ==
      MAILACTIVESYNC_ERROR_BAD_STATE,
      "MoveItems NULL arguments did not fail as bad state"))
    return 0;
  memset(&draft, 0, sizeof(draft));
  if (!check(mailactivesync_add_draft(NULL, "drafts", "1", "client-1",
      &draft, NULL) == MAILACTIVESYNC_ERROR_BAD_STATE,
      "Add draft NULL result did not fail as bad state"))
    return 0;
  if (!check(mailactivesync_update_draft(NULL, "drafts", "1", "server-1",
      &draft, NULL) == MAILACTIVESYNC_ERROR_BAD_STATE,
      "Update draft NULL result did not fail as bad state"))
    return 0;
  if (!check(mailactivesync_ping(NULL, &ping_request, &ping_result) ==
      MAILACTIVESYNC_ERROR_BAD_STATE,
      "Ping NULL session did not fail as bad state"))
    return 0;
  if (!check(mailactivesync_sync_request_set_wait(NULL, 10) ==
      MAILACTIVESYNC_ERROR_BAD_STATE,
      "Sync Wait NULL request did not fail as bad state"))
    return 0;
  if (!check(mailactivesync_sync_request_set_heartbeat_interval(NULL, 60) ==
      MAILACTIVESYNC_ERROR_BAD_STATE,
      "Sync HeartbeatInterval NULL request did not fail as bad state"))
    return 0;
  if (!check(mailactivesync_sync_request_set_conversation_mode(NULL, 1) ==
      MAILACTIVESYNC_ERROR_BAD_STATE,
      "Sync ConversationMode NULL request did not fail as bad state"))
    return 0;
  if (!check(mailactivesync_sync_request_set_rights_management_support(NULL,
      1) == MAILACTIVESYNC_ERROR_BAD_STATE,
      "Sync RightsManagementSupport NULL request did not fail as bad state"))
    return 0;
  if (!check(mailactivesync_sync_request_set_body_preference_all_or_none(
      NULL, 1) == MAILACTIVESYNC_ERROR_BAD_STATE,
      "Sync BodyPreference AllOrNone NULL request did not fail as bad state"))
    return 0;
  if (!check(mailactivesync_sync_request_add_body_preference(NULL,
      MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_MIME, 0) ==
      MAILACTIVESYNC_ERROR_BAD_STATE,
      "Sync add BodyPreference NULL request did not fail as bad state"))
    return 0;
  if (!check(mailactivesync_sync_request_add_supported_property(NULL,
      MAILACTIVESYNC_CP_EMAIL, MAILACTIVESYNC_EMAIL_READ) ==
      MAILACTIVESYNC_ERROR_BAD_STATE,
      "Sync add Supported property NULL request did not fail as bad state"))
    return 0;
  sync_request = mailactivesync_sync_request_new("5", "1");
  if (!check(sync_request != NULL, "Sync bad-state request allocation failed"))
    return 0;
  if (!check(mailactivesync_sync_request_set_body_preference_all_or_none(
      sync_request, 1) == MAILACTIVESYNC_ERROR_BAD_STATE,
      "Sync BodyPreference AllOrNone without preference did not fail"))
    return 0;
  if (!check(mailactivesync_sync_request_add_body_preference(sync_request,
      0, 0) == MAILACTIVESYNC_ERROR_BAD_STATE,
      "Sync add BodyPreference invalid type did not fail as bad state"))
    return 0;
  if (!check(mailactivesync_sync_request_set_wait(sync_request, 60) ==
      MAILACTIVESYNC_ERROR_BAD_STATE,
      "Sync invalid Wait did not fail as bad state"))
    return 0;
  if (!check(mailactivesync_sync_request_set_heartbeat_interval(sync_request,
      59) == MAILACTIVESYNC_ERROR_BAD_STATE,
      "Sync invalid HeartbeatInterval did not fail as bad state"))
    return 0;
  if (!check(mailactivesync_sync_multi(NULL, NULL, NULL) ==
      MAILACTIVESYNC_ERROR_BAD_STATE,
      "Sync multi NULL arguments did not fail as bad state"))
    return 0;
  if (!check(mailactivesync_sync_result_collection(NULL, "inbox") == NULL,
      "Sync result collection NULL result did not return NULL"))
    return 0;
  if (!check(mailactivesync_sync_result_collection_status_to_error(NULL,
      "inbox") == MAILACTIVESYNC_ERROR_BAD_STATE,
      "Sync result collection status NULL result did not fail as bad state"))
    return 0;
  if (!check(mailactivesync_sync_result_command_response(NULL,
      MAILACTIVESYNC_SYNC_COMMAND_ADD, "client-1", NULL) == NULL,
      "Sync command response NULL result did not return NULL"))
    return 0;
  if (!check(mailactivesync_sync_result_command_response_status_to_error(NULL,
      MAILACTIVESYNC_SYNC_COMMAND_ADD, "client-1", NULL) ==
      MAILACTIVESYNC_ERROR_BAD_STATE,
      "Sync command response status NULL result did not fail as bad state"))
    return 0;
  memset(&sync_result, 0, sizeof(sync_result));
  if (!check(mailactivesync_sync_result_command_response_status_to_error(
      &sync_result, 0, NULL, NULL) == MAILACTIVESYNC_ERROR_BAD_STATE,
      "Sync command response status invalid type did not fail as bad state"))
    return 0;
  mailactivesync_sync_request_free(sync_request);

  return 1;
}

struct activesync_http_test_case {
  const char * name;
  int (* run)(void);
};

#define HTTP_TEST(function) { #function, function }

static const struct activesync_http_test_case http_tests[] = {
  HTTP_TEST(test_response_header_lookup),
  HTTP_TEST(test_request_body_copy),
  HTTP_TEST(test_options_success),
  HTTP_TEST(test_options_trims_empty_list_items),
  HTTP_TEST(test_options_missing_headers),
  HTTP_TEST(test_options_unauthorized),
  HTTP_TEST(test_options_status_mapping),
  HTTP_TEST(test_options_advertised_command_enforcement),
  HTTP_TEST(test_oauth2_token_replacement),
  HTTP_TEST(test_active_sync_status_mapping),
  HTTP_TEST(test_version_specific_email_restrictions),
  HTTP_TEST(test_provision_success),
  HTTP_TEST(test_settings_device_information_success),
  HTTP_TEST(test_get_item_estimate_success_and_empty_response),
  HTTP_TEST(test_get_item_estimate_multi_success),
  HTTP_TEST(test_item_operations_fetch_success),
  HTTP_TEST(test_item_operations_fetch_multi_success),
  HTTP_TEST(test_item_operations_fetch_body_part_success),
  HTTP_TEST(test_item_operations_fetch_attachment_success_and_status),
  HTTP_TEST(test_mail_search_success_and_status),
  HTTP_TEST(test_mail_find_success_and_status),
  HTTP_TEST(test_validate_cert_success_and_status),
  HTTP_TEST(test_resolve_recipients_success_and_status),
  HTTP_TEST(test_outlook_anchor_mailbox_cookie),
  HTTP_TEST(test_anchor_mailbox_cookie_not_sent_to_office365),
  HTTP_TEST(test_command_post_request),
  HTTP_TEST(test_command_post_basic_auth),
  HTTP_TEST(test_folder_sync_success),
  HTTP_TEST(test_folder_mutation_success_and_status),
  HTTP_TEST(test_folder_sync_response_errors),
  HTTP_TEST(test_sync_success),
  HTTP_TEST(test_sync_multi_collection_success),
  HTTP_TEST(test_sync_empty_response_and_request_defaults),
  HTTP_TEST(test_sync_activesync_25_collection_class_shape),
  HTTP_TEST(test_sync_wait_and_heartbeat_request_shape),
  HTTP_TEST(test_sync_conversation_mode_request_shape),
  HTTP_TEST(test_sync_top_level_status_response),
  HTTP_TEST(test_sync_empty_http_response_is_no_change),
  HTTP_TEST(test_sync_change_softdelete_and_html_body),
  HTTP_TEST(test_sync_response_errors),
  HTTP_TEST(test_sync_client_command_request_and_responses),
  HTTP_TEST(test_sync_mail_mutation_helpers),
  HTTP_TEST(test_sync_draft_helpers),
  HTTP_TEST(test_move_items_success),
  HTTP_TEST(test_ping_success_and_statuses),
  HTTP_TEST(test_composemail_send_mail_success_and_error),
  HTTP_TEST(test_composemail_smart_reply_forward_success),
  HTTP_TEST(test_active_sync_public_bad_state),
};

#undef HTTP_TEST

size_t activesync_http_test_count(void)
{
  return sizeof(http_tests) / sizeof(http_tests[0]);
}

const char * activesync_http_test_name(size_t index)
{
  if (index >= activesync_http_test_count())
    return NULL;
  return http_tests[index].name;
}

int activesync_http_test_run_case(size_t index,
    test_failure_callback failure_callback, void * context)
{
  if (index >= activesync_http_test_count())
    return -1;
  if (!http_tests[index].run()) {
    if (failure_callback != NULL)
      failure_callback(__FILE__, __LINE__, http_tests[index].name,
          "ActiveSync HTTP test failed", context);
    return -1;
  }
  return 0;
}

int activesync_http_test_run(void)
{
  size_t index;
  for (index = 0; index < activesync_http_test_count(); index++) {
    if (activesync_http_test_run_case(index, NULL, NULL) != 0)
      return -1;
  }
  return 0;
}

#ifndef ACTIVESYNC_HTTP_NO_MAIN
int main(void)
{
  return activesync_http_test_run() == 0 ? 0 : 1;
}
#endif
