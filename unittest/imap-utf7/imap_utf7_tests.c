#include "imap_utf7_tests.h"

#include <stdlib.h>
#include <string.h>

#include "charconv.h"

struct imap_utf7_case {
  const char * name;
  const char * decoded;
  const char * encoded;
};

static const struct imap_utf7_case cases[] = {
  { "ampersand", "&", "&-" },
  { "mailbox path",
    "~peter/mail/\xe5\x8f\xb0\xe5\x8c\x97/\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e",
    "~peter/mail/&U,BTFw-/&ZeVnLIqe-" },
  { "RFC 2152 equivalent", "A\xe2\x89\xa2\xce\x91.", "A&ImIDkQ-." },
  { "shifted punctuation", "Hi Mom -\xe2\x98\xba-!", "Hi Mom -&Jjo--!" },
  { "Japanese", "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e", "&ZeVnLIqe-" },
  { "pound sign", "Item 3 is \xc2\xa3" "1.", "Item 3 is &AKM-1." },
};

size_t imap_utf7_test_count(void)
{
  return sizeof(cases) / sizeof(cases[0]);
}

const char * imap_utf7_test_name(size_t index)
{
  if (index >= imap_utf7_test_count())
    return NULL;
  return cases[index].name;
}

int imap_utf7_test_run(size_t index, test_failure_callback failure_callback,
    void * context)
{
  const struct imap_utf7_case * test = NULL;
  char * encoded = NULL;
  char * decoded = NULL;
  int result = 0;

  TEST_CHECK(index < imap_utf7_test_count(), "test case index is out of range");
  test = &cases[index];
  encoded = charconv_encode_mutf7(test->decoded);
  TEST_CHECK(encoded != NULL, "modified UTF-7 encoding failed");
  TEST_CHECK(strcmp(encoded, test->encoded) == 0,
      "modified UTF-7 encoding does not match");

  decoded = charconv_decode_mutf7(test->encoded);
  TEST_CHECK(decoded != NULL, "modified UTF-7 decoding failed");
  TEST_CHECK(strcmp(decoded, test->decoded) == 0,
      "modified UTF-7 decoding does not match");

cleanup:
  free(encoded);
  free(decoded);
  return result;
}
