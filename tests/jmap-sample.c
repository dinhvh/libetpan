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

static int optional_send(mailjmap * session, const char * account_id,
    const char * send_to)
{
  struct mailjmap_identity_get_result * identities;
  struct mailjmap_identity * identity;
  struct mailjmap_blob_upload * upload;
  struct mailjmap_import_result * import_result;
  struct mailjmap_set_result * submit_result;
  struct mailjmap_email_submission_set_item * item;
  clist * create;
  char * raw_message;
  char * email_id;
  int r;

  identities = NULL;
  upload = NULL;
  import_result = NULL;
  submit_result = NULL;
  item = NULL;
  create = NULL;
  raw_message = NULL;
  email_id = NULL;

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

  r = mailjmap_email_import(session, account_id, "m1", upload->blob_id,
      NULL, NULL, NULL, &import_result);
  if (r != MAILJMAP_NO_ERROR) {
    print_error(session, "Email/import", r);
    goto err;
  }

  email_id = (char *) find_imported_email_id(import_result, "m1");
  if (email_id == NULL) {
    fprintf(stderr, "Email/import did not return imported email id\n");
    goto err;
  }

  item = mailjmap_email_submission_set_item_new("s1");
  create = clist_new();
  if ((item == NULL) || (create == NULL))
    goto err;
  r = mailjmap_email_submission_set_item_set_email_id(item, email_id);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = mailjmap_email_submission_set_item_set_identity_id(item, identity->id);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  if (clist_append(create, item) < 0)
    goto err;
  item = NULL;

  r = mailjmap_email_submission_set(session, account_id, NULL, create,
      NULL, NULL, &submit_result);
  if (r != MAILJMAP_NO_ERROR) {
    print_error(session, "EmailSubmission/set", r);
    goto err;
  }

  printf("sent test message to %s\n", send_to);
  clist_free(create);
  mailjmap_set_result_free(submit_result);
  mailjmap_import_result_free(import_result);
  mailjmap_blob_upload_free(upload);
  mailjmap_identity_get_result_free(identities);
  free(raw_message);
  return 0;

 err:
  mailjmap_email_submission_set_item_free(item);
  clist_free(create);
  mailjmap_set_result_free(submit_result);
  mailjmap_import_result_free(import_result);
  mailjmap_blob_upload_free(upload);
  mailjmap_identity_get_result_free(identities);
  free(raw_message);
  return 1;
}

int main(void)
{
  const char * session_url;
  const char * email;
  const char * access_token;
  const char * send_to;
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

  if ((send_to != NULL) && (* send_to != '\0') &&
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
