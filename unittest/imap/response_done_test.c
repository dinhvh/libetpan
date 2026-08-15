#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include "response_done_test.h"

#include "clist.h"
#include "imap_test_utils.h"
#include "mailimap_types.h"

int imap_response_done_test_case(const char * fixture_root,
    const char * fixture_name, int compressed, int cond_type,
    unsigned expected_elements, test_failure_callback failure_callback,
    void * context)
{
  char path[4096];
  struct mailimap_response * response = NULL;
  int result = 0;
  int r;

  r = snprintf(path, sizeof(path), "%s/%s", fixture_root, fixture_name);
  TEST_CHECK(r >= 0 && (size_t) r < sizeof(path), "fixture path is too long");
  r = imap_test_parse_response_file(path, compressed, &response);
  TEST_CHECK(r == MAILIMAP_NO_ERROR, "could not parse IMAP response fixture");
  TEST_CHECK(response != NULL, "parser returned no response");
  TEST_CHECK(response->rsp_resp_done != NULL,
      "response has no completion data");
  TEST_CHECK(response->rsp_resp_done->rsp_type ==
      MAILIMAP_RESP_DONE_TYPE_TAGGED, "response is not tagged");
  TEST_CHECK(response->rsp_resp_done->rsp_data.rsp_tagged != NULL,
      "tagged response is missing");
  TEST_CHECK(response->rsp_resp_done->rsp_data.rsp_tagged->rsp_cond_state !=
      NULL, "tagged response has no condition state");
  TEST_CHECK(response->rsp_resp_done->rsp_data.rsp_tagged->rsp_cond_state
      ->rsp_type == cond_type, "condition state does not match");
  if (response->rsp_cont_req_or_resp_data_list == NULL)
    TEST_CHECK(expected_elements == 0, "response element count does not match");
  else
    TEST_CHECK((unsigned) clist_count(
        response->rsp_cont_req_or_resp_data_list) == expected_elements,
        "response element count does not match");

cleanup:
  if (response != NULL)
    mailimap_response_free(response);
  return result;
}

int imap_response_done_test_run(void)
{
  static const struct {
    const char * path;
    int cond_type;
    unsigned elements;
  } cases[] = {
    { "tagged-ok.imap", MAILIMAP_RESP_COND_STATE_OK, 0 },
    { "tagged-no.imap", MAILIMAP_RESP_COND_STATE_NO, 0 },
    { "tagged-bad.imap", MAILIMAP_RESP_COND_STATE_BAD, 0 },
    { "continue-then-ok.imap",
      MAILIMAP_RESP_COND_STATE_OK, 1 },
    { "multi-response.imap",
      MAILIMAP_RESP_COND_STATE_OK, 5 }
  };
  size_t i;

  for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    assert(imap_response_done_test_case("data/response-done", cases[i].path,
        false, cases[i].cond_type, cases[i].elements, NULL, NULL) == 0);
    assert(imap_response_done_test_case("data/response-done", cases[i].path,
        true, cases[i].cond_type, cases[i].elements, NULL, NULL) == 0);
  }

  puts("response_done_test: ok");
  return 0;
}
