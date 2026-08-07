/*
 * Low-level JMAP smoke sample.
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include <libetpan/mailjmap.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static unsigned int list_count(clist * list)
{
  clistiter * cur;
  unsigned int count;

  count = 0;
  for (cur = list != NULL ? clist_begin(list) : NULL;
      cur != NULL; cur = clist_next(cur))
    count ++;

  return count;
}

static int string_list_append_dup(clist * list, const char * value)
{
  char * dup_value;

  if ((list == NULL) || (value == NULL))
    return -1;

  dup_value = strdup(value);
  if (dup_value == NULL)
    return -1;
  if (clist_append(list, dup_value) < 0) {
    free(dup_value);
    return -1;
  }

  return 0;
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

static void mailbox_set_item_list_free(clist * list)
{
  clistiter * cur;

  if (list == NULL)
    return;
  for (cur = clist_begin(list); cur != NULL; cur = clist_next(cur))
    mailjmap_mailbox_set_item_free(clist_content(cur));
  clist_free(list);
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

static void email_submission_item_list_free(clist * list)
{
  clistiter * cur;

  if (list == NULL)
    return;
  for (cur = clist_begin(list); cur != NULL; cur = clist_next(cur))
    mailjmap_email_submission_set_item_free(clist_content(cur));
  clist_free(list);
}

static char * dup_string_or_null(const char * value)
{
  if (value == NULL)
    return NULL;
  return strdup(value);
}

static int string_list_contains(clist * list, const char * value)
{
  clistiter * cur;

  if ((list == NULL) || (value == NULL))
    return 0;
  for (cur = clist_begin(list); cur != NULL; cur = clist_next(cur)) {
    const char * item;

    item = clist_content(cur);
    if ((item != NULL) && (strcmp(item, value) == 0))
      return 1;
  }
  return 0;
}

static void print_error(mailjmap * session, const char * step, int r)
{
  fprintf(stderr, "%s failed: %d", step, r);
  if (session != NULL) {
    const char * message;
    const char * problem_type;
    const char * method_name;
    const char * method_type;

    message = mailjmap_get_last_error_message(session);
    problem_type = mailjmap_get_last_problem_type(session);
    method_name = mailjmap_get_last_method_name(session);
    method_type = mailjmap_get_last_method_error_type(session);

    fprintf(stderr, " http=%d", mailjmap_get_last_http_status(session));
    if (problem_type != NULL)
      fprintf(stderr, " problem=%s", problem_type);
    if (method_name != NULL)
      fprintf(stderr, " method=%s", method_name);
    if (method_type != NULL)
      fprintf(stderr, " methodError=%s", method_type);
    if (message != NULL)
      fprintf(stderr, " message=%s", message);
  }
  fprintf(stderr, "\n");
}

static const char * find_primary_mail_account(struct mailjmap_session * session)
{
  clistiter * cur;

  if (session == NULL)
    return NULL;

  for (cur = session->primary_accounts != NULL ?
      clist_begin(session->primary_accounts) : NULL;
      cur != NULL; cur = clist_next(cur)) {
    struct mailjmap_session_primary_account * primary;

    primary = clist_content(cur);
    if ((primary->capability != NULL) &&
        (strcmp(primary->capability, "urn:ietf:params:jmap:mail") == 0))
      return primary->account_id;
  }

  for (cur = session->accounts != NULL ? clist_begin(session->accounts) : NULL;
      cur != NULL; cur = clist_next(cur)) {
    struct mailjmap_session_account * account;
    clistiter * cap_cur;

    account = clist_content(cur);
    for (cap_cur = account->capabilities != NULL ?
        clist_begin(account->capabilities) : NULL;
        cap_cur != NULL; cap_cur = clist_next(cap_cur)) {
      const char * capability;

      capability = clist_content(cap_cur);
      if ((capability != NULL) &&
          (strcmp(capability, "urn:ietf:params:jmap:mail") == 0))
        return account->account_id;
    }
  }

  return NULL;
}

static const char * find_first_email_id(struct mailjmap_query_result * result)
{
  clistiter * cur;

  for (cur = (result != NULL) && (result->ids != NULL) ?
      clist_begin(result->ids) : NULL;
      cur != NULL; cur = clist_next(cur)) {
    const char * id;

    id = clist_content(cur);
    if ((id != NULL) && (* id != '\0'))
      return id;
  }

  return NULL;
}

static int fetch_one_email(mailjmap * session, const char * account_id,
    const char * email_id)
{
  struct mailjmap_email_get_result * result;
  clist * ids;
  clist * properties;
  clistiter * cur;
  int r;
  int ok;

  result = NULL;
  ids = clist_new();
  properties = clist_new();
  if ((ids == NULL) || (properties == NULL))
    goto err;

  if ((string_list_append_dup(ids, email_id) < 0) ||
      (string_list_append_dup(properties, "id") < 0) ||
      (string_list_append_dup(properties, "threadId") < 0) ||
      (string_list_append_dup(properties, "subject") < 0) ||
      (string_list_append_dup(properties, "preview") < 0) ||
      (string_list_append_dup(properties, "bodyStructure") < 0))
    goto err;

  r = mailjmap_email_get(session, account_id, ids, properties, &result);
  if (r != MAILJMAP_NO_ERROR) {
    print_error(session, "Email/get", r);
    goto err;
  }

  ok = 1;
  for (cur = result->list != NULL ? clist_begin(result->list) : NULL;
      cur != NULL; cur = clist_next(cur)) {
    struct mailjmap_email * email;

    email = clist_content(cur);
    printf("email id=%s thread=%s subject=%s\n",
        email->id != NULL ? email->id : "",
        email->thread_id != NULL ? email->thread_id : "",
        email->subject != NULL ? email->subject : "");
    if (email->preview != NULL)
      printf("preview: %.160s\n", email->preview);
  }

  mailjmap_email_get_result_free(result);
  string_list_free(properties);
  string_list_free(ids);
  return ok ? 0 : 1;

 err:
  mailjmap_email_get_result_free(result);
  string_list_free(properties);
  string_list_free(ids);
  return 1;
}

static struct mailjmap_identity * first_identity(
    struct mailjmap_identity_get_result * result)
{
  clistiter * cur;

  for (cur = (result != NULL) && (result->list != NULL) ?
      clist_begin(result->list) : NULL;
      cur != NULL; cur = clist_next(cur)) {
    struct mailjmap_identity * identity;

    identity = clist_content(cur);
    if ((identity->id != NULL) && (identity->email != NULL))
      return identity;
  }

  return NULL;
}

static const char * find_imported_email_id(struct mailjmap_import_result * result,
    const char * creation_id)
{
  clistiter * cur;

  for (cur = (result != NULL) && (result->created != NULL) ?
      clist_begin(result->created) : NULL;
      cur != NULL; cur = clist_next(cur)) {
    struct mailjmap_import_created * created;

    created = clist_content(cur);
    if ((created->creation_id != NULL) &&
        (strcmp(created->creation_id, creation_id) == 0) &&
        (created->email != NULL))
      return created->email->id;
  }

  return NULL;
}

static const char * find_imported_thread_id(
    struct mailjmap_import_result * result, const char * creation_id)
{
  clistiter * cur;

  for (cur = (result != NULL) && (result->created != NULL) ?
      clist_begin(result->created) : NULL;
      cur != NULL; cur = clist_next(cur)) {
    struct mailjmap_import_created * created;

    created = clist_content(cur);
    if ((created->creation_id != NULL) &&
        (strcmp(created->creation_id, creation_id) == 0) &&
        (created->email != NULL))
      return created->email->thread_id;
  }

  return NULL;
}

static const char * find_created_id(struct mailjmap_set_result * result,
    const char * creation_id)
{
  clistiter * cur;

  for (cur = (result != NULL) && (result->created != NULL) ?
      clist_begin(result->created) : NULL;
      cur != NULL; cur = clist_next(cur)) {
    struct mailjmap_set_created * created;

    created = clist_content(cur);
    if ((created->creation_id != NULL) &&
        (strcmp(created->creation_id, creation_id) == 0))
      return created->id;
  }

  return NULL;
}

static char * build_marked_raw_message(const char * from, const char * to,
    const char * subject, const char * marker)
{
  static const char body[] =
      "From: %s\r\n"
      "To: %s\r\n"
      "Subject: %s\r\n"
      "Message-ID: <%s@example.libetpan.invalid>\r\n"
      "Content-Type: text/plain; charset=utf-8\r\n"
      "\r\n"
      "This message was generated by tests/jmap-sample.c.\r\n"
      "Search marker: %s\r\n";
  char * message;
  int len;

  len = snprintf(NULL, 0, body, from, to, subject, marker, marker);
  if (len < 0)
    return NULL;

  message = malloc((size_t) len + 1);
  if (message == NULL)
    return NULL;
  snprintf(message, (size_t) len + 1, body, from, to, subject, marker,
      marker);
  return message;
}

static char * build_raw_message(const char * from, const char * to)
{
  static const char body[] =
      "From: %s\r\n"
      "To: %s\r\n"
      "Subject: libetpan JMAP smoke test\r\n"
      "Content-Type: text/plain; charset=utf-8\r\n"
      "\r\n"
      "This message was sent by tests/jmap-sample.c.\r\n";
  char * message;
  int len;

  len = snprintf(NULL, 0, body, from, to);
  if (len < 0)
    return NULL;

  message = malloc((size_t) len + 1);
  if (message == NULL)
    return NULL;
  snprintf(message, (size_t) len + 1, body, from, to);
  return message;
}

static int get_email_state(mailjmap * session, const char * account_id,
    const char * email_id, char ** result)
{
  struct mailjmap_email_get_result * get_result;
  clist * ids;
  int r;

  get_result = NULL;
  ids = clist_new();
  if (ids == NULL)
    return MAILJMAP_ERROR_MEMORY;
  if ((email_id != NULL) && (string_list_append_dup(ids, email_id) < 0)) {
    string_list_free(ids);
    return MAILJMAP_ERROR_MEMORY;
  }

  r = mailjmap_email_get(session, account_id, ids, NULL, &get_result);
  if (r == MAILJMAP_NO_ERROR) {
    * result = dup_string_or_null(get_result->state);
    if (* result == NULL)
      r = MAILJMAP_ERROR_MEMORY;
  }

  mailjmap_email_get_result_free(get_result);
  string_list_free(ids);
  return r;
}

static int get_thread_state(mailjmap * session, const char * account_id,
    char ** result)
{
  struct mailjmap_thread_get_result * get_result;
  clist * ids;
  int r;

  get_result = NULL;
  ids = clist_new();
  if (ids == NULL)
    return MAILJMAP_ERROR_MEMORY;

  r = mailjmap_thread_get(session, account_id, ids, &get_result);
  if (r == MAILJMAP_NO_ERROR) {
    * result = dup_string_or_null(get_result->state);
    if (* result == NULL)
      r = MAILJMAP_ERROR_MEMORY;
  }

  mailjmap_thread_get_result_free(get_result);
  string_list_free(ids);
  return r;
}

static void best_effort_destroy_emails(mailjmap * session,
    const char * account_id, const char * email_id, const char * copied_id)
{
  struct mailjmap_set_result * set_result;
  clist * destroy;

  set_result = NULL;
  destroy = clist_new();
  if (destroy == NULL)
    return;

  if (((copied_id != NULL) && (string_list_append_dup(destroy, copied_id) < 0)) ||
      ((email_id != NULL) && (string_list_append_dup(destroy, email_id) < 0)))
    goto cleanup;

  if (list_count(destroy) > 0)
    (void) mailjmap_email_set(session, account_id, NULL, NULL, NULL, destroy,
        &set_result);

 cleanup:
  string_list_free(destroy);
  mailjmap_set_result_free(set_result);
}

static void best_effort_destroy_mailbox(mailjmap * session,
    const char * account_id, const char * mailbox_id)
{
  struct mailjmap_set_result * set_result;
  clist * destroy;

  if (mailbox_id == NULL)
    return;

  set_result = NULL;
  destroy = clist_new();
  if (destroy == NULL)
    return;
  if (string_list_append_dup(destroy, mailbox_id) < 0)
    goto cleanup;

  (void) mailjmap_mailbox_set(session, account_id, NULL, NULL, NULL,
      destroy, &set_result);

 cleanup:
  string_list_free(destroy);
  mailjmap_set_result_free(set_result);
}

static int full_live_self_test(mailjmap * session, const char * account_id,
    const char * send_to)
{
  struct mailjmap_mailbox_get_result * mailbox_get;
  struct mailjmap_query_result * mailbox_query;
  struct mailjmap_changes_result * mailbox_changes;
  struct mailjmap_identity_get_result * identities;
  struct mailjmap_identity * identity;
  struct mailjmap_blob_upload * upload;
  struct mailjmap_email_parse_result * parse_result;
  struct mailjmap_query_result * initial_search;
  struct mailjmap_query_result * search;
  struct mailjmap_query_changes_result * query_changes;
  struct mailjmap_import_result * import_result;
  struct mailjmap_email_get_result * imported_get;
  struct mailjmap_thread_get_result * thread_get;
  struct mailjmap_changes_result * thread_changes;
  struct mailjmap_changes_result * email_changes;
  struct mailjmap_search_snippet_get_result * snippet_result;
  struct mailjmap_set_result * mailbox_set_result;
  struct mailjmap_set_result * email_set_result;
  struct mailjmap_set_result * copy_result;
  struct mailjmap_set_result * submit_result;
  clist * create;
  clist * update;
  clist * destroy;
  clist * ids;
  clist * properties;
  clist * body_properties;
  clist * mailbox_ids;
  clist * keywords;
  struct mailjmap_mailbox_set_item * mailbox_item;
  struct mailjmap_email_set_item * email_update_item;
  struct mailjmap_email_copy_item * copy_item;
  struct mailjmap_email_submission_set_item * submit_item;
  char marker[96];
  char mailbox_name[128];
  char mailbox_renamed[128];
  char subject[160];
  char * raw_message;
  char * mailbox_state;
  char * mailbox_created_state;
  char * email_state;
  char * thread_state;
  char * temp_mailbox_id;
  char * imported_email_id;
  char * imported_thread_id;
  char * copied_email_id;
  const char * value;
  int r;
  int status;
  int i;
  int create_kind;
  int update_kind;

  mailbox_get = NULL;
  mailbox_query = NULL;
  mailbox_changes = NULL;
  identities = NULL;
  upload = NULL;
  parse_result = NULL;
  initial_search = NULL;
  search = NULL;
  query_changes = NULL;
  import_result = NULL;
  imported_get = NULL;
  thread_get = NULL;
  thread_changes = NULL;
  email_changes = NULL;
  snippet_result = NULL;
  mailbox_set_result = NULL;
  email_set_result = NULL;
  copy_result = NULL;
  submit_result = NULL;
  create = NULL;
  update = NULL;
  destroy = NULL;
  ids = NULL;
  properties = NULL;
  body_properties = NULL;
  mailbox_ids = NULL;
  keywords = NULL;
  mailbox_item = NULL;
  email_update_item = NULL;
  copy_item = NULL;
  submit_item = NULL;
  raw_message = NULL;
  mailbox_state = NULL;
  mailbox_created_state = NULL;
  email_state = NULL;
  thread_state = NULL;
  temp_mailbox_id = NULL;
  imported_email_id = NULL;
  imported_thread_id = NULL;
  copied_email_id = NULL;
  status = 1;
  create_kind = 0;
  update_kind = 0;

  if ((send_to == NULL) || (* send_to == '\0')) {
    fprintf(stderr,
        "Set JMAP_SEND_TO to run full live JMAP submission coverage\n");
    return 1;
  }

  snprintf(marker, sizeof(marker), "libetpan-jmap-live-%ld-%ld",
      (long) time(NULL), (long) getpid());
  snprintf(mailbox_name, sizeof(mailbox_name), "libetpan live %s", marker);
  snprintf(mailbox_renamed, sizeof(mailbox_renamed),
      "libetpan live renamed %s", marker);
  snprintf(subject, sizeof(subject), "libetpan JMAP live %s", marker);

  printf("full live marker: %s\n", marker);

  r = mailjmap_mailbox_get(session, account_id, NULL, NULL, &mailbox_get);
  if (r != MAILJMAP_NO_ERROR) {
    print_error(session, "full Mailbox/get", r);
    goto cleanup;
  }
  mailbox_state = dup_string_or_null(mailbox_get->state);
  if (mailbox_state == NULL)
    goto cleanup;

  r = mailjmap_mailbox_query(session, account_id, 0, 100, &mailbox_query);
  if (r != MAILJMAP_NO_ERROR) {
    print_error(session, "Mailbox/query", r);
    goto cleanup;
  }
  printf("Mailbox/query ids=%u\n", list_count(mailbox_query->ids));

  r = mailjmap_mailbox_changes(session, account_id, mailbox_state,
      &mailbox_changes);
  if (r != MAILJMAP_NO_ERROR) {
    print_error(session, "Mailbox/changes initial", r);
    goto cleanup;
  }
  printf("Mailbox/changes initial created=%u updated=%u destroyed=%u\n",
      list_count(mailbox_changes->created), list_count(mailbox_changes->updated),
      list_count(mailbox_changes->destroyed));
  mailjmap_changes_result_free(mailbox_changes);
  mailbox_changes = NULL;

  r = mailjmap_identity_get(session, account_id, NULL, NULL, &identities);
  if (r != MAILJMAP_NO_ERROR) {
    print_error(session, "Identity/get", r);
    goto cleanup;
  }
  identity = first_identity(identities);
  if (identity == NULL) {
    fprintf(stderr, "Identity/get returned no usable identity\n");
    goto cleanup;
  }

  r = get_email_state(session, account_id, NULL, &email_state);
  if (r != MAILJMAP_NO_ERROR) {
    print_error(session, "Email/get initial sync state", r);
    goto cleanup;
  }
  r = get_thread_state(session, account_id, &thread_state);
  if (r != MAILJMAP_NO_ERROR) {
    print_error(session, "Thread/get initial sync state", r);
    goto cleanup;
  }

  create = clist_new();
  create_kind = 1;
  mailbox_item = mailjmap_mailbox_set_item_new("livebox");
  if ((create == NULL) || (mailbox_item == NULL))
    goto cleanup;
  r = mailjmap_mailbox_set_item_set_name(mailbox_item, mailbox_name);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  mailjmap_mailbox_set_item_set_parent_id_null(mailbox_item);
  mailjmap_mailbox_set_item_set_role_null(mailbox_item);
  mailjmap_mailbox_set_item_set_is_subscribed(mailbox_item, 0);
  if (clist_append(create, mailbox_item) < 0)
    goto cleanup;
  mailbox_item = NULL;
  r = mailjmap_mailbox_set(session, account_id, NULL, create, NULL, NULL,
      &mailbox_set_result);
  if (r != MAILJMAP_NO_ERROR) {
    print_error(session, "Mailbox/set create", r);
    goto cleanup;
  }
  value = find_created_id(mailbox_set_result, "livebox");
  if (value == NULL) {
    fprintf(stderr, "Mailbox/set did not create livebox\n");
    goto cleanup;
  }
  temp_mailbox_id = dup_string_or_null(value);
  mailbox_created_state = dup_string_or_null(mailbox_set_result->new_state);
  if ((temp_mailbox_id == NULL) || (mailbox_created_state == NULL))
    goto cleanup;
  printf("Mailbox/set created %s\n", temp_mailbox_id);
  mailbox_set_item_list_free(create);
  create = NULL;
  create_kind = 0;
  mailjmap_set_result_free(mailbox_set_result);
  mailbox_set_result = NULL;

  r = mailjmap_mailbox_changes(session, account_id, mailbox_state,
      &mailbox_changes);
  if (r != MAILJMAP_NO_ERROR) {
    print_error(session, "Mailbox/changes after create", r);
    goto cleanup;
  }
  if (!string_list_contains(mailbox_changes->created, temp_mailbox_id)) {
    fprintf(stderr, "Mailbox/changes did not report created test mailbox\n");
    goto cleanup;
  }
  mailjmap_changes_result_free(mailbox_changes);
  mailbox_changes = NULL;

  update = clist_new();
  update_kind = 1;
  mailbox_item = mailjmap_mailbox_set_item_new(temp_mailbox_id);
  if ((update == NULL) || (mailbox_item == NULL))
    goto cleanup;
  r = mailjmap_mailbox_set_item_set_name(mailbox_item, mailbox_renamed);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  if (clist_append(update, mailbox_item) < 0)
    goto cleanup;
  mailbox_item = NULL;
  r = mailjmap_mailbox_set(session, account_id, NULL, NULL, update, NULL,
      &mailbox_set_result);
  if (r != MAILJMAP_NO_ERROR) {
    print_error(session, "Mailbox/set update", r);
    goto cleanup;
  }
  mailbox_set_item_list_free(update);
  update = NULL;
  update_kind = 0;
  mailjmap_set_result_free(mailbox_set_result);
  mailbox_set_result = NULL;

  r = mailjmap_mailbox_changes(session, account_id, mailbox_created_state,
      &mailbox_changes);
  if (r != MAILJMAP_NO_ERROR) {
    print_error(session, "Mailbox/changes after update", r);
    goto cleanup;
  }
  if (!string_list_contains(mailbox_changes->updated, temp_mailbox_id)) {
    fprintf(stderr, "Mailbox/changes did not report updated test mailbox\n");
    goto cleanup;
  }
  mailjmap_changes_result_free(mailbox_changes);
  mailbox_changes = NULL;

  raw_message = build_marked_raw_message(identity->email, send_to, subject,
      marker);
  if (raw_message == NULL)
    goto cleanup;
  r = mailjmap_upload(session, account_id, "message/rfc822", raw_message,
      strlen(raw_message), &upload);
  if (r != MAILJMAP_NO_ERROR) {
    print_error(session, "upload", r);
    goto cleanup;
  }

  ids = clist_new();
  properties = clist_new();
  body_properties = clist_new();
  if ((ids == NULL) || (properties == NULL) || (body_properties == NULL))
    goto cleanup;
  if ((string_list_append_dup(ids, upload->blob_id) < 0) ||
      (string_list_append_dup(properties, "subject") < 0) ||
      (string_list_append_dup(properties, "preview") < 0) ||
      (string_list_append_dup(body_properties, "partId") < 0) ||
      (string_list_append_dup(body_properties, "type") < 0))
    goto cleanup;
  r = mailjmap_email_parse(session, account_id, ids, properties,
      body_properties, &parse_result);
  if (r != MAILJMAP_NO_ERROR) {
    print_error(session, "Email/parse", r);
    goto cleanup;
  }
  if (list_count(parse_result->parsed) == 0) {
    fprintf(stderr, "Email/parse returned no parsed item\n");
    goto cleanup;
  }
  printf("Email/parse parsed=%u\n", list_count(parse_result->parsed));
  string_list_free(ids);
  string_list_free(properties);
  string_list_free(body_properties);
  ids = NULL;
  properties = NULL;
  body_properties = NULL;

  r = mailjmap_email_query_with_text_filter(session, account_id, marker, 0,
      10, &initial_search);
  if (r != MAILJMAP_NO_ERROR) {
    print_error(session, "Email/query initial marker search", r);
    goto cleanup;
  }

  mailbox_ids = clist_new();
  keywords = clist_new();
  if ((mailbox_ids == NULL) || (keywords == NULL))
    goto cleanup;
  if ((string_list_append_dup(mailbox_ids, temp_mailbox_id) < 0) ||
      (string_list_append_dup(keywords, "$seen") < 0))
    goto cleanup;
  r = mailjmap_email_import(session, account_id, "liveimport",
      upload->blob_id, mailbox_ids, keywords, NULL, &import_result);
  if (r != MAILJMAP_NO_ERROR) {
    print_error(session, "Email/import", r);
    goto cleanup;
  }
  value = find_imported_email_id(import_result, "liveimport");
  if (value == NULL) {
    fprintf(stderr, "Email/import did not return a created email id\n");
    goto cleanup;
  }
  imported_email_id = dup_string_or_null(value);
  imported_thread_id = dup_string_or_null(
      find_imported_thread_id(import_result, "liveimport"));
  if ((imported_email_id == NULL) || (imported_thread_id == NULL))
    goto cleanup;
  printf("Email/import created %s thread=%s\n", imported_email_id,
      imported_thread_id);

  for (i = 0; i < 10; i ++) {
    mailjmap_query_result_free(search);
    search = NULL;
    r = mailjmap_email_query_with_text_filter(session, account_id, marker,
        0, 10, &search);
    if (r != MAILJMAP_NO_ERROR) {
      print_error(session, "Email/query marker search", r);
      goto cleanup;
    }
    if (string_list_contains(search->ids, imported_email_id))
      break;
    sleep(1);
  }
  if ((search == NULL) || !string_list_contains(search->ids,
      imported_email_id)) {
    fprintf(stderr, "Email/query did not find imported marker email\n");
    goto cleanup;
  }
  printf("Email/query marker ids=%u\n", list_count(search->ids));

  r = mailjmap_email_query_changes_with_text_filter(session, account_id,
      initial_search->query_state, marker, &query_changes);
  if (r != MAILJMAP_NO_ERROR) {
    print_error(session, "Email/queryChanges marker search", r);
    goto cleanup;
  }
  printf("Email/queryChanges added=%u removed=%u\n",
      list_count(query_changes->added), list_count(query_changes->removed));

  r = mailjmap_email_changes(session, account_id, email_state,
      &email_changes);
  if (r != MAILJMAP_NO_ERROR) {
    print_error(session, "Email/changes after import", r);
    goto cleanup;
  }
  if (!string_list_contains(email_changes->created, imported_email_id)) {
    fprintf(stderr, "Email/changes did not report imported email\n");
    goto cleanup;
  }
  free(email_state);
  email_state = dup_string_or_null(email_changes->new_state);
  if (email_state == NULL)
    goto cleanup;
  mailjmap_changes_result_free(email_changes);
  email_changes = NULL;

  ids = clist_new();
  if (ids == NULL)
    goto cleanup;
  if (string_list_append_dup(ids, imported_email_id) < 0)
    goto cleanup;
  r = mailjmap_email_get(session, account_id, ids, NULL, &imported_get);
  if (r != MAILJMAP_NO_ERROR) {
    print_error(session, "Email/get imported", r);
    goto cleanup;
  }
  printf("Email/get imported count=%u\n", list_count(imported_get->list));
  string_list_free(ids);
  ids = NULL;

  ids = clist_new();
  if ((ids == NULL) || (string_list_append_dup(ids, imported_email_id) < 0))
    goto cleanup;
  r = mailjmap_search_snippet_get_with_text_filter(session, account_id, ids,
      marker, &snippet_result);
  if (r != MAILJMAP_NO_ERROR) {
    print_error(session, "SearchSnippet/get marker", r);
    goto cleanup;
  }
  printf("SearchSnippet/get snippets=%u\n", list_count(snippet_result->list));
  string_list_free(ids);
  ids = NULL;

  ids = clist_new();
  if ((ids == NULL) || (string_list_append_dup(ids, imported_thread_id) < 0))
    goto cleanup;
  r = mailjmap_thread_get(session, account_id, ids, &thread_get);
  if (r != MAILJMAP_NO_ERROR) {
    print_error(session, "Thread/get imported", r);
    goto cleanup;
  }
  printf("Thread/get threads=%u\n", list_count(thread_get->list));
  string_list_free(ids);
  ids = NULL;

  r = mailjmap_thread_changes(session, account_id, thread_state,
      &thread_changes);
  if (r != MAILJMAP_NO_ERROR) {
    print_error(session, "Thread/changes after import", r);
    goto cleanup;
  }
  printf("Thread/changes created=%u updated=%u destroyed=%u\n",
      list_count(thread_changes->created), list_count(thread_changes->updated),
      list_count(thread_changes->destroyed));

  update = clist_new();
  update_kind = 2;
  email_update_item = mailjmap_email_set_item_new(imported_email_id);
  if ((update == NULL) || (email_update_item == NULL))
    goto cleanup;
  r = mailjmap_email_set_item_add_mailbox_id(email_update_item,
      temp_mailbox_id);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = mailjmap_email_set_item_add_keyword(email_update_item, "$seen");
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = mailjmap_email_set_item_add_keyword(email_update_item, "$flagged");
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  if (clist_append(update, email_update_item) < 0)
    goto cleanup;
  email_update_item = NULL;
  r = mailjmap_email_set(session, account_id, NULL, NULL, update, NULL,
      &email_set_result);
  if (r != MAILJMAP_NO_ERROR) {
    print_error(session, "Email/set update", r);
    goto cleanup;
  }
  if (!string_list_contains(email_set_result->updated, imported_email_id)) {
    fprintf(stderr, "Email/set did not update imported email\n");
    goto cleanup;
  }
  email_set_item_list_free(update);
  update = NULL;
  update_kind = 0;
  mailjmap_set_result_free(email_set_result);
  email_set_result = NULL;

  r = mailjmap_email_changes(session, account_id, email_state,
      &email_changes);
  if (r != MAILJMAP_NO_ERROR) {
    print_error(session, "Email/changes after update", r);
    goto cleanup;
  }
  if (!string_list_contains(email_changes->updated, imported_email_id)) {
    fprintf(stderr, "Email/changes did not report updated email\n");
    goto cleanup;
  }
  mailjmap_changes_result_free(email_changes);
  email_changes = NULL;

  create = clist_new();
  create_kind = 3;
  copy_item = mailjmap_email_copy_item_new("livecopy", imported_email_id);
  if ((create == NULL) || (copy_item == NULL))
    goto cleanup;
  r = mailjmap_email_copy_item_add_mailbox_id(copy_item, temp_mailbox_id);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = mailjmap_email_copy_item_add_keyword(copy_item, "$seen");
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  if (clist_append(create, copy_item) < 0)
    goto cleanup;
  copy_item = NULL;
  r = mailjmap_email_copy(session, account_id, account_id, NULL, create, 0,
      &copy_result);
  if (r != MAILJMAP_NO_ERROR) {
    const char * method_type;

    method_type = mailjmap_get_last_method_error_type(session);
    if ((r != MAILJMAP_ERROR_METHOD) || (method_type == NULL) ||
        (strcmp(method_type, "invalidArguments") != 0)) {
      print_error(session, "Email/copy", r);
      goto cleanup;
    }
    printf("Email/copy same-account copy unsupported by server\n");
  }
  else {
    value = find_created_id(copy_result, "livecopy");
    if (value == NULL) {
      fprintf(stderr, "Email/copy did not create copied email\n");
      goto cleanup;
    }
    copied_email_id = dup_string_or_null(value);
    if (copied_email_id == NULL)
      goto cleanup;
    printf("Email/copy created %s\n", copied_email_id);
  }
  email_copy_item_list_free(create);
  create = NULL;
  create_kind = 0;
  mailjmap_set_result_free(copy_result);
  copy_result = NULL;

  create = clist_new();
  create_kind = 4;
  submit_item = mailjmap_email_submission_set_item_new("livesubmit");
  if ((create == NULL) || (submit_item == NULL))
    goto cleanup;
  r = mailjmap_email_submission_set_item_set_email_id(submit_item,
      imported_email_id);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = mailjmap_email_submission_set_item_set_identity_id(submit_item,
      identity->id);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  if (clist_append(create, submit_item) < 0)
    goto cleanup;
  submit_item = NULL;
  r = mailjmap_email_submission_set(session, account_id, NULL, create, NULL,
      NULL, &submit_result);
  if (r != MAILJMAP_NO_ERROR) {
    print_error(session, "EmailSubmission/set", r);
    goto cleanup;
  }
  if (list_count(submit_result->created) == 0) {
    fprintf(stderr, "EmailSubmission/set did not create a submission\n");
    goto cleanup;
  }
  printf("EmailSubmission/set sent to %s\n", send_to);
  email_submission_item_list_free(create);
  create = NULL;
  create_kind = 0;
  mailjmap_set_result_free(submit_result);
  submit_result = NULL;

  destroy = clist_new();
  if (destroy == NULL)
    goto cleanup;
  if (((copied_email_id != NULL) &&
      (string_list_append_dup(destroy, copied_email_id) < 0)) ||
      (string_list_append_dup(destroy, imported_email_id) < 0))
    goto cleanup;
  r = mailjmap_email_set(session, account_id, NULL, NULL, NULL, destroy,
      &email_set_result);
  if (r != MAILJMAP_NO_ERROR) {
    print_error(session, "Email/set destroy", r);
    goto cleanup;
  }
  printf("Email/set destroyed generated emails=%u\n",
      list_count(email_set_result->destroyed));
  string_list_free(destroy);
  destroy = NULL;
  mailjmap_set_result_free(email_set_result);
  email_set_result = NULL;

  destroy = clist_new();
  if ((destroy == NULL) || (string_list_append_dup(destroy,
      temp_mailbox_id) < 0))
    goto cleanup;
  r = mailjmap_mailbox_set(session, account_id, NULL, NULL, NULL, destroy,
      &mailbox_set_result);
  if (r != MAILJMAP_NO_ERROR) {
    print_error(session, "Mailbox/set destroy", r);
    goto cleanup;
  }
  if (!string_list_contains(mailbox_set_result->destroyed,
      temp_mailbox_id)) {
    fprintf(stderr, "Mailbox/set did not destroy generated mailbox\n");
    goto cleanup;
  }
  printf("Mailbox/set destroyed %s\n", temp_mailbox_id);

  status = 0;

 cleanup:
  if (status != 0) {
    best_effort_destroy_emails(session, account_id, imported_email_id,
        copied_email_id);
    best_effort_destroy_mailbox(session, account_id, temp_mailbox_id);
  }

  mailjmap_mailbox_get_result_free(mailbox_get);
  mailjmap_query_result_free(mailbox_query);
  mailjmap_changes_result_free(mailbox_changes);
  mailjmap_identity_get_result_free(identities);
  mailjmap_blob_upload_free(upload);
  mailjmap_email_parse_result_free(parse_result);
  mailjmap_query_result_free(initial_search);
  mailjmap_query_result_free(search);
  mailjmap_query_changes_result_free(query_changes);
  mailjmap_import_result_free(import_result);
  mailjmap_email_get_result_free(imported_get);
  mailjmap_thread_get_result_free(thread_get);
  mailjmap_changes_result_free(thread_changes);
  mailjmap_changes_result_free(email_changes);
  mailjmap_search_snippet_get_result_free(snippet_result);
  mailjmap_set_result_free(mailbox_set_result);
  mailjmap_set_result_free(email_set_result);
  mailjmap_set_result_free(copy_result);
  mailjmap_set_result_free(submit_result);
  if (create_kind == 1)
    mailbox_set_item_list_free(create);
  else if (create_kind == 3)
    email_copy_item_list_free(create);
  else if (create_kind == 4)
    email_submission_item_list_free(create);
  else if (create != NULL)
    clist_free(create);
  if (update_kind == 1)
    mailbox_set_item_list_free(update);
  else if (update_kind == 2)
    email_set_item_list_free(update);
  else if (update != NULL)
    clist_free(update);
  string_list_free(destroy);
  string_list_free(ids);
  string_list_free(properties);
  string_list_free(body_properties);
  string_list_free(mailbox_ids);
  string_list_free(keywords);
  mailjmap_mailbox_set_item_free(mailbox_item);
  mailjmap_email_set_item_free(email_update_item);
  mailjmap_email_copy_item_free(copy_item);
  mailjmap_email_submission_set_item_free(submit_item);
  free(raw_message);
  free(mailbox_state);
  free(mailbox_created_state);
  free(email_state);
  free(thread_state);
  free(temp_mailbox_id);
  free(imported_email_id);
  free(imported_thread_id);
  free(copied_email_id);
  return status;
}

static int optional_send(mailjmap * session, const char * account_id,
    const char * send_to)
{
  struct mailjmap_identity_get_result * identities;
  struct mailjmap_identity * identity;
  struct mailjmap_blob_upload * upload;
  struct mailjmap_import_result * import_result;
  struct mailjmap_set_result * mailbox_set_result;
  struct mailjmap_set_result * submit_result;
  struct mailjmap_mailbox_set_item * mailbox_item;
  struct mailjmap_email_submission_set_item * item;
  clist * mailbox_create;
  clist * submission_create;
  clist * mailbox_ids;
  clist * keywords;
  char mailbox_name[128];
  char * raw_message;
  char * email_id;
  char * temp_mailbox_id;
  const char * value;
  int r;

  identities = NULL;
  upload = NULL;
  import_result = NULL;
  mailbox_set_result = NULL;
  submit_result = NULL;
  mailbox_item = NULL;
  item = NULL;
  mailbox_create = NULL;
  submission_create = NULL;
  mailbox_ids = NULL;
  keywords = NULL;
  raw_message = NULL;
  email_id = NULL;
  temp_mailbox_id = NULL;

  snprintf(mailbox_name, sizeof(mailbox_name), "libetpan smoke send %ld-%ld",
      (long) time(NULL), (long) getpid());

  r = mailjmap_identity_get(session, account_id, NULL, NULL, &identities);
  if (r != MAILJMAP_NO_ERROR) {
    print_error(session, "Identity/get", r);
    goto err;
  }

  identity = first_identity(identities);
  if (identity == NULL) {
    fprintf(stderr, "Identity/get returned no usable identity\n");
    goto err;
  }

  raw_message = build_raw_message(identity->email, send_to);
  if (raw_message == NULL)
    goto err;

  r = mailjmap_upload(session, account_id, "message/rfc822", raw_message,
      strlen(raw_message), &upload);
  if (r != MAILJMAP_NO_ERROR) {
    print_error(session, "upload", r);
    goto err;
  }

  mailbox_create = clist_new();
  mailbox_item = mailjmap_mailbox_set_item_new("smokesendbox");
  if ((mailbox_create == NULL) || (mailbox_item == NULL))
    goto err;
  r = mailjmap_mailbox_set_item_set_name(mailbox_item, mailbox_name);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  mailjmap_mailbox_set_item_set_parent_id_null(mailbox_item);
  mailjmap_mailbox_set_item_set_role_null(mailbox_item);
  mailjmap_mailbox_set_item_set_is_subscribed(mailbox_item, 0);
  if (clist_append(mailbox_create, mailbox_item) < 0)
    goto err;
  mailbox_item = NULL;

  r = mailjmap_mailbox_set(session, account_id, NULL, mailbox_create,
      NULL, NULL, &mailbox_set_result);
  if (r != MAILJMAP_NO_ERROR) {
    print_error(session, "Mailbox/set smoke send create", r);
    goto err;
  }
  value = find_created_id(mailbox_set_result, "smokesendbox");
  if (value == NULL) {
    fprintf(stderr, "Mailbox/set did not create smoke send mailbox\n");
    goto err;
  }
  temp_mailbox_id = dup_string_or_null(value);
  if (temp_mailbox_id == NULL)
    goto err;
  mailbox_set_item_list_free(mailbox_create);
  mailbox_create = NULL;
  mailjmap_set_result_free(mailbox_set_result);
  mailbox_set_result = NULL;

  mailbox_ids = clist_new();
  keywords = clist_new();
  if ((mailbox_ids == NULL) || (keywords == NULL))
    goto err;
  if ((string_list_append_dup(mailbox_ids, temp_mailbox_id) < 0) ||
      (string_list_append_dup(keywords, "$seen") < 0))
    goto err;

  r = mailjmap_email_import(session, account_id, "m1", upload->blob_id,
      mailbox_ids, keywords, NULL, &import_result);
  if (r != MAILJMAP_NO_ERROR) {
    print_error(session, "Email/import", r);
    goto err;
  }

  value = find_imported_email_id(import_result, "m1");
  if (value == NULL) {
    fprintf(stderr, "Email/import did not return imported email id\n");
    goto err;
  }
  email_id = dup_string_or_null(value);
  if (email_id == NULL)
    goto err;

  item = mailjmap_email_submission_set_item_new("s1");
  submission_create = clist_new();
  if ((item == NULL) || (submission_create == NULL))
    goto err;
  r = mailjmap_email_submission_set_item_set_email_id(item, email_id);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = mailjmap_email_submission_set_item_set_identity_id(item, identity->id);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  if (clist_append(submission_create, item) < 0)
    goto err;
  item = NULL;

  r = mailjmap_email_submission_set(session, account_id, NULL,
      submission_create,
      NULL, NULL, &submit_result);
  if (r != MAILJMAP_NO_ERROR) {
    print_error(session, "EmailSubmission/set", r);
    goto err;
  }

  printf("sent test message to %s\n", send_to);
  best_effort_destroy_emails(session, account_id, email_id, NULL);
  best_effort_destroy_mailbox(session, account_id, temp_mailbox_id);
  email_submission_item_list_free(submission_create);
  string_list_free(mailbox_ids);
  string_list_free(keywords);
  mailjmap_set_result_free(submit_result);
  mailjmap_set_result_free(mailbox_set_result);
  mailjmap_import_result_free(import_result);
  mailjmap_blob_upload_free(upload);
  mailjmap_identity_get_result_free(identities);
  free(temp_mailbox_id);
  free(email_id);
  free(raw_message);
  return 0;

 err:
  mailjmap_email_submission_set_item_free(item);
  mailjmap_mailbox_set_item_free(mailbox_item);
  mailbox_set_item_list_free(mailbox_create);
  email_submission_item_list_free(submission_create);
  string_list_free(mailbox_ids);
  string_list_free(keywords);
  best_effort_destroy_emails(session, account_id, email_id, NULL);
  best_effort_destroy_mailbox(session, account_id, temp_mailbox_id);
  mailjmap_set_result_free(mailbox_set_result);
  mailjmap_set_result_free(submit_result);
  mailjmap_import_result_free(import_result);
  mailjmap_blob_upload_free(upload);
  mailjmap_identity_get_result_free(identities);
  free(temp_mailbox_id);
  free(email_id);
  free(raw_message);
  return 1;
}

int main(void)
{
  const char * session_url;
  const char * email;
  const char * access_token;
  const char * send_to;
  const char * full_test;
  struct mailjmap_session * jmap_session;
  struct mailjmap_mailbox_get_result * mailboxes;
  struct mailjmap_query_result * query;
  const char * account_id;
  const char * first_email_id;
  mailjmap * session;
  int r;
  int status;

  session_url = getenv("JMAP_SESSION_URL");
  email = getenv("JMAP_EMAIL");
  access_token = getenv("JMAP_ACCESS_TOKEN");
  send_to = getenv("JMAP_SEND_TO");
  full_test = getenv("JMAP_LIVE_FULL_TEST");

  if (((session_url == NULL) || (* session_url == '\0')) &&
      ((email == NULL) || (* email == '\0'))) {
    fprintf(stderr, "Set JMAP_SESSION_URL or JMAP_EMAIL\n");
    return 1;
  }
  if ((access_token == NULL) || (* access_token == '\0')) {
    fprintf(stderr, "Set JMAP_ACCESS_TOKEN\n");
    return 1;
  }

  session = mailjmap_new(0, NULL);
  if (session == NULL)
    return 1;

  jmap_session = NULL;
  mailboxes = NULL;
  query = NULL;
  status = 1;

  r = mailjmap_login_oauth2(session, email, access_token);
  if (r != MAILJMAP_NO_ERROR) {
    print_error(session, "login", r);
    goto cleanup;
  }

  if ((session_url != NULL) && (* session_url != '\0')) {
    r = mailjmap_connect(session, session_url);
    if (r != MAILJMAP_NO_ERROR) {
      print_error(session, "connect", r);
      goto cleanup;
    }
    r = mailjmap_get_session(session, &jmap_session);
    if (r != MAILJMAP_NO_ERROR) {
      print_error(session, "Session GET", r);
      goto cleanup;
    }
  }
  else {
    r = mailjmap_discover(session, email, &jmap_session);
    if (r != MAILJMAP_NO_ERROR) {
      print_error(session, "discovery", r);
      goto cleanup;
    }
  }

  account_id = find_primary_mail_account(jmap_session);
  if (account_id == NULL) {
    fprintf(stderr, "No primary JMAP mail account found\n");
    goto cleanup;
  }
  printf("mail account: %s\n", account_id);

  r = mailjmap_mailbox_get(session, account_id, NULL, NULL, &mailboxes);
  if (r != MAILJMAP_NO_ERROR) {
    print_error(session, "Mailbox/get", r);
    goto cleanup;
  }
  printf("mailboxes: %u\n", list_count(mailboxes->list));

  r = mailjmap_email_query(session, account_id, 0, 10, &query);
  if (r != MAILJMAP_NO_ERROR) {
    print_error(session, "Email/query", r);
    goto cleanup;
  }
  printf("recent email ids: %u", list_count(query->ids));
  if (query->has_total)
    printf(" total=%d", query->total);
  printf("\n");

  first_email_id = find_first_email_id(query);
  if (first_email_id != NULL) {
    if (fetch_one_email(session, account_id, first_email_id) != 0)
      goto cleanup;
  }
  else
    printf("no recent email to fetch\n");

  if ((full_test != NULL) && (* full_test != '\0') &&
      (strcmp(full_test, "0") != 0)) {
    if (full_live_self_test(session, account_id, send_to) != 0)
      goto cleanup;
  }
  else if ((send_to != NULL) && (* send_to != '\0') &&
      (optional_send(session, account_id, send_to) != 0))
    goto cleanup;

  status = 0;

 cleanup:
  mailjmap_query_result_free(query);
  mailjmap_mailbox_get_result_free(mailboxes);
  mailjmap_session_free(jmap_session);
  mailjmap_free(session);
  return status;
}
