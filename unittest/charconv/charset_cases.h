#ifndef LIBETPAN_CHARSET_CASES_H
#define LIBETPAN_CHARSET_CASES_H
#include <stddef.h>

struct charset_bytes { const char * data; size_t length; };
struct charset_expectation { int status; struct charset_bytes bytes; };
struct charset_case {
  const char * name;
  const char * family;
  const char * note;
  struct charset_bytes encoded;
  struct charset_bytes unicode;
  struct charset_expectation iconv_decode, iconv_encode;
  struct charset_expectation icu_decode, icu_encode;
  int dual_decode_icu;
  /* bit 0: four-byte wchar_t; bit 1: recorded native Unicode byte order. */
  int platform;
};
extern const struct charset_case charset_singlebyte_cases[];
extern const size_t charset_singlebyte_count;
extern const struct charset_case charset_multibyte_cases[];
extern const size_t charset_multibyte_count;
int charset_cases_test(void);
int charset_cases_test_with_inventory(const char * inventory_path);
#endif
