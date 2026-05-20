#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

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

int main(void)
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
    { "data/response-data/fetch-rfc822-text.imap",
      MAILIMAP_RESP_DATA_TYPE_MESSAGE_DATA, MAILIMAP_MESSAGE_DATA_FETCH },
    { "data/response-data/enabled.imap",
      MAILIMAP_RESP_DATA_TYPE_EXTENSION_DATA, MAILIMAP_EXTENSION_ENABLE },
    { "data/response-data/namespace.imap",
      MAILIMAP_RESP_DATA_TYPE_EXTENSION_DATA, MAILIMAP_EXTENSION_NAMESPACE }
  };
  size_t i;

  for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    check_case(&cases[i], false);
    check_case(&cases[i], true);
  }

  puts("response_data_test: ok");
  return 0;
}
