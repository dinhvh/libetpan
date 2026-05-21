#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include "unsupported_response_test.h"

#include "imap_test_utils.h"
#include "mailimap_types.h"

static void check_response_data_rejected(const char * path, bool compressed)
{
  struct mailimap_response_data * data = NULL;
  int r;

  r = imap_test_parse_response_data_file(path, compressed, &data);
  assert(r == MAILIMAP_ERROR_PARSE);
  assert(data == NULL);
}

static void check_response_rejected(const char * path, bool compressed)
{
  struct mailimap_response * response = NULL;
  int r;

  r = imap_test_parse_response_file(path, compressed, &response);
  assert(r != MAILIMAP_NO_ERROR);
  assert(response == NULL);
}

int imap_unsupported_response_test_run(void)
{
  static const char * response_data_gaps[] = {
    "data/unsupported/esearch.imap",
    "data/unsupported/list-extended-oldname.imap",
    "data/unsupported/fetch-binary.imap",
    "data/unsupported/fetch-binary-size.imap",
    "data/unsupported/fetch-literal8.imap",
    "data/unsupported/status-rev2-deleted-size.imap"
  };
  size_t i;

  for (i = 0; i < sizeof(response_data_gaps) / sizeof(response_data_gaps[0]);
      i++) {
    check_response_data_rejected(response_data_gaps[i], false);
    check_response_data_rejected(response_data_gaps[i], true);
  }

  check_response_rejected("data/unsupported/response-fatal-bye.imap", false);
  check_response_rejected("data/unsupported/response-fatal-bye.imap", true);

  puts("unsupported_response_test: ok");
  return 0;
}
