/*
 * libEtPan! -- a mail stuff library
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include <libetpan/mailjmap.h>

#include "../src/data-types/mailjson.h"
#include "../src/low-level/jmap/mailjmap_request.h"
#include "../src/low-level/jmap/mailjmap_response.h"
#include "../src/low-level/jmap/mailjmap_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct fake_context {
  int calls;
  int response_mode;
  int api_status_code;
  char * method;
  char * url;
  char * accept_type;
  char * content_type;
  char * authorization;
  char * body;
  time_t timeout;
};

static const char session_json[] =
    "{"
    "\"capabilities\":{\"urn:ietf:params:jmap:core\":{}},"
    "\"accounts\":{},"
    "\"primaryAccounts\":{},"
    "\"apiUrl\":\"https://example.com/jmap/api\","
    "\"downloadUrl\":\"https://example.com/download/{accountId}/{blobId}/{name}\","
    "\"uploadUrl\":\"https://example.com/upload/{accountId}\","
    "\"sessionState\":\"state1\""
    "}";

static const char call_json[] =
    "{"
    "\"methodResponses\":["
    "[\"Mailbox/get\",{\"state\":\"s1\",\"list\":[]},\"c1\"]"
    "],"
    "\"sessionState\":\"state2\""
    "}";

static const char method_error_json[] =
    "{"
    "\"methodResponses\":["
    "[\"error\",{\"type\":\"invalidArguments\","
    "\"description\":\"bad mailbox args\"},\"c1\"]"
    "],"
    "\"sessionState\":\"state-error\""
    "}";

static const char problem_json[] =
    "{"
    "\"type\":\"urn:ietf:params:jmap:error:limit\","
    "\"detail\":\"too many calls\""
    "}";

static const char unknown_capability_problem_json[] =
    "{"
    "\"type\":\"urn:ietf:params:jmap:error:unknownCapability\","
    "\"detail\":\"unknown using capability\""
    "}";

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

static char * dup_string(const char * value)
{
  if (value == NULL)
    return NULL;

  return strdup(value);
}

static int remember_string(char ** target, const char * value)
{
  char * copy;

  copy = NULL;
  if (value != NULL) {
    copy = dup_string(value);
    if (copy == NULL)
      return MAILJMAP_ERROR_MEMORY;
  }

  free(* target);
  * target = copy;
  return MAILJMAP_NO_ERROR;
}

static void fake_context_clear(struct fake_context * context)
{
  if (context == NULL)
    return;

  free(context->method);
  free(context->url);
  free(context->accept_type);
  free(context->content_type);
  free(context->authorization);
  free(context->body);
  memset(context, 0, sizeof(* context));
}

static int capture_request(struct fake_context * context,
    struct mailjmap_http_request * request)
{
  clistiter * cur;
  int r;

  r = remember_string(&context->method, request->method);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  r = remember_string(&context->url, request->url);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  r = remember_string(&context->accept_type, request->accept_type);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  r = remember_string(&context->content_type, request->content_type);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  for (cur = clist_begin(request->headers); cur != NULL;
      cur = clist_next(cur)) {
    struct mailjmap_http_header * header;

    header = clist_content(cur);
    if ((header != NULL) && (strcmp(header->name, "Authorization") == 0)) {
      r = remember_string(&context->authorization, header->value);
      if (r != MAILJMAP_NO_ERROR)
        return r;
    }
  }

  free(context->body);
  context->body = NULL;
  if (request->body_len > 0) {
    context->body = malloc(request->body_len + 1);
    if (context->body == NULL)
      return MAILJMAP_ERROR_MEMORY;
    memcpy(context->body, request->body, request->body_len);
    context->body[request->body_len] = '\0';
  }

  context->timeout = request->timeout;
  return MAILJMAP_NO_ERROR;
}

static int set_response_body(struct mailjmap_http_response * response,
    const char * body)
{
  size_t len;

  len = strlen(body);
  response->body = malloc(len);
  if (response->body == NULL)
    return MAILJMAP_ERROR_MEMORY;

  memcpy(response->body, body, len);
  response->body_len = len;
  return MAILJMAP_NO_ERROR;
}

static int fake_perform(struct mailjmap_http_transport * transport,
    struct mailjmap_http_request * request,
    struct mailjmap_http_response ** result)
{
  struct fake_context * context;
  struct mailjmap_http_response * response;
  const char * body;
  int r;

  context = transport->context;
  context->calls ++;

  r = capture_request(context, request);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  response = mailjmap_http_response_new((context->calls > 1) &&
      (context->api_status_code != 0) ? context->api_status_code : 200);
  if (response == NULL)
    return MAILJMAP_ERROR_MEMORY;

  if (context->calls == 1)
    body = session_json;
  else if (context->response_mode == 1)
    body = method_error_json;
  else if (context->response_mode == 2)
    body = problem_json;
  else if (context->response_mode == 3)
    body = unknown_capability_problem_json;
  else
    body = call_json;
  r = set_response_body(response, body);
  if (r != MAILJMAP_NO_ERROR) {
    mailjmap_http_response_free(response);
    return r;
  }

  * result = response;
  return MAILJMAP_NO_ERROR;
}

static struct mailjmap_http_transport * fake_transport_new(
    struct fake_context * context)
{
  struct mailjmap_http_transport * transport;

  transport = malloc(sizeof(* transport));
  if (transport == NULL)
    return NULL;

  transport->context = context;
  transport->perform = fake_perform;
  transport->free = NULL;
  return transport;
}

static int add_string_property(mailjson_value * object,
    const char * key, const char * value)
{
  mailjson_value * string_value;
  int r;

  string_value = NULL;
  r = mailjson_new_string(value, &string_value);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  r = mailjson_object_set_new(object, key, string_value);
  if (r != MAILJMAP_NO_ERROR) {
    mailjson_free(string_value);
    return r;
  }

  return MAILJMAP_NO_ERROR;
}

static int mailbox_get_request_new(struct mailjmap_request ** result)
{
  struct mailjmap_request * request;
  mailjson_value * arguments;
  int r;

  if (result == NULL)
    return MAILJMAP_ERROR_BAD_STATE;

  * result = NULL;
  request = NULL;
  arguments = NULL;

  request = mailjmap_request_new();
  if (request == NULL)
    return MAILJMAP_ERROR_MEMORY;

  r = mailjson_new_object(&arguments);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = add_string_property(arguments, "accountId", "acc1");
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = mailjmap_request_add_call_json(request, "Mailbox/get", arguments, "c1");
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;

  arguments = NULL;
  * result = request;
  return MAILJMAP_NO_ERROR;

 cleanup:
  mailjson_free(arguments);
  mailjmap_request_free(request);
  return r;
}

static int test_call_posts_to_api_url(void)
{
  mailjmap * client;
  struct fake_context context;
  struct mailjmap_session * session_object;
  struct mailjmap_request * request;
  struct mailjmap_response * response;
  struct mailjmap_method_response * method_response;
  int r;
  int ok;

  memset(&context, 0, sizeof(context));
  client = NULL;
  session_object = NULL;
  request = NULL;
  response = NULL;
  ok = 0;

  client = mailjmap_new(0, NULL);
  if (!check(client != NULL, "client allocation failed"))
    goto cleanup;
  r = mailjmap_set_http_transport(client, fake_transport_new(&context));
  if (!check(r == MAILJMAP_NO_ERROR, "transport set failed"))
    goto cleanup;
  r = mailjmap_connect(client, "https://example.com/jmap/session");
  if (!check(r == MAILJMAP_NO_ERROR, "connect failed"))
    goto cleanup;
  r = mailjmap_login_oauth2(client, "user@example.com", "token");
  if (!check(r == MAILJMAP_NO_ERROR, "login failed"))
    goto cleanup;
  r = mailjmap_set_timeout(client, 9);
  if (!check(r == MAILJMAP_NO_ERROR, "timeout set failed"))
    goto cleanup;

  r = mailjmap_get_session(client, &session_object);
  if (!check(r == MAILJMAP_NO_ERROR, "get session failed"))
    goto cleanup;

  r = mailbox_get_request_new(&request);
  if (!check(r == MAILJMAP_NO_ERROR, "request allocation failed"))
    goto cleanup;

  r = mailjmap_call(client, request, &response);
  if (!check(r == MAILJMAP_NO_ERROR, "JMAP call failed"))
    goto cleanup;

  method_response = clist_content(clist_begin(response->method_responses));
  ok = check(context.calls == 2, "call count mismatch") &&
      check(str_equal(context.method, "POST"), "POST method mismatch") &&
      check(str_equal(context.url, "https://example.com/jmap/api"),
          "API URL mismatch") &&
      check(str_equal(context.accept_type, "application/json"),
          "accept type mismatch") &&
      check(str_equal(context.content_type, "application/json"),
          "content type mismatch") &&
      check(str_equal(context.authorization, "Bearer token"),
          "auth header mismatch") &&
      check(context.timeout == 9, "timeout mismatch") &&
      check(strstr(context.body, "\"methodCalls\"") != NULL,
          "request body missing methodCalls") &&
      check(strstr(context.body, "\"Mailbox/get\"") != NULL,
          "request body missing method name") &&
      check(mailjmap_get_last_http_status(client) == 200,
          "last HTTP status mismatch") &&
      check(str_equal(response->session_state, "state2"),
          "response sessionState mismatch") &&
      check(str_equal(method_response->name, "Mailbox/get"),
          "method response name mismatch") &&
      check(str_equal(method_response->call_id, "c1"),
          "method response call id mismatch") &&
      check(mailjmap_get_last_problem_type(client) == NULL,
          "unexpected problem diagnostic") &&
      check(mailjmap_get_last_method_error_type(client) == NULL,
          "unexpected method error diagnostic");

 cleanup:
  mailjmap_response_free(response);
  mailjmap_request_free(request);
  mailjmap_session_free(session_object);
  mailjmap_free(client);
  fake_context_clear(&context);
  return ok;
}

static int test_call_records_method_error_diagnostics(void)
{
  mailjmap * client;
  struct fake_context context;
  struct mailjmap_session * session_object;
  struct mailjmap_request * request;
  struct mailjmap_response * response;
  int r;
  int ok;

  memset(&context, 0, sizeof(context));
  context.response_mode = 1;
  client = NULL;
  session_object = NULL;
  request = NULL;
  response = NULL;
  ok = 0;

  client = mailjmap_new(0, NULL);
  if (!check(client != NULL, "client allocation failed"))
    goto cleanup;
  r = mailjmap_set_http_transport(client, fake_transport_new(&context));
  if (!check(r == MAILJMAP_NO_ERROR, "transport set failed"))
    goto cleanup;
  r = mailjmap_connect(client, "https://example.com/jmap/session");
  if (!check(r == MAILJMAP_NO_ERROR, "connect failed"))
    goto cleanup;
  r = mailjmap_login_oauth2(client, "user@example.com", "token");
  if (!check(r == MAILJMAP_NO_ERROR, "login failed"))
    goto cleanup;
  r = mailjmap_get_session(client, &session_object);
  if (!check(r == MAILJMAP_NO_ERROR, "get session failed"))
    goto cleanup;
  r = mailbox_get_request_new(&request);
  if (!check(r == MAILJMAP_NO_ERROR, "request allocation failed"))
    goto cleanup;

  r = mailjmap_call(client, request, &response);
  if (!check(r == MAILJMAP_NO_ERROR, "JMAP method error call failed"))
    goto cleanup;

  ok = check(str_equal(mailjmap_get_last_method_name(client),
          "Mailbox/get"), "method diagnostic name mismatch") &&
      check(str_equal(mailjmap_get_last_method_call_id(client), "c1"),
          "method diagnostic call id mismatch") &&
      check(str_equal(mailjmap_get_last_method_error_type(client),
          "invalidArguments"), "method diagnostic type mismatch") &&
      check(str_equal(mailjmap_get_last_method_error_description(client),
          "bad mailbox args"), "method diagnostic description mismatch") &&
      check(str_equal(mailjmap_get_last_error_message(client),
          "bad mailbox args"), "method diagnostic message mismatch") &&
      check(mailjmap_get_last_problem_type(client) == NULL,
          "unexpected problem type for method error");

 cleanup:
  mailjmap_response_free(response);
  mailjmap_request_free(request);
  mailjmap_session_free(session_object);
  mailjmap_free(client);
  fake_context_clear(&context);
  return ok;
}

static int test_call_records_problem_diagnostics(void)
{
  mailjmap * client;
  struct fake_context context;
  struct mailjmap_session * session_object;
  struct mailjmap_request * request;
  struct mailjmap_response * response;
  int r;
  int ok;

  memset(&context, 0, sizeof(context));
  context.response_mode = 2;
  client = NULL;
  session_object = NULL;
  request = NULL;
  response = NULL;
  ok = 0;

  client = mailjmap_new(0, NULL);
  if (!check(client != NULL, "client allocation failed"))
    goto cleanup;
  r = mailjmap_set_http_transport(client, fake_transport_new(&context));
  if (!check(r == MAILJMAP_NO_ERROR, "transport set failed"))
    goto cleanup;
  r = mailjmap_connect(client, "https://example.com/jmap/session");
  if (!check(r == MAILJMAP_NO_ERROR, "connect failed"))
    goto cleanup;
  r = mailjmap_login_oauth2(client, "user@example.com", "token");
  if (!check(r == MAILJMAP_NO_ERROR, "login failed"))
    goto cleanup;
  r = mailjmap_get_session(client, &session_object);
  if (!check(r == MAILJMAP_NO_ERROR, "get session failed"))
    goto cleanup;
  r = mailbox_get_request_new(&request);
  if (!check(r == MAILJMAP_NO_ERROR, "request allocation failed"))
    goto cleanup;

  r = mailjmap_call(client, request, &response);
  if (!check(r == MAILJMAP_NO_ERROR, "JMAP problem call failed"))
    goto cleanup;

  ok = check(str_equal(mailjmap_get_last_problem_type(client),
          "urn:ietf:params:jmap:error:limit"),
          "problem diagnostic type mismatch") &&
      check(str_equal(mailjmap_get_last_error_message(client),
          "too many calls"), "problem diagnostic message mismatch") &&
      check(mailjmap_get_last_method_error_type(client) == NULL,
          "unexpected method error type for problem");

 cleanup:
  mailjmap_response_free(response);
  mailjmap_request_free(request);
  mailjmap_session_free(session_object);
  mailjmap_free(client);
  fake_context_clear(&context);
  return ok;
}

static int test_call_records_http_problem_diagnostics(void)
{
  mailjmap * client;
  struct fake_context context;
  struct mailjmap_session * session_object;
  struct mailjmap_request * request;
  struct mailjmap_response * response;
  int r;
  int ok;

  memset(&context, 0, sizeof(context));
  context.response_mode = 2;
  context.api_status_code = 429;
  client = NULL;
  session_object = NULL;
  request = NULL;
  response = NULL;
  ok = 0;

  client = mailjmap_new(0, NULL);
  if (!check(client != NULL, "client allocation failed"))
    goto cleanup;
  r = mailjmap_set_http_transport(client, fake_transport_new(&context));
  if (!check(r == MAILJMAP_NO_ERROR, "transport set failed"))
    goto cleanup;
  r = mailjmap_connect(client, "https://example.com/jmap/session");
  if (!check(r == MAILJMAP_NO_ERROR, "connect failed"))
    goto cleanup;
  r = mailjmap_login_oauth2(client, "user@example.com", "token");
  if (!check(r == MAILJMAP_NO_ERROR, "login failed"))
    goto cleanup;
  r = mailjmap_get_session(client, &session_object);
  if (!check(r == MAILJMAP_NO_ERROR, "get session failed"))
    goto cleanup;
  r = mailbox_get_request_new(&request);
  if (!check(r == MAILJMAP_NO_ERROR, "request allocation failed"))
    goto cleanup;

  r = mailjmap_call(client, request, &response);
  if (!check(r == MAILJMAP_ERROR_LIMIT, "HTTP problem call status mismatch"))
    goto cleanup;

  ok = check(response == NULL, "unexpected response for HTTP problem") &&
      check(mailjmap_get_last_http_status(client) == 429,
          "HTTP problem status diagnostic mismatch") &&
      check(str_equal(mailjmap_get_last_problem_type(client),
          "urn:ietf:params:jmap:error:limit"),
          "HTTP problem diagnostic type mismatch") &&
      check(str_equal(mailjmap_get_last_error_message(client),
          "too many calls"), "HTTP problem diagnostic message mismatch") &&
      check(mailjmap_get_last_method_error_type(client) == NULL,
          "unexpected method error type for HTTP problem");

 cleanup:
  mailjmap_response_free(response);
  mailjmap_request_free(request);
  mailjmap_session_free(session_object);
  mailjmap_free(client);
  fake_context_clear(&context);
  return ok;
}

static int test_call_maps_unknown_capability_problem(void)
{
  mailjmap * client;
  struct fake_context context;
  struct mailjmap_session * session_object;
  struct mailjmap_request * request;
  struct mailjmap_response * response;
  int r;
  int ok;

  memset(&context, 0, sizeof(context));
  context.response_mode = 3;
  context.api_status_code = 400;
  client = NULL;
  session_object = NULL;
  request = NULL;
  response = NULL;
  ok = 0;

  client = mailjmap_new(0, NULL);
  if (!check(client != NULL, "client allocation failed"))
    goto cleanup;
  r = mailjmap_set_http_transport(client, fake_transport_new(&context));
  if (!check(r == MAILJMAP_NO_ERROR, "transport set failed"))
    goto cleanup;
  r = mailjmap_connect(client, "https://example.com/jmap/session");
  if (!check(r == MAILJMAP_NO_ERROR, "connect failed"))
    goto cleanup;
  r = mailjmap_login_oauth2(client, "user@example.com", "token");
  if (!check(r == MAILJMAP_NO_ERROR, "login failed"))
    goto cleanup;
  r = mailjmap_get_session(client, &session_object);
  if (!check(r == MAILJMAP_NO_ERROR, "get session failed"))
    goto cleanup;
  r = mailbox_get_request_new(&request);
  if (!check(r == MAILJMAP_NO_ERROR, "request allocation failed"))
    goto cleanup;

  r = mailjmap_call(client, request, &response);
  if (!check(r == MAILJMAP_ERROR_CAPABILITY,
      "unknownCapability problem status mismatch"))
    goto cleanup;

  ok = check(response == NULL, "unexpected response for capability problem") &&
      check(mailjmap_get_last_http_status(client) == 400,
          "capability problem status diagnostic mismatch") &&
      check(str_equal(mailjmap_get_last_problem_type(client),
          "urn:ietf:params:jmap:error:unknownCapability"),
          "capability problem diagnostic type mismatch") &&
      check(str_equal(mailjmap_get_last_error_message(client),
          "unknown using capability"),
          "capability problem diagnostic message mismatch");

 cleanup:
  mailjmap_response_free(response);
  mailjmap_request_free(request);
  mailjmap_session_free(session_object);
  mailjmap_free(client);
  fake_context_clear(&context);
  return ok;
}

int main(void)
{
  return (test_call_posts_to_api_url() &&
      test_call_records_method_error_diagnostics() &&
      test_call_records_problem_diagnostics() &&
      test_call_records_http_problem_diagnostics() &&
      test_call_maps_unknown_capability_problem()) ? 0 : 1;
}
