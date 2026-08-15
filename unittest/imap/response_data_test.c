#include <assert.h>
#include <stdbool.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "response_data_test.h"

#include "imap_test_utils.h"
#include "mailimap_extension_types.h"
#include "mailimap_types.h"

struct response_data_case {
  const char * path;
  int type;
  int subtype;
};

static jmp_buf test_abort;
static test_failure_callback active_failure_callback;
static void * active_failure_context;
static const char * active_fixture_root;

static void fail_assertion(const char * file, unsigned line,
    const char * expression)
{
  if (active_failure_callback != NULL)
    active_failure_callback(file, line, expression, "assertion failed",
        active_failure_context);
  longjmp(test_abort, 1);
}

#undef assert
#define assert(condition) \
  do { if (!(condition)) fail_assertion(__FILE__, __LINE__, #condition); } while (0)

static const char * fixture_path(const char * name)
{
  static char path[4096];
  int written = snprintf(path, sizeof(path), "%s/%s", active_fixture_root,
      name);

  assert(written >= 0 && (size_t) written < sizeof(path));
  return path;
}

static void check_case(const struct response_data_case * test_case,
    bool compressed)
{
  struct mailimap_response_data * data = NULL;
  int r;

  r = imap_test_parse_response_data_file(test_case->path, compressed, &data);
  assert(r == MAILIMAP_NO_ERROR);
  assert(data != NULL);
  assert(data->rsp_type == test_case->type);

  switch (data->rsp_type) {
  case MAILIMAP_RESP_DATA_TYPE_COND_STATE:
    assert(data->rsp_data.rsp_cond_state->rsp_type == test_case->subtype);
    break;
  case MAILIMAP_RESP_DATA_TYPE_MAILBOX_DATA:
    assert(data->rsp_data.rsp_mailbox_data->mbd_type == test_case->subtype);
    break;
  case MAILIMAP_RESP_DATA_TYPE_MESSAGE_DATA:
    assert(data->rsp_data.rsp_message_data->mdt_type == test_case->subtype);
    break;
  case MAILIMAP_RESP_DATA_TYPE_EXTENSION_DATA:
    assert(data->rsp_data.rsp_extension_data->ext_extension->ext_id ==
        test_case->subtype);
    break;
  default:
    (void) test_case;
    break;
  }

  mailimap_response_data_free(data);
}

static void check_nested_invalid_flags(bool compressed)
{
  struct mailimap_response_data * data = NULL;
  struct mailimap_flag_list * flags;
  int r;

  r = imap_test_parse_response_data_file(
      fixture_path("flags-nested-invalid.imap"), compressed, &data);
  assert(r == MAILIMAP_NO_ERROR);
  assert(data != NULL);
  assert(data->rsp_type == MAILIMAP_RESP_DATA_TYPE_MAILBOX_DATA);
  assert(data->rsp_data.rsp_mailbox_data->mbd_type ==
      MAILIMAP_MAILBOX_DATA_FLAGS);

  flags = data->rsp_data.rsp_mailbox_data->mbd_data.mbd_flags;
  assert(flags != NULL);
  assert(flags->fl_list != NULL);
  assert(clist_count(flags->fl_list) == 6);

  mailimap_response_data_free(data);
}

static void check_nested_invalid_permanentflags(bool compressed)
{
  struct mailimap_response_data * data = NULL;
  struct mailimap_resp_text_code * code;
  clist * flags;
  int r;

  r = imap_test_parse_response_data_file(
      fixture_path("cond-state-ok-permanentflags-nested-invalid.imap"),
      compressed, &data);
  assert(r == MAILIMAP_NO_ERROR);
  assert(data != NULL);
  assert(data->rsp_type == MAILIMAP_RESP_DATA_TYPE_COND_STATE);
  assert(data->rsp_data.rsp_cond_state->rsp_type ==
      MAILIMAP_RESP_COND_STATE_OK);
  assert(data->rsp_data.rsp_cond_state->rsp_text != NULL);

  code = data->rsp_data.rsp_cond_state->rsp_text->rsp_code;
  assert(code != NULL);
  assert(code->rc_type == MAILIMAP_RESP_TEXT_CODE_PERMANENTFLAGS);

  flags = code->rc_data.rc_perm_flags;
  assert(flags != NULL);
  assert(clist_count(flags) == 6);

  mailimap_response_data_free(data);
}

static void check_nested_invalid_fetch_flags(bool compressed)
{
  struct mailimap_response_data * data = NULL;
  struct mailimap_msg_att * msg_att;
  struct mailimap_msg_att_item * item;
  struct mailimap_msg_att_dynamic * dyn;
  int r;

  r = imap_test_parse_response_data_file(
      fixture_path("fetch-flags-nested-invalid.imap"), compressed,
      &data);
  assert(r == MAILIMAP_NO_ERROR);
  assert(data != NULL);
  assert(data->rsp_type == MAILIMAP_RESP_DATA_TYPE_MESSAGE_DATA);
  assert(data->rsp_data.rsp_message_data->mdt_type ==
      MAILIMAP_MESSAGE_DATA_FETCH);

  msg_att = data->rsp_data.rsp_message_data->mdt_msg_att;
  assert(msg_att != NULL);
  assert(msg_att->att_list != NULL);

  item = clist_content(clist_begin(msg_att->att_list));
  assert(item != NULL);
  assert(item->att_type == MAILIMAP_MSG_ATT_ITEM_DYNAMIC);

  dyn = item->att_data.att_dyn;
  assert(dyn != NULL);
  assert(dyn->att_list != NULL);
  assert(clist_count(dyn->att_list) == 2);

  mailimap_response_data_free(data);
}

static void check_icloud_message_id(bool compressed)
{
  static const char expected_message_id[] =
      "<\"392889836.11.1529401004417.JavaMail.Redacted\"@Redacted>";
  struct mailimap_response_data * data = NULL;
  struct mailimap_msg_att * msg_att;
  clistiter * cur;
  int found;
  int r;

  r = imap_test_parse_response_data_file(
      fixture_path("fetch-envelope-icloud-message-id.imap"), compressed,
      &data);
  assert(r == MAILIMAP_NO_ERROR);
  assert(data != NULL);
  assert(data->rsp_type == MAILIMAP_RESP_DATA_TYPE_MESSAGE_DATA);
  assert(data->rsp_data.rsp_message_data->mdt_type ==
      MAILIMAP_MESSAGE_DATA_FETCH);

  msg_att = data->rsp_data.rsp_message_data->mdt_msg_att;
  assert(msg_att != NULL);
  assert(msg_att->att_list != NULL);

  found = 0;
  for (cur = clist_begin(msg_att->att_list); cur != NULL; cur = clist_next(cur)) {
    struct mailimap_msg_att_item * item;
    struct mailimap_msg_att_static * static_att;
    struct mailimap_envelope * envelope;

    item = clist_content(cur);
    assert(item != NULL);
    if (item->att_type != MAILIMAP_MSG_ATT_ITEM_STATIC)
      continue;

    static_att = item->att_data.att_static;
    assert(static_att != NULL);
    if (static_att->att_type != MAILIMAP_MSG_ATT_ENVELOPE)
      continue;

    envelope = static_att->att_data.att_env;
    assert(envelope != NULL);
    assert(envelope->env_message_id != NULL);
    assert(strcmp(envelope->env_message_id, expected_message_id) == 0);
    found = 1;
    break;
  }

  assert(found);
  mailimap_response_data_free(data);
}

static void check_number_overflow(bool compressed)
{
  struct mailimap_response_data * data = NULL;
  int r;

  r = imap_test_parse_response_data_file(
      fixture_path("fetch-literal-overflow.imap"), compressed, &data);
  assert(r == MAILIMAP_ERROR_PARSE);
  assert(data == NULL);
}

static const struct response_data_case cases[] = {
    { "cond-state-ok.imap",
      MAILIMAP_RESP_DATA_TYPE_COND_STATE, MAILIMAP_RESP_COND_STATE_OK },
    { "cond-state-ok-alert.imap",
      MAILIMAP_RESP_DATA_TYPE_COND_STATE, MAILIMAP_RESP_COND_STATE_OK },
    { "cond-state-ok-badcharset.imap",
      MAILIMAP_RESP_DATA_TYPE_COND_STATE, MAILIMAP_RESP_COND_STATE_OK },
    { "cond-state-ok-capability-code.imap",
      MAILIMAP_RESP_DATA_TYPE_COND_STATE, MAILIMAP_RESP_COND_STATE_OK },
    { "cond-state-ok-read-only.imap",
      MAILIMAP_RESP_DATA_TYPE_COND_STATE, MAILIMAP_RESP_COND_STATE_OK },
    { "cond-state-ok-uidnext.imap",
      MAILIMAP_RESP_DATA_TYPE_COND_STATE, MAILIMAP_RESP_COND_STATE_OK },
    { "cond-state-ok-uidvalidity.imap",
      MAILIMAP_RESP_DATA_TYPE_COND_STATE, MAILIMAP_RESP_COND_STATE_OK },
    { "cond-state-ok-unknown-code.imap",
      MAILIMAP_RESP_DATA_TYPE_COND_STATE, MAILIMAP_RESP_COND_STATE_OK },
    { "cond-state-no.imap",
      MAILIMAP_RESP_DATA_TYPE_COND_STATE, MAILIMAP_RESP_COND_STATE_NO },
    { "cond-state-bad.imap",
      MAILIMAP_RESP_DATA_TYPE_COND_STATE, MAILIMAP_RESP_COND_STATE_BAD },
    { "cond-bye.imap",
      MAILIMAP_RESP_DATA_TYPE_COND_BYE, 0 },
    { "capability.imap",
      MAILIMAP_RESP_DATA_TYPE_CAPABILITY_DATA, 0 },
    { "flags.imap",
      MAILIMAP_RESP_DATA_TYPE_MAILBOX_DATA, MAILIMAP_MAILBOX_DATA_FLAGS },
    { "flags-empty.imap",
      MAILIMAP_RESP_DATA_TYPE_MAILBOX_DATA, MAILIMAP_MAILBOX_DATA_FLAGS },
    { "list.imap",
      MAILIMAP_RESP_DATA_TYPE_MAILBOX_DATA, MAILIMAP_MAILBOX_DATA_LIST },
    { "list-empty-flag-extension.imap",
      MAILIMAP_RESP_DATA_TYPE_MAILBOX_DATA, MAILIMAP_MAILBOX_DATA_LIST },
    { "list-nil-delimiter.imap",
      MAILIMAP_RESP_DATA_TYPE_MAILBOX_DATA, MAILIMAP_MAILBOX_DATA_LIST },
    { "lsub.imap",
      MAILIMAP_RESP_DATA_TYPE_MAILBOX_DATA, MAILIMAP_MAILBOX_DATA_LSUB },
    { "status.imap",
      MAILIMAP_RESP_DATA_TYPE_MAILBOX_DATA, MAILIMAP_MAILBOX_DATA_STATUS },
    { "exists.imap",
      MAILIMAP_RESP_DATA_TYPE_MAILBOX_DATA, MAILIMAP_MAILBOX_DATA_EXISTS },
    { "obsolete-search.imap",
      MAILIMAP_RESP_DATA_TYPE_MAILBOX_DATA, MAILIMAP_MAILBOX_DATA_SEARCH },
    { "obsolete-search-empty.imap",
      MAILIMAP_RESP_DATA_TYPE_MAILBOX_DATA, MAILIMAP_MAILBOX_DATA_SEARCH },
    { "obsolete-recent.imap",
      MAILIMAP_RESP_DATA_TYPE_MAILBOX_DATA, MAILIMAP_MAILBOX_DATA_RECENT },
    { "expunge.imap",
      MAILIMAP_RESP_DATA_TYPE_MESSAGE_DATA, MAILIMAP_MESSAGE_DATA_EXPUNGE },
    { "fetch-flags.imap",
      MAILIMAP_RESP_DATA_TYPE_MESSAGE_DATA, MAILIMAP_MESSAGE_DATA_FETCH },
    { "fetch-literal.imap",
      MAILIMAP_RESP_DATA_TYPE_MESSAGE_DATA, MAILIMAP_MESSAGE_DATA_FETCH },
    { "fetch-bodystructure.imap",
      MAILIMAP_RESP_DATA_TYPE_MESSAGE_DATA, MAILIMAP_MESSAGE_DATA_FETCH },
    { "fetch-envelope.imap",
      MAILIMAP_RESP_DATA_TYPE_MESSAGE_DATA, MAILIMAP_MESSAGE_DATA_FETCH },
    { "fetch-envelope-icloud-message-id.imap",
      MAILIMAP_RESP_DATA_TYPE_MESSAGE_DATA, MAILIMAP_MESSAGE_DATA_FETCH },
    { "fetch-rfc822-text.imap",
      MAILIMAP_RESP_DATA_TYPE_MESSAGE_DATA, MAILIMAP_MESSAGE_DATA_FETCH },
    { "enabled.imap",
      MAILIMAP_RESP_DATA_TYPE_EXTENSION_DATA, MAILIMAP_EXTENSION_ENABLE },
    { "namespace.imap",
      MAILIMAP_RESP_DATA_TYPE_EXTENSION_DATA, MAILIMAP_EXTENSION_NAMESPACE },
    { "xlist-empty-flag-extension.imap",
      MAILIMAP_RESP_DATA_TYPE_EXTENSION_DATA, MAILIMAP_EXTENSION_XLIST }
};

static const char * special_case_names[] = {
  "nested-invalid-flags",
  "nested-invalid-permanentflags",
  "nested-invalid-fetch-flags",
  "icloud-message-id",
  "number-overflow",
};

size_t imap_response_data_test_count(void)
{
  return sizeof(cases) / sizeof(cases[0]) +
      sizeof(special_case_names) / sizeof(special_case_names[0]);
}

const char * imap_response_data_test_name(size_t index)
{
  size_t regular_count = sizeof(cases) / sizeof(cases[0]);
  if (index < regular_count)
    return cases[index].path;
  index -= regular_count;
  if (index < sizeof(special_case_names) / sizeof(special_case_names[0]))
    return special_case_names[index];
  return NULL;
}

int imap_response_data_test_run_case(size_t index, int compressed,
    const char * fixture_root, test_failure_callback failure_callback,
    void * context)
{
  size_t regular_count = sizeof(cases) / sizeof(cases[0]);

  if (index >= imap_response_data_test_count()) {
    if (failure_callback != NULL)
      failure_callback(__FILE__, __LINE__,
          "index < imap_response_data_test_count()",
          "test case index is out of range", context);
    return -1;
  }

  active_failure_callback = failure_callback;
  active_failure_context = context;
  active_fixture_root = fixture_root;
  if (setjmp(test_abort) != 0)
    return -1;

  if (index < regular_count) {
    struct response_data_case test_case = cases[index];
    test_case.path = fixture_path(test_case.path);
    check_case(&test_case, compressed != 0);
  }
  else {
    switch (index - regular_count) {
    case 0: check_nested_invalid_flags(compressed != 0); break;
    case 1: check_nested_invalid_permanentflags(compressed != 0); break;
    case 2: check_nested_invalid_fetch_flags(compressed != 0); break;
    case 3: check_icloud_message_id(compressed != 0); break;
    case 4: check_number_overflow(compressed != 0); break;
    }
  }
  return 0;
}

int imap_response_data_test_run(void)
{
  size_t i;

  for (i = 0; i < imap_response_data_test_count(); i++) {
    if (imap_response_data_test_run_case(i, false, "data/response-data",
          NULL, NULL) != 0)
      abort();
    if (imap_response_data_test_run_case(i, true, "data/response-data",
          NULL, NULL) != 0)
      abort();
  }

  puts("response_data_test: ok");
  return 0;
}
