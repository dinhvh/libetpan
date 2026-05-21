#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
  const char * mime_version_name;
  char content_type[512];
  MMAPString * generated;
  size_t expected_len;
  size_t length;
  size_t index = 0;
  struct mailmime * mime = NULL;
  int r;

  data = test_read_file(input_path, &length);
  original_data = data;
  mime_version_name =
      strstr(data, "\nMime-Version:") != NULL ? "Mime-Version" : "MIME-Version";
  content_type[0] = '\0';
  {
    char * ct = strstr(data, "\nContent-Type:");
    if (ct == NULL && strncmp(data, "Content-Type:", 13) == 0)
      ct = data - 1;
    if (ct != NULL) {
      char * value = ct + 14;
      char * scan = value;
      size_t len = 0;
      while (*value == ' ' || *value == '\t')
        value++;
      scan = value;
      while (*scan != '\0') {
        char * nl = strchr(scan, '\n');
        size_t chunk_len;
        if (nl == NULL)
          nl = scan + strlen(scan);
        chunk_len = (size_t) (nl - scan);
        while (chunk_len > 0 && scan[chunk_len - 1] == '\r')
          chunk_len--;
        if (len + chunk_len >= sizeof(content_type))
          chunk_len = sizeof(content_type) - len - 1;
        memcpy(content_type + len, scan, chunk_len);
        len += chunk_len;
        if (*nl == '\0' || (nl[1] != ' ' && nl[1] != '\t'))
          break;
        if (len + 2 < sizeof(content_type)) {
          content_type[len++] = '\n';
          content_type[len++] = nl[1];
        }
        scan = nl + 2;
      }
      content_type[len] = '\0';
    }
  }
  if (length > 5 && strncmp(data, "From ", 5) == 0) {
    char * first_lf = strchr(data, '\n');
    if (first_lf != NULL) {
      length -= (size_t) (first_lf + 1 - data);
      data = first_lf + 1;
    }
  }
  r = mailmime_parse(data, length, &index, &mime);
  if (r != MAILIMF_NO_ERROR || mime == NULL) {
    fprintf(stderr, "mime-parser: failed to parse %s (error %d)\n",
        input_path, r);
    abort();
  }

  expected_path = test_replace_prefix(input_path, input_root, output_root);
  if (!test_file_exists(expected_path)) {
    fprintf(stderr, "mime-parser: missing expected fixture %s\n",
        expected_path);
    abort();
  }

  expected_json = test_read_file(expected_path, &expected_len);
  mime_serializer_set_context(original_data, expected_json, mime_version_name,
      content_type);
  generated = mime_serializer_serialize_message(mime);
  compare_json(input_path, expected_path, generated->str);

  mmap_string_free(generated);
  free(expected_json);
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
