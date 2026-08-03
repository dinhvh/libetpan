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
  char * mailbox_get_body;
  char * mailbox_changes_body;
  char * mailbox_query_body;
  char * mailbox_set_body;
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

static const char mailbox_method_limit_error_json[] =
    "{"
    "\"methodResponses\":["
    "[\"error\",{\"type\":\"requestTooLarge\","
    "\"description\":\"mailbox get too large\"},\"c1\"]"
    "],"
    "\"sessionState\":\"state-error\""
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

static char * read_fixture_file(const char * directory, const char * name,
    size_t * result_len)
{
  char path[512];
  char * data;
  FILE * f;
  long len;
  size_t read_len;
  const char * prefixes[] = {
    "tests/jmap/data/",
    "jmap/data/"
  };
  size_t i;

  for (i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); i ++) {
    snprintf(path, sizeof(path), "%s%s/%s", prefixes[i], directory, name);
    f = fopen(path, "rb");
    if (f != NULL)
      break;
  }
  if (f == NULL)
    return NULL;

  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return NULL;
  }
  len = ftell(f);
  if (len < 0) {
    fclose(f);
    return NULL;
  }
  if (fseek(f, 0, SEEK_SET) != 0) {
    fclose(f);
    return NULL;
  }

  data = malloc((size_t) len + 1);
  if (data == NULL) {
    fclose(f);
    return NULL;
  }

  read_len = fread(data, 1, (size_t) len, f);
  fclose(f);
  if (read_len != (size_t) len) {
    free(data);
    return NULL;
  }

  data[read_len] = '\0';
  if (result_len != NULL)
    * result_len = read_len;
  return data;
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
  free(context->mailbox_get_body);
  free(context->mailbox_changes_body);
  free(context->mailbox_query_body);
  free(context->mailbox_set_body);
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
  else if ((context->body != NULL) &&
      (strstr(context->body, "\"Mailbox/set\"") != NULL))
    body = context->mailbox_set_body;
  else if (context->calls == 2)
    body = context->mailbox_get_body;
  else if (context->calls == 3)
    body = context->mailbox_changes_body;
  else
    body = context->mailbox_query_body;
  if (body == NULL) {
    mailjmap_http_response_free(response);
    return MAILJMAP_ERROR_BAD_STATE;
  }

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

static void mailbox_set_item_list_free(clist * list)
{
  clistiter * cur;

  if (list == NULL)
    return;

  for (cur = clist_begin(list); cur != NULL; cur = clist_next(cur))
    mailjmap_mailbox_set_item_free(clist_content(cur));
  clist_free(list);
}

static int test_mailbox_get(void)
{
  mailjmap * client;
  struct fake_context context;
  struct mailjmap_session * session_object;
  struct mailjmap_mailbox_get_result * result;
  struct mailjmap_mailbox * mailbox;
  clist * ids;
  chashdatum map_key;
  chashdatum map_value;
  int right_value;
  char * not_found;
  int r;
  int ok;

  memset(&context, 0, sizeof(context));
  client = NULL;
  session_object = NULL;
  result = NULL;
  ids = NULL;
  ok = 0;

  context.mailbox_get_body = read_fixture_file("mailbox", "get.json", NULL);
  if (!check(context.mailbox_get_body != NULL,
      "mailbox get fixture load failed"))
    goto cleanup;

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
  r = string_list_append(ids, "inbox");
  if (!check(r == MAILJMAP_NO_ERROR, "id append failed"))
    goto cleanup;

  r = mailjmap_mailbox_get(client, "acc1", ids, NULL, &result);
  if (!check(r == MAILJMAP_NO_ERROR, "Mailbox/get failed"))
    goto cleanup;

  mailbox = clist_content(clist_begin(result->list));
  not_found = clist_content(clist_begin(result->not_found));
  map_key.data = "mayReadItems";
  map_key.len = (unsigned int) strlen((const char *) map_key.data) + 1;

  ok = check(context.calls == 2, "call count mismatch") &&
      check(str_equal(context.method, "POST"), "method mismatch") &&
      check(str_equal(context.url, "https://example.com/jmap/api"),
          "API URL mismatch") &&
      check(str_equal(context.authorization, "Bearer token"),
          "authorization mismatch") &&
      check(strstr(context.body, "\"Mailbox/get\"") != NULL,
          "request body missing Mailbox/get") &&
      check(strstr(context.body, "\"urn:ietf:params:jmap:mail\"") != NULL,
          "request body missing mail capability") &&
      check(strstr(context.body, "\"ids\":[\"inbox\"]") != NULL,
          "request body missing ids") &&
      check(str_equal(result->account_id, "acc1"), "accountId mismatch") &&
      check(str_equal(result->state, "mbox-state"), "state mismatch") &&
      check(clist_count(result->list) == 1, "mailbox count mismatch") &&
      check(str_equal(mailbox->id, "inbox"), "mailbox id mismatch") &&
      check(str_equal(mailbox->name, "Inbox"), "mailbox name mismatch") &&
      check(mailbox->parent_id == NULL, "parentId should be null") &&
      check(str_equal(mailbox->role, "inbox"), "role mismatch") &&
      check(mailbox->sort_order == 10, "sortOrder mismatch") &&
      check(mailbox->is_subscribed == 1, "isSubscribed mismatch") &&
      check(mailbox->total_emails == 42, "totalEmails mismatch") &&
      check(mailbox->unread_emails == 5, "unreadEmails mismatch") &&
      check(mailbox->total_threads == 30, "totalThreads mismatch") &&
      check(mailbox->unread_threads == 4, "unreadThreads mismatch") &&
      check(chash_get(mailbox->my_rights, &map_key, &map_value) == 0,
          "myRights missing mayReadItems") &&
      check(str_equal(not_found, "missing"), "notFound mismatch");
  if (ok) {
    memcpy(&right_value, map_value.data, sizeof(right_value));
    ok = check(right_value == 1, "myRights mayReadItems mismatch");
  }

 cleanup:
  string_list_free(ids);
  mailjmap_mailbox_get_result_free(result);
  mailjmap_session_free(session_object);
  mailjmap_free(client);
  fake_context_clear(&context);
  return ok;
}

static int test_mailbox_set(void)
{
  mailjmap * client;
  struct fake_context context;
  struct mailjmap_session * session_object;
  struct mailjmap_mailbox_set_item * create_item;
  struct mailjmap_mailbox_set_item * update_item;
  struct mailjmap_set_result * set_result;
  struct mailjmap_set_created * created;
  struct mailjmap_set_error * not_created;
  struct mailjmap_set_error * not_updated;
  struct mailjmap_set_error * not_destroyed;
  clist * create;
  clist * update;
  clist * destroy;
  char * updated;
  char * destroyed;
  int r;
  int ok;

  memset(&context, 0, sizeof(context));
  client = NULL;
  session_object = NULL;
  create_item = NULL;
  update_item = NULL;
  set_result = NULL;
  create = NULL;
  update = NULL;
  destroy = NULL;
  ok = 0;

  context.mailbox_set_body = read_fixture_file("mailbox", "set.json", NULL);
  if (!check(context.mailbox_set_body != NULL,
      "mailbox set fixture load failed"))
    goto cleanup;

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

  create = clist_new();
  update = clist_new();
  destroy = clist_new();
  if (!check((create != NULL) && (update != NULL) && (destroy != NULL),
      "set list allocation failed"))
    goto cleanup;

  create_item = mailjmap_mailbox_set_item_new("newbox");
  update_item = mailjmap_mailbox_set_item_new("inbox");
  if (!check((create_item != NULL) && (update_item != NULL),
      "set item allocation failed"))
    goto cleanup;

  r = mailjmap_mailbox_set_item_set_name(create_item, "Archive");
  if (!check(r == MAILJMAP_NO_ERROR, "create name set failed"))
    goto cleanup;
  mailjmap_mailbox_set_item_set_parent_id_null(create_item);
  mailjmap_mailbox_set_item_set_role_null(create_item);
  mailjmap_mailbox_set_item_set_sort_order(create_item, 20);
  mailjmap_mailbox_set_item_set_is_subscribed(create_item, 1);

  r = mailjmap_mailbox_set_item_set_name(update_item, "Inbox Renamed");
  if (!check(r == MAILJMAP_NO_ERROR, "update name set failed"))
    goto cleanup;
  r = mailjmap_mailbox_set_item_set_parent_id(update_item, "parent");
  if (!check(r == MAILJMAP_NO_ERROR, "update parent set failed"))
    goto cleanup;
  mailjmap_mailbox_set_item_set_is_subscribed(update_item, 0);

  if (!check(clist_append(create, create_item) >= 0,
      "create append failed"))
    goto cleanup;
  create_item = NULL;
  if (!check(clist_append(update, update_item) >= 0,
      "update append failed"))
    goto cleanup;
  update_item = NULL;
  r = string_list_append(destroy, "trash");
  if (!check(r == MAILJMAP_NO_ERROR, "destroy append failed"))
    goto cleanup;

  r = mailjmap_mailbox_set(client, "acc1", "old", create, update,
      destroy, &set_result);
  if (!check(r == MAILJMAP_NO_ERROR, "Mailbox/set failed"))
    goto cleanup;

  if (!check(strstr(context.body, "\"Mailbox/set\"") != NULL,
      "request body missing Mailbox/set"))
    goto cleanup;
  if (!check(strstr(context.body, "\"ifInState\":\"old\"") != NULL,
      "request body missing ifInState"))
    goto cleanup;
  if (!check(strstr(context.body, "\"create\":{\"newbox\"") != NULL,
      "request body missing create map"))
    goto cleanup;
  if (!check(strstr(context.body, "\"name\":\"Archive\"") != NULL,
      "request body missing create name"))
    goto cleanup;
  if (!check(strstr(context.body, "\"parentId\":null") != NULL,
      "request body missing null parentId"))
    goto cleanup;
  if (!check(strstr(context.body, "\"role\":null") != NULL,
      "request body missing null role"))
    goto cleanup;
  if (!check(strstr(context.body, "\"sortOrder\":20") != NULL,
      "request body missing sortOrder"))
    goto cleanup;
  if (!check(strstr(context.body, "\"isSubscribed\":false") != NULL,
      "request body missing false isSubscribed"))
    goto cleanup;
  if (!check(strstr(context.body, "\"destroy\":[\"trash\"]") != NULL,
      "request body missing destroy ids"))
    goto cleanup;

  created = clist_content(clist_begin(set_result->created));
  updated = clist_content(clist_begin(set_result->updated));
  destroyed = clist_content(clist_begin(set_result->destroyed));
  not_created = clist_content(clist_begin(set_result->not_created));
  not_updated = clist_content(clist_begin(set_result->not_updated));
  not_destroyed = clist_content(clist_begin(set_result->not_destroyed));

  ok = check(context.calls == 2, "call count mismatch") &&
      check(str_equal(set_result->account_id, "acc1"),
          "set accountId mismatch") &&
      check(str_equal(set_result->old_state, "old"),
          "set oldState mismatch") &&
      check(str_equal(set_result->new_state, "set-new"),
          "set newState mismatch") &&
      check(str_equal(created->creation_id, "newbox"),
          "set created creation id mismatch") &&
      check(str_equal(created->id, "archive"),
          "set created id mismatch") &&
      check(str_equal(updated, "inbox"), "set updated mismatch") &&
      check(str_equal(destroyed, "trash"), "set destroyed mismatch") &&
      check(str_equal(not_created->id, "badCreate"),
          "notCreated id mismatch") &&
      check(str_equal(not_created->type, "invalidProperties"),
          "notCreated type mismatch") &&
      check(str_equal(not_created->description, "name required"),
          "notCreated description mismatch") &&
      check(str_equal(not_updated->id, "badUpdate"),
          "notUpdated id mismatch") &&
      check(str_equal(not_updated->type, "notFound"),
          "notUpdated type mismatch") &&
      check(not_updated->description == NULL,
          "notUpdated description should be null") &&
      check(str_equal(not_destroyed->id, "badDestroy"),
          "notDestroyed id mismatch") &&
      check(str_equal(not_destroyed->type, "forbidden"),
          "notDestroyed type mismatch") &&
      check(str_equal(not_destroyed->description, "protected"),
          "notDestroyed description mismatch");

 cleanup:
  mailjmap_mailbox_set_item_free(update_item);
  mailjmap_mailbox_set_item_free(create_item);
  string_list_free(destroy);
  mailbox_set_item_list_free(update);
  mailbox_set_item_list_free(create);
  mailjmap_set_result_free(set_result);
  mailjmap_session_free(session_object);
  mailjmap_free(client);
  fake_context_clear(&context);
  return ok;
}

static int test_mailbox_get_maps_limit_method_error(void)
{
  mailjmap * client;
  struct fake_context context;
  struct mailjmap_session * session_object;
  struct mailjmap_mailbox_get_result * result;
  int r;
  int ok;

  memset(&context, 0, sizeof(context));
  client = NULL;
  session_object = NULL;
  result = NULL;
  ok = 0;

  context.mailbox_get_body = dup_string(mailbox_method_limit_error_json);
  if (!check(context.mailbox_get_body != NULL,
      "mailbox method error allocation failed"))
    goto cleanup;

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

  r = mailjmap_mailbox_get(client, "acc1", NULL, NULL, &result);
  ok = check(r == MAILJMAP_ERROR_LIMIT,
          "Mailbox/get method error code mismatch") &&
      check(result == NULL, "Mailbox/get method error returned result") &&
      check(str_equal(mailjmap_get_last_method_name(client), "Mailbox/get"),
          "last method name mismatch") &&
      check(str_equal(mailjmap_get_last_method_call_id(client), "c1"),
          "last method call id mismatch") &&
      check(str_equal(mailjmap_get_last_method_error_type(client),
          "requestTooLarge"), "last method error type mismatch") &&
      check(str_equal(mailjmap_get_last_error_message(client),
          "mailbox get too large"), "last error message mismatch");

 cleanup:
  mailjmap_mailbox_get_result_free(result);
  mailjmap_session_free(session_object);
  mailjmap_free(client);
  fake_context_clear(&context);
  return ok;
}

static int test_mailbox_changes_and_query(void)
{
  mailjmap * client;
  struct fake_context context;
  struct mailjmap_session * session_object;
  struct mailjmap_mailbox_get_result * get_result;
  struct mailjmap_changes_result * changes_result;
  struct mailjmap_query_result * query_result;
  char * created;
  char * updated;
  char * destroyed;
  char * first_id;
  char * second_id;
  int r;
  int ok;

  memset(&context, 0, sizeof(context));
  client = NULL;
  session_object = NULL;
  get_result = NULL;
  changes_result = NULL;
  query_result = NULL;
  ok = 0;

  context.mailbox_get_body = read_fixture_file("mailbox", "get.json", NULL);
  if (!check(context.mailbox_get_body != NULL,
      "mailbox get fixture load failed"))
    goto cleanup;
  context.mailbox_changes_body = read_fixture_file("mailbox", "changes.json",
      NULL);
  if (!check(context.mailbox_changes_body != NULL,
      "mailbox changes fixture load failed"))
    goto cleanup;
  context.mailbox_query_body = read_fixture_file("mailbox", "query.json",
      NULL);
  if (!check(context.mailbox_query_body != NULL,
      "mailbox query fixture load failed"))
    goto cleanup;

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

  r = mailjmap_mailbox_get(client, "acc1", NULL, NULL, &get_result);
  if (!check(r == MAILJMAP_NO_ERROR, "Mailbox/get setup call failed"))
    goto cleanup;

  r = mailjmap_mailbox_changes(client, "acc1", "old", &changes_result);
  if (!check(r == MAILJMAP_NO_ERROR, "Mailbox/changes failed"))
    goto cleanup;

  created = clist_content(clist_begin(changes_result->created));
  updated = clist_content(clist_begin(changes_result->updated));
  destroyed = clist_content(clist_begin(changes_result->destroyed));

  if (!check(strstr(context.body, "\"Mailbox/changes\"") != NULL,
      "request body missing Mailbox/changes"))
    goto cleanup;
  if (!check(strstr(context.body, "\"sinceState\":\"old\"") != NULL,
      "request body missing sinceState"))
    goto cleanup;

  r = mailjmap_mailbox_query(client, "acc1", 0, 10, &query_result);
  if (!check(r == MAILJMAP_NO_ERROR, "Mailbox/query failed"))
    goto cleanup;

  first_id = clist_content(clist_begin(query_result->ids));
  second_id = clist_content(clist_next(clist_begin(query_result->ids)));

  ok = check(context.calls == 4, "call count mismatch") &&
      check(strstr(context.body, "\"Mailbox/query\"") != NULL,
          "request body missing Mailbox/query") &&
      check(strstr(context.body, "\"position\":0") != NULL,
          "request body missing position") &&
      check(strstr(context.body, "\"limit\":10") != NULL,
          "request body missing limit") &&
      check(str_equal(changes_result->account_id, "acc1"),
          "changes accountId mismatch") &&
      check(str_equal(changes_result->old_state, "old"),
          "oldState mismatch") &&
      check(str_equal(changes_result->new_state, "new"),
          "newState mismatch") &&
      check(changes_result->has_more_changes == 1,
          "hasMoreChanges mismatch") &&
      check(str_equal(created, "archive"), "created mismatch") &&
      check(str_equal(updated, "inbox"), "updated mismatch") &&
      check(str_equal(destroyed, "trash"), "destroyed mismatch") &&
      check(str_equal(query_result->account_id, "acc1"),
          "query accountId mismatch") &&
      check(str_equal(query_result->query_state, "query-state"),
          "queryState mismatch") &&
      check(query_result->can_calculate_changes == 1,
          "canCalculateChanges mismatch") &&
      check(query_result->position == 0, "query position mismatch") &&
      check(query_result->has_total == 1, "query total presence mismatch") &&
      check(query_result->total == 2, "query total mismatch") &&
      check(str_equal(first_id, "inbox"), "first query id mismatch") &&
      check(str_equal(second_id, "archive"), "second query id mismatch");

 cleanup:
  mailjmap_query_result_free(query_result);
  mailjmap_changes_result_free(changes_result);
  mailjmap_mailbox_get_result_free(get_result);
  mailjmap_session_free(session_object);
  mailjmap_free(client);
  fake_context_clear(&context);
  return ok;
}

int main(void)
{
  if (!test_mailbox_get())
    return 1;
  if (!test_mailbox_set())
    return 1;
  if (!test_mailbox_get_maps_limit_method_error())
    return 1;
  if (!test_mailbox_changes_and_query())
    return 1;

  return 0;
}
