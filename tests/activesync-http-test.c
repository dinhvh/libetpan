/*
 * libEtPan! -- a mail stuff library
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include <libetpan/mailactivesync.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct fake_context {
  struct mailactivesync_http_request * last_request;
  int status_code;
};

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
    if (strcmp(header->name, name) == 0)
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

  r = mailactivesync_http_response_add_header(response,
      "MS-ASProtocolVersions", "14.1, 16.0, 16.1");
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  r = mailactivesync_http_response_add_header(response,
      "MS-ASProtocolCommands", "Sync, FolderSync, ItemOperations");
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;

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
  transport->context = context;
  transport->perform = fake_perform;
  transport->free = fake_free;

  * context_result = context;
  return transport;
}

static int test_options_success(void)
{
  mailactivesync * session;
  struct fake_context * context;
  struct mailactivesync_http_transport * transport;
  struct mailactivesync_options * options;
  const char * authorization;
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

int main(void)
{
  if (!test_options_success())
    return 1;
  if (!test_options_unauthorized())
    return 1;

  return 0;
}
