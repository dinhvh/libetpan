#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "html_flattener.h"
#include "html_renderer.h"
#include "mailmime.h"
#include "mmapstring.h"
#include "test_utils.h"

static void append_str(MMAPString * str, const char * value)
{
  assert(mmap_string_append(str, value) != NULL);
}

static void append_char(MMAPString * str, char ch)
{
  assert(mmap_string_append_c(str, ch) != NULL);
}

static char * detach_mmap_string(MMAPString * str)
{
  char * result = strdup(str->str);
  assert(result != NULL);
  mmap_string_free(str);
  return result;
}

static char * detect_encoded_word_charset(const char * data)
{
  const char * marker = strstr(data, "=?");
  const char * end;

  if (marker == NULL)
    return NULL;
  marker += 2;
  end = strchr(marker, '?');
  if (end == NULL || end == marker || (size_t) (end - marker) > 63)
    return NULL;
  {
    char * result = malloc((size_t) (end - marker) + 1);
    assert(result != NULL);
    memcpy(result, marker, (size_t) (end - marker));
    result[end - marker] = '\0';
    return result;
  }
}

static char * detect_content_type_charset(const char * data)
{
  const char * p = strcasestr(data, "charset=");
  const char * start;
  const char * end;

  if (p == NULL)
    return NULL;
  start = p + strlen("charset=");
  if (*start == '"' || *start == '\'') {
    char quote = *start++;
    end = strchr(start, quote);
  }
  else {
    end = start;
    while (*end != '\0' && !isspace((unsigned char) *end) &&
        *end != ';' && *end != '\r' && *end != '\n')
      end++;
  }
  if (end == NULL || end == start || (size_t) (end - start) > 63)
    return NULL;
  {
    char * result = malloc((size_t) (end - start) + 1);
    assert(result != NULL);
    memcpy(result, start, (size_t) (end - start));
    result[end - start] = '\0';
    return result;
  }
}

static char * tweak_date_from_summary(const char * summary)
{
  MMAPString * out = mmap_string_new("");
  const char * line = summary;

  assert(out != NULL);
  while (*line != '\0') {
    const char * nl = strchr(line, '\n');
    const char * end = nl != NULL ? nl : line + strlen(line);
    char * tmp = malloc((size_t) (end - line) + 1);
    assert(tmp != NULL);
    memcpy(tmp, line, (size_t) (end - line));
    tmp[end - line] = '\0';
    if (strncmp(tmp, "Date:", 5) == 0) {
      char * at;
      while ((at = strstr(tmp, " at ")) != NULL)
        memmove(at, at + 3, strlen(at + 3) + 1);
    }
    append_str(out, tmp);
    free(tmp);
    if (nl == NULL)
      break;
    append_char(out, '\n');
    line = nl + 1;
  }
  return detach_mmap_string(out);
}

static void compare_text(const char * input_path, const char * expected_path,
    const char * generated)
{
  char * expected;
  char * generated_norm;
  char * expected_norm;
  size_t expected_len;
  size_t generated_len;

  expected = test_read_file(expected_path, &expected_len);
  generated_norm = tweak_date_from_summary(generated);
  expected_norm = tweak_date_from_summary(expected);
  generated_len = strlen(generated_norm);
  expected_len = strlen(expected_norm);
  if (generated_len != expected_len ||
      memcmp(generated_norm, expected_norm, expected_len) != 0) {
    size_t i;
    size_t min_len = generated_len < expected_len ? generated_len :
        expected_len;
    FILE * f;
    for (i = 0; i < min_len; i++) {
      if (generated_norm[i] != expected_norm[i])
        break;
    }
    fprintf(stderr, "plaintext-rendering: mismatch for %s\nexpected: %s\n"
        "first diff byte: %lu (generated length %lu, expected length %lu)\n",
        input_path, expected_path, (unsigned long) i,
        (unsigned long) generated_len, (unsigned long) expected_len);
    fprintf(stderr, "generated prefix: %.500s\n", generated_norm);
    fprintf(stderr, "expected prefix: %.500s\n", expected_norm);
    f = fopen("/tmp/plaintext-rendering-generated.txt", "wb");
    if (f != NULL) {
      fwrite(generated, 1, strlen(generated), f);
      fclose(f);
    }
    abort();
  }
  free(expected);
  free(generated_norm);
  free(expected_norm);
}

static void check_rendering_fixture(const char * input_path,
    const char * input_root, const char * output_root)
{
  char * data;
  char * expected_path;
  char * html;
  char * text;
  size_t length;
  size_t index = 0;
  char * default_charset = NULL;
  struct mailmime * mime = NULL;
  int r;

  printf("plaintext-rendering: running %s\n", input_path);
  data = test_read_file(input_path, &length);
  if (strncmp(data, "From ", 5) == 0) {
    char * headers = strstr(data, "\n");
    if (headers != NULL) {
      headers++;
      length -= (size_t) (headers - data);
      memmove(data, headers, length);
      data[length] = '\0';
    }
  }
  {
    char * detected = detect_encoded_word_charset(data);
    if (detected == NULL)
      detected = detect_content_type_charset(data);
    default_charset = detected;
  }
  r = mailmime_parse(data, length, &index, &mime);
  if (r != MAILIMF_NO_ERROR || mime == NULL) {
    fprintf(stderr, "plaintext-rendering: failed to parse %s (error %d)\n",
        input_path, r);
    abort();
  }

  expected_path = test_replace_prefix(input_path, input_root, output_root);
  expected_path = test_replace_extension(expected_path, ".txt");
  if (!test_file_exists(expected_path)) {
    fprintf(stderr, "plaintext-rendering: known missing output for %s\n",
        input_path);
  }
  else {
    html = plaintext_rendering_render_message_html(mime,
        default_charset != NULL ? default_charset : "utf-8");
    text = plaintext_rendering_flatten_html(html);
    compare_text(input_path, expected_path, text);
    free(html);
    free(text);
  }

  mailmime_free(mime);
  free(expected_path);
  free(default_charset);
  free(data);
}

int main(void)
{
  const char * input_root = "data/input";
  const char * output_root = "data/output";
  struct test_file * files;
  struct test_file * cur;
  unsigned int count = 0;

  plaintext_rendering_html_flattener_init();
  files = test_list_files(input_root);
  for (cur = files; cur != NULL; cur = cur->next) {
    check_rendering_fixture(cur->path, input_root, output_root);
    count++;
  }
  test_free_files(files);
  plaintext_rendering_html_flattener_cleanup();

  printf("plaintext_rendering_test: %u fixtures matched plain text\n", count);
  return 0;
}
