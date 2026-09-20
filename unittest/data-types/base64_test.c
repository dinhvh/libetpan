#include "data_types_tests.h"

#include <stdlib.h>
#include <string.h>

#include "base64.h"

static int check_base64_codec(test_failure_callback failure_callback,
    void * context)
{
  char * encoded;
  char * decoded;
  int result;
  const char plain[] = "Hello, base64!";
  const char encoded_plain[] = "SGVsbG8sIGJhc2U2NCE=";
  const char prefixed_encoded[] = "+ SGVsbG8sIGJhc2U2NCE=";

  result = 0;
  encoded = NULL;
  decoded = NULL;

  encoded = encode_base64(plain, (int) strlen(plain));
  TEST_CHECK(encoded != NULL, "encode_base64 failed");
  TEST_CHECK(strcmp(encoded, encoded_plain) == 0,
      "encoded value did not match expected output");
  free(encoded);
  encoded = NULL;

  decoded = decode_base64(encoded_plain, (int) strlen(encoded_plain));
  TEST_CHECK(decoded != NULL, "decode_base64 failed");
  TEST_CHECK(strcmp(decoded, plain) == 0,
      "decoded value did not match original input");
  free(decoded);
  decoded = NULL;

  decoded = decode_base64(prefixed_encoded, (int) strlen(prefixed_encoded));
  TEST_CHECK(decoded != NULL, "decode_base64 with prefix failed");
  TEST_CHECK(strcmp(decoded, plain) == 0,
      "prefixed decoded value did not match original input");

 cleanup:
  free(encoded);
  free(decoded);
  return result;
}

struct base64_case {
  const char * name;
  int (* run)(test_failure_callback failure_callback, void * context);
};

static const struct base64_case cases[] = {
  { "Base64 codec", check_base64_codec },
};

size_t base64_test_count(void)
{
  return sizeof(cases) / sizeof(cases[0]);
}

const char * base64_test_name(size_t index)
{
  if (index >= base64_test_count())
    return NULL;
  return cases[index].name;
}

int base64_test_run_case(size_t index,
    test_failure_callback failure_callback, void * context)
{
  if (index >= base64_test_count()) {
    if (failure_callback != NULL)
      failure_callback(__FILE__, __LINE__, "index < base64_test_count()",
          "test case index is out of range", context);
    return -1;
  }

  return cases[index].run(failure_callback, context);
}
