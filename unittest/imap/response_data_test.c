#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
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
      "data/response-data/flags-nested-invalid.imap", compressed, &data);
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
      "data/response-data/cond-state-ok-permanentflags-nested-invalid.imap",
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
      "data/response-data/fetch-flags-nested-invalid.imap", compressed,
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
      "data/response-data/fetch-envelope-icloud-message-id.imap", compressed,
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

int imap_response_data_test_run(void)
{
  static const struct response_data_case cases[] = {
    { "data/response-data/cond-state-ok.imap",
      MAILIMAP_RESP_DATA_TYPE_COND_STATE, MAILIMAP_RESP_COND_STATE_OK },
    { "data/response-data/cond-state-ok-alert.imap",
      MAILIMAP_RESP_DATA_TYPE_COND_STATE, MAILIMAP_RESP_COND_STATE_OK },
    { "data/response-data/cond-state-ok-badcharset.imap",
      MAILIMAP_RESP_DATA_TYPE_COND_STATE, MAILIMAP_RESP_COND_STATE_OK },
    { "data/response-data/cond-state-ok-capability-code.imap",
      MAILIMAP_RESP_DATA_TYPE_COND_STATE, MAILIMAP_RESP_COND_STATE_OK },
    { "data/response-data/cond-state-ok-read-only.imap",
      MAILIMAP_RESP_DATA_TYPE_COND_STATE, MAILIMAP_RESP_COND_STATE_OK },
    { "data/response-data/cond-state-ok-uidnext.imap",
      MAILIMAP_RESP_DATA_TYPE_COND_STATE, MAILIMAP_RESP_COND_STATE_OK },
    { "data/response-data/cond-state-ok-uidvalidity.imap",
      MAILIMAP_RESP_DATA_TYPE_COND_STATE, MAILIMAP_RESP_COND_STATE_OK },
    { "data/response-data/cond-state-ok-unknown-code.imap",
      MAILIMAP_RESP_DATA_TYPE_COND_STATE, MAILIMAP_RESP_COND_STATE_OK },
    { "data/response-data/cond-state-no.imap",
      MAILIMAP_RESP_DATA_TYPE_COND_STATE, MAILIMAP_RESP_COND_STATE_NO },
    { "data/response-data/cond-state-bad.imap",
      MAILIMAP_RESP_DATA_TYPE_COND_STATE, MAILIMAP_RESP_COND_STATE_BAD },
    { "data/response-data/cond-bye.imap",
      MAILIMAP_RESP_DATA_TYPE_COND_BYE, 0 },
    { "data/response-data/capability.imap",
      MAILIMAP_RESP_DATA_TYPE_CAPABILITY_DATA, 0 },
    { "data/response-data/flags.imap",
      MAILIMAP_RESP_DATA_TYPE_MAILBOX_DATA, MAILIMAP_MAILBOX_DATA_FLAGS },
    { "data/response-data/flags-empty.imap",
      MAILIMAP_RESP_DATA_TYPE_MAILBOX_DATA, MAILIMAP_MAILBOX_DATA_FLAGS },
    { "data/response-data/list.imap",
      MAILIMAP_RESP_DATA_TYPE_MAILBOX_DATA, MAILIMAP_MAILBOX_DATA_LIST },
    { "data/response-data/list-empty-flag-extension.imap",
      MAILIMAP_RESP_DATA_TYPE_MAILBOX_DATA, MAILIMAP_MAILBOX_DATA_LIST },
    { "data/response-data/list-nil-delimiter.imap",
      MAILIMAP_RESP_DATA_TYPE_MAILBOX_DATA, MAILIMAP_MAILBOX_DATA_LIST },
    { "data/response-data/lsub.imap",
      MAILIMAP_RESP_DATA_TYPE_MAILBOX_DATA, MAILIMAP_MAILBOX_DATA_LSUB },
    { "data/response-data/status.imap",
      MAILIMAP_RESP_DATA_TYPE_MAILBOX_DATA, MAILIMAP_MAILBOX_DATA_STATUS },
    { "data/response-data/exists.imap",
      MAILIMAP_RESP_DATA_TYPE_MAILBOX_DATA, MAILIMAP_MAILBOX_DATA_EXISTS },
    { "data/response-data/obsolete-search.imap",
      MAILIMAP_RESP_DATA_TYPE_MAILBOX_DATA, MAILIMAP_MAILBOX_DATA_SEARCH },
    { "data/response-data/obsolete-search-empty.imap",
      MAILIMAP_RESP_DATA_TYPE_MAILBOX_DATA, MAILIMAP_MAILBOX_DATA_SEARCH },
    { "data/response-data/obsolete-recent.imap",
      MAILIMAP_RESP_DATA_TYPE_MAILBOX_DATA, MAILIMAP_MAILBOX_DATA_RECENT },
    { "data/response-data/expunge.imap",
      MAILIMAP_RESP_DATA_TYPE_MESSAGE_DATA, MAILIMAP_MESSAGE_DATA_EXPUNGE },
    { "data/response-data/fetch-flags.imap",
      MAILIMAP_RESP_DATA_TYPE_MESSAGE_DATA, MAILIMAP_MESSAGE_DATA_FETCH },
    { "data/response-data/fetch-literal.imap",
      MAILIMAP_RESP_DATA_TYPE_MESSAGE_DATA, MAILIMAP_MESSAGE_DATA_FETCH },
    { "data/response-data/fetch-bodystructure.imap",
      MAILIMAP_RESP_DATA_TYPE_MESSAGE_DATA, MAILIMAP_MESSAGE_DATA_FETCH },
    { "data/response-data/fetch-envelope.imap",
      MAILIMAP_RESP_DATA_TYPE_MESSAGE_DATA, MAILIMAP_MESSAGE_DATA_FETCH },
    { "data/response-data/fetch-envelope-icloud-message-id.imap",
      MAILIMAP_RESP_DATA_TYPE_MESSAGE_DATA, MAILIMAP_MESSAGE_DATA_FETCH },
    { "data/response-data/fetch-rfc822-text.imap",
      MAILIMAP_RESP_DATA_TYPE_MESSAGE_DATA, MAILIMAP_MESSAGE_DATA_FETCH },
    { "data/response-data/enabled.imap",
      MAILIMAP_RESP_DATA_TYPE_EXTENSION_DATA, MAILIMAP_EXTENSION_ENABLE },
    { "data/response-data/namespace.imap",
      MAILIMAP_RESP_DATA_TYPE_EXTENSION_DATA, MAILIMAP_EXTENSION_NAMESPACE },
    { "data/response-data/xlist-empty-flag-extension.imap",
      MAILIMAP_RESP_DATA_TYPE_EXTENSION_DATA, MAILIMAP_EXTENSION_XLIST }
  };
  size_t i;

  for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    check_case(&cases[i], false);
    check_case(&cases[i], true);
  }
  check_nested_invalid_flags(false);
  check_nested_invalid_flags(true);
  check_nested_invalid_permanentflags(false);
  check_nested_invalid_permanentflags(true);
  check_nested_invalid_fetch_flags(false);
  check_nested_invalid_fetch_flags(true);
  check_icloud_message_id(false);
  check_icloud_message_id(true);

  puts("response_data_test: ok");
  return 0;
}
