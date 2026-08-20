#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "charconv.h"

struct conversion_case {
  const char * charset;
  const unsigned char * input;
  size_t input_length;
};

static int check_conversion(const struct conversion_case * test)
{
  static const unsigned char expected[] = { 0xe3, 0x81, 0x82 };
  char * result;
  size_t result_length;
  int r;

  r = charconv_buffer("utf-8", test->charset,
      (const char *) test->input, test->input_length, &result,
      &result_length);
  if (r != MAIL_CHARCONV_NO_ERROR) {
    fprintf(stderr, "%s: conversion failed with error %d\n",
        test->charset, r);
    return 1;
  }
  if (result_length != sizeof(expected) ||
      memcmp(result, expected, sizeof(expected)) != 0) {
    fprintf(stderr, "%s: unexpected conversion result\n", test->charset);
    charconv_buffer_free(result);
    return 1;
  }
  charconv_buffer_free(result);
  return 0;
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
  puts("charconv_test: ok");
  return 0;
}
