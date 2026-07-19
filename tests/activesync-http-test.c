/*
 * libEtPan! -- a mail stuff library
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include <libetpan/mailactivesync.h>

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
  context->protocol_versions = "14.1, 16.0, 16.1";
  context->commands = "Sync, FolderSync, ItemOperations";
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

  return 0;
}
