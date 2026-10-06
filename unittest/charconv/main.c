#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "charconv.h"

struct conversion_case {
  const char * charset;
  const unsigned char * input;
  size_t input_length;
};

static int check_bytes(const char * tocode, const char * fromcode,
    const char * input, size_t input_length,
    const char * expected, size_t expected_length)
{
  char * result = NULL;
  size_t result_length = 0;
  int r;

  r = charconv(tocode, fromcode, input, input_length, &result);
  if (r != MAIL_CHARCONV_NO_ERROR) {
    fprintf(stderr, "%s -> %s: charconv failed with %d\n",
        fromcode, tocode, r);
    return 1;
  }
  if (strlen(result) != strlen(expected) ||
      memcmp(result, expected, expected_length) != 0 ||
      result[expected_length] != '\0') {
    fprintf(stderr, "%s -> %s: unexpected charconv bytes\n", fromcode, tocode);
    free(result);
    return 1;
  }
  free(result);

  r = charconv_buffer(tocode, fromcode, input, input_length,
      &result, &result_length);
  if (r != MAIL_CHARCONV_NO_ERROR) {
    fprintf(stderr, "%s -> %s: charconv_buffer failed with %d\n",
        fromcode, tocode, r);
    return 1;
  }
  if (result_length != expected_length ||
      memcmp(result, expected, expected_length) != 0 ||
      result[result_length] != '\0') {
    fprintf(stderr, "%s -> %s: unexpected buffer bytes/length\n", fromcode, tocode);
    charconv_buffer_free(result);
    return 1;
  }
  charconv_buffer_free(result);
  return 0;
}

static int check_conversion(const struct conversion_case * test)
{
  return check_bytes("utf-8", test->charset,
      (const char *) test->input, test->input_length, "\xe3\x81\x82", 3);
}

static int check_error(const char * tocode, const char * fromcode,
    const char * input, size_t input_length, int expected)
{
  char * result = NULL;
  size_t result_length = 0;
  int r;

  r = charconv(tocode, fromcode, input, input_length, &result);
  if (r == MAIL_CHARCONV_NO_ERROR)
    free(result);
  if (r != expected) {
    fprintf(stderr, "charconv: expected error %d, got %d\n", expected, r);
    return 1;
  }
  r = charconv_buffer(tocode, fromcode, input, input_length,
      &result, &result_length);
  if (r == MAIL_CHARCONV_NO_ERROR)
    charconv_buffer_free(result);
  if (r != expected) {
    fprintf(stderr, "charconv_buffer: expected error %d, got %d\n", expected, r);
    return 1;
  }
  return 0;
}

static int extension_result;
static int extension_calls;

static int test_extension(const char * tocode, const char * fromcode,
    const char * input, size_t length, char * result, size_t * result_length)
{
  (void) tocode;
  (void) fromcode;
  (void) input;
  (void) length;
  extension_calls++;
  if (extension_result == MAIL_CHARCONV_NO_ERROR) {
    if (*result_length < 2)
      return MAIL_CHARCONV_ERROR_MEMORY;
    memcpy(result, "ok", 2);
    *result_length = 2;
  }
  return extension_result;
}

static int check_general_conversions(void)
{
  struct byte_case {
    const char * tocode;
    const char * fromcode;
    const char * input;
    size_t input_length;
    const char * expected;
    size_t expected_length;
  };
  static const struct byte_case cases[] = {
    { "utf-8", "ascii", "Hello", 5, "Hello", 5 },
    { "utf-8", "utf-8", "A\0B", 3, "A\0B", 3 },
    { "utf-8", "utf-8", "", 0, "", 0 },
    { "utf-8", "iso-8859-1", "caf\xe9", 4, "caf\xc3\xa9", 5 },
    { "utf-8", "windows-1252", "\x80", 1, "\xe2\x82\xac", 3 },
    { "utf-8", "windows-1251", "\xc0", 1, "\xd0\x90", 2 },
    { "utf-8", "koi8-r", "\xe1", 1, "\xd0\x90", 2 },
    { "utf-8", "koi8_r", "\xe1", 1, "\xd0\x90", 2 },
    { "utf-8", "GBK", "\xd6\xd0", 2, "\xe4\xb8\xad", 3 },
    { "utf-8", "GB2312", "\xd6\xd0", 2, "\xe4\xb8\xad", 3 },
    { "utf-8", "GB_2312-80", "\xd6\xd0", 2, "\xe4\xb8\xad", 3 },
    { "utf-8", "GB18030", "\xd6\xd0", 2, "\xe4\xb8\xad", 3 },
    { "utf-8", "Big5", "\xa4\xa4", 2, "\xe4\xb8\xad", 3 },
    { "utf-8", "ks_c_5601-1987", "\xb0\xa1", 2, "\xea\xb0\x80", 3 },
    { "utf-8", "iso-8859-8-i", "\xe0", 1, "\xd7\x90", 2 },
    { "iso-8859-1", "utf-8", "caf\xc3\xa9", 5, "caf\xe9", 4 },
    { "windows-1252", "utf-8", "\xe2\x82\xac", 3, "\x80", 1 },
    { "shift_jis", "utf-8", "\xe3\x81\x82", 3, "\x82\xa0", 2 },
    { "iso-2022-jp", "utf-8", "\xe3\x81\x82", 3,
#if defined(HAVE_ICU) && !defined(HAVE_ICONV)
      "\x1b$B$\x22\x1b(B", 8 },
#else
      "\x1b$B$\x22", 5 },
#endif
    { "utf-32le", "ascii", "A", 1, "A\0\0\0", 4 },
  };
  size_t index;
  int failed;

  for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index++) {
    const struct byte_case * test = &cases[index];
    if (check_bytes(test->tocode, test->fromcode, test->input,
        test->input_length, test->expected, test->expected_length) != 0)
      return 1;
  }
  if (check_error("utf-8", "x-libetpan-unknown", "A", 1,
          MAIL_CHARCONV_ERROR_UNKNOWN_CHARSET) ||
      check_error("x-libetpan-unknown", "utf-8", "A", 1,
          MAIL_CHARCONV_ERROR_UNKNOWN_CHARSET) ||
      check_error("utf-8", "ascii", "A", (size_t) -1,
          MAIL_CHARCONV_ERROR_MEMORY))
    return 1;

#if defined(HAVE_ICU) && !defined(HAVE_ICONV)
  /* ICU replaces malformed/truncated input with U+FFFD by default. */
  if (check_bytes("utf-8", "utf-8", "\xff", 1, "\xef\xbf\xbd", 3) ||
      check_bytes("utf-8", "utf-8", "\xe2\x82", 2, "\xef\xbf\xbd", 3) ||
      /* ASCII's default ICU substitution for an unmappable character is SUB. */
      check_bytes("ascii", "utf-8", "\xe2\x82\xac", 3, "\x1a", 1))
    return 1;
#endif

  extended_charconv = test_extension;
  extension_result = MAIL_CHARCONV_ERROR_UNKNOWN_CHARSET;
  failed = check_bytes("utf-8", "windows-1252", "\x80", 1,
      "\xe2\x82\xac", 3);
  extension_result = MAIL_CHARCONV_NO_ERROR;
  failed |= check_bytes("utf-8", "x-extension", "A", 1, "ok", 2);
  extension_result = MAIL_CHARCONV_ERROR_CONV;
  failed |= check_error("utf-8", "ascii", "A", 1, MAIL_CHARCONV_ERROR_CONV);
  extended_charconv = NULL;
  if (extension_calls != 6) {
    fprintf(stderr, "extension callback precedence changed\n");
    return 1;
  }
  return failed;
}

int main(void)
{
  static const unsigned char iso2022jp[] = {
    0x1b, 0x24, 0x42, 0x24, 0x22, 0x1b, 0x28, 0x42
  };
  static const unsigned char shiftjis[] = { 0x82, 0xa0 };
  static const unsigned char eucjp[] = { 0xa4, 0xa2 };
  static const struct conversion_case cases[] = {
    { "iso-2022-jp", iso2022jp, sizeof(iso2022jp) },
    { "iso-2022-jp-2", iso2022jp, sizeof(iso2022jp) },
    { "shift_jis", shiftjis, sizeof(shiftjis) },
    { "shift-jis", shiftjis, sizeof(shiftjis) },
    { "euc-jp", eucjp, sizeof(eucjp) },
    { "eucjp", eucjp, sizeof(eucjp) },
#ifdef HAVE_COREFOUNDATION_CHARCONV
    { "iso-2022-jp-1", iso2022jp, sizeof(iso2022jp) },
    { "windows-31j", shiftjis, sizeof(shiftjis) },
    { "cp932", shiftjis, sizeof(shiftjis) },
    { "ms932", shiftjis, sizeof(shiftjis) },
    { "x-sjis", shiftjis, sizeof(shiftjis) },
    { "x-euc-jp", eucjp, sizeof(eucjp) },
#endif
  };
  size_t index;

  for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index++) {
    if (check_conversion(&cases[index]) != 0)
      return 1;
  }
  if (check_general_conversions() != 0)
    return 1;
  puts("charconv_test: ok");
  return 0;
}
