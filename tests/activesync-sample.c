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
  char * folder_sync_key;
  char * inbox_id;
  char * inbox_sync_key;
};

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
  state->folder_sync_key = NULL;
  state->inbox_id = NULL;
  state->inbox_sync_key = NULL;
}

static void sample_state_free(struct sample_state * state)
{
  free_string(&state->protocol_version);
  free_string(&state->folder_sync_key);
  free_string(&state->inbox_id);
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

static void print_usage(const char * progname)
{
  fprintf(stderr,
      "usage: %s --server URL --login USER --oauth-token TOKEN "
      "--state-file PATH\n",
      progname);
}

static int parse_args(int argc, char ** argv, const char ** server,
    const char ** login, const char ** oauth_token, const char ** state_file)
{
  int i;

  for (i = 1; i < argc; i ++) {
    if ((strcmp(argv[i], "--server") == 0) && (i + 1 < argc))
      * server = argv[++ i];
    else if ((strcmp(argv[i], "--login") == 0) && (i + 1 < argc))
      * login = argv[++ i];
    else if ((strcmp(argv[i], "--oauth-token") == 0) && (i + 1 < argc))
      * oauth_token = argv[++ i];
    else if ((strcmp(argv[i], "--state-file") == 0) && (i + 1 < argc))
      * state_file = argv[++ i];
    else
      return -1;
  }

  if ((* server == NULL) || (* login == NULL) || (* oauth_token == NULL) ||
      (* state_file == NULL))
    return -1;

  return 0;
}

static void print_sync_result(struct mailactivesync_sync_result * result)
{
  clistiter * cur;

  if (result == NULL)
    return;

  for (cur = clist_begin(result->added); cur != NULL; cur = clist_next(cur)) {
    struct mailactivesync_message * message;

    message = clist_content(cur);
    printf("added server_id=%s subject=%s from=%s read=%d flagged=%d size=%u\n",
        message->server_id != NULL ? message->server_id : "",
        message->subject != NULL ? message->subject : "",
        message->from != NULL ? message->from : "",
        message->read, message->flagged,
        (unsigned int) message->estimated_size);
  }

  for (cur = result->changed != NULL ? clist_begin(result->changed) : NULL;
      cur != NULL; cur = clist_next(cur)) {
    struct mailactivesync_message * message;

    message = clist_content(cur);
    printf("changed server_id=%s subject=%s from=%s read=%d flagged=%d size=%u\n",
        message->server_id != NULL ? message->server_id : "",
        message->subject != NULL ? message->subject : "",
        message->from != NULL ? message->from : "",
        message->read, message->flagged,
        (unsigned int) message->estimated_size);
  }

  for (cur = result->deleted != NULL ? clist_begin(result->deleted) : NULL;
      cur != NULL; cur = clist_next(cur))
    printf("deleted server_id=%s\n", (char *) clist_content(cur));
}

int main(int argc, char ** argv)
{
  const char * server;
  const char * login;
  const char * oauth_token;
  const char * state_file;
  struct sample_state state;
  mailactivesync * as;
  struct mailactivesync_options * options;
  struct mailactivesync_folder_sync_result * folders;
  struct mailactivesync_sync_request * sync_request;
  struct mailactivesync_sync_result * sync_result;
  const char * folder_sync_key;
  const char * inbox_sync_key;
  int r;

  server = NULL;
  login = NULL;
  oauth_token = NULL;
  state_file = NULL;
  as = NULL;
  options = NULL;
  folders = NULL;
  sync_request = NULL;
  sync_result = NULL;
  r = 1;

  if (parse_args(argc, argv, &server, &login, &oauth_token, &state_file) < 0) {
    print_usage(argv[0]);
    return 1;
  }

  sample_state_init(&state);
  state_load(state_file, &state);

  if (state.protocol_version == NULL)
    set_string(&state.protocol_version, "16.1");

  as = mailactivesync_new(0, NULL);
  if (as == NULL)
    goto cleanup;

  mailactivesync_set_device(as, "libetpan-test-device-001", "libetpan");
  mailactivesync_set_protocol_version(as, state.protocol_version);

  r = mailactivesync_connect(as, server);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;

  r = mailactivesync_login_oauth2(as, login, oauth_token);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;

  r = mailactivesync_options(as, &options);
  if (r == MAILACTIVESYNC_ERROR_NOT_IMPLEMENTED) {
    fprintf(stderr, "ActiveSync OPTIONS is not implemented yet.\n");
    goto cleanup;
  }
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;

  folder_sync_key = state.folder_sync_key != NULL ? state.folder_sync_key : "0";
  r = mailactivesync_folder_sync(as, folder_sync_key, &folders);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;

  if (folders->sync_key != NULL)
    set_string(&state.folder_sync_key, folders->sync_key);
  if (state.inbox_id == NULL)
    set_string(&state.inbox_id, find_folder_server_id(folders, "Inbox"));

  if (state.inbox_id == NULL) {
    fprintf(stderr, "Inbox was not found in FolderSync results.\n");
    goto cleanup;
  }

  state_save(state_file, &state);

  inbox_sync_key = state.inbox_sync_key != NULL ? state.inbox_sync_key : "0";
  sync_request = mailactivesync_sync_request_new(state.inbox_id, inbox_sync_key);
  if (sync_request == NULL)
    goto cleanup;

  mailactivesync_sync_request_set_get_changes(sync_request, 1);
  mailactivesync_sync_request_set_window_size(sync_request, 25);
  mailactivesync_sync_request_set_mime_body_preference(sync_request, 200 * 1024);

  r = mailactivesync_sync(as, sync_request, &sync_result);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;

  if (sync_result->sync_key != NULL)
    set_string(&state.inbox_sync_key, sync_result->sync_key);

  print_sync_result(sync_result);
  state_save(state_file, &state);
  r = 0;

 cleanup:
  mailactivesync_sync_result_free(sync_result);
  mailactivesync_sync_request_free(sync_request);
  mailactivesync_folder_sync_result_free(folders);
  mailactivesync_options_free(options);
  mailactivesync_free(as);
  sample_state_free(&state);

  return r == 0 ? 0 : 1;
}
