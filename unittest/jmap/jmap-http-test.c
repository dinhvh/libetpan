/*
 * libEtPan! -- a mail stuff library
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include <libetpan/mailjmap_http.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct fake_context {
  int calls;
  char * method;
  char * url;
  char * content_type;
  char * accept_type;
  char * authorization;
  unsigned char * body;
  size_t body_len;
  time_t timeout;
};

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
  free(context->content_type);
  free(context->accept_type);
  free(context->authorization);
  free(context->body);
  memset(context, 0, sizeof(* context));
}

static int fake_perform(struct mailjmap_http_transport * transport,
    struct mailjmap_http_request * request,
    struct mailjmap_http_response ** result)
{
  struct fake_context * context;
  struct mailjmap_http_response * response;
  const char * authorization;
  int r;

  context = transport->context;
  context->calls ++;
  context->timeout = request->timeout;

  r = remember_string(&context->method, request->method);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  r = remember_string(&context->url, request->url);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  r = remember_string(&context->content_type, request->content_type);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  r = remember_string(&context->accept_type, request->accept_type);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  authorization = NULL;
  {
    clistiter * cur;

    for (cur = clist_begin(request->headers); cur != NULL;
        cur = clist_next(cur)) {
      struct mailjmap_http_header * header;

      header = clist_content(cur);
      if ((header != NULL) && (strcmp(header->name, "Authorization") == 0))
        authorization = header->value;
    }
  }
  r = remember_string(&context->authorization, authorization);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  free(context->body);
  context->body = NULL;
  context->body_len = 0;
  if (request->body_len > 0) {
    context->body = malloc(request->body_len);
    if (context->body == NULL)
      return MAILJMAP_ERROR_MEMORY;
    memcpy(context->body, request->body, request->body_len);
    context->body_len = request->body_len;
  }

  response = mailjmap_http_response_new(200);
  if (response == NULL)
    return MAILJMAP_ERROR_MEMORY;

  r = mailjmap_http_response_add_header(response, "Content-Type",
      "application/json");
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  response->body = malloc(2);
  if (response->body == NULL) {
    r = MAILJMAP_ERROR_MEMORY;
    goto err;
  }
  memcpy(response->body, "{}", 2);
  response->body_len = 2;
  r = mailjmap_http_response_set_final_url(response,
      "https://example.com/jmap/session");
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  * result = response;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_http_response_free(response);
  return r;
}

static int test_request_response_helpers(void)
{
  struct mailjmap_http_request * request;
  struct mailjmap_http_response * response;
  int r;
  int ok;

  request = NULL;
  response = NULL;
  ok = 0;

  request = mailjmap_http_request_new("POST", "https://example.com/api");
  response = mailjmap_http_response_new(201);
  if (!check((request != NULL) && (response != NULL), "allocation failed"))
    goto cleanup;

  r = mailjmap_http_request_set_content_type(request, "application/json");
  if (!check(r == MAILJMAP_NO_ERROR, "content type set failed"))
    goto cleanup;
  r = mailjmap_http_request_set_accept_type(request, "application/json");
  if (!check(r == MAILJMAP_NO_ERROR, "accept type set failed"))
    goto cleanup;
  r = mailjmap_http_request_add_header(request, "Authorization",
      "Bearer token");
  if (!check(r == MAILJMAP_NO_ERROR, "header add failed"))
    goto cleanup;
  r = mailjmap_http_request_set_body(request,
      (const unsigned char *) "body", 4);
  if (!check(r == MAILJMAP_NO_ERROR, "body set failed"))
    goto cleanup;

  r = mailjmap_http_response_add_header(response, "Content-Type",
      "application/json");
  if (!check(r == MAILJMAP_NO_ERROR, "response header add failed"))
    goto cleanup;
  r = mailjmap_http_response_set_final_url(response,
      "https://example.com/final");
  if (!check(r == MAILJMAP_NO_ERROR, "final URL set failed"))
    goto cleanup;

  ok = check(str_equal(request->content_type, "application/json"),
      "content type mismatch") &&
      check(str_equal(request->accept_type, "application/json"),
          "accept type mismatch") &&
      check(request->body_len == 4, "request body length mismatch") &&
      check(memcmp(request->body, "body", 4) == 0,
          "request body mismatch") &&
      check(str_equal(mailjmap_http_response_header_value(response,
          "content-type"), "application/json"),
          "case-insensitive response header lookup failed") &&
      check(str_equal(response->final_url, "https://example.com/final"),
          "final URL mismatch");

 cleanup:
  mailjmap_http_response_free(response);
  mailjmap_http_request_free(request);
  return ok;
}

static int test_fake_transport(void)
{
  struct fake_context context;
  struct mailjmap_http_transport transport;
  struct mailjmap_http_request * request;
  struct mailjmap_http_response * response;
  int r;
  int ok;

  memset(&context, 0, sizeof(context));
  memset(&transport, 0, sizeof(transport));
  transport.context = &context;
  transport.perform = fake_perform;

  request = NULL;
  response = NULL;
  ok = 0;

  request = mailjmap_http_request_new("POST", "https://example.com/api");
  if (!check(request != NULL, "request allocation failed"))
    goto cleanup;

  request->timeout = 25;
  r = mailjmap_http_request_set_content_type(request, "application/json");
  if (!check(r == MAILJMAP_NO_ERROR, "content type set failed"))
    goto cleanup;
  r = mailjmap_http_request_set_accept_type(request, "application/json");
  if (!check(r == MAILJMAP_NO_ERROR, "accept type set failed"))
    goto cleanup;
  r = mailjmap_http_request_add_header(request, "Authorization",
      "Bearer test-token");
  if (!check(r == MAILJMAP_NO_ERROR, "auth header add failed"))
    goto cleanup;
  r = mailjmap_http_request_set_body(request,
      (const unsigned char *) "{\"using\":[]}", 12);
  if (!check(r == MAILJMAP_NO_ERROR, "body set failed"))
    goto cleanup;

  r = mailjmap_http_perform(&transport, request, &response);
  if (!check(r == MAILJMAP_NO_ERROR, "transport perform failed"))
    goto cleanup;

  ok = check(context.calls == 1, "transport call count mismatch") &&
      check(str_equal(context.method, "POST"), "method mismatch") &&
      check(str_equal(context.url, "https://example.com/api"),
          "URL mismatch") &&
      check(str_equal(context.content_type, "application/json"),
          "content type mismatch") &&
      check(str_equal(context.accept_type, "application/json"),
          "accept type mismatch") &&
      check(str_equal(context.authorization, "Bearer test-token"),
          "auth header mismatch") &&
      check(context.timeout == 25, "timeout mismatch") &&
      check(context.body_len == 12, "body length mismatch") &&
      check(memcmp(context.body, "{\"using\":[]}", 12) == 0,
          "body mismatch") &&
      check(response->status_code == 200, "response status mismatch") &&
      check(str_equal(response->final_url,
          "https://example.com/jmap/session"), "final URL mismatch");

 cleanup:
  mailjmap_http_response_free(response);
  mailjmap_http_request_free(request);
  fake_context_clear(&context);
  return ok;
}

int main(void)
{
  int ok;

  ok = 1;
  ok = test_request_response_helpers() && ok;
  ok = test_fake_transport() && ok;

  if (!ok)
    return 1;

  puts("jmap-http-test: ok");
  return 0;
}
