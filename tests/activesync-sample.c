/*
 * Low-level ActiveSync sample.
 */

#include <libetpan/mailactivesync.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#ifndef _WIN32
#include <unistd.h>
#endif

struct sample_state {
  char * protocol_version;
  char * policy_key;
  char * folder_sync_key;
  char * inbox_id;
  char * inbox_sync_key;
  char * drafts_id;
  char * drafts_sync_key;
  char * attachment_file_reference;
};

struct sample_args {
  const char * server;
  const char * login;
  const char * oauth_token;
  const char * state_file;
  const char * user_agent;
  const char * validate_cert_file;
  const char * device_id;
  const char * device_type;
  int body_type;
  int debug;
  int skip_options;
  int follow_redirect;
  int send_self_test;
  int smart_reply_self_test;
  int smart_forward_self_test;
  int resolve_self_test;
  int resolve_cert_self_test;
  int validate_cert_self_test;
  int search_self_test;
  int ping_self_test;
  int ping_change_self_test;
  int draft_self_test;
  int multi_sync_self_test;
  int attachment_self_test;
  int mutation_self_test;
  int folder_self_test;
  int move_self_test;
};

#define BODY_PRINT_LIMIT 8192

static void free_string(char ** value)
{
  free(* value);
  * value = NULL;
}

static int set_string(char ** target, const char * value)
{
  char * dup_value;

  if (value == NULL)
    dup_value = NULL;
  else {
    dup_value = strdup(value);
    if (dup_value == NULL)
      return -1;
  }

  free(* target);
  * target = dup_value;
  return 0;
}

static void sample_sleep_seconds(unsigned int seconds)
{
#ifndef _WIN32
  sleep(seconds);
#else
  (void) seconds;
#endif
}

static void sample_state_init(struct sample_state * state)
{
  state->protocol_version = NULL;
  state->policy_key = NULL;
  state->folder_sync_key = NULL;
  state->inbox_id = NULL;
  state->inbox_sync_key = NULL;
  state->drafts_id = NULL;
  state->drafts_sync_key = NULL;
  state->attachment_file_reference = NULL;
}

static void sample_state_free(struct sample_state * state)
{
  free_string(&state->protocol_version);
  free_string(&state->policy_key);
  free_string(&state->folder_sync_key);
  free_string(&state->inbox_id);
  free_string(&state->inbox_sync_key);
  free_string(&state->drafts_id);
  free_string(&state->drafts_sync_key);
  free_string(&state->attachment_file_reference);
}

static void sample_state_clear_account_sync(struct sample_state * state)
{
  free_string(&state->folder_sync_key);
  free_string(&state->inbox_id);
  free_string(&state->inbox_sync_key);
  free_string(&state->drafts_id);
  free_string(&state->drafts_sync_key);
  free_string(&state->attachment_file_reference);
}

static void sample_state_clear_inbox_sync(struct sample_state * state)
{
  free_string(&state->inbox_sync_key);
}

static void sample_state_clear_drafts_sync(struct sample_state * state)
{
  free_string(&state->drafts_sync_key);
}

static void trim_newline(char * str)
{
  size_t len;

  len = strlen(str);
  while ((len > 0) && ((str[len - 1] == '\n') || (str[len - 1] == '\r'))) {
    str[len - 1] = '\0';
    len --;
  }
}

static int state_load(const char * filename, struct sample_state * state)
{
  FILE * f;
  char line[4096];

  f = fopen(filename, "r");
  if (f == NULL)
    return 0;

  while (fgets(line, sizeof(line), f) != NULL) {
    char * value;

    trim_newline(line);
    value = strchr(line, '=');
    if (value == NULL)
      continue;
    * value = '\0';
    value ++;

    if (strcmp(line, "protocol_version") == 0)
      set_string(&state->protocol_version, value);
    else if (strcmp(line, "policy_key") == 0)
      set_string(&state->policy_key, value);
    else if (strcmp(line, "folder_sync_key") == 0)
      set_string(&state->folder_sync_key, value);
    else if (strcmp(line, "folder.Inbox") == 0)
      set_string(&state->inbox_id, value);
    else if (strcmp(line, "sync.Inbox") == 0)
      set_string(&state->inbox_sync_key, value);
    else if (strcmp(line, "folder.Drafts") == 0)
      set_string(&state->drafts_id, value);
    else if (strcmp(line, "sync.Drafts") == 0)
      set_string(&state->drafts_sync_key, value);
  }

  fclose(f);
  return 0;
}

static int state_save(const char * filename, struct sample_state * state)
{
  FILE * f;

  f = fopen(filename, "w");
  if (f == NULL)
    return -1;

  fprintf(f, "protocol_version=%s\n",
      state->protocol_version != NULL ? state->protocol_version : "16.1");
  if (state->policy_key != NULL)
    fprintf(f, "policy_key=%s\n", state->policy_key);
  if (state->folder_sync_key != NULL)
    fprintf(f, "folder_sync_key=%s\n", state->folder_sync_key);
  if (state->inbox_id != NULL)
    fprintf(f, "folder.Inbox=%s\n", state->inbox_id);
  if (state->inbox_sync_key != NULL)
    fprintf(f, "sync.Inbox=%s\n", state->inbox_sync_key);
  if (state->drafts_id != NULL)
    fprintf(f, "folder.Drafts=%s\n", state->drafts_id);
  if (state->drafts_sync_key != NULL)
    fprintf(f, "sync.Drafts=%s\n", state->drafts_sync_key);

  fclose(f);
  return 0;
}

static const char * find_folder_server_id(
    struct mailactivesync_folder_sync_result * result, const char * name)
{
  clistiter * cur;

  if ((result == NULL) || (result->added == NULL))
    return NULL;

  for (cur = result->added != NULL ? clist_begin(result->added) : NULL;
      cur != NULL; cur = clist_next(cur)) {
    struct mailactivesync_folder * folder;

    folder = clist_content(cur);
    if ((folder->display_name != NULL) &&
        (strcasecmp(folder->display_name, name) == 0))
      return folder->server_id;
  }

  return NULL;
}

static const char * find_folder_server_id_by_name_or_type_in_list(
    clist * folders, const char * name, int type)
{
  clistiter * cur;

  for (cur = folders != NULL ? clist_begin(folders) : NULL; cur != NULL;
      cur = clist_next(cur)) {
    struct mailactivesync_folder * folder;

    folder = clist_content(cur);
    if ((folder->server_id != NULL) &&
        (((folder->display_name != NULL) &&
            (strcasecmp(folder->display_name, name) == 0)) ||
          (folder->type == type)))
      return folder->server_id;
  }

  return NULL;
}

static const char * find_folder_server_id_by_name_or_type(
    struct mailactivesync_folder_sync_result * result, const char * name,
    int type)
{
  const char * server_id;

  if (result == NULL)
    return NULL;

  server_id = find_folder_server_id_by_name_or_type_in_list(result->added,
      name, type);
  if (server_id != NULL)
    return server_id;

  return find_folder_server_id_by_name_or_type_in_list(result->updated, name,
      type);
}

static struct mailactivesync_folder * find_folder_by_id_in_list(
    clist * folders, const char * server_id)
{
  clistiter * cur;

  for (cur = folders != NULL ? clist_begin(folders) : NULL; cur != NULL;
      cur = clist_next(cur)) {
    struct mailactivesync_folder * folder;

    folder = clist_content(cur);
    if ((folder->server_id != NULL) &&
        (strcmp(folder->server_id, server_id) == 0))
      return folder;
  }

  return NULL;
}

static struct mailactivesync_folder * find_folder_by_id(
    struct mailactivesync_folder_sync_result * result,
    const char * server_id)
{
  struct mailactivesync_folder * folder;

  if ((result == NULL) || (server_id == NULL))
    return NULL;

  folder = find_folder_by_id_in_list(result->added, server_id);
  if (folder != NULL)
    return folder;

  return find_folder_by_id_in_list(result->updated, server_id);
}

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

static int string_list_contains(clist * list, const char * value)
{
  clistiter * cur;

  for (cur = list != NULL ? clist_begin(list) : NULL;
      cur != NULL; cur = clist_next(cur)) {
    const char * item;

    item = clist_content(cur);
    if ((item != NULL) && (strcmp(item, value) == 0))
      return 1;
  }

  return 0;
}

static const char * activesync_error_name(int error)
{
  switch (error) {
  case MAILACTIVESYNC_NO_ERROR:
    return "MAILACTIVESYNC_NO_ERROR";
  case MAILACTIVESYNC_ERROR_BAD_STATE:
    return "MAILACTIVESYNC_ERROR_BAD_STATE";
  case MAILACTIVESYNC_ERROR_UNAUTHORIZED:
    return "MAILACTIVESYNC_ERROR_UNAUTHORIZED";
  case MAILACTIVESYNC_ERROR_STREAM:
    return "MAILACTIVESYNC_ERROR_STREAM";
  case MAILACTIVESYNC_ERROR_HTTP:
    return "MAILACTIVESYNC_ERROR_HTTP";
  case MAILACTIVESYNC_ERROR_PROTOCOL:
    return "MAILACTIVESYNC_ERROR_PROTOCOL";
  case MAILACTIVESYNC_ERROR_PARSE:
    return "MAILACTIVESYNC_ERROR_PARSE";
  case MAILACTIVESYNC_ERROR_MEMORY:
    return "MAILACTIVESYNC_ERROR_MEMORY";
  case MAILACTIVESYNC_ERROR_SSL:
    return "MAILACTIVESYNC_ERROR_SSL";
  case MAILACTIVESYNC_ERROR_NOT_IMPLEMENTED:
    return "MAILACTIVESYNC_ERROR_NOT_IMPLEMENTED";
  case MAILACTIVESYNC_ERROR_HTTP_UNAVAILABLE:
    return "MAILACTIVESYNC_ERROR_HTTP_UNAVAILABLE";
  case MAILACTIVESYNC_ERROR_PROVISION_REQUIRED:
    return "MAILACTIVESYNC_ERROR_PROVISION_REQUIRED";
  case MAILACTIVESYNC_ERROR_REDIRECT:
    return "MAILACTIVESYNC_ERROR_REDIRECT";
  case MAILACTIVESYNC_ERROR_RESPONSE_NOT_WBXML:
    return "MAILACTIVESYNC_ERROR_RESPONSE_NOT_WBXML";
  case MAILACTIVESYNC_ERROR_INVALID_SYNC_KEY:
    return "MAILACTIVESYNC_ERROR_INVALID_SYNC_KEY";
  case MAILACTIVESYNC_ERROR_FOLDER_RESYNC_REQUIRED:
    return "MAILACTIVESYNC_ERROR_FOLDER_RESYNC_REQUIRED";
  case MAILACTIVESYNC_ERROR_ACCOUNT_RESYNC_REQUIRED:
    return "MAILACTIVESYNC_ERROR_ACCOUNT_RESYNC_REQUIRED";
  case MAILACTIVESYNC_ERROR_SERVER_BUSY:
    return "MAILACTIVESYNC_ERROR_SERVER_BUSY";
  case MAILACTIVESYNC_ERROR_CLIENT_DENIED:
    return "MAILACTIVESYNC_ERROR_CLIENT_DENIED";
  default:
    return "MAILACTIVESYNC_ERROR_UNKNOWN";
  }
}

static void debug_step(int debug, const char * message)
{
  if (debug)
    fprintf(stderr, "debug: %s\n", message);
}

static void debug_error(int debug, const char * step, int error)
{
  if (debug || (error != MAILACTIVESYNC_NO_ERROR))
    fprintf(stderr, "%s failed: %s (%d)\n", step,
        activesync_error_name(error), error);
}

static void debug_auth_retry_hint(mailactivesync * as, int error)
{
  const char * authenticate_header;

  if (error != MAILACTIVESYNC_ERROR_UNAUTHORIZED)
    return;

  fprintf(stderr,
      "OAuth access token was rejected; refresh the token and retry the "
      "ActiveSync command.\n");
  authenticate_header = mailactivesync_get_last_authenticate_header(as);
  if (authenticate_header != NULL)
    fprintf(stderr, "WWW-Authenticate: %s\n", authenticate_header);
}

static void debug_options(int debug, struct mailactivesync_options * options)
{
  if (!debug || (options == NULL))
    return;

  fprintf(stderr, "debug: OPTIONS protocol_versions=%u commands=%u\n",
      list_count(options->protocol_versions), list_count(options->commands));
}

static void debug_folder_sync(int debug,
    struct mailactivesync_folder_sync_result * result)
{
  if (!debug || (result == NULL))
    return;

  fprintf(stderr,
      "debug: FolderSync status=%d sync_key=%s added=%u updated=%u "
      "deleted=%u\n",
      result->status,
      result->sync_key != NULL ? result->sync_key : "",
      list_count(result->added), list_count(result->updated),
      list_count(result->deleted));
}

static void debug_sync(int debug, struct mailactivesync_sync_result * result)
{
  if (!debug || (result == NULL))
    return;

  fprintf(stderr,
      "debug: Sync status=%d sync_key=%s more_available=%d added=%u "
      "changed=%u deleted=%u\n",
      result->status,
      result->sync_key != NULL ? result->sync_key : "",
      result->more_available, list_count(result->added),
      list_count(result->changed), list_count(result->deleted));
}

static void print_usage(const char * progname)
{
  fprintf(stderr,
      "usage: %s --server URL --login USER --oauth-token TOKEN "
      "--state-file PATH [--debug] [--skip-options] [--follow-redirect] "
      "[--user-agent VALUE] [--device-id VALUE] [--device-type VALUE] "
      "[--validate-cert-file PATH] "
      "[--body-type plain|html|mime] [--send-self-test] "
      "[--smart-reply-self-test] [--smart-forward-self-test] "
      "[--resolve-self-test] [--resolve-cert-self-test] "
      "[--validate-cert-self-test] "
      "[--search-self-test] [--ping-self-test] [--ping-change-self-test] "
      "[--draft-self-test] [--multi-sync-self-test] "
      "[--attachment-self-test] [--mutation-self-test] "
      "[--folder-self-test] [--move-self-test]\n",
      progname);
}

static int body_type_from_name(const char * name)
{
  if (name == NULL)
    return 0;
  if (strcmp(name, "plain") == 0)
    return MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_PLAIN_TEXT;
  if (strcmp(name, "html") == 0)
    return MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_HTML;
  if (strcmp(name, "mime") == 0)
    return MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_MIME;
  return 0;
}

static int parse_args(int argc, char ** argv, struct sample_args * args)
{
  int i;

  for (i = 1; i < argc; i ++) {
    if ((strcmp(argv[i], "--server") == 0) && (i + 1 < argc))
      args->server = argv[++ i];
    else if ((strcmp(argv[i], "--login") == 0) && (i + 1 < argc))
      args->login = argv[++ i];
    else if ((strcmp(argv[i], "--oauth-token") == 0) && (i + 1 < argc))
      args->oauth_token = argv[++ i];
    else if ((strcmp(argv[i], "--state-file") == 0) && (i + 1 < argc))
      args->state_file = argv[++ i];
    else if ((strcmp(argv[i], "--user-agent") == 0) && (i + 1 < argc))
      args->user_agent = argv[++ i];
    else if ((strcmp(argv[i], "--validate-cert-file") == 0) &&
        (i + 1 < argc))
      args->validate_cert_file = argv[++ i];
    else if ((strcmp(argv[i], "--device-id") == 0) && (i + 1 < argc))
      args->device_id = argv[++ i];
    else if ((strcmp(argv[i], "--device-type") == 0) && (i + 1 < argc))
      args->device_type = argv[++ i];
    else if ((strcmp(argv[i], "--body-type") == 0) && (i + 1 < argc)) {
      args->body_type = body_type_from_name(argv[++ i]);
      if (args->body_type == 0)
        return -1;
    }
    else if (strcmp(argv[i], "--debug") == 0)
      args->debug = 1;
    else if (strcmp(argv[i], "--skip-options") == 0)
      args->skip_options = 1;
    else if (strcmp(argv[i], "--follow-redirect") == 0)
      args->follow_redirect = 1;
    else if (strcmp(argv[i], "--send-self-test") == 0)
      args->send_self_test = 1;
    else if (strcmp(argv[i], "--smart-reply-self-test") == 0)
      args->smart_reply_self_test = 1;
    else if (strcmp(argv[i], "--smart-forward-self-test") == 0)
      args->smart_forward_self_test = 1;
    else if (strcmp(argv[i], "--resolve-self-test") == 0)
      args->resolve_self_test = 1;
    else if (strcmp(argv[i], "--resolve-cert-self-test") == 0)
      args->resolve_cert_self_test = 1;
    else if (strcmp(argv[i], "--validate-cert-self-test") == 0)
      args->validate_cert_self_test = 1;
    else if (strcmp(argv[i], "--search-self-test") == 0)
      args->search_self_test = 1;
    else if (strcmp(argv[i], "--ping-self-test") == 0)
      args->ping_self_test = 1;
    else if (strcmp(argv[i], "--ping-change-self-test") == 0)
      args->ping_change_self_test = 1;
    else if (strcmp(argv[i], "--draft-self-test") == 0)
      args->draft_self_test = 1;
    else if (strcmp(argv[i], "--multi-sync-self-test") == 0)
      args->multi_sync_self_test = 1;
    else if (strcmp(argv[i], "--attachment-self-test") == 0)
      args->attachment_self_test = 1;
    else if (strcmp(argv[i], "--mutation-self-test") == 0)
      args->mutation_self_test = 1;
    else if (strcmp(argv[i], "--folder-self-test") == 0)
      args->folder_self_test = 1;
    else if (strcmp(argv[i], "--move-self-test") == 0)
      args->move_self_test = 1;
    else
      return -1;
  }

  if ((args->server == NULL) || (args->login == NULL) ||
      (args->oauth_token == NULL) || (args->state_file == NULL))
    return -1;

  return 0;
}

static const char * body_type_name(int type)
{
  switch (type) {
  case MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_PLAIN_TEXT:
    return "plain";
  case MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_HTML:
    return "html";
  case MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_RTF:
    return "rtf";
  case MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_MIME:
    return "mime";
  default:
    return "unknown";
  }
}

static void print_text_block(const char * label, const char * value,
    size_t value_len)
{
  size_t i;
  size_t limit;

  printf("%s:\n", label);
  if ((value == NULL) || (value_len == 0)) {
    printf("\n");
    return;
  }

  limit = value_len;
  if (limit > BODY_PRINT_LIMIT)
    limit = BODY_PRINT_LIMIT;

  for (i = 0; i < limit; i ++) {
    unsigned char ch;

    ch = (unsigned char) value[i];
    if ((ch == '\r') || (ch == '\n') || (ch == '\t') ||
        ((ch >= 0x20) && (ch < 0x7F)))
      putchar((int) ch);
    else
      putchar('.');
  }
  if (limit < value_len)
    printf("\n[truncated after %u bytes]", (unsigned int) BODY_PRINT_LIMIT);
  printf("\n");
}

static char * read_text_file_trimmed(const char * filename)
{
  FILE * f;
  char * data;
  long size;
  size_t read_size;

  f = fopen(filename, "rb");
  if (f == NULL)
    return NULL;
  if (fseek(f, 0, SEEK_END) < 0) {
    fclose(f);
    return NULL;
  }
  size = ftell(f);
  if (size < 0) {
    fclose(f);
    return NULL;
  }
  if (fseek(f, 0, SEEK_SET) < 0) {
    fclose(f);
    return NULL;
  }

  data = malloc((size_t) size + 1);
  if (data == NULL) {
    fclose(f);
    return NULL;
  }
  read_size = fread(data, 1, (size_t) size, f);
  fclose(f);
  if (read_size != (size_t) size) {
    free(data);
    return NULL;
  }
  data[read_size] = '\0';
  while ((read_size > 0) &&
      ((data[read_size - 1] == '\n') || (data[read_size - 1] == '\r') ||
        (data[read_size - 1] == ' ') || (data[read_size - 1] == '\t')))
    data[-- read_size] = '\0';

  return data;
}

static void remember_first_attachment_reference(struct sample_state * state,
    struct mailactivesync_message * message)
{
  clistiter * cur;

  if ((state == NULL) || (state->attachment_file_reference != NULL) ||
      (message == NULL) || (message->body == NULL) ||
      (message->body->attachments == NULL))
    return;

  for (cur = clist_begin(message->body->attachments); cur != NULL;
      cur = clist_next(cur)) {
    struct mailactivesync_attachment * attachment;

    attachment = clist_content(cur);
    if ((attachment != NULL) && (attachment->file_reference != NULL)) {
      set_string(&state->attachment_file_reference,
          attachment->file_reference);
      return;
    }
  }
}

static void print_attachments(clist * attachments)
{
  clistiter * cur;

  for (cur = attachments != NULL ? clist_begin(attachments) : NULL;
      cur != NULL; cur = clist_next(cur)) {
    struct mailactivesync_attachment * attachment;

    attachment = clist_content(cur);
    printf("Attachment: name=%s type=%s size=%u file_reference=%s\n",
        attachment->display_name != NULL ? attachment->display_name : "",
        attachment->content_type != NULL ? attachment->content_type : "",
        (unsigned int) attachment->estimated_data_size,
        attachment->file_reference != NULL ? attachment->file_reference : "");
  }
}

static void print_message(const char * change_type,
    struct mailactivesync_message * message)
{
  const char * body_data;
  size_t body_len;

  if (message == NULL)
    return;

  printf("=== %s ===\n", change_type);
  printf("Server-Id: %s\n",
      message->server_id != NULL ? message->server_id : "");
  printf("From: %s\n", message->from != NULL ? message->from : "");
  printf("To: %s\n", message->to != NULL ? message->to : "");
  printf("Cc: %s\n", message->cc != NULL ? message->cc : "");
  printf("Subject: %s\n",
      message->subject != NULL ? message->subject : "");
  printf("Date: %s\n",
      message->date_received != NULL ? message->date_received : "");
  printf("Read: %d\n", message->read);
  if (message->body != NULL) {
    printf("Body-Type: %s (%d)\n", body_type_name(message->body->type),
        message->body->type);
    printf("Body-Estimated-Size: %u\n",
        (unsigned int) message->body->estimated_data_size);
    printf("Body-Truncated: %d\n", message->body->truncated);
    body_data = message->body->data;
    body_len = message->body->data_len;
    print_attachments(message->body->attachments);
  }
  else {
    printf("Body-Type:\n");
    printf("Body-Estimated-Size: 0\n");
    printf("Body-Truncated: 0\n");
    body_data = NULL;
    body_len = 0;
  }
  print_text_block("Body", body_data, body_len);
  printf("\n");
}

static void print_sync_result(struct mailactivesync_sync_result * result,
    struct sample_state * state)
{
  clistiter * cur;

  if (result == NULL)
    return;

  for (cur = clist_begin(result->added); cur != NULL; cur = clist_next(cur)) {
    struct mailactivesync_message * message;

    message = clist_content(cur);
    remember_first_attachment_reference(state, message);
    print_message("added", message);
  }

  for (cur = result->changed != NULL ? clist_begin(result->changed) : NULL;
      cur != NULL; cur = clist_next(cur)) {
    struct mailactivesync_message * message;

    message = clist_content(cur);
    remember_first_attachment_reference(state, message);
    print_message("changed", message);
  }

  for (cur = result->deleted != NULL ? clist_begin(result->deleted) : NULL;
      cur != NULL; cur = clist_next(cur))
    printf("deleted server_id=%s\n", (char *) clist_content(cur));
}

static int is_initial_sync_key(const char * sync_key)
{
  return (sync_key == NULL) || (strcmp(sync_key, "0") == 0) ||
      (* sync_key == '\0');
}

static int run_inbox_sync(mailactivesync * as, const struct sample_args * args,
    struct sample_state * state, const char * sync_key, int get_changes,
    int print_changes, int * more_available)
{
  struct mailactivesync_sync_request * sync_request;
  struct mailactivesync_sync_result * sync_result;
  int r;

  sync_request = NULL;
  sync_result = NULL;
  if (more_available != NULL)
    * more_available = 0;

  if (args->debug)
    fprintf(stderr, "debug: creating Sync request collection_id=%s "
        "sync_key=%s get_changes=%d\n", state->inbox_id, sync_key,
        get_changes);

  sync_request = mailactivesync_sync_request_new(state->inbox_id, sync_key);
  if (sync_request == NULL) {
    fprintf(stderr, "mailactivesync_sync_request_new failed: out of memory\n");
    return MAILACTIVESYNC_ERROR_MEMORY;
  }

  mailactivesync_sync_request_set_get_changes(sync_request, get_changes);
  if (get_changes) {
    mailactivesync_sync_request_set_collection_class(sync_request, "Email");
    mailactivesync_sync_request_set_deletes_as_moves(sync_request, 0);
    mailactivesync_sync_request_set_window_size(sync_request, 25);
    mailactivesync_sync_request_set_body_preference(sync_request,
        args->body_type, 200 * 1024);
  }

  debug_step(args->debug,
      get_changes ? "requesting Sync changes" : "requesting Sync init");
  r = mailactivesync_sync(as, sync_request, &sync_result);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug, "mailactivesync_sync", r);
    debug_auth_retry_hint(as, r);
    goto cleanup;
  }
  debug_sync(args->debug, sync_result);

  r = mailactivesync_sync_status_to_error(sync_result->status);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug, "mailactivesync_sync status", r);
    if (r == MAILACTIVESYNC_ERROR_FOLDER_RESYNC_REQUIRED)
      sample_state_clear_inbox_sync(state);
    else if (r == MAILACTIVESYNC_ERROR_ACCOUNT_RESYNC_REQUIRED)
      sample_state_clear_account_sync(state);
    goto cleanup;
  }

  if (sync_result->sync_key_from_response && (sync_result->sync_key != NULL))
    set_string(&state->inbox_sync_key, sync_result->sync_key);
  if (more_available != NULL)
    * more_available = sync_result->more_available;

  if (print_changes)
    print_sync_result(sync_result, state);

 cleanup:
  mailactivesync_sync_result_free(sync_result);
  mailactivesync_sync_request_free(sync_request);
  return r;
}

static int run_provision(mailactivesync * as, const struct sample_args * args,
    struct sample_state * state)
{
  struct mailactivesync_provision_result * result;
  int r;

  result = NULL;
  debug_step(args->debug, "requesting Provision");
  r = mailactivesync_provision(as, &result);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug, "mailactivesync_provision", r);
    debug_auth_retry_hint(as, r);
    goto cleanup;
  }

  if (args->debug && (result != NULL))
    fprintf(stderr, "debug: Provision status=%d policy_status=%d "
        "policy_key=%s\n", result->status, result->policy_status,
        result->policy_key != NULL ? "<present>" : "");
  if ((result == NULL) || (result->policy_key == NULL)) {
    r = MAILACTIVESYNC_ERROR_PROVISION_REQUIRED;
    debug_error(args->debug, "mailactivesync_provision policy", r);
    goto cleanup;
  }

  set_string(&state->policy_key, result->policy_key);

 cleanup:
  mailactivesync_provision_result_free(result);
  return r;
}

static int run_device_information(mailactivesync * as,
    const struct sample_args * args)
{
  struct mailactivesync_device_information device_information;
  struct mailactivesync_settings_result * result;
  int r;

  result = NULL;
  memset(&device_information, 0, sizeof(device_information));
  device_information.model = "libetpan";
  device_information.os = "libetpan";
  device_information.user_agent = args->user_agent != NULL ?
      args->user_agent : "libEtPan ActiveSync";

  debug_step(args->debug, "requesting Settings DeviceInformation");
  r = mailactivesync_settings_set_device_information(as, &device_information,
      &result);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug,
        "mailactivesync_settings_set_device_information", r);
    debug_auth_retry_hint(as, r);
    goto cleanup;
  }
  if (args->debug && (result != NULL))
    fprintf(stderr, "debug: Settings status=%d device_information_status=%d\n",
        result->status, result->device_information_status);

  if ((result == NULL) ||
      (mailactivesync_global_status_to_error(result->status) !=
          MAILACTIVESYNC_NO_ERROR) ||
      (mailactivesync_global_status_to_error(
          result->device_information_status) != MAILACTIVESYNC_NO_ERROR)) {
    r = MAILACTIVESYNC_ERROR_PROTOCOL;
    debug_error(args->debug, "Settings DeviceInformation status", r);
  }

 cleanup:
  mailactivesync_settings_result_free(result);
  return r;
}

static int run_get_item_estimate(mailactivesync * as,
    const struct sample_args * args, const struct sample_state * state)
{
  struct mailactivesync_get_item_estimate_result * result;
  int r;

  result = NULL;
  if ((state->inbox_id == NULL) || (state->inbox_sync_key == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  debug_step(args->debug, "requesting GetItemEstimate");
  r = mailactivesync_get_item_estimate(as, state->inbox_id,
      state->inbox_sync_key, &result);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug, "mailactivesync_get_item_estimate", r);
    debug_auth_retry_hint(as, r);
    goto cleanup;
  }
  if (args->debug && (result != NULL))
    fprintf(stderr, "debug: GetItemEstimate status=%d "
        "collection_status=%d estimate=%u empty_response=%d\n",
        result->status, result->collection_status,
        (unsigned int) result->estimate, result->empty_response);

  if ((result != NULL) &&
      (mailactivesync_global_status_to_error(result->status) ==
          MAILACTIVESYNC_NO_ERROR))
    r = mailactivesync_sync_status_to_error(result->collection_status);

 cleanup:
  mailactivesync_get_item_estimate_result_free(result);
  return r;
}

static int run_send_self_test(mailactivesync * as,
    const struct sample_args * args, struct mailactivesync_options * options)
{
  char subject[160];
  char client_id[160];
  char message[2048];
  struct mailactivesync_composemail_request request;
  time_t now;
  int len;
  int r;

  if ((options != NULL) && !string_list_contains(options->commands,
      "SendMail")) {
    fprintf(stderr, "SendMail was not advertised by OPTIONS.\n");
    return MAILACTIVESYNC_ERROR_NOT_IMPLEMENTED;
  }

  now = time(NULL);
  snprintf(subject, sizeof(subject),
      "libetpan ActiveSync send self test %lu", (unsigned long) now);
  snprintf(client_id, sizeof(client_id), "libetpan-send-self-%lu",
      (unsigned long) now);
  len = snprintf(message, sizeof(message),
      "From: <%s>\r\n"
      "To: <%s>\r\n"
      "Subject: %s\r\n"
      "MIME-Version: 1.0\r\n"
      "Content-Type: text/plain; charset=utf-8\r\n"
      "Content-Transfer-Encoding: 7bit\r\n"
      "\r\n"
      "This message was generated by tests/activesync-sample "
      "--send-self-test.\r\n"
      "ClientId: %s\r\n",
      args->login, args->login, subject, client_id);
  if ((len < 0) || ((size_t) len >= sizeof(message))) {
    fprintf(stderr, "SendMail test message did not fit in the buffer.\n");
    return MAILACTIVESYNC_ERROR_BAD_STATE;
  }

  memset(&request, 0, sizeof(request));
  request.client_id = client_id;
  request.mime_message = message;
  request.mime_message_len = (size_t) len;
  request.save_in_sent = 1;

  debug_step(args->debug, "requesting SendMail self-test");
  r = mailactivesync_send_mail_ext(as, &request);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug, "mailactivesync_send_mail_ext", r);
    debug_auth_retry_hint(as, r);
    return r;
  }

  printf("SendMail self-test sent subject=%s\n", subject);
  return MAILACTIVESYNC_NO_ERROR;
}

static int send_compose_seed_message(mailactivesync * as,
    const struct sample_args * args, const char * subject,
    const char * client_id)
{
  char message[2048];
  struct mailactivesync_composemail_request request;
  int len;
  int r;

  len = snprintf(message, sizeof(message),
      "From: <%s>\r\n"
      "To: <%s>\r\n"
      "Subject: %s\r\n"
      "MIME-Version: 1.0\r\n"
      "Content-Type: text/plain; charset=utf-8\r\n"
      "Content-Transfer-Encoding: 7bit\r\n"
      "\r\n"
      "This seed message was generated by tests/activesync-sample "
      "for SmartReply/SmartForward validation.\r\n"
      "ClientId: %s\r\n",
      args->login, args->login, subject, client_id);
  if ((len < 0) || ((size_t) len >= sizeof(message)))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  memset(&request, 0, sizeof(request));
  request.client_id = client_id;
  request.mime_message = message;
  request.mime_message_len = (size_t) len;
  request.save_in_sent = 1;

  debug_step(args->debug, "sending generated compose source message");
  r = mailactivesync_send_mail_ext(as, &request);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug, "mailactivesync_send_mail_ext compose seed", r);
    debug_auth_retry_hint(as, r);
  }

  return r;
}

static const char * find_message_server_id_by_subject(clist * messages,
    const char * subject)
{
  clistiter * cur;

  for (cur = messages != NULL ? clist_begin(messages) : NULL; cur != NULL;
      cur = clist_next(cur)) {
    struct mailactivesync_message * message;

    message = clist_content(cur);
    if ((message != NULL) && (message->server_id != NULL) &&
        (message->subject != NULL) &&
        (strcmp(message->subject, subject) == 0))
      return message->server_id;
  }

  return NULL;
}

static void remember_attachments_from_messages(struct sample_state * state,
    clist * messages)
{
  clistiter * cur;

  for (cur = messages != NULL ? clist_begin(messages) : NULL; cur != NULL;
      cur = clist_next(cur)) {
    struct mailactivesync_message * message;

    message = clist_content(cur);
    remember_first_attachment_reference(state, message);
  }
}

static int sync_find_message_by_subject(mailactivesync * as,
    const struct sample_args * args, struct sample_state * state,
    const char * subject, int body_type, char ** server_id)
{
  struct mailactivesync_sync_request * request;
  struct mailactivesync_sync_result * result;
  const char * found;
  int r;

  request = NULL;
  result = NULL;
  if ((state->inbox_id == NULL) || (state->inbox_sync_key == NULL) ||
      (subject == NULL) || (server_id == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;
  * server_id = NULL;

  request = mailactivesync_sync_request_new(state->inbox_id,
      state->inbox_sync_key);
  if (request == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  mailactivesync_sync_request_set_get_changes(request, 1);
  mailactivesync_sync_request_set_collection_class(request, "Email");
  mailactivesync_sync_request_set_deletes_as_moves(request, 0);
  mailactivesync_sync_request_set_window_size(request, 25);
  mailactivesync_sync_request_set_body_preference(request,
      body_type, 64 * 1024);

  debug_step(args->debug, "syncing Inbox for generated compose source");
  r = mailactivesync_sync(as, request, &result);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug, "mailactivesync_sync compose source", r);
    debug_auth_retry_hint(as, r);
    goto cleanup;
  }

  r = mailactivesync_sync_status_to_error(result->status);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug, "compose source Sync status", r);
    if (r == MAILACTIVESYNC_ERROR_FOLDER_RESYNC_REQUIRED)
      sample_state_clear_inbox_sync(state);
    else if (r == MAILACTIVESYNC_ERROR_ACCOUNT_RESYNC_REQUIRED)
      sample_state_clear_account_sync(state);
    goto cleanup;
  }

  found = find_message_server_id_by_subject(result->added, subject);
  if (found == NULL)
    found = find_message_server_id_by_subject(result->changed, subject);
  remember_attachments_from_messages(state, result->added);
  remember_attachments_from_messages(state, result->changed);
  if (found != NULL) {
    * server_id = strdup(found);
    if (* server_id == NULL) {
      r = MAILACTIVESYNC_ERROR_MEMORY;
      goto cleanup;
    }
  }

  if (result->sync_key_from_response && (result->sync_key != NULL))
    set_string(&state->inbox_sync_key, result->sync_key);

 cleanup:
  mailactivesync_sync_result_free(result);
  mailactivesync_sync_request_free(request);
  return r;
}

static int find_generated_compose_source_with_body_type(mailactivesync * as,
    const struct sample_args * args, struct sample_state * state,
    const char * subject, int body_type, char ** server_id);

static int find_generated_compose_source(mailactivesync * as,
    const struct sample_args * args, struct sample_state * state,
    const char * subject, char ** server_id)
{
  return find_generated_compose_source_with_body_type(as, args, state,
      subject, args->body_type, server_id);
}

static int find_generated_compose_source_with_body_type(mailactivesync * as,
    const struct sample_args * args, struct sample_state * state,
    const char * subject, int body_type, char ** server_id)
{
  int attempt;
  int r;

  for (attempt = 0; attempt < 12; attempt ++) {
    r = sync_find_message_by_subject(as, args, state, subject, body_type,
        server_id);
    if (r != MAILACTIVESYNC_NO_ERROR)
      return r;
    if (* server_id != NULL)
      return MAILACTIVESYNC_NO_ERROR;
    if (attempt < 11)
      sample_sleep_seconds(5);
  }

  fprintf(stderr, "Generated compose source was not found in Inbox.\n");
  return MAILACTIVESYNC_ERROR_BAD_STATE;
}

static int run_smart_compose_self_test(mailactivesync * as,
    const struct sample_args * args, struct sample_state * state,
    struct mailactivesync_options * options, int forward)
{
  static char * shared_source_server_id = NULL;
  static char shared_source_subject[160];
  char seed_subject[160];
  char seed_client_id[160];
  char compose_subject[160];
  char compose_client_id[160];
  char message[2048];
  char * source_server_id;
  struct mailactivesync_composemail_request request;
  const char * command_name;
  time_t now;
  int len;
  int r;

  command_name = forward ? "SmartForward" : "SmartReply";
  if ((options != NULL) &&
      (!string_list_contains(options->commands, "SendMail") ||
        !string_list_contains(options->commands, command_name))) {
    fprintf(stderr, "SendMail or %s was not advertised by OPTIONS.\n",
        command_name);
    return MAILACTIVESYNC_ERROR_NOT_IMPLEMENTED;
  }
  if ((state->inbox_id == NULL) || (state->inbox_sync_key == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  source_server_id = NULL;
  now = time(NULL);
  snprintf(seed_subject, sizeof(seed_subject),
      "libetpan ActiveSync %s source %lu",
      forward ? "smart forward" : "smart reply", (unsigned long) now);
  snprintf(seed_client_id, sizeof(seed_client_id), "libetpan-%s-source-%lu",
      forward ? "smart-forward" : "smart-reply", (unsigned long) now);

  if (forward && (shared_source_server_id != NULL)) {
    source_server_id = strdup(shared_source_server_id);
    if (source_server_id == NULL) {
      r = MAILACTIVESYNC_ERROR_MEMORY;
      goto cleanup;
    }
    printf("%s self-test reusing source subject=%s server_id=%s\n",
        command_name, shared_source_subject, source_server_id);
  }
  else {
    r = send_compose_seed_message(as, args, seed_subject, seed_client_id);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto cleanup;
    printf("%s self-test sent source subject=%s\n", command_name,
        seed_subject);

    r = find_generated_compose_source(as, args, state, seed_subject,
        &source_server_id);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto cleanup;
    printf("%s self-test found source server_id=%s\n", command_name,
        source_server_id);

    if (!forward) {
      char * copy;

      copy = strdup(source_server_id);
      if (copy == NULL) {
        r = MAILACTIVESYNC_ERROR_MEMORY;
        goto cleanup;
      }
      free(shared_source_server_id);
      shared_source_server_id = copy;
      snprintf(shared_source_subject, sizeof(shared_source_subject), "%s",
          seed_subject);
    }
  }

  snprintf(compose_subject, sizeof(compose_subject),
      "libetpan ActiveSync %s self test %lu",
      forward ? "smart forward" : "smart reply", (unsigned long) now);
  snprintf(compose_client_id, sizeof(compose_client_id),
      "libetpan-%s-self-%lu",
      forward ? "smart-forward" : "smart-reply", (unsigned long) now);
  len = snprintf(message, sizeof(message),
      "From: <%s>\r\n"
      "To: <%s>\r\n"
      "Subject: %s\r\n"
      "MIME-Version: 1.0\r\n"
      "Content-Type: text/plain; charset=utf-8\r\n"
      "Content-Transfer-Encoding: 7bit\r\n"
      "\r\n"
      "This message was generated by tests/activesync-sample --%s.\r\n"
      "ClientId: %s\r\n",
      args->login, args->login, compose_subject,
      forward ? "smart-forward-self-test" : "smart-reply-self-test",
      compose_client_id);
  if ((len < 0) || ((size_t) len >= sizeof(message))) {
    r = MAILACTIVESYNC_ERROR_BAD_STATE;
    goto cleanup;
  }

  memset(&request, 0, sizeof(request));
  request.client_id = compose_client_id;
  request.collection_id = state->inbox_id;
  request.server_id = source_server_id;
  request.mime_message = message;
  request.mime_message_len = (size_t) len;
  request.save_in_sent = 1;

  debug_step(args->debug, forward ?
      "requesting SmartForward self-test" :
      "requesting SmartReply self-test");
  if (forward)
    r = mailactivesync_smart_forward_ext(as, &request);
  else
    r = mailactivesync_smart_reply_ext(as, &request);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug, forward ?
        "mailactivesync_smart_forward_ext" :
        "mailactivesync_smart_reply_ext", r);
    debug_auth_retry_hint(as, r);
    goto cleanup;
  }

  printf("%s self-test sent subject=%s source_server_id=%s\n", command_name,
      compose_subject, source_server_id);

 cleanup:
  free(source_server_id);
  return r;
}

static int run_resolve_self_test(mailactivesync * as,
    const struct sample_args * args, struct mailactivesync_options * options,
    int certificates)
{
  struct mailactivesync_resolve_recipients_request request;
  struct mailactivesync_resolve_recipients_result * result;
  clist * recipients;
  clistiter * response_iter;
  int r;

  if ((options != NULL) && !string_list_contains(options->commands,
      "ResolveRecipients")) {
    fprintf(stderr, "ResolveRecipients was not advertised by OPTIONS.\n");
    return MAILACTIVESYNC_ERROR_NOT_IMPLEMENTED;
  }

  recipients = clist_new();
  if (recipients == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;
  if (clist_append(recipients, (void *) args->login) < 0) {
    clist_free(recipients);
    return MAILACTIVESYNC_ERROR_MEMORY;
  }

  memset(&request, 0, sizeof(request));
  request.recipients = recipients;
  request.max_ambiguous_recipients = 5;
  if (certificates) {
    request.certificate_retrieval = 2;
    request.max_certificates = 3;
  }
  result = NULL;

  debug_step(args->debug, certificates ?
      "requesting ResolveRecipients certificate self-test" :
      "requesting ResolveRecipients self-test");
  r = mailactivesync_resolve_recipients_ext(as, &request, &result);
  clist_free(recipients);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    int command_error;
    int tolerated;

    command_error = result != NULL ?
      mailactivesync_resolve_recipients_status_to_error(result->status) :
      r;
    tolerated = 0;
    if ((command_error == MAILACTIVESYNC_NO_ERROR) &&
        (result != NULL) && (result->responses != NULL)) {
      for (response_iter = clist_begin(result->responses);
          response_iter != NULL; response_iter = clist_next(response_iter)) {
        struct mailactivesync_resolve_recipients_response * response;

        response = clist_content(response_iter);
        if (response == NULL)
          continue;
        if ((response->status >= 1) && (response->status <= 4))
          tolerated = 1;
        else {
          tolerated = 0;
          break;
        }
      }
    }
    if (!tolerated) {
      debug_error(args->debug, "mailactivesync_resolve_recipients_ext", r);
      debug_auth_retry_hint(as, r);
      mailactivesync_resolve_recipients_result_free(result);
      return r;
    }
    fprintf(stderr,
        "ResolveRecipients returned non-success per-input status; "
        "continuing to report server result.\n");
  }

  if (result == NULL) {
    fprintf(stderr, "ResolveRecipients returned no result.\n");
    return MAILACTIVESYNC_ERROR_PROTOCOL;
  }

  r = mailactivesync_resolve_recipients_status_to_error(result->status);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug, "ResolveRecipients status", r);
    mailactivesync_resolve_recipients_result_free(result);
    return r;
  }

  printf("ResolveRecipients %sself-test status=%d responses=%u\n",
      certificates ? "certificate " : "",
      result->status, list_count(result->responses));
  for (response_iter = result->responses != NULL ?
      clist_begin(result->responses) : NULL; response_iter != NULL;
      response_iter = clist_next(response_iter)) {
    struct mailactivesync_resolve_recipients_response * response;
    clistiter * recipient_iter;

    response = clist_content(response_iter);
    printf("ResolveRecipients to=%s status=%d recipient_count=%u\n",
        response->to != NULL ? response->to : "", response->status,
        (unsigned int) response->recipient_count);
    for (recipient_iter = response->recipients != NULL ?
        clist_begin(response->recipients) : NULL; recipient_iter != NULL;
        recipient_iter = clist_next(recipient_iter)) {
      struct mailactivesync_resolved_recipient * recipient;

      recipient = clist_content(recipient_iter);
      printf("  recipient type=%d name=%s address=%s\n", recipient->type,
          recipient->display_name != NULL ? recipient->display_name : "",
          recipient->email_address != NULL ? recipient->email_address : "");
      if (certificates) {
        printf("    certificates_status=%d certificate_count=%u "
            "certificates=%u mini_certificate=%s\n",
            recipient->certificates_status,
            (unsigned int) recipient->certificate_count,
            list_count(recipient->certificates),
            recipient->mini_certificate != NULL ? "<present>" : "");
      }
    }
  }

  mailactivesync_resolve_recipients_result_free(result);
  return MAILACTIVESYNC_NO_ERROR;
}

static int run_validate_cert_self_test(mailactivesync * as,
    const struct sample_args * args, struct mailactivesync_options * options)
{
  struct mailactivesync_validate_cert_request request;
  struct mailactivesync_validate_cert_result * result;
  struct mailactivesync_validate_cert_certificate * certificate;
  clist * certificates;
  clistiter * cur;
  char * certificate_data;
  unsigned int certificate_index;
  int r;

  if ((options != NULL) && !string_list_contains(options->commands,
      "ValidateCert")) {
    fprintf(stderr, "ValidateCert was not advertised by OPTIONS.\n");
    return MAILACTIVESYNC_ERROR_NOT_IMPLEMENTED;
  }
  if (args->validate_cert_file == NULL) {
    fprintf(stderr, "--validate-cert-file is required for ValidateCert.\n");
    return MAILACTIVESYNC_ERROR_BAD_STATE;
  }

  certificate_data = read_text_file_trimmed(args->validate_cert_file);
  if ((certificate_data == NULL) || (* certificate_data == '\0')) {
    fprintf(stderr, "Could not read certificate data from %s.\n",
        args->validate_cert_file);
    free(certificate_data);
    return MAILACTIVESYNC_ERROR_BAD_STATE;
  }

  certificates = clist_new();
  if (certificates == NULL) {
    free(certificate_data);
    return MAILACTIVESYNC_ERROR_MEMORY;
  }
  if (clist_append(certificates, certificate_data) < 0) {
    clist_free(certificates);
    free(certificate_data);
    return MAILACTIVESYNC_ERROR_MEMORY;
  }

  memset(&request, 0, sizeof(request));
  request.certificates = certificates;
  request.check_crl = 1;
  result = NULL;

  debug_step(args->debug, "requesting ValidateCert self-test");
  r = mailactivesync_validate_cert(as, &request, &result);
  clist_free(certificates);
  free(certificate_data);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug, "mailactivesync_validate_cert", r);
    debug_auth_retry_hint(as, r);
    mailactivesync_validate_cert_result_free(result);
    return r;
  }
  if (result == NULL) {
    fprintf(stderr, "ValidateCert returned no result.\n");
    return MAILACTIVESYNC_ERROR_PROTOCOL;
  }

  r = mailactivesync_validate_cert_status_to_error(result->status);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug, "ValidateCert status", r);
    mailactivesync_validate_cert_result_free(result);
    return r;
  }

  printf("ValidateCert self-test status=%d certificates=%u\n",
      result->status, list_count(result->certificates));
  certificate_index = 0;
  for (cur = result->certificates != NULL ?
      clist_begin(result->certificates) : NULL; cur != NULL;
      cur = clist_next(cur)) {
    clistiter * status_iter;

    certificate = clist_content(cur);
    printf("  certificate[%u] statuses=%u\n", certificate_index,
        list_count(certificate != NULL ? certificate->statuses : NULL));
    for (status_iter = (certificate != NULL) &&
        (certificate->statuses != NULL) ?
        clist_begin(certificate->statuses) : NULL; status_iter != NULL;
        status_iter = clist_next(status_iter)) {
      int * status;

      status = clist_content(status_iter);
      printf("    status=%d error=%d\n", status != NULL ? * status : 0,
          status != NULL ?
          mailactivesync_validate_cert_certificate_status_to_error(* status) :
          MAILACTIVESYNC_ERROR_PROTOCOL);
    }
    certificate_index ++;
  }

  mailactivesync_validate_cert_result_free(result);
  return MAILACTIVESYNC_NO_ERROR;
}

static void print_search_items(clist * items)
{
  clistiter * cur;

  for (cur = items != NULL ? clist_begin(items) : NULL; cur != NULL;
      cur = clist_next(cur)) {
    struct mailactivesync_mail_search_item * item;

    item = clist_content(cur);
    printf("  search item collection_id=%s server_id=%s long_id=%s\n",
        item->collection_id != NULL ? item->collection_id : "",
        item->server_id != NULL ? item->server_id : "",
        item->long_id != NULL ? item->long_id : "");
    if ((item->message != NULL) && (item->message->subject != NULL))
      printf("    subject=%s\n", item->message->subject);
  }
}

static void print_find_items(clist * items)
{
  clistiter * cur;

  for (cur = items != NULL ? clist_begin(items) : NULL; cur != NULL;
      cur = clist_next(cur)) {
    struct mailactivesync_mail_find_item * item;

    item = clist_content(cur);
    printf("  find item collection_id=%s server_id=%s class=%s "
        "has_attachments=%d\n",
        item->collection_id != NULL ? item->collection_id : "",
        item->server_id != NULL ? item->server_id : "",
        item->collection_class != NULL ? item->collection_class : "",
        item->has_attachments);
    if ((item->message != NULL) && (item->message->subject != NULL))
      printf("    subject=%s\n", item->message->subject);
    if (item->preview != NULL)
      printf("    preview=%s\n", item->preview);
  }
}

static int run_search_self_test(mailactivesync * as,
    const struct sample_args * args, const struct sample_state * state,
    struct mailactivesync_options * options)
{
  int ran_command;
  int r;

  ran_command = 0;

  if ((options == NULL) || string_list_contains(options->commands, "Search")) {
    struct mailactivesync_mail_search_request request;
    struct mailactivesync_mail_search_result * result;

    memset(&request, 0, sizeof(request));
    request.collection_id = state->inbox_id;
    request.free_text = args->login;
    request.range_start = 0;
    request.range_end = 9;
    result = NULL;

    debug_step(args->debug, "requesting Search self-test");
    r = mailactivesync_mail_search(as, &request, &result);
    if (r != MAILACTIVESYNC_NO_ERROR) {
      debug_error(args->debug, "mailactivesync_mail_search", r);
      debug_auth_retry_hint(as, r);
      mailactivesync_mail_search_result_free(result);
      return r;
    }
    if (result == NULL) {
      fprintf(stderr, "Search returned no result.\n");
      return MAILACTIVESYNC_ERROR_PROTOCOL;
    }
    r = mailactivesync_global_status_to_error(result->status);
    if (r != MAILACTIVESYNC_NO_ERROR) {
      debug_error(args->debug, "Search status", r);
      mailactivesync_mail_search_result_free(result);
      return r;
    }

    printf("Search self-test status=%d range=%s total=%u items=%u\n",
        result->status, result->range != NULL ? result->range : "",
        (unsigned int) result->total, list_count(result->items));
    print_search_items(result->items);
    mailactivesync_mail_search_result_free(result);
    ran_command = 1;
  }

  if ((options == NULL) || string_list_contains(options->commands, "Find")) {
    struct mailactivesync_mail_find_request request;
    struct mailactivesync_mail_find_result * result;
    char search_id[64];
    time_t now;

    now = time(NULL);
    snprintf(search_id, sizeof(search_id),
        "00000000-0000-4000-8000-0000%08lx",
        (unsigned long) now & 0xffffffffUL);
    memset(&request, 0, sizeof(request));
    request.search_id = search_id;
    request.collection_id = state->inbox_id;
    request.query = args->login;
    request.range_start = 0;
    request.range_end = 9;
    result = NULL;

    debug_step(args->debug, "requesting Find self-test");
    r = mailactivesync_mail_find(as, &request, &result);
    if (r != MAILACTIVESYNC_NO_ERROR) {
      debug_error(args->debug, "mailactivesync_mail_find", r);
      debug_auth_retry_hint(as, r);
      mailactivesync_mail_find_result_free(result);
      return r;
    }
    if (result == NULL) {
      fprintf(stderr, "Find returned no result.\n");
      return MAILACTIVESYNC_ERROR_PROTOCOL;
    }
    r = mailactivesync_global_status_to_error(result->status);
    if (r == MAILACTIVESYNC_NO_ERROR)
      r = mailactivesync_global_status_to_error(result->response_status);
    if (r != MAILACTIVESYNC_NO_ERROR) {
      debug_error(args->debug, "Find status", r);
      mailactivesync_mail_find_result_free(result);
      return r;
    }

    printf("Find self-test status=%d response_status=%d range=%s total=%u "
        "items=%u\n", result->status, result->response_status,
        result->range != NULL ? result->range : "",
        (unsigned int) result->total, list_count(result->items));
    print_find_items(result->items);
    mailactivesync_mail_find_result_free(result);
    ran_command = 1;
  }

  if (!ran_command) {
    fprintf(stderr, "Search and Find were not advertised by OPTIONS.\n");
    return MAILACTIVESYNC_ERROR_NOT_IMPLEMENTED;
  }

  return MAILACTIVESYNC_NO_ERROR;
}

static int string_list_has(clist * values, const char * value)
{
  clistiter * cur;

  for (cur = values != NULL ? clist_begin(values) : NULL; cur != NULL;
      cur = clist_next(cur)) {
    const char * item;

    item = clist_content(cur);
    if ((item != NULL) && (strcmp(item, value) == 0))
      return 1;
  }

  return 0;
}

static int run_ping_self_test(mailactivesync * as,
    const struct sample_args * args, struct sample_state * state,
    struct mailactivesync_options * options, int controlled_change)
{
  struct mailactivesync_ping_request request;
  struct mailactivesync_ping_result * result;
  clist * collection_ids;
  clistiter * cur;
  char subject[160];
  char client_id[160];
  char * server_id;
  time_t now;
  int r;

  if ((options != NULL) && !string_list_contains(options->commands, "Ping")) {
    fprintf(stderr, "Ping was not advertised by OPTIONS.\n");
    return MAILACTIVESYNC_ERROR_NOT_IMPLEMENTED;
  }
  if (controlled_change && (options != NULL) &&
      !string_list_contains(options->commands, "SendMail")) {
    fprintf(stderr, "SendMail was not advertised by OPTIONS.\n");
    return MAILACTIVESYNC_ERROR_NOT_IMPLEMENTED;
  }
  if (state->inbox_id == NULL)
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  server_id = NULL;
  if (controlled_change) {
    now = time(NULL);
    snprintf(subject, sizeof(subject),
        "libetpan ActiveSync ping change self test %lu",
        (unsigned long) now);
    snprintf(client_id, sizeof(client_id), "libetpan-ping-change-%lu",
        (unsigned long) now);
    r = send_compose_seed_message(as, args, subject, client_id);
    if (r != MAILACTIVESYNC_NO_ERROR)
      return r;
    printf("Ping change self-test sent source subject=%s\n", subject);
  }

  collection_ids = clist_new();
  if (collection_ids == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;
  if (clist_append(collection_ids, state->inbox_id) < 0) {
    clist_free(collection_ids);
    return MAILACTIVESYNC_ERROR_MEMORY;
  }

  memset(&request, 0, sizeof(request));
  request.heartbeat_interval = 60;
  request.collection_ids = collection_ids;
  result = NULL;

  debug_step(args->debug, controlled_change ?
      "requesting Ping controlled-change self-test" :
      "requesting Ping self-test");
  r = mailactivesync_ping(as, &request, &result);
  clist_free(collection_ids);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug, "mailactivesync_ping", r);
    debug_auth_retry_hint(as, r);
    mailactivesync_ping_result_free(result);
    return r;
  }
  if (result == NULL) {
    fprintf(stderr, "Ping returned no result.\n");
    return MAILACTIVESYNC_ERROR_PROTOCOL;
  }

  r = mailactivesync_ping_status_to_error(result->status);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug, "Ping status", r);
    mailactivesync_ping_result_free(result);
    return r;
  }

  printf("Ping self-test status=%d heartbeat_interval=%u max_folders=%u "
      "changed=%u\n", result->status,
      (unsigned int) result->heartbeat_interval,
      (unsigned int) result->max_folders,
      list_count(result->changed_collection_ids));
  for (cur = result->changed_collection_ids != NULL ?
      clist_begin(result->changed_collection_ids) : NULL; cur != NULL;
      cur = clist_next(cur))
    printf("  changed collection_id=%s\n", (char *) clist_content(cur));

  if (controlled_change &&
      !string_list_has(result->changed_collection_ids, state->inbox_id)) {
    fprintf(stderr, "Ping did not report the Inbox collection as changed.\n");
    mailactivesync_ping_result_free(result);
    return MAILACTIVESYNC_ERROR_PROTOCOL;
  }

  mailactivesync_ping_result_free(result);
  if (controlled_change) {
    r = find_generated_compose_source(as, args, state, subject, &server_id);
    if (r != MAILACTIVESYNC_NO_ERROR)
      return r;
    printf("Ping change self-test synced source server_id=%s\n", server_id);
    free(server_id);
  }

  return MAILACTIVESYNC_NO_ERROR;
}

static int create_attachment_message_file_reference(mailactivesync * as,
    const struct sample_args * args, struct sample_state * state,
    char ** file_reference);

static int delete_generated_draft_best_effort(mailactivesync * as,
    const struct sample_args * args, struct sample_state * state,
    const char * server_id);

static int run_attachment_self_test(mailactivesync * as,
    const struct sample_args * args, struct sample_state * state,
    struct mailactivesync_options * options)
{
  struct mailactivesync_attachment_data * data;
  char * generated_file_reference;
  int r;

  if ((options != NULL) && !string_list_contains(options->commands,
      "ItemOperations")) {
    fprintf(stderr, "ItemOperations was not advertised by OPTIONS.\n");
    return MAILACTIVESYNC_ERROR_NOT_IMPLEMENTED;
  }
  generated_file_reference = NULL;
  if (state->attachment_file_reference == NULL) {
    r = create_attachment_message_file_reference(as, args, state,
        &generated_file_reference);
    if (r != MAILACTIVESYNC_NO_ERROR)
      return r;
  }

  data = NULL;
  debug_step(args->debug, "requesting attachment fetch self-test");
  r = mailactivesync_item_operations_fetch_attachment(as,
      state->attachment_file_reference != NULL ?
      state->attachment_file_reference : generated_file_reference,
      "0-1023", &data);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug,
        "mailactivesync_item_operations_fetch_attachment", r);
    debug_auth_retry_hint(as, r);
    mailactivesync_attachment_data_free(data);
    free(generated_file_reference);
    return r;
  }
  if (data == NULL) {
    fprintf(stderr, "Attachment fetch returned no result.\n");
    free(generated_file_reference);
    return MAILACTIVESYNC_ERROR_PROTOCOL;
  }

  r = mailactivesync_item_operations_status_to_error(data->status);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug, "attachment fetch status", r);
    mailactivesync_attachment_data_free(data);
    free(generated_file_reference);
    return r;
  }

  printf("Attachment self-test status=%d range=%s total=%u bytes=%u "
      "file_reference=%s\n", data->status,
      data->range != NULL ? data->range : "",
      (unsigned int) data->total, (unsigned int) data->data_len,
      data->file_reference != NULL ? data->file_reference : "");
  mailactivesync_attachment_data_free(data);
  free(generated_file_reference);
  return MAILACTIVESYNC_NO_ERROR;
}

static int run_drafts_initial_sync(mailactivesync * as,
    const struct sample_args * args, struct sample_state * state)
{
  struct mailactivesync_sync_request * request;
  struct mailactivesync_sync_result * result;
  const char * sync_key;
  int r;

  request = NULL;
  result = NULL;
  if (state->drafts_id == NULL)
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  sync_key = state->drafts_sync_key != NULL ? state->drafts_sync_key : "0";
  if (!is_initial_sync_key(sync_key))
    return MAILACTIVESYNC_NO_ERROR;

  request = mailactivesync_sync_request_new(state->drafts_id, sync_key);
  if (request == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  mailactivesync_sync_request_set_get_changes(request, 0);
  debug_step(args->debug, "requesting Drafts initial Sync");
  r = mailactivesync_sync(as, request, &result);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug, "mailactivesync_sync Drafts init", r);
    debug_auth_retry_hint(as, r);
    goto cleanup;
  }

  r = mailactivesync_sync_status_to_error(result->status);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug, "Drafts initial Sync status", r);
    if (r == MAILACTIVESYNC_ERROR_FOLDER_RESYNC_REQUIRED)
      sample_state_clear_drafts_sync(state);
    else if (r == MAILACTIVESYNC_ERROR_ACCOUNT_RESYNC_REQUIRED)
      sample_state_clear_account_sync(state);
    goto cleanup;
  }

  if (result->sync_key_from_response && (result->sync_key != NULL))
    set_string(&state->drafts_sync_key, result->sync_key);

 cleanup:
  mailactivesync_sync_result_free(result);
  mailactivesync_sync_request_free(request);
  return r;
}

static int update_drafts_sync_key_from_result(struct sample_state * state,
    struct mailactivesync_sync_result * result)
{
  if ((result != NULL) && result->sync_key_from_response &&
      (result->sync_key != NULL))
    return set_string(&state->drafts_sync_key, result->sync_key) == 0 ?
        MAILACTIVESYNC_NO_ERROR : MAILACTIVESYNC_ERROR_MEMORY;

  return MAILACTIVESYNC_NO_ERROR;
}

static int verify_draft_fetch(mailactivesync * as,
    const struct sample_args * args, struct sample_state * state,
    const char * server_id, const char * label)
{
  struct mailactivesync_item * item;
  int r;

  if ((state->drafts_id == NULL) || (server_id == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  item = NULL;
  debug_step(args->debug, "fetching generated draft for verification");
  r = mailactivesync_item_operations_fetch(as, state->drafts_id, server_id,
      &item);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug, "mailactivesync_item_operations_fetch draft", r);
    debug_auth_retry_hint(as, r);
    mailactivesync_item_free(item);
    return r;
  }
  if (item == NULL) {
    fprintf(stderr, "Draft fetch returned no item.\n");
    return MAILACTIVESYNC_ERROR_PROTOCOL;
  }

  r = mailactivesync_item_operations_status_to_error(item->status);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug, "draft fetch item status", r);
    mailactivesync_item_free(item);
    return r;
  }
  if ((item->collection_id == NULL) || (item->server_id == NULL) ||
      (strcmp(item->collection_id, state->drafts_id) != 0) ||
      (strcmp(item->server_id, server_id) != 0)) {
    fprintf(stderr, "Draft fetch returned the wrong item identity.\n");
    mailactivesync_item_free(item);
    return MAILACTIVESYNC_ERROR_PROTOCOL;
  }
  if ((item->body == NULL) && (item->mime == NULL)) {
    fprintf(stderr, "Draft fetch returned no body or MIME data.\n");
    mailactivesync_item_free(item);
    return MAILACTIVESYNC_ERROR_PROTOCOL;
  }

  printf("Draft self-test fetched %s server_id=%s body=%s mime_bytes=%u\n",
      label, server_id, item->body != NULL ? "<present>" : "",
      (unsigned int) item->mime_len);
  mailactivesync_item_free(item);
  return MAILACTIVESYNC_NO_ERROR;
}

static int create_attachment_message_file_reference(mailactivesync * as,
    const struct sample_args * args, struct sample_state * state,
    char ** file_reference)
{
  char message[4096];
  char client_id[160];
  char subject[160];
  char * server_id;
  struct mailactivesync_composemail_request request;
  time_t now;
  int len;
  int r;

  if ((state->inbox_id == NULL) || (state->inbox_sync_key == NULL) ||
      (file_reference == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * file_reference = NULL;
  server_id = NULL;
  now = time(NULL);
  snprintf(client_id, sizeof(client_id),
      "libetpan-attachment-message-%lu", (unsigned long) now);
  snprintf(subject, sizeof(subject),
      "libetpan ActiveSync attachment self test %lu",
      (unsigned long) now);

  len = snprintf(message, sizeof(message),
      "From: <%s>\r\n"
      "To: <%s>\r\n"
      "Subject: %s\r\n"
      "MIME-Version: 1.0\r\n"
      "Content-Type: multipart/mixed; boundary=\"libetpan-attach\"\r\n"
      "\r\n"
      "--libetpan-attach\r\n"
      "Content-Type: text/plain; charset=utf-8\r\n"
      "Content-Transfer-Encoding: 7bit\r\n"
      "\r\n"
      "This generated message was created by tests/activesync-sample "
      "--attachment-self-test.\r\n"
      "ClientId: %s\r\n"
      "\r\n"
      "--libetpan-attach\r\n"
      "Content-Type: text/plain; name=\"libetpan-attachment-self-test.txt\"\r\n"
      "Content-Disposition: attachment; "
      "filename=\"libetpan-attachment-self-test.txt\"\r\n"
      "Content-Transfer-Encoding: base64\r\n"
      "\r\n"
      "bGliZXRwYW4gYXR0YWNobWVudCBzZWxmLXRlc3QK\r\n"
      "--libetpan-attach--\r\n",
      args->login, args->login, subject, client_id);
  if ((len < 0) || ((size_t) len >= sizeof(message)))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  memset(&request, 0, sizeof(request));
  request.client_id = client_id;
  request.mime_message = message;
  request.mime_message_len = (size_t) len;
  request.save_in_sent = 1;

  debug_step(args->debug, "sending generated attachment message");
  r = mailactivesync_send_mail_ext(as, &request);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug, "mailactivesync_send_mail_ext attachment", r);
    debug_auth_retry_hint(as, r);
    return r;
  }

  r = find_generated_compose_source_with_body_type(as, args, state, subject,
      MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_PLAIN_TEXT, &server_id);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;
  if (state->attachment_file_reference != NULL) {
    * file_reference = strdup(state->attachment_file_reference);
    if (* file_reference == NULL)
      r = MAILACTIVESYNC_ERROR_MEMORY;
    else {
      printf("Attachment self-test synced message server_id=%s "
          "file_reference=%s\n", server_id, * file_reference);
      r = MAILACTIVESYNC_NO_ERROR;
    }
  }
  else {
    fprintf(stderr, "Generated attachment message had no FileReference.\n");
    r = MAILACTIVESYNC_ERROR_PROTOCOL;
  }

 cleanup:
  if (r != MAILACTIVESYNC_NO_ERROR) {
    free(* file_reference);
    * file_reference = NULL;
  }
  free(server_id);
  return r;
}

static int add_multi_sync_request(clist * requests,
    struct mailactivesync_sync_request ** request_slot,
    const struct sample_args * args, const char * collection_id,
    const char * sync_key)
{
  struct mailactivesync_sync_request * request;
  int get_changes;
  int r;

  request = mailactivesync_sync_request_new(collection_id, sync_key);
  if (request == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  get_changes = !is_initial_sync_key(sync_key);
  r = mailactivesync_sync_request_set_get_changes(request, get_changes);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  r = mailactivesync_sync_request_set_collection_class(request, "Email");
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  if (get_changes) {
    r = mailactivesync_sync_request_set_window_size(request, 5);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
    r = mailactivesync_sync_request_set_body_preference(request,
        args->body_type, 64 * 1024);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto err;
  }
  if (clist_append(requests, request) < 0) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto err;
  }

  * request_slot = request;
  return MAILACTIVESYNC_NO_ERROR;

 err:
  mailactivesync_sync_request_free(request);
  return r;
}

static int update_sync_key_from_collection(char ** target,
    struct mailactivesync_sync_result * collection)
{
  if ((collection != NULL) && collection->sync_key_from_response &&
      (collection->sync_key != NULL))
    return set_string(target, collection->sync_key) == 0 ?
        MAILACTIVESYNC_NO_ERROR : MAILACTIVESYNC_ERROR_MEMORY;

  return MAILACTIVESYNC_NO_ERROR;
}

static int run_multi_sync_self_test(mailactivesync * as,
    const struct sample_args * args, struct sample_state * state)
{
  clist * requests;
  struct mailactivesync_sync_request * inbox_request;
  struct mailactivesync_sync_request * drafts_request;
  struct mailactivesync_sync_result * result;
  struct mailactivesync_sync_result * collection;
  const char * inbox_sync_key;
  const char * drafts_sync_key;
  int r;

  requests = NULL;
  inbox_request = NULL;
  drafts_request = NULL;
  result = NULL;
  if ((state->inbox_id == NULL) || (state->drafts_id == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  requests = clist_new();
  if (requests == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  inbox_sync_key = state->inbox_sync_key != NULL ? state->inbox_sync_key : "0";
  drafts_sync_key = state->drafts_sync_key != NULL ?
      state->drafts_sync_key : "0";
  r = add_multi_sync_request(requests, &inbox_request, args, state->inbox_id,
      inbox_sync_key);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;
  r = add_multi_sync_request(requests, &drafts_request, args,
      state->drafts_id, drafts_sync_key);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;

  debug_step(args->debug, "requesting multi-folder Sync self-test");
  r = mailactivesync_sync_multi(as, requests, &result);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug, "mailactivesync_sync_multi", r);
    debug_auth_retry_hint(as, r);
    goto cleanup;
  }
  if (result->empty_response) {
    printf("Multi Sync self-test returned empty no-change response\n");
    goto cleanup;
  }

  collection = mailactivesync_sync_result_collection(result,
      state->inbox_id);
  if (collection == NULL)
    printf("Multi Sync self-test Inbox omitted as no-change collection\n");
  else {
    r = mailactivesync_sync_result_collection_status_to_error(result,
        state->inbox_id);
    if (r != MAILACTIVESYNC_NO_ERROR) {
      debug_error(args->debug, "multi Sync Inbox status", r);
      goto cleanup;
    }
    r = update_sync_key_from_collection(&state->inbox_sync_key, collection);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto cleanup;
    printf("Multi Sync self-test Inbox sync_key=%s added=%u changed=%u "
        "deleted=%u more=%d\n", collection->sync_key != NULL ?
        collection->sync_key : "", list_count(collection->added),
        list_count(collection->changed), list_count(collection->deleted),
        collection->more_available);
  }

  collection = mailactivesync_sync_result_collection(result,
      state->drafts_id);
  if (collection == NULL)
    printf("Multi Sync self-test Drafts omitted as no-change collection\n");
  else {
    r = mailactivesync_sync_result_collection_status_to_error(result,
        state->drafts_id);
    if (r != MAILACTIVESYNC_NO_ERROR) {
      debug_error(args->debug, "multi Sync Drafts status", r);
      goto cleanup;
    }
    r = update_sync_key_from_collection(&state->drafts_sync_key, collection);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto cleanup;
    printf("Multi Sync self-test Drafts sync_key=%s added=%u changed=%u "
        "deleted=%u more=%d\n", collection->sync_key != NULL ?
        collection->sync_key : "", list_count(collection->added),
        list_count(collection->changed), list_count(collection->deleted),
        collection->more_available);
  }

 cleanup:
  mailactivesync_sync_result_free(result);
  clist_free(requests);
  mailactivesync_sync_request_free(inbox_request);
  mailactivesync_sync_request_free(drafts_request);
  return r;
}

static int run_draft_self_test(mailactivesync * as,
    const struct sample_args * args, struct sample_state * state)
{
  struct mailactivesync_draft draft;
  struct mailactivesync_sync_result * result;
  struct mailactivesync_sync_command_response * response;
  char client_id[160];
  char subject[160];
  char updated_subject[160];
  char body[256];
  char updated_body[256];
  char * server_id;
  time_t now;
  int deleted;
  int r;

  result = NULL;
  server_id = NULL;
  deleted = 0;
  if ((state->drafts_id == NULL) || (state->drafts_sync_key == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  now = time(NULL);
  snprintf(client_id, sizeof(client_id), "libetpan-draft-self-%lu",
      (unsigned long) now);
  snprintf(subject, sizeof(subject),
      "libetpan ActiveSync draft self test %lu", (unsigned long) now);
  snprintf(updated_subject, sizeof(updated_subject),
      "libetpan ActiveSync draft self test updated %lu",
      (unsigned long) now);
  snprintf(body, sizeof(body),
      "This generated draft was created by tests/activesync-sample "
      "--draft-self-test.\n");
  snprintf(updated_body, sizeof(updated_body),
      "This generated draft was updated by tests/activesync-sample "
      "--draft-self-test.\n");

  memset(&draft, 0, sizeof(draft));
  draft.to = args->login;
  draft.subject = subject;
  draft.body_type = MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_PLAIN_TEXT;
  draft.body = body;

  debug_step(args->debug, "requesting draft Add self-test");
  r = mailactivesync_add_draft(as, state->drafts_id, state->drafts_sync_key,
      client_id, &draft, &result);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug, "mailactivesync_add_draft", r);
    debug_auth_retry_hint(as, r);
    goto cleanup;
  }
  r = mailactivesync_sync_result_command_response_status_to_error(result,
      MAILACTIVESYNC_SYNC_COMMAND_ADD, client_id, NULL);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug, "draft Add response", r);
    goto cleanup;
  }
  response = mailactivesync_sync_result_command_response(result,
      MAILACTIVESYNC_SYNC_COMMAND_ADD, client_id, NULL);
  if ((response == NULL) || (response->server_id == NULL)) {
    fprintf(stderr, "Draft Add did not return a server id.\n");
    r = MAILACTIVESYNC_ERROR_PROTOCOL;
    goto cleanup;
  }
  server_id = strdup(response->server_id);
  if (server_id == NULL) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto cleanup;
  }
  r = update_drafts_sync_key_from_result(state, result);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;
  printf("Draft self-test created server_id=%s subject=%s\n", server_id,
      subject);
  mailactivesync_sync_result_free(result);
  result = NULL;
  r = verify_draft_fetch(as, args, state, server_id, "created");
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;

  draft.subject = updated_subject;
  draft.body = updated_body;
  debug_step(args->debug, "requesting draft Change self-test");
  r = mailactivesync_update_draft(as, state->drafts_id,
      state->drafts_sync_key, server_id, &draft, &result);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug, "mailactivesync_update_draft", r);
    debug_auth_retry_hint(as, r);
    goto cleanup;
  }
  r = mailactivesync_sync_result_command_response_status_to_error(result,
      MAILACTIVESYNC_SYNC_COMMAND_CHANGE, NULL, server_id);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug, "draft Change response", r);
    goto cleanup;
  }
  r = update_drafts_sync_key_from_result(state, result);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;
  printf("Draft self-test updated server_id=%s subject=%s\n", server_id,
      updated_subject);
  mailactivesync_sync_result_free(result);
  result = NULL;
  r = verify_draft_fetch(as, args, state, server_id, "updated");
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;

  debug_step(args->debug, "requesting draft Delete self-test");
  r = mailactivesync_delete_message(as, state->drafts_id,
      state->drafts_sync_key, server_id, 0, &result);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug, "mailactivesync_delete_message draft", r);
    debug_auth_retry_hint(as, r);
    goto cleanup;
  }
  r = mailactivesync_sync_result_command_response_status_to_error(result,
      MAILACTIVESYNC_SYNC_COMMAND_DELETE, NULL, server_id);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug, "draft Delete response", r);
    goto cleanup;
  }
  r = update_drafts_sync_key_from_result(state, result);
  if (r == MAILACTIVESYNC_NO_ERROR)
    printf("Draft self-test deleted server_id=%s\n", server_id);
  if (r == MAILACTIVESYNC_NO_ERROR)
    deleted = 1;

 cleanup:
  if (!deleted && (server_id != NULL))
    delete_generated_draft_best_effort(as, args, state, server_id);
  free(server_id);
  mailactivesync_sync_result_free(result);
  return r;
}

static int delete_generated_draft_best_effort(mailactivesync * as,
    const struct sample_args * args, struct sample_state * state,
    const char * server_id)
{
  struct mailactivesync_sync_result * result;
  int r;

  if ((server_id == NULL) || (state->drafts_id == NULL) ||
      (state->drafts_sync_key == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  result = NULL;
  r = mailactivesync_delete_message(as, state->drafts_id,
      state->drafts_sync_key, server_id, 0, &result);
  if (r == MAILACTIVESYNC_NO_ERROR) {
    update_drafts_sync_key_from_result(state, result);
    printf("Cleaned up generated draft server_id=%s\n", server_id);
  }
  else
    debug_error(args->debug, "generated draft cleanup", r);
  mailactivesync_sync_result_free(result);
  return r;
}

static int run_mutation_self_test(mailactivesync * as,
    const struct sample_args * args, struct sample_state * state)
{
  struct mailactivesync_draft draft;
  struct mailactivesync_sync_result * result;
  struct mailactivesync_sync_command_response * response;
  char client_id[160];
  char subject[160];
  char body[256];
  char * server_id;
  time_t now;
  int deleted;
  int r;

  result = NULL;
  server_id = NULL;
  deleted = 0;
  if ((state->drafts_id == NULL) || (state->drafts_sync_key == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  now = time(NULL);
  snprintf(client_id, sizeof(client_id), "libetpan-mutation-draft-%lu",
      (unsigned long) now);
  snprintf(subject, sizeof(subject),
      "libetpan ActiveSync mutation self test %lu", (unsigned long) now);
  snprintf(body, sizeof(body),
      "This generated draft was created by tests/activesync-sample "
      "--mutation-self-test.\n");

  memset(&draft, 0, sizeof(draft));
  draft.to = args->login;
  draft.subject = subject;
  draft.body_type = MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_PLAIN_TEXT;
  draft.body = body;

  debug_step(args->debug, "creating generated draft for mutation self-test");
  r = mailactivesync_add_draft(as, state->drafts_id, state->drafts_sync_key,
      client_id, &draft, &result);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug, "mailactivesync_add_draft mutation", r);
    debug_auth_retry_hint(as, r);
    goto cleanup;
  }
  r = mailactivesync_sync_result_command_response_status_to_error(result,
      MAILACTIVESYNC_SYNC_COMMAND_ADD, client_id, NULL);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug, "mutation draft Add response", r);
    goto cleanup;
  }
  response = mailactivesync_sync_result_command_response(result,
      MAILACTIVESYNC_SYNC_COMMAND_ADD, client_id, NULL);
  if ((response == NULL) || (response->server_id == NULL)) {
    fprintf(stderr, "Mutation draft Add did not return a server id.\n");
    r = MAILACTIVESYNC_ERROR_PROTOCOL;
    goto cleanup;
  }
  server_id = strdup(response->server_id);
  if (server_id == NULL) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto cleanup;
  }
  r = update_drafts_sync_key_from_result(state, result);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;
  printf("Mutation self-test created server_id=%s subject=%s\n", server_id,
      subject);
  mailactivesync_sync_result_free(result);
  result = NULL;

  debug_step(args->debug, "marking generated draft unread");
  r = mailactivesync_mark_read(as, state->drafts_id, state->drafts_sync_key,
      server_id, 0, &result);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug, "mailactivesync_mark_read unread", r);
    debug_auth_retry_hint(as, r);
    goto cleanup;
  }
  r = mailactivesync_sync_result_command_response_status_to_error(result,
      MAILACTIVESYNC_SYNC_COMMAND_CHANGE, NULL, server_id);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug, "mark unread response", r);
    goto cleanup;
  }
  r = update_drafts_sync_key_from_result(state, result);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;
  printf("Mutation self-test marked unread server_id=%s\n", server_id);
  mailactivesync_sync_result_free(result);
  result = NULL;

  debug_step(args->debug, "marking generated draft read");
  r = mailactivesync_mark_read(as, state->drafts_id, state->drafts_sync_key,
      server_id, 1, &result);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug, "mailactivesync_mark_read read", r);
    debug_auth_retry_hint(as, r);
    goto cleanup;
  }
  r = mailactivesync_sync_result_command_response_status_to_error(result,
      MAILACTIVESYNC_SYNC_COMMAND_CHANGE, NULL, server_id);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug, "mark read response", r);
    goto cleanup;
  }
  r = update_drafts_sync_key_from_result(state, result);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;
  printf("Mutation self-test marked read server_id=%s\n", server_id);
  mailactivesync_sync_result_free(result);
  result = NULL;

  debug_step(args->debug, "flagging generated draft");
  r = mailactivesync_set_flagged(as, state->drafts_id,
      state->drafts_sync_key, server_id, 1, &result);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug, "mailactivesync_set_flagged flag", r);
    debug_auth_retry_hint(as, r);
    goto cleanup;
  }
  r = mailactivesync_sync_result_command_response_status_to_error(result,
      MAILACTIVESYNC_SYNC_COMMAND_CHANGE, NULL, server_id);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug, "flag response", r);
    goto cleanup;
  }
  r = update_drafts_sync_key_from_result(state, result);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;
  printf("Mutation self-test flagged server_id=%s\n", server_id);
  mailactivesync_sync_result_free(result);
  result = NULL;

  debug_step(args->debug, "unflagging generated draft");
  r = mailactivesync_set_flagged(as, state->drafts_id,
      state->drafts_sync_key, server_id, 0, &result);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug, "mailactivesync_set_flagged unflag", r);
    debug_auth_retry_hint(as, r);
    goto cleanup;
  }
  r = mailactivesync_sync_result_command_response_status_to_error(result,
      MAILACTIVESYNC_SYNC_COMMAND_CHANGE, NULL, server_id);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug, "unflag response", r);
    goto cleanup;
  }
  r = update_drafts_sync_key_from_result(state, result);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;
  printf("Mutation self-test unflagged server_id=%s\n", server_id);
  mailactivesync_sync_result_free(result);
  result = NULL;

  debug_step(args->debug, "deleting generated draft after mutation self-test");
  r = mailactivesync_delete_message(as, state->drafts_id,
      state->drafts_sync_key, server_id, 0, &result);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug, "mailactivesync_delete_message mutation", r);
    debug_auth_retry_hint(as, r);
    goto cleanup;
  }
  r = mailactivesync_sync_result_command_response_status_to_error(result,
      MAILACTIVESYNC_SYNC_COMMAND_DELETE, NULL, server_id);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug, "mutation delete response", r);
    goto cleanup;
  }
  r = update_drafts_sync_key_from_result(state, result);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;
  deleted = 1;
  printf("Mutation self-test deleted server_id=%s\n", server_id);

 cleanup:
  if (!deleted && (server_id != NULL))
    delete_generated_draft_best_effort(as, args, state, server_id);
  free(server_id);
  mailactivesync_sync_result_free(result);
  return r;
}

static int verify_folder_resync(mailactivesync * as,
    const struct sample_args * args, struct sample_state * state,
    const char * folder_id, const char * expected_name, int expected_present)
{
  struct mailactivesync_folder_sync_result * result;
  struct mailactivesync_folder * folder;
  int r;

  result = NULL;
  if (folder_id == NULL)
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  debug_step(args->debug, "verifying folder self-test with FolderSync resync");
  r = mailactivesync_folder_resync(as, &result);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug, "mailactivesync_folder_resync self-test", r);
    debug_auth_retry_hint(as, r);
    goto cleanup;
  }
  r = mailactivesync_folder_sync_status_to_error(result->status);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug, "folder self-test FolderSync status", r);
    goto cleanup;
  }
  if (result->sync_key != NULL) {
    if (set_string(&state->folder_sync_key, result->sync_key) < 0) {
      r = MAILACTIVESYNC_ERROR_MEMORY;
      goto cleanup;
    }
  }

  folder = find_folder_by_id(result, folder_id);
  if (expected_present) {
    if (folder == NULL) {
      fprintf(stderr, "FolderSync did not return temporary folder id=%s.\n",
          folder_id);
      r = MAILACTIVESYNC_ERROR_PROTOCOL;
      goto cleanup;
    }
    if ((expected_name != NULL) &&
        ((folder->display_name == NULL) ||
          (strcmp(folder->display_name, expected_name) != 0))) {
      fprintf(stderr, "FolderSync returned unexpected folder name for %s.\n",
          folder_id);
      r = MAILACTIVESYNC_ERROR_PROTOCOL;
      goto cleanup;
    }
    printf("Folder self-test verified id=%s folder=%s\n", folder_id,
        folder->display_name != NULL ? folder->display_name : "");
  }
  else {
    if (folder != NULL) {
      fprintf(stderr, "FolderSync still returned deleted folder id=%s.\n",
          folder_id);
      r = MAILACTIVESYNC_ERROR_PROTOCOL;
      goto cleanup;
    }
    printf("Folder self-test verified deleted id=%s\n", folder_id);
  }

 cleanup:
  mailactivesync_folder_sync_result_free(result);
  return r;
}

static int run_folder_self_test(mailactivesync * as,
    const struct sample_args * args, struct sample_state * state,
    struct mailactivesync_options * options)
{
  struct mailactivesync_folder_mutation_result * result;
  char folder_name[160];
  char updated_name[160];
  char * folder_id;
  time_t now;
  int deleted;
  int r;

  if ((options != NULL) &&
      (!string_list_contains(options->commands, "FolderCreate") ||
        !string_list_contains(options->commands, "FolderUpdate") ||
        !string_list_contains(options->commands, "FolderDelete"))) {
    fprintf(stderr,
        "FolderCreate, FolderUpdate, or FolderDelete was not advertised.\n");
    return MAILACTIVESYNC_ERROR_NOT_IMPLEMENTED;
  }
  if (state->folder_sync_key == NULL)
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  result = NULL;
  folder_id = NULL;
  deleted = 0;

  now = time(NULL);
  snprintf(folder_name, sizeof(folder_name), "libetpan folder self %lu",
      (unsigned long) now);
  snprintf(updated_name, sizeof(updated_name),
      "libetpan folder self updated %lu", (unsigned long) now);

  debug_step(args->debug, "creating temporary folder self-test folder");
  r = mailactivesync_folder_create(as, state->folder_sync_key, "0",
      folder_name, 12, &result);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug, "mailactivesync_folder_create self-test", r);
    debug_auth_retry_hint(as, r);
    goto cleanup;
  }
  r = mailactivesync_folder_mutation_status_to_error(result->status);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug, "folder self-test FolderCreate status", r);
    goto cleanup;
  }
  if ((result->sync_key == NULL) || (result->server_id == NULL)) {
    r = MAILACTIVESYNC_ERROR_PROTOCOL;
    goto cleanup;
  }
  if (set_string(&state->folder_sync_key, result->sync_key) < 0) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto cleanup;
  }
  folder_id = strdup(result->server_id);
  if (folder_id == NULL) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto cleanup;
  }
  printf("Folder self-test created folder=%s id=%s\n", folder_name,
      folder_id);
  mailactivesync_folder_mutation_result_free(result);
  result = NULL;
  r = verify_folder_resync(as, args, state, folder_id, folder_name, 1);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;

  debug_step(args->debug, "updating temporary folder self-test folder");
  r = mailactivesync_folder_update(as, state->folder_sync_key, folder_id,
      "0", updated_name, &result);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug, "mailactivesync_folder_update self-test", r);
    debug_auth_retry_hint(as, r);
    goto cleanup;
  }
  r = mailactivesync_folder_mutation_status_to_error(result->status);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug, "folder self-test FolderUpdate status", r);
    goto cleanup;
  }
  if (result->sync_key == NULL) {
    r = MAILACTIVESYNC_ERROR_PROTOCOL;
    goto cleanup;
  }
  if (set_string(&state->folder_sync_key, result->sync_key) < 0) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto cleanup;
  }
  printf("Folder self-test updated id=%s folder=%s\n", folder_id,
      updated_name);
  mailactivesync_folder_mutation_result_free(result);
  result = NULL;
  r = verify_folder_resync(as, args, state, folder_id, updated_name, 1);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;

  debug_step(args->debug, "deleting temporary folder self-test folder");
  r = mailactivesync_folder_delete(as, state->folder_sync_key, folder_id,
      &result);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug, "mailactivesync_folder_delete self-test", r);
    debug_auth_retry_hint(as, r);
    goto cleanup;
  }
  r = mailactivesync_folder_mutation_status_to_error(result->status);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug, "folder self-test FolderDelete status", r);
    goto cleanup;
  }
  if (result->sync_key == NULL) {
    r = MAILACTIVESYNC_ERROR_PROTOCOL;
    goto cleanup;
  }
  if (set_string(&state->folder_sync_key, result->sync_key) < 0) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto cleanup;
  }
  deleted = 1;
  printf("Folder self-test deleted id=%s\n", folder_id);
  mailactivesync_folder_mutation_result_free(result);
  result = NULL;
  r = verify_folder_resync(as, args, state, folder_id, NULL, 0);

 cleanup:
  if (!deleted && (folder_id != NULL) &&
      (state->folder_sync_key != NULL)) {
    struct mailactivesync_folder_mutation_result * delete_result;
    int delete_error;

    delete_result = NULL;
    delete_error = mailactivesync_folder_delete(as, state->folder_sync_key,
        folder_id, &delete_result);
    if (delete_error == MAILACTIVESYNC_NO_ERROR) {
      if (delete_result->sync_key != NULL)
        set_string(&state->folder_sync_key, delete_result->sync_key);
      printf("Cleaned up temporary folder id=%s\n", folder_id);
    }
    else
      debug_error(args->debug, "folder self-test cleanup", delete_error);
    mailactivesync_folder_mutation_result_free(delete_result);
  }
  free(folder_id);
  mailactivesync_folder_mutation_result_free(result);
  return r;
}

static int run_move_self_test(mailactivesync * as,
    const struct sample_args * args, struct sample_state * state,
    struct mailactivesync_options * options)
{
  struct mailactivesync_folder_mutation_result * folder_result;
  struct mailactivesync_sync_result * sync_result;
  struct mailactivesync_sync_command_response * command_response;
  struct mailactivesync_move_items_result * move_result;
  struct mailactivesync_move_response * move_response;
  struct mailactivesync_draft draft;
  struct mailactivesync_move move;
  clist * moves;
  char folder_name[160];
  char client_id[160];
  char subject[160];
  char body[256];
  char * temp_folder_id;
  char * draft_server_id;
  time_t now;
  int moved;
  int r;

  if ((options != NULL) &&
      (!string_list_contains(options->commands, "FolderCreate") ||
        !string_list_contains(options->commands, "FolderDelete") ||
        !string_list_contains(options->commands, "MoveItems"))) {
    fprintf(stderr,
        "FolderCreate, FolderDelete, or MoveItems was not advertised.\n");
    return MAILACTIVESYNC_ERROR_NOT_IMPLEMENTED;
  }
  if ((state->folder_sync_key == NULL) || (state->drafts_id == NULL) ||
      (state->drafts_sync_key == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  folder_result = NULL;
  sync_result = NULL;
  move_result = NULL;
  moves = NULL;
  temp_folder_id = NULL;
  draft_server_id = NULL;
  moved = 0;

  now = time(NULL);
  snprintf(folder_name, sizeof(folder_name), "libetpan move self %lu",
      (unsigned long) now);
  snprintf(client_id, sizeof(client_id), "libetpan-move-draft-%lu",
      (unsigned long) now);
  snprintf(subject, sizeof(subject),
      "libetpan ActiveSync move self test %lu", (unsigned long) now);
  snprintf(body, sizeof(body),
      "This generated draft was created for tests/activesync-sample "
      "--move-self-test.\n");

  debug_step(args->debug, "creating temporary move folder");
  r = mailactivesync_folder_create(as, state->folder_sync_key, "0",
      folder_name, 12, &folder_result);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug, "mailactivesync_folder_create move", r);
    debug_auth_retry_hint(as, r);
    goto cleanup;
  }
  r = mailactivesync_folder_mutation_status_to_error(folder_result->status);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug, "move FolderCreate status", r);
    goto cleanup;
  }
  if ((folder_result->sync_key == NULL) ||
      (folder_result->server_id == NULL)) {
    r = MAILACTIVESYNC_ERROR_PROTOCOL;
    goto cleanup;
  }
  if (set_string(&state->folder_sync_key, folder_result->sync_key) < 0) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto cleanup;
  }
  temp_folder_id = strdup(folder_result->server_id);
  if (temp_folder_id == NULL) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto cleanup;
  }
  printf("Move self-test created folder=%s id=%s\n", folder_name,
      temp_folder_id);
  mailactivesync_folder_mutation_result_free(folder_result);
  folder_result = NULL;

  memset(&draft, 0, sizeof(draft));
  draft.to = args->login;
  draft.subject = subject;
  draft.body_type = MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_PLAIN_TEXT;
  draft.body = body;

  debug_step(args->debug, "creating generated draft for MoveItems");
  r = mailactivesync_add_draft(as, state->drafts_id, state->drafts_sync_key,
      client_id, &draft, &sync_result);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug, "mailactivesync_add_draft move", r);
    debug_auth_retry_hint(as, r);
    goto cleanup;
  }
  r = mailactivesync_sync_result_command_response_status_to_error(sync_result,
      MAILACTIVESYNC_SYNC_COMMAND_ADD, client_id, NULL);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug, "move draft Add response", r);
    goto cleanup;
  }
  command_response = mailactivesync_sync_result_command_response(sync_result,
      MAILACTIVESYNC_SYNC_COMMAND_ADD, client_id, NULL);
  if ((command_response == NULL) || (command_response->server_id == NULL)) {
    r = MAILACTIVESYNC_ERROR_PROTOCOL;
    goto cleanup;
  }
  draft_server_id = strdup(command_response->server_id);
  if (draft_server_id == NULL) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto cleanup;
  }
  r = update_drafts_sync_key_from_result(state, sync_result);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;
  printf("Move self-test created draft server_id=%s\n", draft_server_id);
  mailactivesync_sync_result_free(sync_result);
  sync_result = NULL;

  moves = clist_new();
  if (moves == NULL) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto cleanup;
  }
  move.src_msg_id = draft_server_id;
  move.src_folder_id = state->drafts_id;
  move.dst_folder_id = temp_folder_id;
  if (clist_append(moves, &move) < 0) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto cleanup;
  }

  debug_step(args->debug, "requesting MoveItems self-test");
  r = mailactivesync_move_items(as, moves, &move_result);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug, "mailactivesync_move_items", r);
    debug_auth_retry_hint(as, r);
    goto cleanup;
  }
  r = mailactivesync_move_items_status_to_error(move_result->status);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug, "MoveItems status", r);
    goto cleanup;
  }
  if ((move_result->responses == NULL) ||
      (clist_begin(move_result->responses) == NULL)) {
    r = MAILACTIVESYNC_ERROR_PROTOCOL;
    goto cleanup;
  }
  move_response = clist_content(clist_begin(move_result->responses));
  r = mailactivesync_move_items_status_to_error(move_response->status);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args->debug, "MoveItems response status", r);
    goto cleanup;
  }
  moved = 1;
  printf("Move self-test moved src=%s dst=%s\n",
      move_response->src_msg_id != NULL ? move_response->src_msg_id : "",
      move_response->dst_msg_id != NULL ? move_response->dst_msg_id : "");

 cleanup:
  if (!moved && (draft_server_id != NULL))
    delete_generated_draft_best_effort(as, args, state, draft_server_id);
  if (temp_folder_id != NULL) {
    struct mailactivesync_folder_mutation_result * delete_result;
    int delete_error;

    delete_result = NULL;
    delete_error = mailactivesync_folder_delete(as, state->folder_sync_key,
        temp_folder_id, &delete_result);
    if (delete_error == MAILACTIVESYNC_NO_ERROR) {
      if ((delete_result != NULL) && (delete_result->sync_key != NULL))
        set_string(&state->folder_sync_key, delete_result->sync_key);
      printf("Move self-test deleted temporary folder id=%s\n",
          temp_folder_id);
    }
    else
      debug_error(args->debug, "temporary folder cleanup", delete_error);
    mailactivesync_folder_mutation_result_free(delete_result);
  }
  free(temp_folder_id);
  free(draft_server_id);
  if (moves != NULL)
    clist_free(moves);
  mailactivesync_move_items_result_free(move_result);
  mailactivesync_sync_result_free(sync_result);
  mailactivesync_folder_mutation_result_free(folder_result);
  return r;
}

int main(int argc, char ** argv)
{
  struct sample_args args;
  struct sample_state state;
  mailactivesync * as;
  struct mailactivesync_options * options;
  struct mailactivesync_folder_sync_result * folders;
  const char * folder_sync_key;
  const char * inbox_sync_key;
  int more_available;
  int page_count;
  int r;

  args.server = NULL;
  args.login = NULL;
  args.oauth_token = NULL;
  args.state_file = NULL;
  args.user_agent = NULL;
  args.validate_cert_file = NULL;
  args.device_id = "libetpantestdevice001";
  args.device_type = "libetpan";
  args.body_type = MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_MIME;
  args.debug = 0;
  args.skip_options = 0;
  args.follow_redirect = 0;
  args.send_self_test = 0;
  args.smart_reply_self_test = 0;
  args.smart_forward_self_test = 0;
  args.resolve_self_test = 0;
  args.resolve_cert_self_test = 0;
  args.validate_cert_self_test = 0;
  args.search_self_test = 0;
  args.ping_self_test = 0;
  args.ping_change_self_test = 0;
  args.draft_self_test = 0;
  args.multi_sync_self_test = 0;
  args.attachment_self_test = 0;
  args.mutation_self_test = 0;
  args.folder_self_test = 0;
  args.move_self_test = 0;
  as = NULL;
  options = NULL;
  folders = NULL;
  r = 1;

  if (parse_args(argc, argv, &args) < 0) {
    print_usage(argv[0]);
    return 1;
  }

  sample_state_init(&state);
  debug_step(args.debug, "loading state file");
  state_load(args.state_file, &state);

  if (state.protocol_version == NULL)
    set_string(&state.protocol_version, "16.1");

  if (args.debug) {
#ifndef _WIN32
    setenv("LIBETPAN_ACTIVESYNC_HTTP_DEBUG", "1", 0);
    setenv("LIBETPAN_ACTIVESYNC_WBXML_DEBUG", "1", 0);
#endif
    fprintf(stderr, "debug: server=%s\n", args.server);
    fprintf(stderr, "debug: login=%s\n", args.login);
    fprintf(stderr, "debug: token=<redacted, length=%u>\n",
        (unsigned int) strlen(args.oauth_token));
    fprintf(stderr, "debug: state_file=%s\n", args.state_file);
    fprintf(stderr, "debug: user_agent=%s\n",
        args.user_agent != NULL ? args.user_agent : "libEtPan ActiveSync");
    fprintf(stderr, "debug: device_id=%s\n", args.device_id);
    fprintf(stderr, "debug: device_type=%s\n", args.device_type);
    fprintf(stderr, "debug: body_type=%s\n", body_type_name(args.body_type));
    fprintf(stderr, "debug: send_self_test=%d\n", args.send_self_test);
    fprintf(stderr, "debug: smart_reply_self_test=%d\n",
        args.smart_reply_self_test);
    fprintf(stderr, "debug: smart_forward_self_test=%d\n",
        args.smart_forward_self_test);
    fprintf(stderr, "debug: resolve_self_test=%d\n",
        args.resolve_self_test);
    fprintf(stderr, "debug: resolve_cert_self_test=%d\n",
        args.resolve_cert_self_test);
    fprintf(stderr, "debug: validate_cert_self_test=%d\n",
        args.validate_cert_self_test);
    fprintf(stderr, "debug: validate_cert_file=%s\n",
        args.validate_cert_file != NULL ? args.validate_cert_file : "");
    fprintf(stderr, "debug: search_self_test=%d\n", args.search_self_test);
    fprintf(stderr, "debug: ping_self_test=%d\n", args.ping_self_test);
    fprintf(stderr, "debug: ping_change_self_test=%d\n",
        args.ping_change_self_test);
    fprintf(stderr, "debug: draft_self_test=%d\n", args.draft_self_test);
    fprintf(stderr, "debug: multi_sync_self_test=%d\n",
        args.multi_sync_self_test);
    fprintf(stderr, "debug: attachment_self_test=%d\n",
        args.attachment_self_test);
    fprintf(stderr, "debug: mutation_self_test=%d\n",
        args.mutation_self_test);
    fprintf(stderr, "debug: folder_self_test=%d\n",
        args.folder_self_test);
    fprintf(stderr, "debug: move_self_test=%d\n", args.move_self_test);
    fprintf(stderr, "debug: protocol_version=%s\n", state.protocol_version);
    fprintf(stderr, "debug: policy_key=%s\n",
        state.policy_key != NULL ? "<present>" : "");
    fprintf(stderr, "debug: folder_sync_key=%s\n",
        state.folder_sync_key != NULL ? state.folder_sync_key : "0");
    fprintf(stderr, "debug: inbox_id=%s\n",
        state.inbox_id != NULL ? state.inbox_id : "");
    fprintf(stderr, "debug: inbox_sync_key=%s\n",
        state.inbox_sync_key != NULL ? state.inbox_sync_key : "0");
    fprintf(stderr, "debug: drafts_id=%s\n",
        state.drafts_id != NULL ? state.drafts_id : "");
    fprintf(stderr, "debug: drafts_sync_key=%s\n",
        state.drafts_sync_key != NULL ? state.drafts_sync_key : "0");
  }

  debug_step(args.debug, "creating ActiveSync session");
  as = mailactivesync_new();
  if (as == NULL) {
    fprintf(stderr, "mailactivesync_new failed: out of memory\n");
    goto cleanup;
  }

  r = mailactivesync_set_device(as, args.device_id, args.device_type);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args.debug, "mailactivesync_set_device", r);
    goto cleanup;
  }
  r = mailactivesync_set_protocol_version(as, state.protocol_version);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args.debug, "mailactivesync_set_protocol_version", r);
    goto cleanup;
  }
  if (args.user_agent != NULL) {
    r = mailactivesync_set_user_agent(as, args.user_agent);
    if (r != MAILACTIVESYNC_NO_ERROR) {
      debug_error(args.debug, "mailactivesync_set_user_agent", r);
      goto cleanup;
    }
  }
  if (state.policy_key != NULL) {
    r = mailactivesync_set_policy_key(as, state.policy_key);
    if (r != MAILACTIVESYNC_NO_ERROR) {
      debug_error(args.debug, "mailactivesync_set_policy_key", r);
      goto cleanup;
    }
  }

  debug_step(args.debug, "connecting");
  r = mailactivesync_connect(as, args.server);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args.debug, "mailactivesync_connect", r);
    goto cleanup;
  }

  debug_step(args.debug, "configuring OAuth2 login");
  r = mailactivesync_login_oauth2(as, args.login, args.oauth_token);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args.debug, "mailactivesync_login_oauth2", r);
    goto cleanup;
  }

  if (!args.skip_options) {
    debug_step(args.debug, "requesting OPTIONS");
    r = mailactivesync_options(as, &options);
    if (r == MAILACTIVESYNC_ERROR_NOT_IMPLEMENTED) {
      fprintf(stderr, "ActiveSync OPTIONS is not implemented yet.\n");
      goto cleanup;
    }
    if (r != MAILACTIVESYNC_NO_ERROR) {
      debug_error(args.debug, "mailactivesync_options", r);
      debug_auth_retry_hint(as, r);
      if (r == MAILACTIVESYNC_ERROR_REDIRECT) {
        const char * redirect_url;

        redirect_url = mailactivesync_get_last_redirect_url(as);
        if (redirect_url != NULL) {
          fprintf(stderr, "ActiveSync redirect endpoint=%s\n", redirect_url);
          if (args.follow_redirect) {
            debug_step(args.debug, "following ActiveSync redirect");
            r = mailactivesync_connect(as, redirect_url);
            if (r != MAILACTIVESYNC_NO_ERROR) {
              debug_error(args.debug, "mailactivesync_connect redirect", r);
              goto cleanup;
            }
            r = mailactivesync_options(as, &options);
            if (r != MAILACTIVESYNC_NO_ERROR) {
              debug_error(args.debug, "mailactivesync_options redirect", r);
              debug_auth_retry_hint(as, r);
              goto cleanup;
            }
            debug_options(args.debug, options);
            goto options_done;
          }
        }
      }
      goto cleanup;
    }
    debug_options(args.debug, options);
options_done:
    ;
  }
  else
    debug_step(args.debug, "skipping OPTIONS");

  if ((options != NULL) && string_list_contains(options->commands, "Settings")) {
    r = run_device_information(as, &args);
    if (r == MAILACTIVESYNC_ERROR_PROVISION_REQUIRED) {
      r = run_provision(as, &args, &state);
      if (r != MAILACTIVESYNC_NO_ERROR)
        goto cleanup;
      if (state_save(args.state_file, &state) < 0)
        fprintf(stderr, "Could not save state file %s.\n", args.state_file);
      r = run_device_information(as, &args);
    }
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto cleanup;
  }

  folder_sync_key = state.folder_sync_key != NULL ? state.folder_sync_key : "0";
  if (args.debug)
    fprintf(stderr, "debug: requesting FolderSync sync_key=%s\n",
        folder_sync_key);
  r = mailactivesync_folder_sync(as, folder_sync_key, &folders);
  if (r == MAILACTIVESYNC_ERROR_PROVISION_REQUIRED) {
    r = run_provision(as, &args, &state);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto cleanup;
    if (state_save(args.state_file, &state) < 0)
      fprintf(stderr, "Could not save state file %s.\n", args.state_file);
    r = mailactivesync_folder_sync(as, folder_sync_key, &folders);
  }
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args.debug, "mailactivesync_folder_sync", r);
    debug_auth_retry_hint(as, r);
    goto cleanup;
  }
  debug_folder_sync(args.debug, folders);

  r = mailactivesync_folder_sync_status_to_error(folders->status);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    debug_error(args.debug, "mailactivesync_folder_sync status", r);
    if (r == MAILACTIVESYNC_ERROR_ACCOUNT_RESYNC_REQUIRED)
      sample_state_clear_account_sync(&state);
    state_save(args.state_file, &state);
    goto cleanup;
  }

  if (folders->sync_key != NULL)
    set_string(&state.folder_sync_key, folders->sync_key);
  if (state.inbox_id == NULL)
    set_string(&state.inbox_id, find_folder_server_id(folders, "Inbox"));
  if (state.drafts_id == NULL)
    set_string(&state.drafts_id, find_folder_server_id_by_name_or_type(
        folders, "Drafts", 3));

  if (state.inbox_id == NULL) {
    fprintf(stderr, "Inbox was not found in FolderSync results.\n");
    goto cleanup;
  }

  if ((args.draft_self_test || args.multi_sync_self_test ||
        args.mutation_self_test || args.move_self_test) &&
      (state.drafts_id == NULL)) {
    mailactivesync_folder_sync_result_free(folders);
    folders = NULL;
    debug_step(args.debug, "requesting FolderSync resync for Drafts");
    r = mailactivesync_folder_resync(as, &folders);
    if (r != MAILACTIVESYNC_NO_ERROR) {
      debug_error(args.debug, "mailactivesync_folder_resync", r);
      debug_auth_retry_hint(as, r);
      goto cleanup;
    }
    debug_folder_sync(args.debug, folders);
    r = mailactivesync_folder_sync_status_to_error(folders->status);
    if (r != MAILACTIVESYNC_NO_ERROR) {
      debug_error(args.debug, "mailactivesync_folder_resync status", r);
      goto cleanup;
    }
    if (folders->sync_key != NULL)
      set_string(&state.folder_sync_key, folders->sync_key);
    if (state.inbox_id == NULL)
      set_string(&state.inbox_id, find_folder_server_id_by_name_or_type(
          folders, "Inbox", 2));
    set_string(&state.drafts_id, find_folder_server_id_by_name_or_type(
        folders, "Drafts", 3));
  }

  if ((args.draft_self_test || args.multi_sync_self_test ||
        args.mutation_self_test || args.move_self_test) &&
      (state.drafts_id == NULL)) {
    fprintf(stderr, "Drafts folder was not found in FolderSync results.\n");
    goto cleanup;
  }

  debug_step(args.debug, "saving state after FolderSync");
  if (state_save(args.state_file, &state) < 0)
    fprintf(stderr, "Could not save state file %s.\n", args.state_file);

  inbox_sync_key = state.inbox_sync_key != NULL ? state.inbox_sync_key : "0";
  if (is_initial_sync_key(inbox_sync_key)) {
    r = run_inbox_sync(as, &args, &state, inbox_sync_key, 0, 0, NULL);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto cleanup;
    if (state.inbox_sync_key == NULL) {
      fprintf(stderr, "Initial Sync did not return an Inbox sync key.\n");
      goto cleanup;
    }
    debug_step(args.debug, "saving state after initial Sync");
    if (state_save(args.state_file, &state) < 0)
      fprintf(stderr, "Could not save state file %s.\n", args.state_file);
    inbox_sync_key = state.inbox_sync_key;
  }

  page_count = 0;
  if ((options != NULL) &&
      string_list_contains(options->commands, "GetItemEstimate")) {
    r = run_get_item_estimate(as, &args, &state);
    if (r != MAILACTIVESYNC_NO_ERROR) {
      debug_error(args.debug, "GetItemEstimate status", r);
      goto cleanup;
    }
  }

  do {
    more_available = 0;
    inbox_sync_key = state.inbox_sync_key != NULL ? state.inbox_sync_key : "0";
    r = run_inbox_sync(as, &args, &state, inbox_sync_key, 1, 1,
        &more_available);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto cleanup;
    page_count ++;
    if (page_count >= 20) {
      fprintf(stderr, "Stopping after 20 Sync pages to avoid an infinite loop.\n");
      break;
    }
  } while (more_available);

  if (args.send_self_test) {
    r = run_send_self_test(as, &args, options);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto cleanup;
  }

  if (args.smart_reply_self_test) {
    r = run_smart_compose_self_test(as, &args, &state, options, 0);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto cleanup;
  }

  if (args.smart_forward_self_test) {
    r = run_smart_compose_self_test(as, &args, &state, options, 1);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto cleanup;
  }

  if (args.resolve_self_test) {
    r = run_resolve_self_test(as, &args, options, 0);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto cleanup;
  }

  if (args.resolve_cert_self_test) {
    r = run_resolve_self_test(as, &args, options, 1);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto cleanup;
  }

  if (args.validate_cert_self_test) {
    r = run_validate_cert_self_test(as, &args, options);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto cleanup;
  }

  if (args.search_self_test) {
    r = run_search_self_test(as, &args, &state, options);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto cleanup;
  }

  if (args.ping_self_test) {
    r = run_ping_self_test(as, &args, &state, options, 0);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto cleanup;
  }

  if (args.ping_change_self_test) {
    r = run_ping_self_test(as, &args, &state, options, 1);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto cleanup;
  }

  if (args.attachment_self_test) {
    r = run_attachment_self_test(as, &args, &state, options);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto cleanup;
  }

  if (args.multi_sync_self_test) {
    r = run_drafts_initial_sync(as, &args, &state);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto cleanup;
    r = run_multi_sync_self_test(as, &args, &state);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto cleanup;
  }

  if (args.draft_self_test) {
    r = run_drafts_initial_sync(as, &args, &state);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto cleanup;
    r = run_draft_self_test(as, &args, &state);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto cleanup;
  }

  if (args.mutation_self_test) {
    r = run_drafts_initial_sync(as, &args, &state);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto cleanup;
    r = run_mutation_self_test(as, &args, &state);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto cleanup;
  }

  if (args.folder_self_test) {
    r = run_folder_self_test(as, &args, &state, options);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto cleanup;
  }

  if (args.move_self_test) {
    r = run_drafts_initial_sync(as, &args, &state);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto cleanup;
    r = run_move_self_test(as, &args, &state, options);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto cleanup;
  }

  debug_step(args.debug, "saving final state");
  if (state_save(args.state_file, &state) < 0)
    fprintf(stderr, "Could not save state file %s.\n", args.state_file);
  r = 0;

 cleanup:
  mailactivesync_folder_sync_result_free(folders);
  mailactivesync_options_free(options);
  mailactivesync_free(as);
  sample_state_free(&state);

  return r == 0 ? 0 : 1;
}
