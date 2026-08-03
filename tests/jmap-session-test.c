/*
 * libEtPan! -- a mail stuff library
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include <libetpan/mailjmap.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct fake_context {
  int calls;
  char * method;
  char * url;
  char * accept_type;
  char * authorization;
  const char * session_body;
  time_t timeout;
  int status_code;
};

static const char session_json[] =
    "{"
    "\"capabilities\":{"
    "\"urn:ietf:params:jmap:core\":{},"
    "\"urn:ietf:params:jmap:mail\":{}"
    "},"
    "\"accounts\":{"
    "\"acc1\":{"
    "\"name\":\"Personal\","
    "\"isPersonal\":true,"
    "\"isReadOnly\":false,"
    "\"accountCapabilities\":{"
    "\"urn:ietf:params:jmap:mail\":{}"
    "}"
    "}"
    "},"
    "\"primaryAccounts\":{"
    "\"urn:ietf:params:jmap:mail\":\"acc1\""
    "},"
    "\"apiUrl\":\"https://example.com/jmap/api\","
    "\"downloadUrl\":\"https://example.com/download/{accountId}/{blobId}/{name}?accept={type}\","
    "\"uploadUrl\":\"https://example.com/upload/{accountId}\","
    "\"eventSourceUrl\":\"https://example.com/events\","
    "\"sessionState\":\"state1\""
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

static struct mailjmap_session_capability * find_capability_detail(
    clist * list, const char * capability)
{
  clistiter * cur;

  if ((list == NULL) || (capability == NULL))
    return NULL;

  for (cur = clist_begin(list); cur != NULL; cur = clist_next(cur)) {
    struct mailjmap_session_capability * detail;

    detail = clist_content(cur);
    if ((detail != NULL) && str_equal(detail->capability, capability))
      return detail;
  }

  return NULL;
}

static char * dup_string(const char * value)
{
  if (value == NULL)
    return NULL;

  return strdup(value);
}

static char * read_file(const char * path, size_t * result_len)
{
  FILE * file;
  char * data;
  char prefixed_path[512];
  long len;
  size_t read_len;

  file = fopen(path, "rb");
  if (file == NULL) {
    snprintf(prefixed_path, sizeof(prefixed_path), "tests/%s", path);
    file = fopen(prefixed_path, "rb");
    if (file == NULL)
      return NULL;
  }

  if (fseek(file, 0, SEEK_END) < 0)
    goto err;
  len = ftell(file);
  if (len < 0)
    goto err;
  if (fseek(file, 0, SEEK_SET) < 0)
    goto err;

  data = malloc((size_t) len + 1);
  if (data == NULL)
    goto err;

  read_len = fread(data, 1, (size_t) len, file);
  if (read_len != (size_t) len) {
    free(data);
    goto err;
  }
  data[len] = '\0';
  fclose(file);

  if (result_len != NULL)
    * result_len = read_len;
  return data;

 err:
  fclose(file);
  return NULL;
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
  free(context->authorization);
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
  int r;

  context = transport->context;
  context->calls ++;

  r = capture_request(context, request);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  response = mailjmap_http_response_new(context->status_code);
  if (response == NULL)
    return MAILJMAP_ERROR_MEMORY;

  r = mailjmap_http_response_add_header(response, "Content-Type",
      "application/json");
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  if (context->status_code == 200) {
    const char * body;

    body = context->session_body;
    if (body == NULL)
      body = session_json;
    r = set_response_body(response, body);
  }
  else
    r = set_response_body(response, "{\"type\":\"about:blank\"}");
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  if ((request->url != NULL) &&
      (strstr(request->url, "/.well-known/jmap") != NULL)) {
    r = mailjmap_http_response_set_final_url(response,
        "https://example.com/jmap/session");
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }

  * result = response;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_http_response_free(response);
  return r;
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

static int test_get_session_success(void)
{
  mailjmap * client;
  struct fake_context context;
  struct mailjmap_session * parsed;
  struct mailjmap_session_account * account;
  struct mailjmap_session_primary_account * primary_account;
  int r;
  int ok;

  memset(&context, 0, sizeof(context));
  context.status_code = 200;
  client = NULL;
  parsed = NULL;
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
  r = mailjmap_set_timeout(client, 17);
  if (!check(r == MAILJMAP_NO_ERROR, "timeout set failed"))
    goto cleanup;

  r = mailjmap_get_session(client, &parsed);
  if (!check(r == MAILJMAP_NO_ERROR, "get session failed"))
    goto cleanup;

  account = clist_content(clist_begin(parsed->accounts));
  primary_account = clist_content(clist_begin(parsed->primary_accounts));

  ok = check(context.calls == 1, "call count mismatch") &&
      check(str_equal(context.method, "GET"), "method mismatch") &&
      check(str_equal(context.url, "https://example.com/jmap/session"),
          "URL mismatch") &&
      check(str_equal(context.accept_type, "application/json"),
          "accept type mismatch") &&
      check(str_equal(context.authorization, "Bearer token"),
          "auth header mismatch") &&
      check(context.timeout == 17, "timeout mismatch") &&
      check(mailjmap_get_last_http_status(client) == 200,
          "last HTTP status mismatch") &&
      check(str_equal(parsed->api_url, "https://example.com/jmap/api"),
          "apiUrl mismatch") &&
      check(str_equal(parsed->upload_url,
          "https://example.com/upload/{accountId}"), "uploadUrl mismatch") &&
      check(str_equal(parsed->download_url,
          "https://example.com/download/{accountId}/{blobId}/{name}?accept={type}"),
          "downloadUrl mismatch") &&
      check(str_equal(parsed->event_source_url, "https://example.com/events"),
          "eventSourceUrl mismatch") &&
      check(str_equal(parsed->session_state, "state1"),
          "sessionState mismatch") &&
      check(clist_count(parsed->capabilities) == 2,
          "capability count mismatch") &&
      check(clist_count(parsed->accounts) == 1, "account count mismatch") &&
      check(account != NULL && str_equal(account->account_id, "acc1"),
          "account id mismatch") &&
      check(account != NULL && str_equal(account->name, "Personal"),
          "account name mismatch") &&
      check(account != NULL && account->is_personal == 1,
          "account personal flag mismatch") &&
      check(account != NULL && account->is_read_only == 0,
          "account read-only flag mismatch") &&
      check(account != NULL && clist_count(account->capabilities) == 1,
          "account capability count mismatch") &&
      check(clist_count(parsed->primary_accounts) == 1,
          "primary account count mismatch") &&
      check(primary_account != NULL &&
          str_equal(primary_account->capability,
              "urn:ietf:params:jmap:mail"), "primary capability mismatch") &&
      check(primary_account != NULL &&
          str_equal(primary_account->account_id, "acc1"),
          "primary account id mismatch");

 cleanup:
  mailjmap_session_free(parsed);
  mailjmap_free(client);
  fake_context_clear(&context);
  return ok;
}

static int test_minimal_session_fixture(void)
{
  mailjmap * client;
  struct fake_context context;
  struct mailjmap_session * parsed;
  struct mailjmap_session_account * account;
  struct mailjmap_session_primary_account * primary_account;
  char * fixture;
  int r;
  int ok;

  memset(&context, 0, sizeof(context));
  context.status_code = 200;
  client = NULL;
  parsed = NULL;
  fixture = NULL;
  ok = 0;

  fixture = read_file("jmap/data/session/minimal.json", NULL);
  if (!check(fixture != NULL, "minimal session fixture load failed"))
    goto cleanup;
  context.session_body = fixture;

  client = mailjmap_new(0, NULL);
  if (!check(client != NULL, "client allocation failed"))
    goto cleanup;

  r = mailjmap_set_http_transport(client, fake_transport_new(&context));
  if (!check(r == MAILJMAP_NO_ERROR, "transport set failed"))
    goto cleanup;
  r = mailjmap_connect(client, "https://example.com/jmap/session");
  if (!check(r == MAILJMAP_NO_ERROR, "connect failed"))
    goto cleanup;
  r = mailjmap_set_oauth2_token(client, "token");
  if (!check(r == MAILJMAP_NO_ERROR, "token set failed"))
    goto cleanup;

  r = mailjmap_get_session(client, &parsed);
  if (!check(r == MAILJMAP_NO_ERROR, "minimal session parse failed"))
    goto cleanup;

  account = clist_content(clist_begin(parsed->accounts));
  primary_account = clist_content(clist_begin(parsed->primary_accounts));

  ok = check(context.calls == 1, "minimal call count mismatch") &&
      check(str_equal(parsed->api_url, "https://example.com/jmap/api"),
          "minimal apiUrl mismatch") &&
      check(str_equal(parsed->upload_url,
          "https://example.com/upload/{accountId}"),
          "minimal uploadUrl mismatch") &&
      check(str_equal(parsed->download_url,
          "https://example.com/download/{accountId}/{blobId}/{name}?accept={type}"),
          "minimal downloadUrl mismatch") &&
      check(parsed->event_source_url == NULL,
          "minimal eventSourceUrl should be optional") &&
      check(str_equal(parsed->session_state, "state1"),
          "minimal sessionState mismatch") &&
      check(clist_count(parsed->capabilities) == 2,
          "minimal capability count mismatch") &&
      check(clist_count(parsed->accounts) == 1,
          "minimal account count mismatch") &&
      check(account != NULL && str_equal(account->account_id, "acc1"),
          "minimal account id mismatch") &&
      check(account != NULL && str_equal(account->name, "Personal"),
          "minimal account name mismatch") &&
      check(account != NULL && account->is_personal == 1,
          "minimal personal flag mismatch") &&
      check(account != NULL && account->is_read_only == 0,
          "minimal read-only flag mismatch") &&
      check(account != NULL && clist_count(account->capabilities) == 1,
          "minimal account capability count mismatch") &&
      check(clist_count(parsed->primary_accounts) == 1,
          "minimal null primary account was not ignored") &&
      check(primary_account != NULL &&
          str_equal(primary_account->capability,
              "urn:ietf:params:jmap:mail"),
          "minimal primary capability mismatch") &&
      check(primary_account != NULL &&
          str_equal(primary_account->account_id, "acc1"),
          "minimal primary account id mismatch");

 cleanup:
  mailjmap_session_free(parsed);
  mailjmap_free(client);
  fake_context_clear(&context);
  free(fixture);
  return ok;
}

static int test_fastmail_style_session_fixture(void)
{
  mailjmap * client;
  struct fake_context context;
  struct mailjmap_session * parsed;
  struct mailjmap_session_account * account;
  struct mailjmap_session_primary_account * primary_account;
  struct mailjmap_session_capability * core_capability;
  struct mailjmap_session_capability * vendor_capability;
  struct mailjmap_session_capability * account_vendor_capability;
  char * fixture;
  int r;
  int ok;

  memset(&context, 0, sizeof(context));
  context.status_code = 200;
  client = NULL;
  parsed = NULL;
  core_capability = NULL;
  vendor_capability = NULL;
  account_vendor_capability = NULL;
  fixture = NULL;
  ok = 0;

  fixture = read_file("jmap/data/session/fastmail-style.json", NULL);
  if (!check(fixture != NULL, "fastmail-style session fixture load failed"))
    goto cleanup;
  context.session_body = fixture;

  client = mailjmap_new(0, NULL);
  if (!check(client != NULL, "client allocation failed"))
    goto cleanup;

  r = mailjmap_set_http_transport(client, fake_transport_new(&context));
  if (!check(r == MAILJMAP_NO_ERROR, "transport set failed"))
    goto cleanup;
  r = mailjmap_connect(client, "https://api.fastmail.example/jmap/session");
  if (!check(r == MAILJMAP_NO_ERROR, "connect failed"))
    goto cleanup;
  r = mailjmap_set_oauth2_token(client, "token");
  if (!check(r == MAILJMAP_NO_ERROR, "token set failed"))
    goto cleanup;

  r = mailjmap_get_session(client, &parsed);
  if (!check(r == MAILJMAP_NO_ERROR,
      "fastmail-style session parse failed"))
    goto cleanup;

  account = clist_content(clist_begin(parsed->accounts));
  primary_account = clist_content(clist_begin(parsed->primary_accounts));
  core_capability = find_capability_detail(parsed->capability_details,
      "urn:ietf:params:jmap:core");
  vendor_capability = find_capability_detail(parsed->capability_details,
      "https://www.fastmail.com/dev/protocol/jmap");
  if (account != NULL)
    account_vendor_capability = find_capability_detail(
        account->capability_details,
        "https://www.fastmail.com/dev/protocol/jmap");

  ok = check(context.calls == 1, "fastmail-style call count mismatch") &&
      check(str_equal(parsed->api_url,
          "https://api.fastmail.example/jmap/api/"),
          "fastmail-style apiUrl mismatch") &&
      check(str_equal(parsed->upload_url,
          "https://api.fastmail.example/jmap/upload/{accountId}/"),
          "fastmail-style uploadUrl mismatch") &&
      check(str_equal(parsed->download_url,
          "https://api.fastmail.example/jmap/download/{accountId}/{blobId}/{name}?accept={type}"),
          "fastmail-style downloadUrl mismatch") &&
      check(str_equal(parsed->event_source_url,
          "https://api.fastmail.example/jmap/eventsource/"),
          "fastmail-style eventSourceUrl mismatch") &&
      check(str_equal(parsed->session_state, "fastmail-state-1"),
          "fastmail-style sessionState mismatch") &&
      check(clist_count(parsed->capabilities) == 3,
          "fastmail-style capability count mismatch") &&
      check(clist_count(parsed->capability_details) == 3,
          "fastmail-style capability detail count mismatch") &&
      check(core_capability != NULL,
          "fastmail-style core capability detail missing") &&
      check(strstr(core_capability->json,
          "\"maxSizeUpload\":50000000") != NULL,
          "fastmail-style core capability json mismatch") &&
      check(vendor_capability != NULL,
          "fastmail-style vendor capability detail missing") &&
      check(strstr(vendor_capability->json,
          "\"vendorAccountName\":\"Personal\"") != NULL,
          "fastmail-style vendor capability json mismatch") &&
      check(clist_count(parsed->accounts) == 2,
          "fastmail-style account count mismatch") &&
      check(account != NULL && str_equal(account->account_id, "u12345"),
          "fastmail-style first account id mismatch") &&
      check(account != NULL && str_equal(account->name, "Personal"),
          "fastmail-style first account name mismatch") &&
      check(account != NULL && account->is_personal == 1,
          "fastmail-style first account personal flag mismatch") &&
      check(account != NULL && account->is_read_only == 0,
          "fastmail-style first account read-only flag mismatch") &&
      check(account != NULL && clist_count(account->capabilities) == 2,
          "fastmail-style account capability count mismatch") &&
      check(account != NULL && clist_count(account->capability_details) == 2,
          "fastmail-style account capability detail count mismatch") &&
      check(account_vendor_capability != NULL,
          "fastmail-style account vendor capability detail missing") &&
      check(strstr(account_vendor_capability->json,
          "\"mayUseForTests\":true") != NULL,
          "fastmail-style account vendor capability json mismatch") &&
      check(clist_count(parsed->primary_accounts) == 1,
          "fastmail-style null primary account was not ignored") &&
      check(primary_account != NULL &&
          str_equal(primary_account->capability,
              "urn:ietf:params:jmap:mail"),
          "fastmail-style primary capability mismatch") &&
      check(primary_account != NULL &&
          str_equal(primary_account->account_id, "u12345"),
          "fastmail-style primary account id mismatch");

 cleanup:
  mailjmap_session_free(parsed);
  mailjmap_free(client);
  fake_context_clear(&context);
  free(fixture);
  return ok;
}

static int test_get_session_auth_failure(void)
{
  mailjmap * client;
  struct fake_context context;
  struct mailjmap_session * parsed;
  int r;
  int ok;

  memset(&context, 0, sizeof(context));
  context.status_code = 401;
  client = NULL;
  parsed = NULL;
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
  r = mailjmap_set_oauth2_token(client, "token");
  if (!check(r == MAILJMAP_NO_ERROR, "token set failed"))
    goto cleanup;

  r = mailjmap_get_session(client, &parsed);
  ok = check(r == MAILJMAP_ERROR_AUTHENTICATION,
      "auth failure was not mapped") &&
      check(parsed == NULL, "auth failure returned session") &&
      check(mailjmap_get_last_http_status(client) == 401,
          "auth failure HTTP status mismatch") &&
      check(str_equal(mailjmap_get_last_error_message(client),
          "JMAP authentication failed"), "auth failure message mismatch");

 cleanup:
  mailjmap_session_free(parsed);
  mailjmap_free(client);
  fake_context_clear(&context);
  return ok;
}

static int test_discover_success(void)
{
  mailjmap * client;
  struct fake_context context;
  struct mailjmap_session * discovered;
  struct mailjmap_session * fetched;
  int r;
  int ok;

  memset(&context, 0, sizeof(context));
  context.status_code = 200;
  client = NULL;
  discovered = NULL;
  fetched = NULL;
  ok = 0;

  client = mailjmap_new(0, NULL);
  if (!check(client != NULL, "client allocation failed"))
    goto cleanup;

  r = mailjmap_set_http_transport(client, fake_transport_new(&context));
  if (!check(r == MAILJMAP_NO_ERROR, "transport set failed"))
    goto cleanup;
  r = mailjmap_set_timeout(client, 23);
  if (!check(r == MAILJMAP_NO_ERROR, "timeout set failed"))
    goto cleanup;

  r = mailjmap_discover(client, "user@example.com", &discovered);
  if (!check(r == MAILJMAP_NO_ERROR, "discover failed"))
    goto cleanup;

  if (!check(context.calls == 1, "discover call count mismatch"))
    goto cleanup;
  if (!check(str_equal(context.method, "GET"), "discover method mismatch"))
    goto cleanup;
  if (!check(str_equal(context.url, "https://example.com/.well-known/jmap"),
      "discover URL mismatch"))
    goto cleanup;
  if (!check(str_equal(context.accept_type, "application/json"),
      "discover accept type mismatch"))
    goto cleanup;
  if (!check(context.authorization == NULL,
      "discover should not require auth header"))
    goto cleanup;
  if (!check(context.timeout == 23, "discover timeout mismatch"))
    goto cleanup;

  r = mailjmap_set_oauth2_token(client, "token");
  if (!check(r == MAILJMAP_NO_ERROR, "token set failed"))
    goto cleanup;
  r = mailjmap_get_session(client, &fetched);
  if (!check(r == MAILJMAP_NO_ERROR, "get discovered session failed"))
    goto cleanup;

  ok = check(context.calls == 2, "post-discover call count mismatch") &&
      check(str_equal(context.url, "https://example.com/jmap/session"),
          "discovered session URL was not remembered") &&
      check(str_equal(context.authorization, "Bearer token"),
          "post-discover auth header mismatch") &&
      check(str_equal(discovered->api_url, "https://example.com/jmap/api"),
          "discovered apiUrl mismatch") &&
      check(str_equal(discovered->session_state, "state1"),
          "discovered sessionState mismatch") &&
      check(str_equal(fetched->session_state, "state1"),
          "fetched sessionState mismatch");

 cleanup:
  mailjmap_session_free(fetched);
  mailjmap_session_free(discovered);
  mailjmap_free(client);
  fake_context_clear(&context);
  return ok;
}

static int test_discover_not_found(void)
{
  mailjmap * client;
  struct fake_context context;
  struct mailjmap_session * discovered;
  int r;
  int ok;

  memset(&context, 0, sizeof(context));
  context.status_code = 404;
  client = NULL;
  discovered = NULL;
  ok = 0;

  client = mailjmap_new(0, NULL);
  if (!check(client != NULL, "client allocation failed"))
    goto cleanup;

  r = mailjmap_set_http_transport(client, fake_transport_new(&context));
  if (!check(r == MAILJMAP_NO_ERROR, "transport set failed"))
    goto cleanup;

  r = mailjmap_discover(client, "example.com", &discovered);
  ok = check(r == MAILJMAP_ERROR_DISCOVERY,
      "discovery 404 was not mapped") &&
      check(discovered == NULL, "discovery failure returned session") &&
      check(str_equal(context.url, "https://example.com/.well-known/jmap"),
          "discovery 404 URL mismatch") &&
      check(mailjmap_get_last_http_status(client) == 404,
          "discovery 404 status mismatch") &&
      check(str_equal(mailjmap_get_last_error_message(client),
          "JMAP discovery endpoint not found"),
          "discovery 404 message mismatch");

 cleanup:
  mailjmap_session_free(discovered);
  mailjmap_free(client);
  fake_context_clear(&context);
  return ok;
}

int main(void)
{
  int ok;

  ok = 1;
  ok = test_discover_success() && ok;
  ok = test_discover_not_found() && ok;
  ok = test_get_session_success() && ok;
  ok = test_minimal_session_fixture() && ok;
  ok = test_fastmail_style_session_fixture() && ok;
  ok = test_get_session_auth_failure() && ok;

  return ok ? 0 : 1;
}
