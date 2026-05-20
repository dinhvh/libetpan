#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include "clist.h"
#include "imap_test_utils.h"
#include "mailimap_types.h"

static void check_done(const char * path, bool compressed, int cond_type,
    unsigned expected_elements)
{
  struct mailimap_response * response = NULL;
  int r;

  r = imap_test_parse_response_file(path, compressed, &response);
  assert(r == MAILIMAP_NO_ERROR);
  assert(response != NULL);
  assert(response->rsp_resp_done->rsp_type == MAILIMAP_RESP_DONE_TYPE_TAGGED);
  assert(response->rsp_resp_done->rsp_data.rsp_tagged->rsp_cond_state
      ->rsp_type == cond_type);
  if (response->rsp_cont_req_or_resp_data_list == NULL)
    assert(expected_elements == 0);
  else
    assert((unsigned) clist_count(response->rsp_cont_req_or_resp_data_list) ==
        expected_elements);

  mailimap_response_free(response);
}

int main(void)
{
  static const struct {
    const char * path;
    int cond_type;
    unsigned elements;
  } cases[] = {
    { "data/response-done/tagged-ok.imap", MAILIMAP_RESP_COND_STATE_OK, 0 },
    { "data/response-done/tagged-no.imap", MAILIMAP_RESP_COND_STATE_NO, 0 },
    { "data/response-done/tagged-bad.imap", MAILIMAP_RESP_COND_STATE_BAD, 0 },
    { "data/response-done/continue-then-ok.imap",
      MAILIMAP_RESP_COND_STATE_OK, 1 },
    { "data/response-done/multi-response.imap",
      MAILIMAP_RESP_COND_STATE_OK, 5 }
  };
  size_t i;

  for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    check_done(cases[i].path, false, cases[i].cond_type, cases[i].elements);
    check_done(cases[i].path, true, cases[i].cond_type, cases[i].elements);
  }

  puts("response_done_test: ok");
  return 0;
}
