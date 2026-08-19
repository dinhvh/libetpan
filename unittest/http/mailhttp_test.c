#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libetpan/mailhttp.h>

#include <stdio.h>
#include <string.h>

struct fake_context {
  int calls;
};

static int check(int condition, const char * message)
{
  if (!condition)
    fprintf(stderr, "%s\n", message);
  return condition;
}

static int fake_perform(struct mailhttp_transport * transport,
    struct mailhttp_request * request,
    struct mailhttp_response ** result)
{
  struct fake_context * context = transport->data;
  struct mailhttp_response * response;
  static const char body[] = "response";

  context->calls ++;
  if ((strcmp(request->method, "POST") != 0) ||
      (strcmp(request->url, "https://example.test/") != 0) ||
      (request->body_len != 3))
    return MAILHTTP_ERROR_PROTOCOL;
  response = mailhttp_response_new(201);
  if (response == NULL)
    return MAILHTTP_ERROR_MEMORY;
  if ((mailhttp_response_add_header(response, "Content-Type", "text/plain") !=
          MAILHTTP_NO_ERROR) ||
      (mailhttp_response_set_final_url(response,
          "https://example.test/final") != MAILHTTP_NO_ERROR) ||
      (mailhttp_response_append_body(response, body, sizeof(body) - 1) !=
          MAILHTTP_NO_ERROR)) {
    mailhttp_response_free(response);
    return MAILHTTP_ERROR_MEMORY;
  }
  * result = response;
  return MAILHTTP_NO_ERROR;
}

static const struct mailhttp_transport_driver fake_driver = {
  fake_perform,
  NULL
};

static int test_request_and_transport(void)
{
  struct fake_context context = { 0 };
  struct mailhttp_transport * transport;
  struct mailhttp_request * request;
  struct mailhttp_response * response = NULL;
  static const unsigned char body[] = { 0, 1, 2 };
  int r;
  int ok;

  transport = mailhttp_transport_new(&context, &fake_driver,
      MAILHTTP_BACKEND_CURL);
  request = mailhttp_request_new("POST", "https://example.test/");
  if (!check((transport != NULL) && (request != NULL), "allocation failed"))
    return 0;
  r = mailhttp_request_add_header(request, "X-Test", "one");
  r |= mailhttp_request_add_header(request, "x-test", "two");
  r |= mailhttp_request_set_body(request, body, sizeof(body));
  if (!check(r == MAILHTTP_NO_ERROR, "request construction failed"))
    return 0;
  r = mailhttp_perform(transport, request, &response);
  ok = check(r == MAILHTTP_NO_ERROR, "fake perform failed") &&
      check(context.calls == 1, "fake transport call count mismatch") &&
      check(response->status_code == 201, "response status mismatch") &&
      check(strcmp(mailhttp_response_header_value(response, "content-type"),
          "text/plain") == 0, "case-insensitive header lookup failed") &&
      check(response->body_len == 8, "binary response size mismatch") &&
      check(strcmp(response->final_url, "https://example.test/final") == 0,
          "final URL mismatch");
  mailhttp_response_free(response);
  mailhttp_request_free(request);
  mailhttp_transport_free(transport);
  return ok;
}

static int test_backend_selection(void)
{
  enum mailhttp_backend original = mailhttp_get_backend();
  enum mailhttp_backend unavailable = MAILHTTP_BACKEND_CURL;

  if (!mailhttp_backend_is_available(unavailable))
    return check(mailhttp_set_backend(unavailable) == -1,
        "unavailable backend selection succeeded") &&
        check(mailhttp_get_backend() == original,
            "failed selection changed backend");
  unavailable = MAILHTTP_BACKEND_NSURLSESSION;
  if (!mailhttp_backend_is_available(unavailable))
    return check(mailhttp_set_backend(unavailable) == -1,
        "unavailable backend selection succeeded") &&
        check(mailhttp_get_backend() == original,
            "failed selection changed backend");
  return check(mailhttp_set_backend(original) == 0,
      "available backend selection failed");
}

int main(void)
{
  if (!test_request_and_transport() || !test_backend_selection())
    return 1;
  printf("mailhttp tests passed\n");
  return 0;
}
