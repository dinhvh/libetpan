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
  const char * email_get_body;
  const char * email_query_changes_body;
};

static const char session_json[] =
    "{"
    "\"capabilities\":{"
    "\"urn:ietf:params:jmap:core\":{},"
    "\"urn:ietf:params:jmap:mail\":{},"
    "\"urn:ietf:params:jmap:submission\":{}"
    "},"
    "\"accounts\":{},"
    "\"primaryAccounts\":{},"
    "\"apiUrl\":\"https://example.com/jmap/api\","
    "\"downloadUrl\":\"https://example.com/download/{accountId}/{blobId}/{name}\","
    "\"uploadUrl\":\"https://example.com/upload/{accountId}\","
    "\"sessionState\":\"state1\""
    "}";

static const char email_query_json[] =
    "{"
    "\"methodResponses\":["
    "[\"Email/query\",{"
    "\"accountId\":\"acc1\","
    "\"queryState\":\"email-query-state\","
    "\"canCalculateChanges\":true,"
    "\"position\":0,"
    "\"ids\":[\"e1\",\"e2\"],"
    "\"total\":2"
    "},\"c1\"]"
    "],"
    "\"sessionState\":\"state2\""
    "}";

static const char email_get_json[] =
    "{"
    "\"methodResponses\":["
    "[\"Email/get\",{"
    "\"accountId\":\"acc1\","
    "\"state\":\"email-state\","
    "\"list\":[{"
    "\"id\":\"e1\","
    "\"blobId\":\"blob1\","
    "\"threadId\":\"t1\","
    "\"mailboxIds\":{\"mbox1\":true},"
    "\"keywords\":{\"$seen\":true,\"$flagged\":false},"
    "\"size\":1234,"
    "\"receivedAt\":\"2026-08-02T10:00:00Z\","
    "\"messageId\":[\"msg@example.com\"],"
    "\"inReplyTo\":[\"parent@example.com\"],"
    "\"subject\":\"Hello\","
    "\"sentAt\":\"2026-08-02T09:59:00Z\","
    "\"preview\":\"Short preview\","
    "\"headers\":[{\"name\":\"X-Test\",\"value\":\"yes\"}],"
    "\"bodyStructure\":{"
    "\"partId\":\"p-root\","
    "\"type\":\"multipart/alternative\","
    "\"subParts\":[{\"partId\":\"p-text\",\"blobId\":\"b-text\","
    "\"type\":\"text/plain\",\"charset\":\"utf-8\",\"size\":12}]"
    "},"
    "\"textBody\":[{\"partId\":\"p-text\",\"blobId\":\"b-text\","
    "\"type\":\"text/plain\",\"charset\":\"utf-8\",\"size\":12}],"
    "\"htmlBody\":[],"
    "\"attachments\":[{\"partId\":\"p-att\",\"blobId\":\"b-att\","
    "\"type\":\"text/plain\",\"name\":\"note.txt\","
    "\"disposition\":\"attachment\",\"size\":5}],"
    "\"bodyValues\":{\"p-text\":{\"value\":\"body text\","
    "\"isTruncated\":false}}"
    "}],"
    "\"notFound\":[\"missing\"]"
    "},\"c1\"]"
    "],"
    "\"sessionState\":\"state-get\""
    "}";

static const char email_query_changes_json[] =
    "{"
    "\"methodResponses\":["
    "[\"Email/queryChanges\",{"
    "\"accountId\":\"acc1\","
    "\"oldQueryState\":\"email-query-state\","
    "\"newQueryState\":\"email-query-state-2\","
    "\"total\":3,"
    "\"removed\":[\"e0\"],"
    "\"added\":[{\"id\":\"e3\",\"index\":2}]"
    "},\"c1\"]"
    "],"
    "\"sessionState\":\"state-query-changes\""
    "}";

static const char email_query_changes_fastmail_nulls_json[] =
    "{"
    "\"methodResponses\":["
    "[\"Email/queryChanges\",{"
    "\"accountId\":\"acc1\","
    "\"oldQueryState\":\"email-query-state\","
    "\"newQueryState\":\"email-query-state-2\","
    "\"total\":1,"
    "\"removed\":null,"
    "\"added\":[{\"id\":\"e3\",\"index\":0}]"
    "},\"c1\"]"
    "],"
    "\"sessionState\":\"state-query-changes\""
    "}";

static const char email_changes_json[] =
    "{"
    "\"methodResponses\":["
    "[\"Email/changes\",{"
    "\"accountId\":\"acc1\","
    "\"oldState\":\"email-old\","
    "\"newState\":\"email-new\","
    "\"hasMoreChanges\":false,"
    "\"created\":[\"e3\"],"
    "\"updated\":[\"e1\"],"
    "\"destroyed\":[\"e0\"]"
    "},\"c1\"]"
    "],"
    "\"sessionState\":\"state3\""
    "}";

static const char email_import_json[] =
    "{"
    "\"methodResponses\":["
    "[\"Email/import\",{"
    "\"accountId\":\"acc1\","
    "\"oldState\":\"email-old\","
    "\"newState\":\"email-import-new\","
    "\"created\":{"
    "\"import1\":{"
    "\"id\":\"e-imported\","
    "\"blobId\":\"blob1\","
    "\"threadId\":\"t-imported\","
    "\"mailboxIds\":{\"mbox1\":true},"
    "\"keywords\":{\"$seen\":true},"
    "\"size\":321"
    "}"
    "},"
    "\"notCreated\":{"
    "\"bad1\":{\"type\":\"invalidEmail\","
    "\"description\":\"bad input\"}"
    "}"
    "},\"c1\"]"
    "],"
    "\"sessionState\":\"state-import\""
    "}";

static const char email_parse_json[] =
    "{"
    "\"methodResponses\":["
    "[\"Email/parse\",{"
    "\"accountId\":\"acc1\","
    "\"parsed\":{"
    "\"blob-parse\":{"
    "\"blobId\":\"blob-parse\","
    "\"threadId\":\"t-parsed\","
    "\"mailboxIds\":{},"
    "\"keywords\":{},"
    "\"subject\":\"Parsed subject\","
    "\"preview\":\"Parsed preview\","
    "\"textBody\":[{\"partId\":\"parsed-text\","
    "\"type\":\"text/plain\",\"size\":11}],"
    "\"bodyValues\":{\"parsed-text\":{\"value\":\"parsed body\","
    "\"isTruncated\":false}}"
    "},"
    "\"bad-blob\":null"
    "},"
    "\"notParsable\":[\"bad-blob-2\"]"
    "},\"c1\"]"
    "],"
    "\"sessionState\":\"state-parse\""
    "}";

static const char search_snippet_json[] =
    "{"
    "\"methodResponses\":["
    "[\"SearchSnippet/get\",{"
    "\"accountId\":\"acc1\","
    "\"list\":[{"
    "\"emailId\":\"e1\","
    "\"subject\":\"<mark>Hello</mark>\","
    "\"preview\":\"preview <mark>hit</mark>\""
    "}],"
    "\"notFound\":[\"missing-snippet\"]"
    "},\"c1\"]"
    "],"
    "\"sessionState\":\"state-snippet\""
    "}";

static const char email_set_json[] =
    "{"
    "\"methodResponses\":["
    "[\"Email/set\",{"
    "\"accountId\":\"acc1\","
    "\"oldState\":\"email-old\","
    "\"newState\":\"email-set-new\","
    "\"created\":{\"create1\":{\"id\":\"e-created\"}},"
    "\"updated\":[\"e1\"],"
    "\"destroyed\":[\"e0\"],"
    "\"notCreated\":{\"badCreate\":{\"type\":\"invalidProperties\","
    "\"description\":\"missing body\"}},"
    "\"notUpdated\":{\"badUpdate\":{\"type\":\"notFound\"}},"
    "\"notDestroyed\":{\"badDestroy\":{\"type\":\"forbidden\","
    "\"description\":\"cannot destroy\"}}"
    "},\"c1\"]"
    "],"
    "\"sessionState\":\"state-email-set\""
    "}";

static const char email_copy_json[] =
    "{"
    "\"methodResponses\":["
    "[\"Email/copy\",{"
    "\"accountId\":\"acc1\","
    "\"oldState\":\"email-set-new\","
    "\"newState\":\"email-copy-new\","
    "\"created\":{\"copy1\":{\"id\":\"e-copy\"}},"
    "\"notCreated\":{\"badCopy\":{\"type\":\"notFound\","
    "\"description\":\"source missing\"}},"
    "\"destroyed\":[\"e-source\"],"
    "\"notDestroyed\":{\"blocked\":{\"type\":\"forbidden\"}}"
    "},\"c1\"]"
    "],"
    "\"sessionState\":\"state-email-copy\""
    "}";

static const char identity_get_json[] =
    "{"
    "\"methodResponses\":["
    "[\"Identity/get\",{"
    "\"accountId\":\"acc1\","
    "\"state\":\"identity-state\","
    "\"list\":[{"
    "\"id\":\"id1\","
    "\"name\":\"Example Sender\","
    "\"email\":\"sender@example.com\","
    "\"replyTo\":[{\"name\":\"Replies\","
    "\"email\":\"reply@example.com\"}],"
    "\"bcc\":[{\"name\":\"Archive\","
    "\"email\":\"archive@example.com\"}],"
    "\"textSignature\":\"-- text\","
    "\"htmlSignature\":\"<p>html</p>\""
    "}],"
    "\"notFound\":[\"missing-id\"]"
    "},\"c1\"]"
    "],"
    "\"sessionState\":\"state-identity\""
    "}";

static const char email_submission_set_json[] =
    "{"
    "\"methodResponses\":["
    "[\"EmailSubmission/set\",{"
    "\"accountId\":\"acc1\","
    "\"oldState\":\"submission-old\","
    "\"newState\":\"submission-new\","
    "\"created\":{\"submit1\":{"
    "\"id\":\"s1\","
    "\"emailId\":\"e-imported\","
    "\"identityId\":\"id1\","
    "\"threadId\":\"t-submit\","
    "\"sendAt\":\"2026-08-02T11:00:00Z\","
    "\"undoStatus\":\"pending\","
    "\"envelope\":{"
    "\"mailFrom\":{\"name\":\"Sender\","
    "\"email\":\"sender@example.com\"},"
    "\"rcptTo\":[{\"name\":\"Recipient\","
    "\"email\":\"to@example.com\"}]"
    "},"
    "\"deliveryStatus\":{"
    "\"to@example.com\":{"
    "\"smtpReply\":\"250 2.0.0 queued\","
    "\"delivered\":\"queued\","
    "\"displayed\":\"unknown\""
    "}"
    "},"
    "\"dsnBlobIds\":{\"to@example.com\":\"dsn-blob\"}"
    "}},"
    "\"updated\":[\"s2\"],"
    "\"destroyed\":[\"s0\"],"
    "\"notCreated\":{\"badSubmit\":{\"type\":\"invalidEmail\","
    "\"description\":\"cannot submit\"}},"
    "\"notUpdated\":{\"badUpdate\":{\"type\":\"notFound\"}},"
    "\"notDestroyed\":{\"badDestroy\":{\"type\":\"forbidden\"}}"
    "},\"c1\"]"
    "],"
    "\"sessionState\":\"state-submission\""
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

static char * read_fixture_file(const char * name, size_t * result_len)
{
  const char * prefixes[] = {
    "tests/jmap/data/email/",
    "jmap/data/email/"
  };
  char path[512];
  char * data;
  FILE * f;
  long len;
  size_t read_len;
  size_t i;

  for (i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); i ++) {
    snprintf(path, sizeof(path), "%s%s", prefixes[i], name);
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
      (strstr(context->body, "\"Email/get\"") != NULL))
    body = context->email_get_body != NULL ?
        context->email_get_body : email_get_json;
  else if ((context->body != NULL) &&
      (strstr(context->body, "\"Email/queryChanges\"") != NULL))
    body = context->email_query_changes_body != NULL ?
        context->email_query_changes_body : email_query_changes_json;
  else if ((context->body != NULL) &&
      (strstr(context->body, "\"Email/query\"") != NULL))
    body = email_query_json;
  else if ((context->body != NULL) &&
      (strstr(context->body, "\"Email/import\"") != NULL))
    body = email_import_json;
  else if ((context->body != NULL) &&
      (strstr(context->body, "\"Email/parse\"") != NULL))
    body = email_parse_json;
  else if ((context->body != NULL) &&
      (strstr(context->body, "\"SearchSnippet/get\"") != NULL))
    body = search_snippet_json;
  else if ((context->body != NULL) &&
      (strstr(context->body, "\"Email/set\"") != NULL))
    body = email_set_json;
  else if ((context->body != NULL) &&
      (strstr(context->body, "\"Email/copy\"") != NULL))
    body = email_copy_json;
  else if ((context->body != NULL) &&
      (strstr(context->body, "\"Identity/get\"") != NULL))
    body = identity_get_json;
  else if ((context->body != NULL) &&
      (strstr(context->body, "\"EmailSubmission/set\"") != NULL))
    body = email_submission_set_json;
  else
    body = email_changes_json;

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

static void email_set_item_list_free(clist * list)
{
  clistiter * cur;

  if (list == NULL)
    return;

  for (cur = clist_begin(list); cur != NULL; cur = clist_next(cur))
    mailjmap_email_set_item_free(clist_content(cur));
  clist_free(list);
}

static void email_copy_item_list_free(clist * list)
{
  clistiter * cur;

  if (list == NULL)
    return;

  for (cur = clist_begin(list); cur != NULL; cur = clist_next(cur))
    mailjmap_email_copy_item_free(clist_content(cur));
  clist_free(list);
}

static void email_submission_set_item_list_free(clist * list)
{
  clistiter * cur;

  if (list == NULL)
    return;

  for (cur = clist_begin(list); cur != NULL; cur = clist_next(cur))
    mailjmap_email_submission_set_item_free(clist_content(cur));
  clist_free(list);
}

static void email_query_sort_comparator_list_free(clist * list)
{
  clistiter * cur;

  if (list == NULL)
    return;

  for (cur = clist_begin(list); cur != NULL; cur = clist_next(cur))
    mailjmap_email_query_sort_comparator_free(clist_content(cur));
  clist_free(list);
}

static int test_email_query_and_changes(void)
{
  mailjmap * client;
  struct fake_context context;
  struct mailjmap_session * session_object;
  struct mailjmap_email_get_result * get_result;
  struct mailjmap_query_result * query_result;
  struct mailjmap_query_changes_result * query_changes_result;
  struct mailjmap_changes_result * changes_result;
  struct mailjmap_import_result * import_result;
  struct mailjmap_email_parse_result * parse_result;
  struct mailjmap_search_snippet_get_result * snippet_result;
  struct mailjmap_set_result * set_result;
  struct mailjmap_set_result * copy_result;
  struct mailjmap_identity_get_result * identity_result;
  struct mailjmap_set_result * submission_result;
  struct mailjmap_email * email;
  struct mailjmap_email * imported_email;
  struct mailjmap_identity * identity;
  struct mailjmap_email_header * header;
  struct mailjmap_email_address * sender;
  struct mailjmap_email_address * from;
  struct mailjmap_email_address * to;
  struct mailjmap_email_address * cc;
  struct mailjmap_email_address * bcc;
  struct mailjmap_email_address * reply_to;
  struct mailjmap_email_body_part * text_part;
  struct mailjmap_email_body_part * html_part;
  struct mailjmap_email_body_part * attachment;
  struct mailjmap_email_body_value * body_value;
  struct mailjmap_email_body_part * mixed_alternative;
  struct mailjmap_email_body_part * mixed_attachment;
  struct mailjmap_email_body_part * alternative_text;
  struct mailjmap_email_body_part * alternative_html;
  char * mixed_attachment_second_language;
  char * attachment_second_language;
  struct mailjmap_query_change * added_change;
  struct mailjmap_import_created * import_created;
  struct mailjmap_import_not_created * import_not_created;
  struct mailjmap_email_parse_item * parsed_item;
  struct mailjmap_email_body_part * parsed_text_part;
  struct mailjmap_email_body_value * parsed_body_value;
  struct mailjmap_search_snippet * snippet;
  struct mailjmap_email_set_item * update_item;
  struct mailjmap_set_created * set_created;
  struct mailjmap_set_error * set_not_created;
  struct mailjmap_set_error * set_not_updated;
  struct mailjmap_set_error * set_not_destroyed;
  struct mailjmap_email_copy_item * copy_item;
  struct mailjmap_email_query_filter * query_filter;
  struct mailjmap_email_query_filter * query_filter_child;
  struct mailjmap_email_query_sort_comparator * sort_comparator;
  struct mailjmap_set_created * copy_created;
  struct mailjmap_set_error * copy_not_created;
  struct mailjmap_set_error * copy_not_destroyed;
  struct mailjmap_email_submission_set_item * submission_create_item;
  struct mailjmap_email_submission_set_item * submission_update_item;
  struct mailjmap_set_created * submission_created;
  struct mailjmap_set_error * submission_not_created;
  struct mailjmap_set_error * submission_not_updated;
  struct mailjmap_set_error * submission_not_destroyed;
  struct mailjmap_email_address * submission_mail_from;
  struct mailjmap_email_address * submission_rcpt_to;
  struct mailjmap_email_submission_delivery_status *
      submission_delivery_status;
  clist * import_mailbox_ids;
  clist * import_keywords;
  clist * parse_blob_ids;
  clist * parse_properties;
  clist * parse_body_properties;
  clist * snippet_email_ids;
  clist * email_set_create;
  clist * email_set_update;
  clist * email_set_destroy;
  clist * email_copy_create;
  clist * query_sort;
  clist * submission_create;
  clist * submission_update;
  clist * submission_destroy;
  chashdatum map_key;
  chashdatum map_value;
  char * first_id;
  char * second_id;
  char * query_removed;
  char * created;
  char * updated;
  char * destroyed;
  char * parse_not_parsable;
  char * parse_not_parsable_from_list;
  char * snippet_not_found;
  char * set_updated;
  char * set_destroyed;
  char * copy_destroyed;
  char * submission_updated;
  char * submission_destroyed;
  char * submission_dsn_blob;
  char * identity_not_found;
  char * email_get_fixture;
  int r;
  int ok;

  memset(&context, 0, sizeof(context));
  client = NULL;
  session_object = NULL;
  get_result = NULL;
  query_result = NULL;
  query_changes_result = NULL;
  changes_result = NULL;
  import_result = NULL;
  parse_result = NULL;
  snippet_result = NULL;
  set_result = NULL;
  copy_result = NULL;
  identity_result = NULL;
  submission_result = NULL;
  email = NULL;
  imported_email = NULL;
  identity = NULL;
  header = NULL;
  sender = NULL;
  from = NULL;
  to = NULL;
  cc = NULL;
  bcc = NULL;
  reply_to = NULL;
  text_part = NULL;
  html_part = NULL;
  attachment = NULL;
  body_value = NULL;
  mixed_alternative = NULL;
  mixed_attachment = NULL;
  alternative_text = NULL;
  alternative_html = NULL;
  mixed_attachment_second_language = NULL;
  attachment_second_language = NULL;
  added_change = NULL;
  import_created = NULL;
  import_not_created = NULL;
  parsed_item = NULL;
  parsed_text_part = NULL;
  parsed_body_value = NULL;
  snippet = NULL;
  update_item = NULL;
  set_created = NULL;
  set_not_created = NULL;
  set_not_updated = NULL;
  set_not_destroyed = NULL;
  copy_item = NULL;
  query_filter = NULL;
  query_filter_child = NULL;
  sort_comparator = NULL;
  copy_created = NULL;
  copy_not_created = NULL;
  copy_not_destroyed = NULL;
  submission_create_item = NULL;
  submission_update_item = NULL;
  submission_created = NULL;
  submission_not_created = NULL;
  submission_not_updated = NULL;
  submission_not_destroyed = NULL;
  submission_mail_from = NULL;
  submission_rcpt_to = NULL;
  submission_delivery_status = NULL;
  import_mailbox_ids = NULL;
  import_keywords = NULL;
  parse_blob_ids = NULL;
  parse_properties = NULL;
  parse_body_properties = NULL;
  snippet_email_ids = NULL;
  email_set_create = NULL;
  email_set_update = NULL;
  email_set_destroy = NULL;
  email_copy_create = NULL;
  query_sort = NULL;
  submission_create = NULL;
  submission_update = NULL;
  submission_destroy = NULL;
  query_removed = NULL;
  submission_dsn_blob = NULL;
  identity_not_found = NULL;
  email_get_fixture = NULL;
  ok = 0;

  email_get_fixture = read_fixture_file("get-multipart-vendor.json", NULL);
  if (!check(email_get_fixture != NULL, "Email/get fixture read failed"))
    goto cleanup;
  context.email_get_body = email_get_fixture;

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

  r = mailjmap_email_get(client, "acc1", NULL, NULL, &get_result);
  if (!check(r == MAILJMAP_NO_ERROR, "Email/get failed"))
    goto cleanup;
  if (!check(strstr(context.body, "\"Email/get\"") != NULL,
      "request body missing Email/get"))
    goto cleanup;

  email = clist_content(clist_begin(get_result->list));
  header = clist_content(clist_begin(email->headers));
  if (clist_begin(email->sender) != NULL)
    sender = clist_content(clist_begin(email->sender));
  if (clist_begin(email->from) != NULL)
    from = clist_content(clist_begin(email->from));
  if (clist_begin(email->to) != NULL)
    to = clist_content(clist_begin(email->to));
  if (clist_begin(email->cc) != NULL)
    cc = clist_content(clist_begin(email->cc));
  if (clist_begin(email->bcc) != NULL)
    bcc = clist_content(clist_begin(email->bcc));
  if (clist_begin(email->reply_to) != NULL)
    reply_to = clist_content(clist_begin(email->reply_to));
  text_part = clist_content(clist_begin(email->text_body));
  html_part = clist_content(clist_begin(email->html_body));
  attachment = clist_content(clist_begin(email->attachments));
  if ((attachment != NULL) &&
      (clist_next(clist_begin(attachment->languages)) != NULL))
    attachment_second_language =
        clist_content(clist_next(clist_begin(attachment->languages)));
  body_value = clist_content(clist_begin(email->body_values));
  mixed_alternative =
      clist_content(clist_begin(email->body_structure->sub_parts));
  mixed_attachment =
      clist_content(clist_next(clist_begin(email->body_structure->sub_parts)));
  if ((mixed_attachment != NULL) &&
      (clist_next(clist_begin(mixed_attachment->languages)) != NULL))
    mixed_attachment_second_language =
        clist_content(clist_next(clist_begin(mixed_attachment->languages)));
  alternative_text =
      clist_content(clist_begin(mixed_alternative->sub_parts));
  alternative_html =
      clist_content(clist_next(clist_begin(mixed_alternative->sub_parts)));
  map_key.data = "mbox1";
  map_key.len = 6;
  if (!check(chash_get(email->mailbox_ids, &map_key, &map_value) == 0,
      "mailboxIds missing mbox1"))
    goto cleanup;
  map_key.data = "$seen";
  map_key.len = 6;
  if (!check(chash_get(email->keywords, &map_key, &map_value) == 0,
      "keywords missing seen"))
    goto cleanup;

  r = mailjmap_email_query(client, "acc1", 0, 25, &query_result);
  if (!check(r == MAILJMAP_NO_ERROR, "Email/query failed"))
    goto cleanup;

  first_id = clist_content(clist_begin(query_result->ids));
  second_id = clist_content(clist_next(clist_begin(query_result->ids)));

  if (!check(strstr(context.body, "\"Email/query\"") != NULL,
      "request body missing Email/query"))
    goto cleanup;
  if (!check(strstr(context.body, "\"limit\":25") != NULL,
      "request body missing limit"))
    goto cleanup;

  mailjmap_query_result_free(query_result);
  query_result = NULL;
  r = mailjmap_email_query_with_text_filter(client, "acc1", "Hello", 5, 10,
      &query_result);
  if (!check(r == MAILJMAP_NO_ERROR, "Email/query text filter failed"))
    goto cleanup;
  if (!check(strstr(context.body, "\"Email/query\"") != NULL,
      "request body missing filtered Email/query"))
    goto cleanup;
  if (!check(strstr(context.body, "\"position\":5") != NULL,
      "request body missing filtered query position"))
    goto cleanup;
  if (!check(strstr(context.body, "\"limit\":10") != NULL,
      "request body missing filtered query limit"))
    goto cleanup;
  if (!check(strstr(context.body,
      "\"filter\":{\"text\":\"Hello\"}") != NULL,
      "request body missing Email/query text filter"))
    goto cleanup;

  first_id = clist_content(clist_begin(query_result->ids));
  second_id = clist_content(clist_next(clist_begin(query_result->ids)));

  mailjmap_query_result_free(query_result);
  query_result = NULL;
  r = mailjmap_email_query_with_options(client, "acc1", "Hello", -1,
      "e-anchor", -2, 10, 1, 1, &query_result);
  if (!check(r == MAILJMAP_NO_ERROR, "Email/query options failed"))
    goto cleanup;
  if (!check(strstr(context.body, "\"Email/query\"") != NULL,
      "request body missing option Email/query"))
    goto cleanup;
  if (!check(strstr(context.body, "\"anchor\":\"e-anchor\"") != NULL,
      "request body missing query anchor"))
    goto cleanup;
  if (!check(strstr(context.body, "\"anchorOffset\":-2") != NULL,
      "request body missing query anchorOffset"))
    goto cleanup;
  if (!check(strstr(context.body, "\"limit\":10") != NULL,
      "request body missing option query limit"))
    goto cleanup;
  if (!check(strstr(context.body, "\"calculateTotal\":true") != NULL,
      "request body missing query calculateTotal"))
    goto cleanup;
  if (!check(strstr(context.body, "\"collapseThreads\":true") != NULL,
      "request body missing query collapseThreads"))
    goto cleanup;

  query_sort = clist_new();
  if (!check(query_sort != NULL, "query sort allocation failed"))
    goto cleanup;
  sort_comparator = mailjmap_email_query_sort_comparator_new("receivedAt");
  if (!check(sort_comparator != NULL,
      "query sort comparator allocation failed"))
    goto cleanup;
  mailjmap_email_query_sort_comparator_set_is_ascending(sort_comparator, 0);
  r = mailjmap_email_query_sort_comparator_set_collation(sort_comparator,
      "i;unicode-casemap");
  if (!check(r == MAILJMAP_NO_ERROR, "query sort collation set failed"))
    goto cleanup;
  if (!check(clist_append(query_sort, sort_comparator) >= 0,
      "query sort append failed"))
    goto cleanup;
  sort_comparator = NULL;

  mailjmap_query_result_free(query_result);
  query_result = NULL;
  r = mailjmap_email_query_with_sort_options(client, "acc1", "Hello",
      query_sort, -1, "e-anchor", -2, 10, 1, 1, &query_result);
  if (!check(r == MAILJMAP_NO_ERROR, "Email/query sort options failed"))
    goto cleanup;
  if (!check(strstr(context.body, "\"sort\":[{") != NULL,
      "request body missing query sort array"))
    goto cleanup;
  if (!check(strstr(context.body, "\"property\":\"receivedAt\"") != NULL,
      "request body missing query sort property"))
    goto cleanup;
  if (!check(strstr(context.body, "\"isAscending\":false") != NULL,
      "request body missing query sort direction"))
    goto cleanup;
  if (!check(strstr(context.body,
      "\"collation\":\"i;unicode-casemap\"") != NULL,
      "request body missing query sort collation"))
    goto cleanup;

  query_filter = mailjmap_email_query_filter_operator_new("AND");
  if (!check(query_filter != NULL, "query filter allocation failed"))
    goto cleanup;
  query_filter_child = mailjmap_email_query_filter_condition_new();
  if (!check(query_filter_child != NULL,
      "query filter first condition allocation failed"))
    goto cleanup;
  r = mailjmap_email_query_filter_set_in_mailbox(query_filter_child,
      "inbox");
  if (!check(r == MAILJMAP_NO_ERROR, "query filter inMailbox set failed"))
    goto cleanup;
  r = mailjmap_email_query_filter_set_has_keyword(query_filter_child,
      "$seen");
  if (!check(r == MAILJMAP_NO_ERROR, "query filter hasKeyword set failed"))
    goto cleanup;
  r = mailjmap_email_query_filter_add_condition(query_filter,
      query_filter_child);
  if (!check(r == MAILJMAP_NO_ERROR,
      "query filter first condition append failed"))
    goto cleanup;
  query_filter_child = NULL;

  query_filter_child = mailjmap_email_query_filter_condition_new();
  if (!check(query_filter_child != NULL,
      "query filter second condition allocation failed"))
    goto cleanup;
  r = mailjmap_email_query_filter_set_from(query_filter_child,
      "sender@example.com");
  if (!check(r == MAILJMAP_NO_ERROR, "query filter from set failed"))
    goto cleanup;
  r = mailjmap_email_query_filter_set_subject(query_filter_child,
      "Report");
  if (!check(r == MAILJMAP_NO_ERROR, "query filter subject set failed"))
    goto cleanup;
  mailjmap_email_query_filter_set_max_size(query_filter_child, 65536);
  r = mailjmap_email_query_filter_add_condition(query_filter,
      query_filter_child);
  if (!check(r == MAILJMAP_NO_ERROR,
      "query filter second condition append failed"))
    goto cleanup;
  query_filter_child = NULL;

  mailjmap_query_result_free(query_result);
  query_result = NULL;
  r = mailjmap_email_query_with_filter_options(client, "acc1",
      query_filter, query_sort, -1, NULL, 0, 20, 1, 0, &query_result);
  if (!check(r == MAILJMAP_NO_ERROR, "Email/query filter options failed"))
    goto cleanup;
  if (!check(strstr(context.body, "\"operator\":\"AND\"") != NULL,
      "request body missing query filter operator"))
    goto cleanup;
  if (!check(strstr(context.body, "\"conditions\":[{") != NULL,
      "request body missing query filter conditions"))
    goto cleanup;
  if (!check(strstr(context.body, "\"inMailbox\":\"inbox\"") != NULL,
      "request body missing query filter inMailbox"))
    goto cleanup;
  if (!check(strstr(context.body, "\"hasKeyword\":\"$seen\"") != NULL,
      "request body missing query filter hasKeyword"))
    goto cleanup;
  if (!check(strstr(context.body,
      "\"from\":\"sender@example.com\"") != NULL,
      "request body missing query filter from"))
    goto cleanup;
  if (!check(strstr(context.body, "\"subject\":\"Report\"") != NULL,
      "request body missing query filter subject"))
    goto cleanup;
  if (!check(strstr(context.body, "\"maxSize\":65536") != NULL,
      "request body missing query filter maxSize"))
    goto cleanup;

  first_id = clist_content(clist_begin(query_result->ids));
  second_id = clist_content(clist_next(clist_begin(query_result->ids)));

  r = mailjmap_email_query_changes(client, "acc1", "email-query-state",
      &query_changes_result);
  if (!check(r == MAILJMAP_NO_ERROR, "Email/queryChanges failed"))
    goto cleanup;
  if (!check(strstr(context.body, "\"Email/queryChanges\"") != NULL,
      "request body missing Email/queryChanges"))
    goto cleanup;
  if (!check(strstr(context.body,
      "\"sinceQueryState\":\"email-query-state\"") != NULL,
      "request body missing sinceQueryState"))
    goto cleanup;

  mailjmap_query_changes_result_free(query_changes_result);
  query_changes_result = NULL;
  context.email_query_changes_body =
      email_query_changes_fastmail_nulls_json;
  r = mailjmap_email_query_changes(client, "acc1", "email-query-state",
      &query_changes_result);
  if (!check(r == MAILJMAP_NO_ERROR,
      "Fastmail Email/queryChanges null arrays failed"))
    goto cleanup;
  if (!check(clist_count(query_changes_result->removed) == 0,
      "Fastmail queryChanges removed should be empty"))
    goto cleanup;
  added_change = clist_content(clist_begin(query_changes_result->added));
  if (!check(str_equal(added_change->id, "e3"),
      "Fastmail queryChanges added id mismatch"))
    goto cleanup;
  context.email_query_changes_body = NULL;

  mailjmap_query_changes_result_free(query_changes_result);
  query_changes_result = NULL;
  r = mailjmap_email_query_changes_with_text_filter(client, "acc1",
      "email-query-state", "Hello", &query_changes_result);
  if (!check(r == MAILJMAP_NO_ERROR,
      "Email/queryChanges text filter failed"))
    goto cleanup;
  if (!check(strstr(context.body, "\"Email/queryChanges\"") != NULL,
      "request body missing filtered Email/queryChanges"))
    goto cleanup;
  if (!check(strstr(context.body,
      "\"sinceQueryState\":\"email-query-state\"") != NULL,
      "request body missing filtered sinceQueryState"))
    goto cleanup;
  if (!check(strstr(context.body,
      "\"filter\":{\"text\":\"Hello\"}") != NULL,
      "request body missing Email/queryChanges text filter"))
    goto cleanup;

  mailjmap_query_changes_result_free(query_changes_result);
  query_changes_result = NULL;
  r = mailjmap_email_query_changes_with_options(client, "acc1",
      "email-query-state", "Hello", 7, "e2", 1, 0,
      &query_changes_result);
  if (!check(r == MAILJMAP_NO_ERROR,
      "Email/queryChanges options failed"))
    goto cleanup;
  if (!check(strstr(context.body, "\"Email/queryChanges\"") != NULL,
      "request body missing option Email/queryChanges"))
    goto cleanup;
  if (!check(strstr(context.body,
      "\"sinceQueryState\":\"email-query-state\"") != NULL,
      "request body missing option sinceQueryState"))
    goto cleanup;
  if (!check(strstr(context.body, "\"maxChanges\":7") != NULL,
      "request body missing queryChanges maxChanges"))
    goto cleanup;
  if (!check(strstr(context.body, "\"upToId\":\"e2\"") != NULL,
      "request body missing queryChanges upToId"))
    goto cleanup;
  if (!check(strstr(context.body, "\"calculateTotal\":true") != NULL,
      "request body missing queryChanges calculateTotal"))
    goto cleanup;
  if (!check(strstr(context.body, "\"collapseThreads\":false") != NULL,
      "request body missing queryChanges collapseThreads"))
    goto cleanup;

  mailjmap_query_changes_result_free(query_changes_result);
  query_changes_result = NULL;
  r = mailjmap_email_query_changes_with_filter_options(client, "acc1",
      "email-query-state", query_filter, query_sort, 7, "e2", 1, 0,
      &query_changes_result);
  if (!check(r == MAILJMAP_NO_ERROR,
      "Email/queryChanges filter options failed"))
    goto cleanup;
  if (!check(strstr(context.body, "\"Email/queryChanges\"") != NULL,
      "request body missing filtered option Email/queryChanges"))
    goto cleanup;
  if (!check(strstr(context.body, "\"operator\":\"AND\"") != NULL,
      "request body missing queryChanges filter operator"))
    goto cleanup;
  if (!check(strstr(context.body, "\"inMailbox\":\"inbox\"") != NULL,
      "request body missing queryChanges filter inMailbox"))
    goto cleanup;
  if (!check(strstr(context.body, "\"property\":\"receivedAt\"") != NULL,
      "request body missing queryChanges sort property"))
    goto cleanup;
  if (!check(strstr(context.body, "\"maxChanges\":7") != NULL,
      "request body missing filtered queryChanges maxChanges"))
    goto cleanup;
  if (!check(strstr(context.body, "\"upToId\":\"e2\"") != NULL,
      "request body missing filtered queryChanges upToId"))
    goto cleanup;

  query_removed = clist_content(clist_begin(query_changes_result->removed));
  added_change = clist_content(clist_begin(query_changes_result->added));

  r = mailjmap_email_changes(client, "acc1", "email-old",
      &changes_result);
  if (!check(r == MAILJMAP_NO_ERROR, "Email/changes failed"))
    goto cleanup;
  if (!check(strstr(context.body, "\"Email/changes\"") != NULL,
      "request body missing Email/changes"))
    goto cleanup;
  if (!check(strstr(context.body, "\"sinceState\":\"email-old\"") != NULL,
      "request body missing sinceState"))
    goto cleanup;

  created = clist_content(clist_begin(changes_result->created));
  updated = clist_content(clist_begin(changes_result->updated));
  destroyed = clist_content(clist_begin(changes_result->destroyed));

  import_mailbox_ids = clist_new();
  import_keywords = clist_new();
  if (!check((import_mailbox_ids != NULL) && (import_keywords != NULL),
      "import lists allocation failed"))
    goto cleanup;
  if (!check(clist_append(import_mailbox_ids, "mbox1") >= 0,
      "mailbox id append failed"))
    goto cleanup;
  if (!check(clist_append(import_keywords, "$seen") >= 0,
      "keyword append failed"))
    goto cleanup;

  r = mailjmap_email_import(client, "acc1", "import1", "blob1",
      import_mailbox_ids, import_keywords, "2026-08-02T12:00:00Z",
      &import_result);
  if (!check(r == MAILJMAP_NO_ERROR, "Email/import failed"))
    goto cleanup;
  if (!check(strstr(context.body, "\"Email/import\"") != NULL,
      "request body missing Email/import"))
    goto cleanup;
  if (!check(strstr(context.body, "\"blobId\":\"blob1\"") != NULL,
      "request body missing import blobId"))
    goto cleanup;
  if (!check(strstr(context.body, "\"mailboxIds\"") != NULL,
      "request body missing import mailboxIds"))
    goto cleanup;
  if (!check(strstr(context.body, "\"mbox1\":true") != NULL,
      "request body missing import mailbox id"))
    goto cleanup;
  if (!check(strstr(context.body, "\"keywords\"") != NULL,
      "request body missing import keywords"))
    goto cleanup;
  if (!check(strstr(context.body, "\"$seen\":true") != NULL,
      "request body missing import keyword"))
    goto cleanup;
  if (!check(strstr(context.body,
      "\"receivedAt\":\"2026-08-02T12:00:00Z\"") != NULL,
      "request body missing import receivedAt"))
    goto cleanup;

  import_created = clist_content(clist_begin(import_result->created));
  import_not_created = clist_content(clist_begin(import_result->not_created));
  imported_email = import_created->email;

  parse_blob_ids = clist_new();
  parse_properties = clist_new();
  parse_body_properties = clist_new();
  if (!check((parse_blob_ids != NULL) && (parse_properties != NULL) &&
      (parse_body_properties != NULL), "parse lists allocation failed"))
    goto cleanup;
  if (!check(clist_append(parse_blob_ids, "blob-parse") >= 0,
      "parse blob id append failed"))
    goto cleanup;
  if (!check(clist_append(parse_blob_ids, "bad-blob") >= 0,
      "parse bad blob id append failed"))
    goto cleanup;
  if (!check(clist_append(parse_properties, "subject") >= 0,
      "parse property append failed"))
    goto cleanup;
  if (!check(clist_append(parse_properties, "preview") >= 0,
      "parse preview property append failed"))
    goto cleanup;
  if (!check(clist_append(parse_body_properties, "partId") >= 0,
      "parse body property append failed"))
    goto cleanup;
  if (!check(clist_append(parse_body_properties, "type") >= 0,
      "parse type body property append failed"))
    goto cleanup;

  r = mailjmap_email_parse(client, "acc1", parse_blob_ids,
      parse_properties, parse_body_properties, &parse_result);
  if (!check(r == MAILJMAP_NO_ERROR, "Email/parse failed"))
    goto cleanup;
  if (!check(strstr(context.body, "\"Email/parse\"") != NULL,
      "request body missing Email/parse"))
    goto cleanup;
  if (!check(strstr(context.body,
      "\"blobIds\":[\"blob-parse\",\"bad-blob\"]") != NULL,
      "request body missing parse blobIds"))
    goto cleanup;
  if (!check(strstr(context.body,
      "\"properties\":[\"subject\",\"preview\"]") != NULL,
      "request body missing parse properties"))
    goto cleanup;
  if (!check(strstr(context.body,
      "\"bodyProperties\":[\"partId\",\"type\"]") != NULL,
      "request body missing parse bodyProperties"))
    goto cleanup;

  parsed_item = clist_content(clist_begin(parse_result->parsed));
  parsed_text_part = clist_content(clist_begin(parsed_item->email->text_body));
  parsed_body_value =
      clist_content(clist_begin(parsed_item->email->body_values));
  parse_not_parsable = clist_content(clist_begin(parse_result->not_parsable));
  parse_not_parsable_from_list =
      clist_content(clist_next(clist_begin(parse_result->not_parsable)));

  snippet_email_ids = clist_new();
  if (!check(snippet_email_ids != NULL, "snippet ids allocation failed"))
    goto cleanup;
  if (!check(clist_append(snippet_email_ids, "e1") >= 0,
      "snippet id append failed"))
    goto cleanup;
  if (!check(clist_append(snippet_email_ids, "missing-snippet") >= 0,
      "missing snippet id append failed"))
    goto cleanup;

  r = mailjmap_search_snippet_get(client, "acc1", snippet_email_ids,
      &snippet_result);
  if (!check(r == MAILJMAP_NO_ERROR, "SearchSnippet/get failed"))
    goto cleanup;
  if (!check(strstr(context.body, "\"SearchSnippet/get\"") != NULL,
      "request body missing SearchSnippet/get"))
    goto cleanup;
  if (!check(strstr(context.body,
      "\"emailIds\":[\"e1\",\"missing-snippet\"]") != NULL,
      "request body missing snippet emailIds"))
    goto cleanup;

  mailjmap_search_snippet_get_result_free(snippet_result);
  snippet_result = NULL;
  r = mailjmap_search_snippet_get_with_text_filter(client, "acc1",
      snippet_email_ids, "Hello", &snippet_result);
  if (!check(r == MAILJMAP_NO_ERROR,
      "SearchSnippet/get text filter failed"))
    goto cleanup;
  if (!check(strstr(context.body,
      "\"emailIds\":[\"e1\",\"missing-snippet\"]") != NULL,
      "request body missing filtered snippet emailIds"))
    goto cleanup;
  if (!check(strstr(context.body,
      "\"filter\":{\"text\":\"Hello\"}") != NULL,
      "request body missing snippet text filter"))
    goto cleanup;

  mailjmap_search_snippet_get_result_free(snippet_result);
  snippet_result = NULL;
  r = mailjmap_search_snippet_get_with_filter(client, "acc1",
      snippet_email_ids, query_filter, &snippet_result);
  if (!check(r == MAILJMAP_NO_ERROR,
      "SearchSnippet/get typed filter failed"))
    goto cleanup;
  if (!check(strstr(context.body,
      "\"emailIds\":[\"e1\",\"missing-snippet\"]") != NULL,
      "request body missing typed filtered snippet emailIds"))
    goto cleanup;
  if (!check(strstr(context.body, "\"operator\":\"AND\"") != NULL,
      "request body missing snippet typed filter operator"))
    goto cleanup;
  if (!check(strstr(context.body, "\"inMailbox\":\"inbox\"") != NULL,
      "request body missing snippet typed filter inMailbox"))
    goto cleanup;
  if (!check(strstr(context.body, "\"subject\":\"Report\"") != NULL,
      "request body missing snippet typed filter subject"))
    goto cleanup;

  snippet = clist_content(clist_begin(snippet_result->list));
  snippet_not_found = clist_content(clist_begin(snippet_result->not_found));

  email_set_update = clist_new();
  email_set_destroy = clist_new();
  if (!check((email_set_update != NULL) && (email_set_destroy != NULL),
      "Email/set lists allocation failed"))
    goto cleanup;
  update_item = mailjmap_email_set_item_new("e1");
  if (!check(update_item != NULL, "Email/set item allocation failed"))
    goto cleanup;
  r = mailjmap_email_set_item_add_mailbox_id(update_item, "mbox1");
  if (!check(r == MAILJMAP_NO_ERROR, "Email/set mailbox id add failed"))
    goto cleanup;
  r = mailjmap_email_set_item_add_keyword(update_item, "$seen");
  if (!check(r == MAILJMAP_NO_ERROR, "Email/set keyword add failed"))
    goto cleanup;
  r = mailjmap_email_set_item_add_keyword(update_item, "$flagged");
  if (!check(r == MAILJMAP_NO_ERROR, "Email/set second keyword add failed"))
    goto cleanup;
  if (!check(clist_append(email_set_update, update_item) >= 0,
      "Email/set update append failed"))
    goto cleanup;
  update_item = NULL;
  if (!check(clist_append(email_set_destroy, "e0") >= 0,
      "Email/set destroy append failed"))
    goto cleanup;

  r = mailjmap_email_set(client, "acc1", "email-old", email_set_create,
      email_set_update, email_set_destroy, &set_result);
  if (!check(r == MAILJMAP_NO_ERROR, "Email/set failed"))
    goto cleanup;
  if (!check(strstr(context.body, "\"Email/set\"") != NULL,
      "request body missing Email/set"))
    goto cleanup;
  if (!check(strstr(context.body, "\"ifInState\":\"email-old\"") != NULL,
      "request body missing Email/set ifInState"))
    goto cleanup;
  if (!check(strstr(context.body, "\"update\":{\"e1\"") != NULL,
      "request body missing Email/set update map"))
    goto cleanup;
  if (!check(strstr(context.body, "\"mailboxIds\":{\"mbox1\":true}") != NULL,
      "request body missing Email/set mailboxIds"))
    goto cleanup;
  if (!check(strstr(context.body, "\"keywords\"") != NULL,
      "request body missing Email/set keywords"))
    goto cleanup;
  if (!check(strstr(context.body, "\"$seen\":true") != NULL,
      "request body missing Email/set seen keyword"))
    goto cleanup;
  if (!check(strstr(context.body, "\"$flagged\":true") != NULL,
      "request body missing Email/set flagged keyword"))
    goto cleanup;
  if (!check(strstr(context.body, "\"destroy\":[\"e0\"]") != NULL,
      "request body missing Email/set destroy"))
    goto cleanup;

  set_created = clist_content(clist_begin(set_result->created));
  set_updated = clist_content(clist_begin(set_result->updated));
  set_destroyed = clist_content(clist_begin(set_result->destroyed));
  set_not_created = clist_content(clist_begin(set_result->not_created));
  set_not_updated = clist_content(clist_begin(set_result->not_updated));
  set_not_destroyed = clist_content(clist_begin(set_result->not_destroyed));

  email_copy_create = clist_new();
  if (!check(email_copy_create != NULL, "Email/copy list allocation failed"))
    goto cleanup;
  copy_item = mailjmap_email_copy_item_new("copy1", "e1");
  if (!check(copy_item != NULL, "Email/copy item allocation failed"))
    goto cleanup;
  r = mailjmap_email_copy_item_add_mailbox_id(copy_item, "archive");
  if (!check(r == MAILJMAP_NO_ERROR, "Email/copy mailbox id add failed"))
    goto cleanup;
  r = mailjmap_email_copy_item_add_keyword(copy_item, "$seen");
  if (!check(r == MAILJMAP_NO_ERROR, "Email/copy keyword add failed"))
    goto cleanup;
  r = mailjmap_email_copy_item_set_received_at(copy_item,
      "2026-08-02T13:00:00Z");
  if (!check(r == MAILJMAP_NO_ERROR, "Email/copy receivedAt set failed"))
    goto cleanup;
  if (!check(clist_append(email_copy_create, copy_item) >= 0,
      "Email/copy create append failed"))
    goto cleanup;
  copy_item = NULL;

  r = mailjmap_email_copy(client, "acc1", "source-acc",
      "email-source-state", email_copy_create, 1, &copy_result);
  if (!check(r == MAILJMAP_NO_ERROR, "Email/copy failed"))
    goto cleanup;
  if (!check(strstr(context.body, "\"Email/copy\"") != NULL,
      "request body missing Email/copy"))
    goto cleanup;
  if (!check(strstr(context.body, "\"fromAccountId\":\"source-acc\"") != NULL,
      "request body missing Email/copy fromAccountId"))
    goto cleanup;
  if (!check(strstr(context.body,
      "\"ifFromInState\":\"email-source-state\"") != NULL,
      "request body missing Email/copy ifFromInState"))
    goto cleanup;
  if (!check(strstr(context.body, "\"create\":{\"copy1\"") != NULL,
      "request body missing Email/copy create map"))
    goto cleanup;
  if (!check(strstr(context.body, "\"id\":\"e1\"") != NULL,
      "request body missing Email/copy source id"))
    goto cleanup;
  if (!check(strstr(context.body, "\"mailboxIds\":{\"archive\":true}") != NULL,
      "request body missing Email/copy mailboxIds"))
    goto cleanup;
  if (!check(strstr(context.body, "\"$seen\":true") != NULL,
      "request body missing Email/copy keyword"))
    goto cleanup;
  if (!check(strstr(context.body,
      "\"receivedAt\":\"2026-08-02T13:00:00Z\"") != NULL,
      "request body missing Email/copy receivedAt"))
    goto cleanup;
  if (!check(strstr(context.body,
      "\"onSuccessDestroyOriginal\":true") != NULL,
      "request body missing Email/copy destroy original flag"))
    goto cleanup;

  copy_created = clist_content(clist_begin(copy_result->created));
  copy_destroyed = clist_content(clist_begin(copy_result->destroyed));
  copy_not_created = clist_content(clist_begin(copy_result->not_created));
  copy_not_destroyed =
      clist_content(clist_begin(copy_result->not_destroyed));

  r = mailjmap_identity_get(client, "acc1", NULL, NULL, &identity_result);
  if (!check(r == MAILJMAP_NO_ERROR, "Identity/get failed"))
    goto cleanup;
  if (!check(strstr(context.body, "\"Identity/get\"") != NULL,
      "request body missing Identity/get"))
    goto cleanup;
  if (!check(strstr(context.body,
      "\"urn:ietf:params:jmap:submission\"") != NULL,
      "request body missing identity submission capability"))
    goto cleanup;
  if (!check(strstr(context.body, "\"accountId\":\"acc1\"") != NULL,
      "request body missing identity accountId"))
    goto cleanup;

  identity = clist_content(clist_begin(identity_result->list));
  identity_not_found = clist_content(clist_begin(identity_result->not_found));

  submission_create = clist_new();
  submission_update = clist_new();
  submission_destroy = clist_new();
  if (!check((submission_create != NULL) && (submission_update != NULL) &&
      (submission_destroy != NULL), "submission lists allocation failed"))
    goto cleanup;
  submission_create_item =
      mailjmap_email_submission_set_item_new("submit1");
  submission_update_item = mailjmap_email_submission_set_item_new("s2");
  if (!check((submission_create_item != NULL) &&
      (submission_update_item != NULL),
      "submission item allocation failed"))
    goto cleanup;
  r = mailjmap_email_submission_set_item_set_email_id(
      submission_create_item, "e-imported");
  if (!check(r == MAILJMAP_NO_ERROR, "submission emailId set failed"))
    goto cleanup;
  r = mailjmap_email_submission_set_item_set_identity_id(
      submission_create_item, "id1");
  if (!check(r == MAILJMAP_NO_ERROR, "submission identityId set failed"))
    goto cleanup;
  r = mailjmap_email_submission_set_item_set_send_at(
      submission_create_item, "2026-08-02T11:00:00Z");
  if (!check(r == MAILJMAP_NO_ERROR, "submission sendAt set failed"))
    goto cleanup;
  r = mailjmap_email_submission_set_item_set_undo_status(
      submission_create_item, "pending");
  if (!check(r == MAILJMAP_NO_ERROR, "submission undoStatus set failed"))
    goto cleanup;
  r = mailjmap_email_submission_set_item_set_envelope_mail_from(
      submission_create_item, "Sender", "sender@example.com");
  if (!check(r == MAILJMAP_NO_ERROR,
      "submission envelope mailFrom set failed"))
    goto cleanup;
  r = mailjmap_email_submission_set_item_add_envelope_rcpt_to(
      submission_create_item, "Recipient", "to@example.com");
  if (!check(r == MAILJMAP_NO_ERROR,
      "submission envelope rcptTo set failed"))
    goto cleanup;
  r = mailjmap_email_submission_set_item_set_identity_id(
      submission_update_item, "id1");
  if (!check(r == MAILJMAP_NO_ERROR, "submission update identityId failed"))
    goto cleanup;
  if (!check(clist_append(submission_create, submission_create_item) >= 0,
      "submission create append failed"))
    goto cleanup;
  submission_create_item = NULL;
  if (!check(clist_append(submission_update, submission_update_item) >= 0,
      "submission update append failed"))
    goto cleanup;
  submission_update_item = NULL;
  if (!check(clist_append(submission_destroy, "s0") >= 0,
      "submission destroy append failed"))
    goto cleanup;

  r = mailjmap_email_submission_set(client, "acc1", "submission-old",
      submission_create, submission_update, submission_destroy,
      &submission_result);
  if (!check(r == MAILJMAP_NO_ERROR, "EmailSubmission/set failed"))
    goto cleanup;
  if (!check(strstr(context.body, "\"EmailSubmission/set\"") != NULL,
      "request body missing EmailSubmission/set"))
    goto cleanup;
  if (!check(strstr(context.body,
      "\"urn:ietf:params:jmap:submission\"") != NULL,
      "request body missing submission capability"))
    goto cleanup;
  if (!check(strstr(context.body, "\"ifInState\":\"submission-old\"") != NULL,
      "request body missing submission ifInState"))
    goto cleanup;
  if (!check(strstr(context.body, "\"create\":{\"submit1\"") != NULL,
      "request body missing submission create map"))
    goto cleanup;
  if (!check(strstr(context.body, "\"emailId\":\"e-imported\"") != NULL,
      "request body missing submission emailId"))
    goto cleanup;
  if (!check(strstr(context.body, "\"identityId\":\"id1\"") != NULL,
      "request body missing submission identityId"))
    goto cleanup;
  if (!check(strstr(context.body,
      "\"sendAt\":\"2026-08-02T11:00:00Z\"") != NULL,
      "request body missing submission sendAt"))
    goto cleanup;
  if (!check(strstr(context.body, "\"undoStatus\":\"pending\"") != NULL,
      "request body missing submission undoStatus"))
    goto cleanup;
  if (!check(strstr(context.body, "\"envelope\":{") != NULL,
      "request body missing submission envelope"))
    goto cleanup;
  if (!check(strstr(context.body,
      "\"mailFrom\":{") != NULL,
      "request body missing submission envelope mailFrom"))
    goto cleanup;
  if (!check(strstr(context.body, "\"name\":\"Sender\"") != NULL,
      "request body missing submission envelope mailFrom name"))
    goto cleanup;
  if (!check(strstr(context.body, "\"email\":\"sender@example.com\"")
      != NULL, "request body missing submission envelope mailFrom email"))
    goto cleanup;
  if (!check(strstr(context.body,
      "\"rcptTo\":[{") != NULL,
      "request body missing submission envelope rcptTo"))
    goto cleanup;
  if (!check(strstr(context.body, "\"name\":\"Recipient\"") != NULL,
      "request body missing submission envelope rcptTo name"))
    goto cleanup;
  if (!check(strstr(context.body, "\"email\":\"to@example.com\"")
      != NULL, "request body missing submission envelope rcptTo email"))
    goto cleanup;
  if (!check(strstr(context.body, "\"update\":{\"s2\"") != NULL,
      "request body missing submission update map"))
    goto cleanup;
  if (!check(strstr(context.body, "\"destroy\":[\"s0\"]") != NULL,
      "request body missing submission destroy"))
    goto cleanup;

  submission_created =
      clist_content(clist_begin(submission_result->created));
  submission_updated =
      clist_content(clist_begin(submission_result->updated));
  submission_destroyed =
      clist_content(clist_begin(submission_result->destroyed));
  submission_mail_from = submission_created->envelope_mail_from;
  if (clist_begin(submission_created->envelope_rcpt_to) != NULL)
    submission_rcpt_to =
        clist_content(clist_begin(submission_created->envelope_rcpt_to));
  if (clist_begin(submission_created->delivery_status) != NULL)
    submission_delivery_status =
        clist_content(clist_begin(submission_created->delivery_status));
  map_key.data = "to@example.com";
  map_key.len = (unsigned int) strlen((const char *) map_key.data) + 1;
  if (chash_get(submission_created->dsn_blob_ids,
      &map_key, &map_value) == 0)
    submission_dsn_blob = map_value.data;
  submission_not_created =
      clist_content(clist_begin(submission_result->not_created));
  submission_not_updated =
      clist_content(clist_begin(submission_result->not_updated));
  submission_not_destroyed =
      clist_content(clist_begin(submission_result->not_destroyed));

  ok = check(context.calls == 22, "call count mismatch") &&
      check(str_equal(get_result->account_id, "acc1"),
          "get accountId mismatch") &&
      check(str_equal(get_result->state, "email-state"),
          "get state mismatch") &&
      check(clist_count(get_result->list) == 1, "email count mismatch") &&
      check(str_equal(email->id, "e1"), "email id mismatch") &&
      check(str_equal(email->blob_id, "blob1"), "blob id mismatch") &&
      check(str_equal(email->thread_id, "t1"), "thread id mismatch") &&
      check(email->size == 1234, "email size mismatch") &&
      check(str_equal(email->received_at, "2026-08-02T10:00:00Z"),
          "receivedAt mismatch") &&
      check(str_equal(email->message_id, "msg@example.com"),
          "messageId mismatch") &&
      check(clist_count(email->message_ids) == 2,
          "messageId list count mismatch") &&
      check(str_equal(email->in_reply_to, "parent@example.com"),
          "inReplyTo mismatch") &&
      check(clist_count(email->in_reply_to_list) == 2,
          "inReplyTo list count mismatch") &&
      check(clist_count(email->references) == 2,
          "references count mismatch") &&
      check(str_equal(clist_content(clist_begin(email->references)),
          "root@example.com"), "first reference mismatch") &&
      check(clist_count(email->sender) == 1, "sender count mismatch") &&
      check(clist_count(email->from) == 1, "from count mismatch") &&
      check(clist_count(email->to) == 1, "to count mismatch") &&
      check(clist_count(email->cc) == 1, "cc count mismatch") &&
      check(clist_count(email->bcc) == 1, "bcc count mismatch") &&
      check(clist_count(email->reply_to) == 1,
          "replyTo count mismatch") &&
      check(str_equal(sender->name, "Sender"),
          "sender name mismatch") &&
      check(str_equal(sender->email, "sender@example.com"),
          "sender email mismatch") &&
      check(str_equal(from->email, "from@example.com"),
          "from email mismatch") &&
      check(str_equal(to->email, "to@example.com"),
          "to email mismatch") &&
      check(str_equal(cc->email, "cc@example.com"),
          "cc email mismatch") &&
      check(str_equal(bcc->email, "bcc@example.com"),
          "bcc email mismatch") &&
      check(str_equal(reply_to->email, "reply@example.com"),
          "replyTo email mismatch") &&
      check(str_equal(email->subject, "Hello"), "subject mismatch") &&
      check(str_equal(email->sent_at, "2026-08-02T09:59:00Z"),
          "sentAt mismatch") &&
      check(str_equal(email->preview, "Short preview"),
          "preview mismatch") &&
      check(str_equal(header->name, "X-Test"), "header name mismatch") &&
      check(str_equal(header->value, "yes"), "header value mismatch") &&
      check(email->body_structure != NULL, "bodyStructure missing") &&
      check(clist_count(email->body_structure->sub_parts) == 2,
          "bodyStructure subParts mismatch") &&
      check(str_equal(mixed_alternative->type, "multipart/alternative"),
          "alternative body part type mismatch") &&
      check(str_equal(mixed_attachment->part_id, "p-att"),
          "mixed attachment part id mismatch") &&
      check(str_equal(mixed_attachment->cid, "note@example.com"),
          "mixed attachment cid mismatch") &&
      check(str_equal(mixed_attachment->language, "en"),
          "mixed attachment language mismatch") &&
      check(clist_count(mixed_attachment->languages) == 2,
          "mixed attachment language count mismatch") &&
      check(str_equal(mixed_attachment_second_language, "fr"),
          "mixed attachment second language mismatch") &&
      check(str_equal(mixed_attachment->location,
          "https://example.com/note.txt"),
          "mixed attachment location mismatch") &&
      check(clist_count(mixed_alternative->sub_parts) == 2,
          "alternative subParts mismatch") &&
      check(str_equal(alternative_text->part_id, "p-text"),
          "alternative text part id mismatch") &&
      check(str_equal(alternative_html->part_id, "p-html"),
          "alternative html part id mismatch") &&
      check(str_equal(text_part->part_id, "p-text"),
          "text body part mismatch") &&
      check(text_part->has_size == 1 && text_part->size == 12,
          "text body size mismatch") &&
      check(str_equal(html_part->part_id, "p-html"),
          "html body part mismatch") &&
      check(html_part->has_size == 1 && html_part->size == 20,
          "html body size mismatch") &&
      check(str_equal(attachment->name, "note.txt"),
          "attachment name mismatch") &&
      check(str_equal(attachment->cid, "note@example.com"),
          "attachment cid mismatch") &&
      check(str_equal(attachment->language, "en"),
          "attachment language mismatch") &&
      check(clist_count(attachment->languages) == 2,
          "attachment language count mismatch") &&
      check(str_equal(attachment_second_language, "fr"),
          "attachment second language mismatch") &&
      check(str_equal(attachment->location, "https://example.com/note.txt"),
          "attachment location mismatch") &&
      check(clist_count(email->body_values) == 2,
          "body value count mismatch") &&
      check(str_equal(body_value->part_id, "p-text"),
          "body value part id mismatch") &&
      check(str_equal(body_value->value, "body text"),
          "body value mismatch") &&
      check(body_value->is_truncated == 0,
          "body value truncation mismatch") &&
      check(str_equal(query_result->account_id, "acc1"),
          "query accountId mismatch") &&
      check(str_equal(query_result->query_state, "email-query-state"),
          "queryState mismatch") &&
      check(query_result->can_calculate_changes == 1,
          "canCalculateChanges mismatch") &&
      check(query_result->position == 0, "query position mismatch") &&
      check(query_result->has_total == 1, "query total presence mismatch") &&
      check(query_result->total == 2, "query total mismatch") &&
      check(str_equal(first_id, "e1"), "first email id mismatch") &&
      check(str_equal(second_id, "e2"), "second email id mismatch") &&
      check(str_equal(query_changes_result->account_id, "acc1"),
          "queryChanges accountId mismatch") &&
      check(str_equal(query_changes_result->old_query_state,
          "email-query-state"), "oldQueryState mismatch") &&
      check(str_equal(query_changes_result->new_query_state,
          "email-query-state-2"), "newQueryState mismatch") &&
      check(query_changes_result->has_total == 1,
          "queryChanges total presence mismatch") &&
      check(query_changes_result->total == 3,
          "queryChanges total mismatch") &&
      check(str_equal(query_removed, "e0"),
          "removed change id mismatch") &&
      check(str_equal(added_change->id, "e3"),
          "added change id mismatch") &&
      check(added_change->index == 2,
          "added change index mismatch") &&
      check(str_equal(changes_result->account_id, "acc1"),
          "changes accountId mismatch") &&
      check(str_equal(changes_result->old_state, "email-old"),
          "oldState mismatch") &&
      check(str_equal(changes_result->new_state, "email-new"),
          "newState mismatch") &&
      check(changes_result->has_more_changes == 0,
          "hasMoreChanges mismatch") &&
      check(str_equal(created, "e3"), "created mismatch") &&
      check(str_equal(updated, "e1"), "updated mismatch") &&
      check(str_equal(destroyed, "e0"), "destroyed mismatch") &&
      check(str_equal(import_result->account_id, "acc1"),
          "import accountId mismatch") &&
      check(str_equal(import_result->old_state, "email-old"),
          "import oldState mismatch") &&
      check(str_equal(import_result->new_state, "email-import-new"),
          "import newState mismatch") &&
      check(str_equal(import_created->creation_id, "import1"),
          "import creation id mismatch") &&
      check(str_equal(imported_email->id, "e-imported"),
          "imported email id mismatch") &&
      check(str_equal(imported_email->blob_id, "blob1"),
          "imported email blobId mismatch") &&
      check(str_equal(imported_email->thread_id, "t-imported"),
          "imported email threadId mismatch") &&
      check(imported_email->size == 321, "imported email size mismatch") &&
      check(str_equal(import_not_created->creation_id, "bad1"),
          "import notCreated creation id mismatch") &&
      check(str_equal(import_not_created->type, "invalidEmail"),
          "import notCreated type mismatch") &&
      check(str_equal(import_not_created->description, "bad input"),
          "import notCreated description mismatch") &&
      check(str_equal(parse_result->account_id, "acc1"),
          "parse accountId mismatch") &&
      check(clist_count(parse_result->parsed) == 1,
          "parse item count mismatch") &&
      check(str_equal(parsed_item->blob_id, "blob-parse"),
          "parse blob id mismatch") &&
      check(parsed_item->email->id == NULL, "parsed id should be null") &&
      check(str_equal(parsed_item->email->blob_id, "blob-parse"),
          "parsed email blobId mismatch") &&
      check(str_equal(parsed_item->email->thread_id, "t-parsed"),
          "parsed email threadId mismatch") &&
      check(str_equal(parsed_item->email->subject, "Parsed subject"),
          "parsed subject mismatch") &&
      check(str_equal(parsed_item->email->preview, "Parsed preview"),
          "parsed preview mismatch") &&
      check(str_equal(parsed_text_part->part_id, "parsed-text"),
          "parsed text part id mismatch") &&
      check(parsed_text_part->has_size == 1 &&
          parsed_text_part->size == 11, "parsed text part size mismatch") &&
      check(str_equal(parsed_body_value->part_id, "parsed-text"),
          "parsed body value part id mismatch") &&
      check(str_equal(parsed_body_value->value, "parsed body"),
          "parsed body value mismatch") &&
      check(str_equal(parse_not_parsable, "bad-blob"),
          "parse null notParsable mismatch") &&
      check(str_equal(parse_not_parsable_from_list, "bad-blob-2"),
          "parse notParsable list mismatch") &&
      check(str_equal(snippet_result->account_id, "acc1"),
          "snippet accountId mismatch") &&
      check(clist_count(snippet_result->list) == 1,
          "snippet count mismatch") &&
      check(str_equal(snippet->email_id, "e1"),
          "snippet emailId mismatch") &&
      check(str_equal(snippet->subject, "<mark>Hello</mark>"),
          "snippet subject mismatch") &&
      check(str_equal(snippet->preview, "preview <mark>hit</mark>"),
          "snippet preview mismatch") &&
      check(str_equal(snippet_not_found, "missing-snippet"),
          "snippet notFound mismatch") &&
      check(str_equal(set_result->account_id, "acc1"),
          "Email/set accountId mismatch") &&
      check(str_equal(set_result->old_state, "email-old"),
          "Email/set oldState mismatch") &&
      check(str_equal(set_result->new_state, "email-set-new"),
          "Email/set newState mismatch") &&
      check(str_equal(set_created->creation_id, "create1"),
          "Email/set created creation id mismatch") &&
      check(str_equal(set_created->id, "e-created"),
          "Email/set created id mismatch") &&
      check(str_equal(set_updated, "e1"), "Email/set updated mismatch") &&
      check(str_equal(set_destroyed, "e0"),
          "Email/set destroyed mismatch") &&
      check(str_equal(set_not_created->id, "badCreate"),
          "Email/set notCreated id mismatch") &&
      check(str_equal(set_not_created->type, "invalidProperties"),
          "Email/set notCreated type mismatch") &&
      check(str_equal(set_not_created->description, "missing body"),
          "Email/set notCreated description mismatch") &&
      check(str_equal(set_not_updated->id, "badUpdate"),
          "Email/set notUpdated id mismatch") &&
      check(str_equal(set_not_updated->type, "notFound"),
          "Email/set notUpdated type mismatch") &&
      check(set_not_updated->description == NULL,
          "Email/set notUpdated description should be null") &&
      check(str_equal(set_not_destroyed->id, "badDestroy"),
          "Email/set notDestroyed id mismatch") &&
      check(str_equal(set_not_destroyed->type, "forbidden"),
          "Email/set notDestroyed type mismatch") &&
      check(str_equal(set_not_destroyed->description, "cannot destroy"),
          "Email/set notDestroyed description mismatch") &&
      check(str_equal(copy_result->account_id, "acc1"),
          "Email/copy accountId mismatch") &&
      check(str_equal(copy_result->old_state, "email-set-new"),
          "Email/copy oldState mismatch") &&
      check(str_equal(copy_result->new_state, "email-copy-new"),
          "Email/copy newState mismatch") &&
      check(str_equal(copy_created->creation_id, "copy1"),
          "Email/copy created creation id mismatch") &&
      check(str_equal(copy_created->id, "e-copy"),
          "Email/copy created id mismatch") &&
      check(str_equal(copy_not_created->id, "badCopy"),
          "Email/copy notCreated id mismatch") &&
      check(str_equal(copy_not_created->type, "notFound"),
          "Email/copy notCreated type mismatch") &&
      check(str_equal(copy_not_created->description, "source missing"),
          "Email/copy notCreated description mismatch") &&
      check(str_equal(copy_destroyed, "e-source"),
          "Email/copy destroyed mismatch") &&
      check(str_equal(copy_not_destroyed->id, "blocked"),
          "Email/copy notDestroyed id mismatch") &&
      check(str_equal(copy_not_destroyed->type, "forbidden"),
          "Email/copy notDestroyed type mismatch") &&
      check(copy_not_destroyed->description == NULL,
          "Email/copy notDestroyed description should be null") &&
      check(str_equal(identity_result->account_id, "acc1"),
          "identity accountId mismatch") &&
      check(str_equal(identity_result->state, "identity-state"),
          "identity state mismatch") &&
      check(clist_count(identity_result->list) == 1,
          "identity count mismatch") &&
      check(str_equal(identity->id, "id1"), "identity id mismatch") &&
      check(str_equal(identity->name, "Example Sender"),
          "identity name mismatch") &&
      check(str_equal(identity->email, "sender@example.com"),
          "identity email mismatch") &&
      check(str_equal(identity->reply_to, "reply@example.com"),
          "identity replyTo mismatch") &&
      check(str_equal(identity->bcc, "archive@example.com"),
          "identity bcc mismatch") &&
      check(str_equal(identity->text_signature, "-- text"),
          "identity text signature mismatch") &&
      check(str_equal(identity->html_signature, "<p>html</p>"),
          "identity html signature mismatch") &&
      check(str_equal(identity_not_found, "missing-id"),
          "identity notFound mismatch") &&
      check(str_equal(submission_result->account_id, "acc1"),
          "submission accountId mismatch") &&
      check(str_equal(submission_result->old_state, "submission-old"),
          "submission oldState mismatch") &&
      check(str_equal(submission_result->new_state, "submission-new"),
          "submission newState mismatch") &&
      check(str_equal(submission_created->creation_id, "submit1"),
          "submission created creation id mismatch") &&
      check(str_equal(submission_created->id, "s1"),
          "submission created id mismatch") &&
      check(str_equal(submission_created->email_id, "e-imported"),
          "submission created emailId mismatch") &&
      check(str_equal(submission_created->identity_id, "id1"),
          "submission created identityId mismatch") &&
      check(str_equal(submission_created->thread_id, "t-submit"),
          "submission created threadId mismatch") &&
      check(str_equal(submission_created->send_at,
          "2026-08-02T11:00:00Z"),
          "submission created sendAt mismatch") &&
      check(str_equal(submission_created->undo_status, "pending"),
          "submission created undoStatus mismatch") &&
      check(submission_mail_from != NULL,
          "submission created mailFrom missing") &&
      check(submission_rcpt_to != NULL,
          "submission created rcptTo missing") &&
      check(str_equal(submission_mail_from->email, "sender@example.com"),
          "submission created mailFrom mismatch") &&
      check(str_equal(submission_rcpt_to->email, "to@example.com"),
          "submission created rcptTo mismatch") &&
      check(submission_delivery_status != NULL,
          "submission created deliveryStatus missing") &&
      check(str_equal(submission_delivery_status->email,
          "to@example.com"),
          "submission created deliveryStatus email mismatch") &&
      check(str_equal(submission_delivery_status->smtp_reply,
          "250 2.0.0 queued"),
          "submission created deliveryStatus smtpReply mismatch") &&
      check(str_equal(submission_delivery_status->delivered, "queued"),
          "submission created deliveryStatus delivered mismatch") &&
      check(str_equal(submission_delivery_status->displayed, "unknown"),
          "submission created deliveryStatus displayed mismatch") &&
      check(str_equal(submission_dsn_blob, "dsn-blob"),
          "submission created dsnBlobIds mismatch") &&
      check(str_equal(submission_updated, "s2"),
          "submission updated mismatch") &&
      check(str_equal(submission_destroyed, "s0"),
          "submission destroyed mismatch") &&
      check(str_equal(submission_not_created->id, "badSubmit"),
          "submission notCreated id mismatch") &&
      check(str_equal(submission_not_created->type, "invalidEmail"),
          "submission notCreated type mismatch") &&
      check(str_equal(submission_not_created->description, "cannot submit"),
          "submission notCreated description mismatch") &&
      check(str_equal(submission_not_updated->id, "badUpdate"),
          "submission notUpdated id mismatch") &&
      check(str_equal(submission_not_updated->type, "notFound"),
          "submission notUpdated type mismatch") &&
      check(submission_not_updated->description == NULL,
          "submission notUpdated description should be null") &&
      check(str_equal(submission_not_destroyed->id, "badDestroy"),
          "submission notDestroyed id mismatch") &&
      check(str_equal(submission_not_destroyed->type, "forbidden"),
          "submission notDestroyed type mismatch") &&
      check(submission_not_destroyed->description == NULL,
          "submission notDestroyed description should be null");

 cleanup:
  mailjmap_set_result_free(submission_result);
  mailjmap_email_submission_set_item_free(submission_update_item);
  mailjmap_email_submission_set_item_free(submission_create_item);
  clist_free(submission_destroy);
  email_submission_set_item_list_free(submission_update);
  email_submission_set_item_list_free(submission_create);
  mailjmap_identity_get_result_free(identity_result);
  mailjmap_set_result_free(copy_result);
  mailjmap_email_copy_item_free(copy_item);
  email_copy_item_list_free(email_copy_create);
  mailjmap_email_query_filter_free(query_filter_child);
  mailjmap_email_query_filter_free(query_filter);
  mailjmap_email_query_sort_comparator_free(sort_comparator);
  email_query_sort_comparator_list_free(query_sort);
  mailjmap_set_result_free(set_result);
  mailjmap_email_set_item_free(update_item);
  clist_free(email_set_destroy);
  email_set_item_list_free(email_set_update);
  email_set_item_list_free(email_set_create);
  clist_free(import_keywords);
  clist_free(import_mailbox_ids);
  clist_free(parse_body_properties);
  clist_free(parse_properties);
  clist_free(parse_blob_ids);
  clist_free(snippet_email_ids);
  mailjmap_search_snippet_get_result_free(snippet_result);
  mailjmap_email_parse_result_free(parse_result);
  mailjmap_import_result_free(import_result);
  mailjmap_changes_result_free(changes_result);
  mailjmap_query_changes_result_free(query_changes_result);
  mailjmap_query_result_free(query_result);
  mailjmap_email_get_result_free(get_result);
  mailjmap_session_free(session_object);
  mailjmap_free(client);
  fake_context_clear(&context);
  free(email_get_fixture);
  return ok;
}

int main(void)
{
  return test_email_query_and_changes() ? 0 : 1;
}
