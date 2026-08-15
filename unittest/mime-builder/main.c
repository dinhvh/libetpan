#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mailmime.h"
#include "mailmime_write_file.h"
#include "mime_builder_helpers.h"
#include "mime_builder_tests.h"
#include "test_utils.h"

static jmp_buf test_abort;
static test_failure_callback active_failure_callback;
static void * active_failure_context;
static const char * active_fixture_root;

static void fail_assertion(const char * file, unsigned line,
    const char * expression)
{
  if (active_failure_callback != NULL)
    active_failure_callback(file, line, expression, "assertion failed",
        active_failure_context);
  longjmp(test_abort, 1);
}

#undef assert
#define assert(condition) \
  do { if (!(condition)) fail_assertion(__FILE__, __LINE__, #condition); } while (0)

static const char * fixture_path(const char * path)
{
  static char result[4096];
  int written = snprintf(result, sizeof(result), "%s/%s",
      active_fixture_root, path);

  assert(written >= 0 && (size_t) written < sizeof(result));
  return result;
}

static struct mailmime * build_message1(void)
{
  struct mailmime * alternative;

  alternative = mime_builder_new_part(MAILMIME_MULTIPLE,
      "multipart/alternative; boundary=\"1\"", NULL);
  mime_builder_add_part(alternative,
      mime_builder_new_text_part("text/plain; charset=\"utf-8\"",
          MAILMIME_MECHANISM_7BIT, "This is a HTML content"));
  mime_builder_add_part(alternative,
      mime_builder_new_text_part("text/html; charset=\"utf-8\"",
          MAILMIME_MECHANISM_QUOTED_PRINTABLE,
          "<html><body>This is a HTML content</body></html>"));
  return mime_builder_wrap_message(alternative, "testMessageBuilder1", 0);
}

static struct mailmime * build_message2(void)
{
  struct mailmime * mixed;
  struct mailmime * alternative;

  mixed = mime_builder_new_part(MAILMIME_MULTIPLE,
      "multipart/mixed; boundary=\"2\"", NULL);
  alternative = mime_builder_new_part(MAILMIME_MULTIPLE,
      "multipart/alternative; boundary=\"1\"", NULL);
  mime_builder_add_part(alternative,
      mime_builder_new_text_part("text/plain; charset=\"utf-8\"",
          MAILMIME_MECHANISM_7BIT, "This is a HTML content"));
  mime_builder_add_part(alternative,
      mime_builder_new_text_part("text/html; charset=\"utf-8\"",
          MAILMIME_MECHANISM_QUOTED_PRINTABLE,
          "<html><body>This is a HTML content</body></html>"));
  mime_builder_add_part(mixed, alternative);
  mime_builder_add_part(mixed,
      mime_builder_new_attachment_part(fixture_path("input/photo.jpg"), NULL));
  mime_builder_add_part(mixed,
      mime_builder_new_attachment_part(fixture_path("input/photo2.jpg"), NULL));
  return mime_builder_wrap_message(mixed, "testMessageBuilder2", 1);
}

static struct mailmime * build_message3(void)
{
  struct mailmime * mixed;
  struct mailmime * alternative;
  struct mailmime * related;

  mixed = mime_builder_new_part(MAILMIME_MULTIPLE,
      "multipart/mixed; boundary=\"3\"", NULL);
  alternative = mime_builder_new_part(MAILMIME_MULTIPLE,
      "multipart/alternative; boundary=\"2\"", NULL);
  related = mime_builder_new_part(MAILMIME_MULTIPLE,
      "multipart/related; boundary=\"1\"", NULL);

  mime_builder_add_part(alternative,
      mime_builder_new_text_part("text/plain; charset=\"utf-8\"",
          MAILMIME_MECHANISM_7BIT, "This is a HTML content\r\n\r\n"));
  mime_builder_add_part(related,
      mime_builder_new_text_part("text/html; charset=\"utf-8\"",
          MAILMIME_MECHANISM_QUOTED_PRINTABLE,
          "<html><body><div>This is a HTML content</div><div><img "
          "src=\"cid:123\"></div></body></html>"));
  mime_builder_add_part(related,
      mime_builder_new_attachment_part(fixture_path("input/photo2.jpg"), "123"));
  mime_builder_add_part(alternative, related);
  mime_builder_add_part(mixed, alternative);
  mime_builder_add_part(mixed,
      mime_builder_new_attachment_part(fixture_path("input/photo.jpg"), NULL));
  return mime_builder_wrap_message(mixed, "testMessageBuilder3", 1);
}

static char * write_mime_to_memory(struct mailmime * mime, size_t * length)
{
  FILE * f;
  char * data;
  long size;
  int col = 0;
  int r;

  f = tmpfile();
  assert(f != NULL);

  r = mailmime_write_file(f, &col, mime);
  assert(r == MAILIMF_NO_ERROR);
  assert(fflush(f) == 0);

  size = ftell(f);
  assert(size >= 0);
  assert(fseek(f, 0, SEEK_SET) == 0);

  data = malloc((size_t) size + 1);
  assert(data != NULL);
  assert(fread(data, 1, (size_t) size, f) == (size_t) size);
  data[size] = '\0';
  fclose(f);

  *length = (size_t) size;
  return data;
}

static void assert_message_equals_fixture(const char * name,
    struct mailmime * message, const char * expected_path)
{
  char * expected;
  char * generated;
  size_t expected_length;
  size_t generated_length;

  expected = test_read_file(expected_path, &expected_length);
  generated = write_mime_to_memory(message, &generated_length);
  if (generated_length != expected_length ||
      memcmp(generated, expected, expected_length) != 0) {
    size_t i;
    size_t min_len = generated_length < expected_length ?
        generated_length : expected_length;

    for (i = 0; i < min_len; i++) {
      if (generated[i] != expected[i])
        break;
    }
    fprintf(stderr,
        "mime-builder: %s mailmime_write_file() output differs from %s "
        "at byte %lu (generated length %lu, expected length %lu)\n",
        name, expected_path, (unsigned long) i,
        (unsigned long) generated_length, (unsigned long) expected_length);
    if (i < min_len) {
      fprintf(stderr, "generated byte 0x%02x, expected byte 0x%02x\n",
          (unsigned char) generated[i], (unsigned char) expected[i]);
    }
    assert(0);
  }

  free(generated);
  free(expected);
}

static struct mailmime * (* const builders[])(void) = {
  build_message1,
  build_message2,
  build_message3,
};

static const char * const names[] = {
  "testMessageBuilder1",
  "testMessageBuilder2",
  "testMessageBuilder3",
};

static const char * const outputs[] = {
  "output/builder1.eml",
  "output/builder2.eml",
  "output/builder3.eml",
};

size_t mime_builder_test_count(void)
{
  return sizeof(builders) / sizeof(builders[0]);
}

const char * mime_builder_test_name(size_t index)
{
  if (index >= mime_builder_test_count()) return NULL;
  return names[index];
}

int mime_builder_test_run_case(size_t index, const char * fixture_root,
    test_failure_callback failure_callback, void * context)
{
  struct mailmime * message = NULL;

  if (index >= mime_builder_test_count()) return -1;
  active_failure_callback = failure_callback;
  active_failure_context = context;
  active_fixture_root = fixture_root;
  if (setjmp(test_abort) != 0)
    return -1;

  puts(names[index]);
  message = builders[index]();
  assert_message_equals_fixture(names[index], message,
      fixture_path(outputs[index]));
  mailmime_free(message);
  return 0;
}

int mime_builder_test_run(const char * fixture_root)
{
  size_t index;
  for (index = 0; index < mime_builder_test_count(); index++) {
    if (mime_builder_test_run_case(index, fixture_root, NULL, NULL) != 0)
      return -1;
  }
  return 0;
}

#ifndef MIME_BUILDER_NO_MAIN
int main(void)
{
  if (mime_builder_test_run("data") != 0)
    return 1;
  puts("mime_builder_test: 3 messages matched mailmime_write_file() output");
  return 0;
}
#endif
