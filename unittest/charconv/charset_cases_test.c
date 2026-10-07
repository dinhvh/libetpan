#include <config.h>
#include "charset_cases.h"
#include "charconv.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <wchar.h>
#if defined(__APPLE__) && !defined(HAVE_ICU)
#include "charset_apple_cases.h"
#endif

static const struct charset_case * case_at(size_t index)
{
  return index < charset_singlebyte_count ? &charset_singlebyte_cases[index] :
      &charset_multibyte_cases[index - charset_singlebyte_count];
}

static int inventory_check(size_t total)
{
  const char * paths[] = { "data/iconv-glibc-charsets.txt",
    "charconv/data/iconv-glibc-charsets.txt",
    "unittest/charconv/data/iconv-glibc-charsets.txt" };
  unsigned char * seen = calloc(total, 1);
  FILE * file = NULL;
  char line[256];
  size_t index, count = 0;
  int failed = 0;
  if (seen == NULL)
    return 1;
  for (index = 0; index < sizeof(paths) / sizeof(paths[0]); index++) {
    file = fopen(paths[index], "r");
    if (file != NULL)
      break;
  }
  if (file == NULL) {
    fprintf(stderr, "charset inventory: cannot open recorded inventory\n");
    free(seen);
    return 1;
  }
  while (fgets(line, sizeof(line), file) != NULL) {
    size_t matches = 0;
    if (line[0] == '#')
      continue;
    line[strcspn(line, "\r\n")] = '\0';
    if (line[0] == '\0')
      continue;
    count++;
    for (index = 0; index < total; index++) {
      if (strcmp(line, case_at(index)->name) == 0) {
        matches++;
        if (seen[index]++) {
          fprintf(stderr, "charset inventory: repeated name %s\n", line);
          failed = 1;
        }
      }
    }
    if (matches != 1) {
      fprintf(stderr, "charset inventory: %s has %zu fixtures\n", line, matches);
      failed = 1;
    }
  }
  if (ferror(file))
    failed = 1;
  fclose(file);
  for (index = 0; index < total; index++) {
    if (!seen[index]) {
      fprintf(stderr, "charset inventory: extra fixture %s\n", case_at(index)->name);
      failed = 1;
    }
  }
  if (count != 1180 || count != total) {
    fprintf(stderr, "charset inventory: expected 1180 names, inventory=%zu fixtures=%zu\n",
        count, total);
    failed = 1;
  }
  free(seen);
  return failed;
}

static int check_direction(const struct charset_case * test, int encode,
    const struct charset_expectation * expected, const char * backend)
{
  const struct charset_bytes * input = encode ? &test->unicode : &test->encoded;
  const char * tocode = encode ? test->name : "UTF-8";
  const char * fromcode = encode ? "UTF-8" : test->name;
  char * result = NULL;
  size_t length = 0;
  int status, failed = 0, comparable = 0;
  status = charconv_buffer(tocode, fromcode, input->data, input->length,
      &result, &length);
  if (status != expected->status) {
    fprintf(stderr, "%s [%s] charconv_buffer %s: status %d expected %d (%s)\n",
        test->name, backend, encode ? "encode" : "decode", status,
        expected->status, test->note);
    failed = 1;
  }
  if (status == MAIL_CHARCONV_NO_ERROR && expected->status == status) {
    size_t offset = 0, limit = length < expected->bytes.length ? length : expected->bytes.length;
    while (offset < limit && result[offset] == expected->bytes.data[offset])
      offset++;
    comparable = length == expected->bytes.length;
    if (!comparable || offset != limit || result[length] != '\0') {
      fprintf(stderr, "%s [%s] charconv_buffer %s: differing byte %zu, length %zu expected %zu\n",
          test->name, backend, encode ? "encode" : "decode", offset,
          length, expected->bytes.length);
      failed = 1;
    }
  }
  if (status == MAIL_CHARCONV_NO_ERROR)
    charconv_buffer_free(result);
  result = NULL;
  status = charconv(tocode, fromcode, input->data, input->length, &result);
  if (status != expected->status) {
    fprintf(stderr, "%s [%s] charconv %s: status %d expected %d\n",
        test->name, backend, encode ? "encode" : "decode", status, expected->status);
    failed = 1;
  }
  if (status == MAIL_CHARCONV_NO_ERROR) {
    if (expected->status == status && comparable &&
        (memcmp(result, expected->bytes.data, expected->bytes.length) != 0 ||
         result[expected->bytes.length] != '\0')) {
      fprintf(stderr, "%s [%s] charconv %s: byte output/terminator differs\n",
          test->name, backend, encode ? "encode" : "decode");
      failed = 1;
    }
    free(result);
  }
  return failed;
}

int charset_cases_test(void)
{
  size_t total = charset_singlebyte_count + charset_multibyte_count;
  size_t index, decoded = 0, encoded = 0, unknown_decode = 0, unknown_encode = 0;
  size_t conversion_errors = 0, limited = 0, failures = 0;
  if (inventory_check(total))
    return 1;
  for (index = 0; index < total; index++) {
    const struct charset_case * test = case_at(index);
    const struct charset_expectation * decode = &test->iconv_decode;
    const struct charset_expectation * encode = &test->iconv_encode;
    const char * backend = "iconv";
#if defined(HAVE_ICU) && !defined(HAVE_ICONV)
    decode = &test->icu_decode;
    encode = &test->icu_encode;
    backend = "ICU-only";
#elif defined(HAVE_ICU) && defined(HAVE_ICONV)
    if (test->dual_decode_icu)
      decode = &test->icu_decode;
    backend = "ICU+iconv";
#endif
#if defined(__APPLE__) && !defined(HAVE_ICU)
    if (index >= sizeof(apple_charset_cases) / sizeof(apple_charset_cases[0]) ||
        strcmp(test->name, apple_charset_cases[index].name) != 0) {
      fprintf(stderr, "Apple charset inventory mismatch at %zu\n", index);
      return 1;
    }
#ifdef HAVE_ICONV
    decode = &apple_charset_cases[index].iconv_decode;
    encode = &apple_charset_cases[index].iconv_encode;
    backend = "Apple iconv/CoreFoundation";
#else
    decode = &apple_charset_cases[index].cf_decode;
    encode = &apple_charset_cases[index].cf_encode;
    backend = "CoreFoundation-only";
#endif
#endif
    if (test->platform) {
      const uint16_t byte_order = 1;
      if (((test->platform & 1) && sizeof(wchar_t) != 4) ||
          *(const unsigned char *) &byte_order != 1) {
        fprintf(stderr, "%s: platform-limited fixture (recorded little-endian native/Unicode format)\n", test->name);
        limited++;
        continue;
      }
    }
    failures += check_direction(test, 0, decode, backend);
    failures += check_direction(test, 1, encode, backend);
    decoded += decode->status == MAIL_CHARCONV_NO_ERROR;
    encoded += encode->status == MAIL_CHARCONV_NO_ERROR;
    unknown_decode += decode->status == MAIL_CHARCONV_ERROR_UNKNOWN_CHARSET;
    unknown_encode += encode->status == MAIL_CHARCONV_ERROR_UNKNOWN_CHARSET;
    conversion_errors += decode->status == MAIL_CHARCONV_ERROR_CONV;
    conversion_errors += encode->status == MAIL_CHARCONV_ERROR_CONV;
  }
  printf("charset inventory: %zu names; decode=%zu encode=%zu; unsupported decode=%zu encode=%zu; conversion-errors=%zu platform-limited=%zu; failures=%zu\n",
      total, decoded, encoded, unknown_decode, unknown_encode, conversion_errors, limited, failures);
  return failures != 0;
}
