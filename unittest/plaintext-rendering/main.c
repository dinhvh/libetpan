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
#include "plaintext_rendering_tests.h"
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

static int compare_text(const char * input_path, const char * expected_path,
    const char * generated, test_failure_callback failure_callback,
    void * context)
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
    if (failure_callback != NULL)
      failure_callback(__FILE__, __LINE__, "rendered text matches fixture",
          "rendered plain text differs from the expected fixture", context);
    free(expected);
    free(generated_norm);
    free(expected_norm);
    return -1;
  }
  free(expected);
  free(generated_norm);
  free(expected_norm);
  return 0;
}

static int check_rendering_fixture(const char * input_path,
    const char * input_root, const char * output_root,
    test_failure_callback failure_callback, void * context)
{
  char * data;
  char * expected_path = NULL;
  char * replaced_path;
  char * html = NULL;
  char * text = NULL;
  size_t length;
  size_t index = 0;
  char * default_charset = NULL;
  struct mailmime * mime = NULL;
  int r;
  int result = -1;

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
    if (failure_callback != NULL)
      failure_callback(__FILE__, __LINE__, "mailmime_parse",
          "failed to parse MIME message", context);
    goto cleanup;
  }

  expected_path = test_replace_prefix(input_path, input_root, output_root);
  replaced_path = test_replace_extension(expected_path, ".txt");
  free(expected_path);
  expected_path = replaced_path;
  if (!test_file_exists(expected_path)) {
    fprintf(stderr, "plaintext-rendering: known missing output for %s\n",
        input_path);
    result = 0;
  }
  else {
    html = plaintext_rendering_render_message_html(mime,
        default_charset != NULL ? default_charset : "utf-8");
    text = plaintext_rendering_flatten_html(html);
    result = compare_text(input_path, expected_path, text, failure_callback,
        context);
  }

cleanup:
  free(html);
  free(text);
  mailmime_free(mime);
  free(expected_path);
  free(default_charset);
  free(data);
  return result;
}

int plaintext_rendering_test_run_case(const char * relative_path,
    const char * fixture_root, test_failure_callback failure_callback,
    void * context)
{
  char * input_root = test_path_join(fixture_root, "input");
  char * output_root = test_path_join(fixture_root, "output");
  char * input_path = test_path_join(input_root, relative_path);
  int result;

  plaintext_rendering_html_flattener_init();
  result = check_rendering_fixture(input_path, input_root, output_root,
      failure_callback, context);
  plaintext_rendering_html_flattener_cleanup();
  free(input_path);
  free(output_root);
  free(input_root);
  return result;
}

int plaintext_rendering_test_run(const char * fixture_root)
{
  char * input_root = test_path_join(fixture_root, "input");
  struct test_file * files;
  struct test_file * cur;
  unsigned int count = 0;
  int result = 0;

  files = test_list_files(input_root);
  for (cur = files; cur != NULL; cur = cur->next) {
    const char * relative_path = cur->path + strlen(input_root) + 1;
    if (plaintext_rendering_test_run_case(relative_path, fixture_root,
            NULL, NULL) != 0) {
      result = -1;
      break;
    }
    count++;
  }
  test_free_files(files);
  free(input_root);

  printf("plaintext_rendering_test: %u fixtures matched plain text\n", count);
  return result;
}

#ifndef PLAINTEXT_RENDERING_NO_MAIN
int main(void)
{
  return plaintext_rendering_test_run("data") == 0 ? 0 : 1;
}
#endif
