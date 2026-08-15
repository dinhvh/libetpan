#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "unsupported_response_test.h"

#include "imap_test_utils.h"
#include "mailimap_types.h"

struct unsupported_response_case {
  const char * name;
  int full_response;
};

static const struct unsupported_response_case cases[] = {
  { "esearch.imap", 0 },
  { "list-extended-oldname.imap", 0 },
  { "fetch-binary.imap", 0 },
  { "fetch-binary-size.imap", 0 },
  { "fetch-literal8.imap", 0 },
  { "status-rev2-deleted-size.imap", 0 },
  { "response-fatal-bye.imap", 1 },
};

size_t imap_unsupported_response_test_count(void)
{
  return sizeof(cases) / sizeof(cases[0]);
}

const char * imap_unsupported_response_test_name(size_t index)
{
  if (index >= imap_unsupported_response_test_count())
    return NULL;
  return cases[index].name;
}

int imap_unsupported_response_test_run_case(size_t index, int compressed,
    const char * fixture_root, test_failure_callback failure_callback,
    void * context)
{
  char path[4096];
  struct mailimap_response_data * data = NULL;
  struct mailimap_response * response = NULL;
  int result = 0;
  int written;
  int r;

  TEST_CHECK(index < imap_unsupported_response_test_count(),
      "test case index is out of range");
  written = snprintf(path, sizeof(path), "%s/%s", fixture_root,
      cases[index].name);
  TEST_CHECK(written >= 0 && (size_t) written < sizeof(path),
      "fixture path is too long");

  if (cases[index].full_response) {
    r = imap_test_parse_response_file(path, compressed != 0, &response);
    TEST_CHECK(r != MAILIMAP_NO_ERROR,
        "unsupported full response was unexpectedly accepted");
    TEST_CHECK(response == NULL, "rejected response returned parsed data");
  }
  else {
    r = imap_test_parse_response_data_file(path, compressed != 0, &data);
    TEST_CHECK(r == MAILIMAP_ERROR_PARSE,
        "unsupported response data was unexpectedly accepted");
    TEST_CHECK(data == NULL, "rejected response data returned parsed data");
  }

cleanup:
  if (data != NULL)
    mailimap_response_data_free(data);
  if (response != NULL)
    mailimap_response_free(response);
  return result;
}

int imap_unsupported_response_test_run(void)
{
  size_t i;

  for (i = 0; i < imap_unsupported_response_test_count(); i++) {
    if (imap_unsupported_response_test_run_case(i, false, "data/unsupported",
          NULL, NULL) != 0)
      abort();
    if (imap_unsupported_response_test_run_case(i, true, "data/unsupported",
          NULL, NULL) != 0)
      abort();
  }

  puts("unsupported_response_test: ok");
  return 0;
}
