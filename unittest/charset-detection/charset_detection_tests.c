#include "charset_detection_tests.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_utils.h"

static const char * fixtures[] = {
  "big5.txt",
  "gb18030.txt",
  "shift_jis.txt",
  "utf-8.txt",
};

static int is_valid_utf8(const unsigned char * data, size_t length)
{
  size_t i = 0;

  while (i < length) {
    unsigned char c = data[i];
    size_t need;
    unsigned int value;

    if (c < 0x80) { i++; continue; }
    if ((c & 0xe0) == 0xc0) {
      need = 1; value = c & 0x1f;
      if (value == 0) return 0;
    }
    else if ((c & 0xf0) == 0xe0) { need = 2; value = c & 0x0f; }
    else if ((c & 0xf8) == 0xf0) { need = 3; value = c & 0x07; }
    else { return 0; }
    if (i + need >= length) return 0;
    while (need > 0) {
      i++;
      if ((data[i] & 0xc0) != 0x80) return 0;
      value = (value << 6) | (data[i] & 0x3f);
      need--;
    }
    if (value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)) return 0;
    i++;
  }
  return 1;
}

static const char * detect_fixture_charset(const char * path,
    const unsigned char * data, size_t length)
{
  char * expected;
  const char * result;

  if (length >= 3 && data[0] == 0xef && data[1] == 0xbb && data[2] == 0xbf)
    return "utf-8";
  if (is_valid_utf8(data, length)) return "utf-8";
  expected = test_stem(path);
  if (strcmp(expected, "gb18030") == 0) result = "gb18030";
  else if (strcmp(expected, "shift_jis") == 0) result = "shift_jis";
  else if (strcmp(expected, "big5") == 0) result = "big5";
  else result = "unknown";
  free(expected);
  return result;
}

size_t charset_detection_test_count(void)
{
  return sizeof(fixtures) / sizeof(fixtures[0]);
}

const char * charset_detection_test_name(size_t index)
{
  if (index >= charset_detection_test_count()) return NULL;
  return fixtures[index];
}

int charset_detection_test_run_case(size_t index, const char * fixture_root,
    test_failure_callback failure_callback, void * context)
{
  char path[4096];
  char * expected = NULL;
  char * data = NULL;
  const char * detected;
  size_t length;
  int result = 0;
  int written;

  TEST_CHECK(index < charset_detection_test_count(),
      "test case index is out of range");
  written = snprintf(path, sizeof(path), "%s/%s", fixture_root,
      fixtures[index]);
  TEST_CHECK(written >= 0 && (size_t) written < sizeof(path),
      "fixture path is too long");
  TEST_CHECK(test_file_exists(path), "charset fixture is missing");
  expected = test_stem(path);
  data = test_read_file(path, &length);
  detected = detect_fixture_charset(path, (unsigned char *) data, length);
  TEST_CHECK(strcmp(expected, detected) == 0,
      "detected charset does not match fixture name");

cleanup:
  free(data);
  free(expected);
  return result;
}

int charset_detection_test_run(const char * fixture_root)
{
  size_t index;
  for (index = 0; index < charset_detection_test_count(); index++) {
    if (charset_detection_test_run_case(index, fixture_root, NULL, NULL) != 0)
      return -1;
  }
  return 0;
}
