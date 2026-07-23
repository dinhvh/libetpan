/*
 * Low-level ActiveSync sample.
 */

#include <libetpan/mailactivesync.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

struct sample_state {
  char * protocol_version;
  char * policy_key;
  char * folder_sync_key;
  char * inbox_id;
  char * inbox_sync_key;
};

struct sample_args {
  const char * server;
  const char * login;
  const char * oauth_token;
  const char * state_file;
  const char * user_agent;
  const char * device_id;
  const char * device_type;
  int body_type;
  int debug;
  int skip_options;
  int follow_redirect;
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

static void sample_state_init(struct sample_state * state)
{
  state->protocol_version = NULL;
  state->policy_key = NULL;
  state->folder_sync_key = NULL;
  state->inbox_id = NULL;
  state->inbox_sync_key = NULL;
}

static void sample_state_free(struct sample_state * state)
{
  free_string(&state->protocol_version);
  free_string(&state->policy_key);
  free_string(&state->folder_sync_key);
  free_string(&state->inbox_id);
  free_string(&state->inbox_sync_key);
}

static void sample_state_clear_account_sync(struct sample_state * state)
{
  free_string(&state->folder_sync_key);
  free_string(&state->inbox_id);
  free_string(&state->inbox_sync_key);
}

static void sample_state_clear_inbox_sync(struct sample_state * state)
{
  free_string(&state->inbox_sync_key);
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
      "[--body-type plain|html|mime]\n",
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

static void print_sync_result(struct mailactivesync_sync_result * result)
{
  clistiter * cur;

  if (result == NULL)
    return;

  for (cur = clist_begin(result->added); cur != NULL; cur = clist_next(cur)) {
    struct mailactivesync_message * message;

    message = clist_content(cur);
    print_message("added", message);
  }

  for (cur = result->changed != NULL ? clist_begin(result->changed) : NULL;
      cur != NULL; cur = clist_next(cur)) {
    struct mailactivesync_message * message;

    message = clist_content(cur);
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
    print_sync_result(sync_result);

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
  args.device_id = "libetpantestdevice001";
  args.device_type = "libetpan";
  args.body_type = MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_MIME;
  args.debug = 0;
  args.skip_options = 0;
  args.follow_redirect = 0;
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
    fprintf(stderr, "debug: protocol_version=%s\n", state.protocol_version);
    fprintf(stderr, "debug: policy_key=%s\n",
        state.policy_key != NULL ? "<present>" : "");
    fprintf(stderr, "debug: folder_sync_key=%s\n",
        state.folder_sync_key != NULL ? state.folder_sync_key : "0");
    fprintf(stderr, "debug: inbox_id=%s\n",
        state.inbox_id != NULL ? state.inbox_id : "");
    fprintf(stderr, "debug: inbox_sync_key=%s\n",
        state.inbox_sync_key != NULL ? state.inbox_sync_key : "0");
  }

  debug_step(args.debug, "creating ActiveSync session");
  as = mailactivesync_new(0, NULL);
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

  if (state.inbox_id == NULL) {
    fprintf(stderr, "Inbox was not found in FolderSync results.\n");
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
