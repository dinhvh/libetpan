#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mailimf.h"
#include "mailmime.h"
#include "mmapstring.h"
#include "mime_serializer.h"
#include "test_utils.h"

static void compare_json(const char * input_path, const char * expected_path,
    const char * generated)
{
  char * expected;
  size_t expected_len;
  size_t generated_len = strlen(generated);

  expected = test_read_file(expected_path, &expected_len);
  if (generated_len != expected_len ||
      memcmp(generated, expected, expected_len) != 0) {
    size_t i;
    size_t min_len = generated_len < expected_len ? generated_len : expected_len;
    for (i = 0; i < min_len; i++) {
      if (generated[i] != expected[i])
        break;
    }
    fprintf(stderr, "mime-parser: JSON mismatch for %s\nexpected: %s\n"
        "first diff byte: %lu (generated length %lu, expected length %lu)\n",
        input_path, expected_path, (unsigned long) i,
        (unsigned long) generated_len, (unsigned long) expected_len);
    fprintf(stderr, "generated prefix: %.500s\n", generated);
    fprintf(stderr, "expected prefix: %.500s\n", expected);
    {
      FILE * f = fopen("/tmp/mime-parser-generated.json", "wb");
      if (f != NULL) {
        fwrite(generated, 1, generated_len, f);
        fclose(f);
      }
    }
    abort();
  }
  free(expected);
}

static void check_message_fixture(const char * input_path,
    const char * input_root, const char * output_root)
{
  char * data;
  char * original_data;
  char * expected_path;
  char * expected_json;
  MMAPString * generated;
  size_t expected_len;
  size_t length;
  size_t index = 0;
  size_t headers_index = 0;
  struct mailimf_fields * header_fields = NULL;
  struct mailmime * mime = NULL;
  int r;

  data = test_read_file(input_path, &length);
  original_data = data;
  if (length > 5 && strncmp(data, "From ", 5) == 0) {
    char * first_lf = strchr(data, '\n');
    if (first_lf != NULL) {
      length -= (size_t) (first_lf + 1 - data);
      data = first_lf + 1;
    }
  }
  r = mailimf_envelope_and_optional_fields_parse(data, length, &headers_index,
      &header_fields);
  if (r != MAILIMF_NO_ERROR || header_fields == NULL) {
    fprintf(stderr, "mime-parser: failed to parse headers for %s (error %d)\n",
        input_path, r);
    abort();
  }

  r = mailmime_parse(data, length, &index, &mime);
  if (r != MAILIMF_NO_ERROR || mime == NULL) {
    fprintf(stderr, "mime-parser: failed to parse %s (error %d)\n",
        input_path, r);
    abort();
  }

  expected_path = test_replace_prefix(input_path, input_root, output_root);
  if (!test_file_exists(expected_path)) {
    mime_serializer_set_context(NULL, header_fields);
    generated = mime_serializer_serialize_message(mime);
    fprintf(stderr, "mime-parser: missing expected fixture %s\n"
        "generated JSON for %s:\n%s\n",
        expected_path, input_path, generated->str);
    mmap_string_free(generated);
    abort();
  }

  expected_json = test_read_file(expected_path, &expected_len);
  mime_serializer_set_context(expected_json, header_fields);
  generated = mime_serializer_serialize_message(mime);
  compare_json(input_path, expected_path, generated->str);

  mmap_string_free(generated);
  free(expected_json);
  mailimf_fields_free(header_fields);
  mailmime_free(mime);
  free(expected_path);
  free(original_data);
}

int main(void)
{
  const char * input_root = "data/input";
  const char * output_root = "data/output";
  struct test_file * files;
  struct test_file * cur;
  unsigned int count = 0;

  files = test_list_files(input_root);
  for (cur = files; cur != NULL; cur = cur->next) {
    printf("mime-parser: running %s\n", cur->path);
    check_message_fixture(cur->path, input_root, output_root);
    count++;
  }
  test_free_files(files);

  printf("mime_parser_serialization_test: %u fixtures matched JSON\n", count);
  return 0;
}
