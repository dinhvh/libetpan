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
  context->commands = "Sync, FolderSync, ItemOperations";
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
    session = mailactivesync_new(0, NULL);
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

  return 1;
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
  struct mailactivesync_wbxml_document * request_document;
  int r;

  session = NULL;
  context = NULL;
  result = NULL;
  root = NULL;
  request_document = NULL;

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
  device_node = test_node_child(request_document->root,
      MAILACTIVESYNC_CP_SETTINGS,
      MAILACTIVESYNC_SETTINGS_DEVICE_INFORMATION);
  set_node = test_node_child(device_node, MAILACTIVESYNC_CP_SETTINGS,
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
  mailactivesync_settings_result_free(result);
  mailactivesync_free(session);
  return 1;

 err:
  mailactivesync_wbxml_document_free(request_document);
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
  mailactivesync_item_free(item);
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
  struct mailactivesync_wbxml_node * request_options;
  struct mailactivesync_wbxml_node * request_body_pref;
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
  if (!check(mailactivesync_sync_request_set_body_preference(request,
      MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_PLAIN_TEXT, 1024) ==
      MAILACTIVESYNC_NO_ERROR, "set plain BodyPreference failed"))
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
  if (!check(strcmp(test_node_child_text(request_body_pref,
      MAILACTIVESYNC_CP_AIRSYNCBASE, MAILACTIVESYNC_AIRSYNCBASE_TYPE),
      "1") == 0, "plain Sync request body type mismatch"))
    goto err;
  if (!check(strcmp(test_node_child_text(request_body_pref,
      MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_TRUNCATION_SIZE), "1024") == 0,
      "plain Sync request body truncation mismatch"))
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
      (result->sync_key == NULL), "top-level Sync status result mismatch"))
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
  if (!check(mailactivesync_wbxml_node_add_child(body, attachments) ==
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
  if (!test_options_status_mapping())
    return 1;
  if (!test_oauth2_token_replacement())
    return 1;
  if (!test_active_sync_status_mapping())
    return 1;
  if (!test_provision_success())
    return 1;
  if (!test_settings_device_information_success())
    return 1;
  if (!test_get_item_estimate_success_and_empty_response())
    return 1;
  if (!test_item_operations_fetch_success())
    return 1;
  if (!test_outlook_anchor_mailbox_cookie())
    return 1;
  if (!test_anchor_mailbox_cookie_not_sent_to_office365())
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
  if (!test_sync_activesync_25_collection_class_shape())
    return 1;
  if (!test_sync_top_level_status_response())
    return 1;
  if (!test_sync_empty_http_response_is_no_change())
    return 1;
  if (!test_sync_change_softdelete_and_html_body())
    return 1;
  if (!test_sync_response_errors())
    return 1;

  return 0;
}
