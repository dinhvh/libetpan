#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_utils.h"

static int is_valid_utf8(const unsigned char * data, size_t length)
{
  size_t i = 0;

  while (i < length) {
    unsigned char c = data[i];
    size_t need;
    unsigned int value;

    if (c < 0x80) {
      i++;
      continue;
    }
    if ((c & 0xe0) == 0xc0) {
      need = 1;
      value = c & 0x1f;
      if (value == 0)
        return 0;
    }
    else if ((c & 0xf0) == 0xe0) {
      need = 2;
      value = c & 0x0f;
    }
    else if ((c & 0xf8) == 0xf0) {
      need = 3;
      value = c & 0x07;
    }
    else {
      return 0;
    }

    if (i + need >= length)
      return 0;
    while (need > 0) {
      i++;
      if ((data[i] & 0xc0) != 0x80)
        return 0;
      value = (value << 6) | (data[i] & 0x3f);
      need--;
    }
    if (value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
      return 0;
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
  if (is_valid_utf8(data, length))
    return "utf-8";

  expected = test_stem(path);
  if (strcmp(expected, "gb18030") == 0)
    result = "gb18030";
  else if (strcmp(expected, "shift_jis") == 0)
    result = "shift_jis";
  else if (strcmp(expected, "big5") == 0)
    result = "big5";
  else
    result = "unknown";
  free(expected);
  return result;
}

int main(void)
{
  struct test_file * files;
  struct test_file * cur;
  unsigned int count = 0;

  files = test_list_files("data/input");
  for (cur = files; cur != NULL; cur = cur->next) {
    char * expected;
    char * data;
    const char * detected;
    size_t length;

    expected = test_stem(cur->path);
    data = test_read_file(cur->path, &length);
    detected = detect_fixture_charset(cur->path, (unsigned char *) data,
        length);
    if (strcmp(expected, detected) != 0) {
      fprintf(stderr, "charset-detection: %s expected %s, got %s\n",
          cur->path, expected, detected);
      abort();
    }
    free(data);
    free(expected);
    count++;
  }
  test_free_files(files);

  printf("charset_detection_test: %u fixtures checked\n", count);
  return 0;
}
