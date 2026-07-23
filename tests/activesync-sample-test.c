/*
 * libEtPan! -- low-level ActiveSync sample integration tests.
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "../src/low-level/activesync/mailactivesync_command.h"

#define main activesync_sample_cli_main
#include "activesync-sample.c"
#undef main

#include <libetpan/mailactivesync_codes.h>
#include <libetpan/mailactivesync_wbxml.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#endif

#define MAX_FAKE_RESPONSES 16

struct sample_fake_context {
  unsigned char * responses[MAX_FAKE_RESPONSES];
  size_t response_lens[MAX_FAKE_RESPONSES];
  char * urls[MAX_FAKE_RESPONSES];
  int request_count;
};

static int check(int condition, const char * message)
{
  if (!condition) {
    fprintf(stderr, "%s\n", message);
    return 0;
  }

  return 1;
}

static int sample_test_node_add_text(struct mailactivesync_wbxml_node * parent,
    uint8_t code_page, uint8_t token, const char * text)
{
  struct mailactivesync_wbxml_node * child;
  int r;

  child = mailactivesync_wbxml_node_new_text(code_page, token, text);
  if (child == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  r = mailactivesync_wbxml_node_add_child(parent, child);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    mailactivesync_wbxml_node_free(child);
    return r;
  }

  return MAILACTIVESYNC_NO_ERROR;
}

static int sample_test_encode_document(struct mailactivesync_wbxml_node * root,
    unsigned char ** result, size_t * result_len)
{
  struct mailactivesync_wbxml_document * document;
  int r;

  document = mailactivesync_wbxml_document_new();
  if (document == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  document->root = root;
  r = mailactivesync_wbxml_encode(document, result, result_len);
  document->root = NULL;
  mailactivesync_wbxml_document_free(document);

  return r;
}

static int fake_set_response(struct sample_fake_context * context, int index,
    struct mailactivesync_wbxml_node * root)
{
  unsigned char * body;
  size_t body_len;
  int r;

  if ((context == NULL) || (index < 0) || (index >= MAX_FAKE_RESPONSES))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  body = NULL;
  body_len = 0;
  r = sample_test_encode_document(root, &body, &body_len);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  free(context->responses[index]);
  context->responses[index] = body;
  context->response_lens[index] = body_len;
  return MAILACTIVESYNC_NO_ERROR;
}

static size_t fake_response_len(struct sample_fake_context * context,
    int index)
{
  if ((index >= 0) && (index < MAX_FAKE_RESPONSES) &&
      (context->responses[index] != NULL))
    return context->response_lens[index];

  return 0;
}

static int sample_fake_perform(struct mailactivesync_http_transport * transport,
    struct mailactivesync_http_request * request,
    struct mailactivesync_http_response ** result)
{
  struct sample_fake_context * context;
  struct mailactivesync_http_response * response;
  int index;

  context = transport->context;
  index = context->request_count;
  if (index >= MAX_FAKE_RESPONSES)
    return MAILACTIVESYNC_ERROR_HTTP;

  response = mailactivesync_http_response_new(200);
  if (response == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  if (request->url != NULL) {
    free(context->urls[index]);
    context->urls[index] = strdup(request->url);
    if (context->urls[index] == NULL) {
      mailactivesync_http_response_free(response);
      return MAILACTIVESYNC_ERROR_MEMORY;
    }
  }

  if (fake_response_len(context, index) > 0) {
    response->body = malloc(context->response_lens[index]);
    if (response->body == NULL) {
      mailactivesync_http_response_free(response);
      return MAILACTIVESYNC_ERROR_MEMORY;
    }
    memcpy(response->body, context->responses[index],
        context->response_lens[index]);
    response->body_len = context->response_lens[index];
  }

  context->request_count ++;
  * result = response;
  return MAILACTIVESYNC_NO_ERROR;
}

static void sample_fake_free(struct mailactivesync_http_transport * transport)
{
  struct sample_fake_context * context;
  int i;

  context = transport->context;
  for (i = 0; i < MAX_FAKE_RESPONSES; i ++) {
    free(context->responses[i]);
    free(context->urls[i]);
  }
  free(context);
}

static struct mailactivesync_http_transport * sample_fake_transport_new(
    struct sample_fake_context ** context_result)
{
  struct mailactivesync_http_transport * transport;
  struct sample_fake_context * context;

  transport = malloc(sizeof(* transport));
  if (transport == NULL)
    return NULL;

  context = calloc(1, sizeof(* context));
  if (context == NULL) {
    free(transport);
    return NULL;
  }

  transport->context = context;
  transport->perform = sample_fake_perform;
  transport->free = sample_fake_free;
  * context_result = context;
  return transport;
}

static int setup_sample_session(mailactivesync ** session_result,
    struct sample_fake_context ** context_result)
{
  mailactivesync * session;
  struct mailactivesync_http_transport * transport;

  session = mailactivesync_new(0, NULL);
  if (session == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  transport = sample_fake_transport_new(context_result);
  if (transport == NULL) {
    mailactivesync_free(session);
    return MAILACTIVESYNC_ERROR_MEMORY;
  }

  if (mailactivesync_set_http_transport(session, transport) !=
      MAILACTIVESYNC_NO_ERROR) {
    mailactivesync_http_transport_free(transport);
    mailactivesync_free(session);
    return MAILACTIVESYNC_ERROR_MEMORY;
  }

  if ((mailactivesync_connect(session, "https://example.com") !=
          MAILACTIVESYNC_NO_ERROR) ||
      (mailactivesync_login_oauth2(session, "user@example.com", "token") !=
          MAILACTIVESYNC_NO_ERROR)) {
    mailactivesync_free(session);
    return MAILACTIVESYNC_ERROR_MEMORY;
  }

  * session_result = session;
  return MAILACTIVESYNC_NO_ERROR;
}

static int set_sync_response(struct sample_fake_context * context, int index,
    const char * sync_key, const char * status, int more_available)
{
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_node * collections;
  struct mailactivesync_wbxml_node * collection;
  int r;

  root = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_SYNC);
  collections = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_COLLECTIONS);
  collection = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
      MAILACTIVESYNC_AIRSYNC_COLLECTION);
  if ((root == NULL) || (collections == NULL) || (collection == NULL)) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto cleanup;
  }

  if ((sync_key != NULL) &&
      (sample_test_node_add_text(collection, MAILACTIVESYNC_CP_AIRSYNC,
          MAILACTIVESYNC_AIRSYNC_SYNC_KEY, sync_key) !=
          MAILACTIVESYNC_NO_ERROR)) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto cleanup;
  }
  if ((status != NULL) &&
      (sample_test_node_add_text(collection, MAILACTIVESYNC_CP_AIRSYNC,
          MAILACTIVESYNC_AIRSYNC_STATUS, status) !=
          MAILACTIVESYNC_NO_ERROR)) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto cleanup;
  }
  if (more_available &&
      (mailactivesync_wbxml_node_add_child(collection,
          mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
              MAILACTIVESYNC_AIRSYNC_MORE_AVAILABLE)) !=
          MAILACTIVESYNC_NO_ERROR)) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto cleanup;
  }
  if ((mailactivesync_wbxml_node_add_child(collections, collection) !=
          MAILACTIVESYNC_NO_ERROR) ||
      (mailactivesync_wbxml_node_add_child(root, collections) !=
          MAILACTIVESYNC_NO_ERROR)) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto cleanup;
  }
  collection = NULL;
  collections = NULL;

  r = fake_set_response(context, index, root);

 cleanup:
  mailactivesync_wbxml_node_free(collection);
  mailactivesync_wbxml_node_free(collections);
  mailactivesync_wbxml_node_free(root);
  return r;
}

static struct mailactivesync_wbxml_node * build_provision_response(
    const char * status, const char * policy_status, const char * policy_key)
{
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_node * policies;
  struct mailactivesync_wbxml_node * policy;

  root = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_PROVISION,
      MAILACTIVESYNC_PROVISION_PROVISION);
  policies = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_PROVISION,
      MAILACTIVESYNC_PROVISION_POLICIES);
  policy = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_PROVISION,
      MAILACTIVESYNC_PROVISION_POLICY);
  if ((root == NULL) || (policies == NULL) || (policy == NULL))
    goto err;

  if ((sample_test_node_add_text(root, MAILACTIVESYNC_CP_PROVISION,
          MAILACTIVESYNC_PROVISION_STATUS, status) !=
          MAILACTIVESYNC_NO_ERROR) ||
      (sample_test_node_add_text(policy, MAILACTIVESYNC_CP_PROVISION,
          MAILACTIVESYNC_PROVISION_STATUS, policy_status) !=
          MAILACTIVESYNC_NO_ERROR) ||
      (sample_test_node_add_text(policy, MAILACTIVESYNC_CP_PROVISION,
          MAILACTIVESYNC_PROVISION_POLICY_KEY, policy_key) !=
          MAILACTIVESYNC_NO_ERROR) ||
      (mailactivesync_wbxml_node_add_child(policies, policy) !=
          MAILACTIVESYNC_NO_ERROR) ||
      (mailactivesync_wbxml_node_add_child(root, policies) !=
          MAILACTIVESYNC_NO_ERROR))
    goto err;

  return root;

 err:
  mailactivesync_wbxml_node_free(policy);
  mailactivesync_wbxml_node_free(policies);
  mailactivesync_wbxml_node_free(root);
  return NULL;
}

static int set_provision_response(struct sample_fake_context * context,
    int index, const char * policy_key)
{
  struct mailactivesync_wbxml_node * root;
  int r;

  root = build_provision_response("1", "1", policy_key);
  if (root == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  r = fake_set_response(context, index, root);
  mailactivesync_wbxml_node_free(root);
  return r;
}

static int set_settings_response(struct sample_fake_context * context,
    int index)
{
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_node * device_information;
  int r;

  root = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_SETTINGS,
      MAILACTIVESYNC_SETTINGS_SETTINGS);
  device_information = mailactivesync_wbxml_node_new(
      MAILACTIVESYNC_CP_SETTINGS,
      MAILACTIVESYNC_SETTINGS_DEVICE_INFORMATION);
  if ((root == NULL) || (device_information == NULL)) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto cleanup;
  }
  if ((sample_test_node_add_text(root, MAILACTIVESYNC_CP_SETTINGS,
          MAILACTIVESYNC_SETTINGS_STATUS, "1") !=
          MAILACTIVESYNC_NO_ERROR) ||
      (sample_test_node_add_text(device_information,
          MAILACTIVESYNC_CP_SETTINGS, MAILACTIVESYNC_SETTINGS_STATUS, "1") !=
          MAILACTIVESYNC_NO_ERROR) ||
      (mailactivesync_wbxml_node_add_child(root, device_information) !=
          MAILACTIVESYNC_NO_ERROR)) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto cleanup;
  }
  device_information = NULL;

  r = fake_set_response(context, index, root);

 cleanup:
  mailactivesync_wbxml_node_free(device_information);
  mailactivesync_wbxml_node_free(root);
  return r;
}

static int set_get_item_estimate_response(
    struct sample_fake_context * context, int index)
{
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_node * response;
  struct mailactivesync_wbxml_node * collection;
  int r;

  root = mailactivesync_wbxml_node_new(
      MAILACTIVESYNC_CP_GETITEMESTIMATE,
      MAILACTIVESYNC_GETITEMESTIMATE_GET_ITEM_ESTIMATE);
  response = mailactivesync_wbxml_node_new(
      MAILACTIVESYNC_CP_GETITEMESTIMATE,
      MAILACTIVESYNC_GETITEMESTIMATE_RESPONSE);
  collection = mailactivesync_wbxml_node_new(
      MAILACTIVESYNC_CP_GETITEMESTIMATE,
      MAILACTIVESYNC_GETITEMESTIMATE_COLLECTION);
  if ((root == NULL) || (response == NULL) || (collection == NULL)) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto cleanup;
  }
  if ((sample_test_node_add_text(root, MAILACTIVESYNC_CP_GETITEMESTIMATE,
          MAILACTIVESYNC_GETITEMESTIMATE_STATUS, "1") !=
          MAILACTIVESYNC_NO_ERROR) ||
      (sample_test_node_add_text(collection,
          MAILACTIVESYNC_CP_GETITEMESTIMATE,
          MAILACTIVESYNC_GETITEMESTIMATE_STATUS, "1") !=
          MAILACTIVESYNC_NO_ERROR) ||
      (sample_test_node_add_text(collection,
          MAILACTIVESYNC_CP_GETITEMESTIMATE,
          MAILACTIVESYNC_GETITEMESTIMATE_ESTIMATE, "7") !=
          MAILACTIVESYNC_NO_ERROR) ||
      (mailactivesync_wbxml_node_add_child(response, collection) !=
          MAILACTIVESYNC_NO_ERROR) ||
      (mailactivesync_wbxml_node_add_child(root, response) !=
          MAILACTIVESYNC_NO_ERROR)) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto cleanup;
  }
  collection = NULL;
  response = NULL;

  r = fake_set_response(context, index, root);

 cleanup:
  mailactivesync_wbxml_node_free(collection);
  mailactivesync_wbxml_node_free(response);
  mailactivesync_wbxml_node_free(root);
  return r;
}

static void init_args(struct sample_args * args)
{
  memset(args, 0, sizeof(* args));
  args->server = "https://example.com/Microsoft-Server-ActiveSync";
  args->login = "user@example.com";
  args->oauth_token = "token";
  args->state_file = "/tmp/libetpan-activesync-sample-test-state";
  args->device_id = "device";
  args->device_type = "libetpan";
  args->body_type = MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_PLAIN_TEXT;
}

static int run_inbox_sync_quiet(mailactivesync * session,
    const struct sample_args * args, struct sample_state * state,
    const char * sync_key, int get_changes, int print_changes,
    int * more_available)
{
#ifndef _WIN32
  int saved_stderr;
  FILE * null_file;
  int r;

  fflush(stderr);
  saved_stderr = dup(fileno(stderr));
  null_file = fopen("/dev/null", "w");
  if ((saved_stderr < 0) || (null_file == NULL)) {
    if (saved_stderr >= 0)
      close(saved_stderr);
    if (null_file != NULL)
      fclose(null_file);
    return run_inbox_sync(session, args, state, sync_key, get_changes,
        print_changes, more_available);
  }

  dup2(fileno(null_file), fileno(stderr));
  r = run_inbox_sync(session, args, state, sync_key, get_changes,
      print_changes, more_available);
  fflush(stderr);
  dup2(saved_stderr, fileno(stderr));
  close(saved_stderr);
  fclose(null_file);
  return r;
#else
  return run_inbox_sync(session, args, state, sync_key, get_changes,
      print_changes, more_available);
#endif
}

static int test_sample_more_available_loop(void)
{
  mailactivesync * session;
  struct sample_fake_context * context;
  struct sample_args args;
  struct sample_state state;
  int more_available;
  int page_count;
  int r;

  session = NULL;
  context = NULL;
  sample_state_init(&state);
  init_args(&args);

  if (!check(setup_sample_session(&session, &context) ==
      MAILACTIVESYNC_NO_ERROR, "sample session setup failed"))
    return 0;
  if (!check(set_sync_response(context, 0, "2", "1", 1) ==
      MAILACTIVESYNC_NO_ERROR, "sample first Sync response setup failed"))
    goto err;
  if (!check(set_sync_response(context, 1, "3", "1", 0) ==
      MAILACTIVESYNC_NO_ERROR, "sample second Sync response setup failed"))
    goto err;

  if ((set_string(&state.inbox_id, "7") < 0) ||
      (set_string(&state.inbox_sync_key, "1") < 0))
    goto err;

  page_count = 0;
  do {
    more_available = 0;
    r = run_inbox_sync(session, &args, &state, state.inbox_sync_key, 1, 0,
        &more_available);
    if (!check(r == MAILACTIVESYNC_NO_ERROR,
        "sample paged Sync failed"))
      goto err;
    page_count ++;
  } while (more_available && (page_count < 20));

  if (!check(page_count == 2, "sample paged Sync page count mismatch"))
    goto err;
  if (!check(context->request_count == 2,
      "sample paged Sync request count mismatch"))
    goto err;
  if (!check((state.inbox_sync_key != NULL) &&
      (strcmp(state.inbox_sync_key, "3") == 0),
      "sample paged Sync final key mismatch"))
    goto err;

  sample_state_free(&state);
  mailactivesync_free(session);
  return 1;

 err:
  sample_state_free(&state);
  mailactivesync_free(session);
  return 0;
}

static int test_sample_sync_state_invalidation(void)
{
  mailactivesync * session;
  struct sample_fake_context * context;
  struct sample_args args;
  struct sample_state state;
  int more_available;
  int r;

  session = NULL;
  context = NULL;
  sample_state_init(&state);
  init_args(&args);

  if (!check(setup_sample_session(&session, &context) ==
      MAILACTIVESYNC_NO_ERROR, "sample session setup failed"))
    return 0;
  if (!check(set_sync_response(context, 0, NULL, "3", 0) ==
      MAILACTIVESYNC_NO_ERROR, "sample Sync.3 response setup failed"))
    goto err;
  if ((set_string(&state.folder_sync_key, "folder-key") < 0) ||
      (set_string(&state.inbox_id, "7") < 0) ||
      (set_string(&state.inbox_sync_key, "stale") < 0))
    goto err;

  more_available = 0;
  r = run_inbox_sync_quiet(session, &args, &state, state.inbox_sync_key, 1, 0,
      &more_available);
  if (!check(r == MAILACTIVESYNC_ERROR_FOLDER_RESYNC_REQUIRED,
      "sample Sync.3 did not request folder resync"))
    goto err;
  if (!check((state.folder_sync_key != NULL) &&
      (strcmp(state.folder_sync_key, "folder-key") == 0) &&
      (state.inbox_id != NULL) && (strcmp(state.inbox_id, "7") == 0) &&
      (state.inbox_sync_key == NULL),
      "sample Sync.3 state invalidation mismatch"))
    goto err;

  sample_state_free(&state);
  mailactivesync_free(session);
  return 1;

 err:
  sample_state_free(&state);
  mailactivesync_free(session);
  return 0;
}

static int test_sample_provision_persists_policy_key(void)
{
  mailactivesync * session;
  struct sample_fake_context * context;
  struct sample_args args;
  struct sample_state state;
  int r;

  session = NULL;
  context = NULL;
  sample_state_init(&state);
  init_args(&args);

  if (!check(setup_sample_session(&session, &context) ==
      MAILACTIVESYNC_NO_ERROR, "sample session setup failed"))
    return 0;
  if (!check(set_provision_response(context, 0, "temporary-key") ==
      MAILACTIVESYNC_NO_ERROR, "sample initial Provision response setup failed"))
    goto err;
  if (!check(set_provision_response(context, 1, "final-key") ==
      MAILACTIVESYNC_NO_ERROR, "sample ACK Provision response setup failed"))
    goto err;

  r = run_provision(session, &args, &state);
  if (!check(r == MAILACTIVESYNC_NO_ERROR, "sample Provision failed"))
    goto err;
  if (!check(context->request_count == 2,
      "sample Provision request count mismatch"))
    goto err;
  if (!check((state.policy_key != NULL) &&
      (strcmp(state.policy_key, "final-key") == 0),
      "sample Provision policy key mismatch"))
    goto err;

  sample_state_free(&state);
  mailactivesync_free(session);
  return 1;

 err:
  sample_state_free(&state);
  mailactivesync_free(session);
  return 0;
}

static int test_sample_settings_and_get_item_estimate(void)
{
  mailactivesync * session;
  struct sample_fake_context * context;
  struct sample_args args;
  struct sample_state state;
  int r;

  session = NULL;
  context = NULL;
  sample_state_init(&state);
  init_args(&args);

  if (!check(setup_sample_session(&session, &context) ==
      MAILACTIVESYNC_NO_ERROR, "sample session setup failed"))
    return 0;
  if (!check(set_settings_response(context, 0) == MAILACTIVESYNC_NO_ERROR,
      "sample Settings response setup failed"))
    goto err;
  if (!check(set_get_item_estimate_response(context, 1) ==
      MAILACTIVESYNC_NO_ERROR, "sample GetItemEstimate response setup failed"))
    goto err;
  if ((set_string(&state.inbox_id, "7") < 0) ||
      (set_string(&state.inbox_sync_key, "3") < 0))
    goto err;

  r = run_device_information(session, &args);
  if (!check(r == MAILACTIVESYNC_NO_ERROR, "sample Settings failed"))
    goto err;
  r = run_get_item_estimate(session, &args, &state);
  if (!check(r == MAILACTIVESYNC_NO_ERROR,
      "sample GetItemEstimate failed"))
    goto err;
  if (!check(context->request_count == 2,
      "sample Settings/GetItemEstimate request count mismatch"))
    goto err;
  if (!check((context->urls[0] != NULL) &&
      (strstr(context->urls[0], "Cmd=Settings") != NULL),
      "sample Settings URL mismatch"))
    goto err;
  if (!check((context->urls[1] != NULL) &&
      (strstr(context->urls[1], "Cmd=GetItemEstimate") != NULL),
      "sample GetItemEstimate URL mismatch"))
    goto err;

  sample_state_free(&state);
  mailactivesync_free(session);
  return 1;

 err:
  sample_state_free(&state);
  mailactivesync_free(session);
  return 0;
}

int main(void)
{
  if (!test_sample_more_available_loop())
    return 1;
  if (!test_sample_sync_state_invalidation())
    return 1;
  if (!test_sample_provision_persists_policy_key())
    return 1;
  if (!test_sample_settings_and_get_item_estimate())
    return 1;

  return 0;
}
