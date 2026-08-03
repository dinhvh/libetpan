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
  char * authorization;
  char * body;
};

static const char session_json[] =
    "{"
    "\"capabilities\":{"
    "\"urn:ietf:params:jmap:core\":{},"
    "\"urn:ietf:params:jmap:mail\":{}"
    "},"
    "\"accounts\":{},"
    "\"primaryAccounts\":{},"
    "\"apiUrl\":\"https://example.com/jmap/api\","
    "\"downloadUrl\":\"https://example.com/download/{accountId}/{blobId}/{name}\","
    "\"uploadUrl\":\"https://example.com/upload/{accountId}\","
    "\"sessionState\":\"state1\""
    "}";

static const char thread_get_json[] =
    "{"
    "\"methodResponses\":["
    "[\"Thread/get\",{"
    "\"accountId\":\"acc1\","
    "\"state\":\"thread-state\","
    "\"list\":[{"
    "\"id\":\"t1\","
    "\"emailIds\":[\"e1\",\"e2\"]"
    "}],"
    "\"notFound\":[\"missing-thread\"]"
    "},\"c1\"]"
    "],"
    "\"sessionState\":\"state2\""
    "}";

static const char thread_changes_json[] =
    "{"
    "\"methodResponses\":["
    "[\"Thread/changes\",{"
    "\"accountId\":\"acc1\","
    "\"oldState\":\"thread-old\","
    "\"newState\":\"thread-new\","
    "\"hasMoreChanges\":false,"
    "\"created\":[\"t2\"],"
    "\"updated\":[\"t1\"],"
    "\"destroyed\":[\"t0\"]"
    "},\"c1\"]"
    "],"
    "\"sessionState\":\"state3\""
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

  response = mailjmap_http_response_new(200);
  if (response == NULL)
    return MAILJMAP_ERROR_MEMORY;

  if (context->calls == 1)
    body = session_json;
  else if (context->calls == 2)
    body = thread_get_json;
  else
    body = thread_changes_json;

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

static void string_list_free(clist * list)
{
  clistiter * cur;

  if (list == NULL)
    return;

  for (cur = clist_begin(list); cur != NULL; cur = clist_next(cur))
    free(clist_content(cur));
  clist_free(list);
}

static int string_list_append(clist * list, const char * value)
{
  char * copy;

  copy = dup_string(value);
  if (copy == NULL)
    return MAILJMAP_ERROR_MEMORY;
  if (clist_append(list, copy) < 0) {
    free(copy);
    return MAILJMAP_ERROR_MEMORY;
  }

  return MAILJMAP_NO_ERROR;
}

static int test_thread_get_and_changes(void)
{
  mailjmap * client;
  struct fake_context context;
  struct mailjmap_session * session_object;
  struct mailjmap_thread_get_result * get_result;
  struct mailjmap_changes_result * changes_result;
  struct mailjmap_thread * thread;
  clist * ids;
  char * email_id_1;
  char * email_id_2;
  char * not_found;
  char * created;
  char * updated;
  char * destroyed;
  int r;
  int ok;

  memset(&context, 0, sizeof(context));
  client = NULL;
  session_object = NULL;
  get_result = NULL;
  changes_result = NULL;
  ids = NULL;
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

  ids = clist_new();
  if (!check(ids != NULL, "ids allocation failed"))
    goto cleanup;
  r = string_list_append(ids, "t1");
  if (!check(r == MAILJMAP_NO_ERROR, "id append failed"))
    goto cleanup;

  r = mailjmap_thread_get(client, "acc1", ids, &get_result);
  if (!check(r == MAILJMAP_NO_ERROR, "Thread/get failed"))
    goto cleanup;

  thread = clist_content(clist_begin(get_result->list));
  email_id_1 = clist_content(clist_begin(thread->email_ids));
  email_id_2 = clist_content(clist_next(clist_begin(thread->email_ids)));
  not_found = clist_content(clist_begin(get_result->not_found));

  if (!check(strstr(context.body, "\"Thread/get\"") != NULL,
      "request body missing Thread/get"))
    goto cleanup;
  if (!check(strstr(context.body, "\"ids\":[\"t1\"]") != NULL,
      "request body missing ids"))
    goto cleanup;

  r = mailjmap_thread_changes(client, "acc1", "thread-old",
      &changes_result);
  if (!check(r == MAILJMAP_NO_ERROR, "Thread/changes failed"))
    goto cleanup;

  created = clist_content(clist_begin(changes_result->created));
  updated = clist_content(clist_begin(changes_result->updated));
  destroyed = clist_content(clist_begin(changes_result->destroyed));

  ok = check(context.calls == 3, "call count mismatch") &&
      check(strstr(context.body, "\"Thread/changes\"") != NULL,
          "request body missing Thread/changes") &&
      check(strstr(context.body, "\"sinceState\":\"thread-old\"") != NULL,
          "request body missing sinceState") &&
      check(str_equal(get_result->account_id, "acc1"),
          "get accountId mismatch") &&
      check(str_equal(get_result->state, "thread-state"),
          "thread state mismatch") &&
      check(clist_count(get_result->list) == 1, "thread count mismatch") &&
      check(str_equal(thread->id, "t1"), "thread id mismatch") &&
      check(str_equal(email_id_1, "e1"), "first email id mismatch") &&
      check(str_equal(email_id_2, "e2"), "second email id mismatch") &&
      check(str_equal(not_found, "missing-thread"), "notFound mismatch") &&
      check(str_equal(changes_result->account_id, "acc1"),
          "changes accountId mismatch") &&
      check(str_equal(changes_result->old_state, "thread-old"),
          "oldState mismatch") &&
      check(str_equal(changes_result->new_state, "thread-new"),
          "newState mismatch") &&
      check(changes_result->has_more_changes == 0,
          "hasMoreChanges mismatch") &&
      check(str_equal(created, "t2"), "created mismatch") &&
      check(str_equal(updated, "t1"), "updated mismatch") &&
      check(str_equal(destroyed, "t0"), "destroyed mismatch");

 cleanup:
  mailjmap_changes_result_free(changes_result);
  mailjmap_thread_get_result_free(get_result);
  string_list_free(ids);
  mailjmap_session_free(session_object);
  mailjmap_free(client);
  fake_context_clear(&context);
  return ok;
}

int main(void)
{
  return test_thread_get_and_changes() ? 0 : 1;
}
