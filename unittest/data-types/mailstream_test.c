#include "data_types_tests.h"

#include <stdlib.h>
#include <string.h>

#include "mailstream.h"
#include "mailstream_low.h"

struct scripted_stream {
  const ssize_t * writes;
  size_t writes_count;
  size_t writes_index;
  char output[64];
  size_t output_len;
};

static ssize_t scripted_read(mailstream_low * s, void * buf, size_t count)
{
  (void) s;
  (void) buf;
  (void) count;
  return -1;
}

static ssize_t scripted_write(mailstream_low * s, const void * buf,
    size_t count)
{
  struct scripted_stream * data;
  ssize_t result;
  size_t written;

  data = s->data;
  if (data->writes_count == 0) {
    data->writes_index ++;
    return 0;
  }
  if (data->writes_index >= data->writes_count)
    return -1;

  result = data->writes[data->writes_index ++];
  if (result <= 0)
    return result;

  written = (size_t) result;
  if (written > count)
    written = count;
  if (data->output_len + written > sizeof(data->output))
    return -1;

  memcpy(data->output + data->output_len, buf, written);
  data->output_len += written;
  return result;
}

static int scripted_close(mailstream_low * s)
{
  (void) s;
  return 0;
}

static int scripted_get_fd(mailstream_low * s)
{
  (void) s;
  return -1;
}

static void scripted_free(mailstream_low * s)
{
  free(s->data);
  free(s);
}

static void scripted_cancel(mailstream_low * s)
{
  (void) s;
}

static carray * scripted_get_certificate_chain(mailstream_low * s)
{
  (void) s;
  return NULL;
}

static int scripted_idle(mailstream_low * s)
{
  (void) s;
  return -1;
}

static mailstream_low_driver scripted_driver = {
  scripted_read,
  scripted_write,
  scripted_close,
  scripted_get_fd,
  scripted_free,
  scripted_cancel,
  NULL,
  scripted_get_certificate_chain,
  scripted_idle,
  scripted_idle,
  scripted_idle
};

static mailstream * scripted_stream_new(const ssize_t * writes,
    size_t writes_count, struct scripted_stream ** data_result)
{
  struct scripted_stream * data;
  mailstream_low * low;
  mailstream * stream;

  data = calloc(1, sizeof(* data));
  if (data == NULL)
    return NULL;

  data->writes = writes;
  data->writes_count = writes_count;

  low = mailstream_low_new(data, &scripted_driver);
  if (low == NULL) {
    free(data);
    return NULL;
  }

  stream = mailstream_new(low, 4);
  if (stream == NULL) {
    mailstream_low_free(low);
    return NULL;
  }

  * data_result = data;
  return stream;
}

static int check_mailstream_retries_zero_writes(
    test_failure_callback failure_callback, void * context)
{
  static const ssize_t writes[] = { 0, 0, 3, 0, 3 };
  struct scripted_stream * data;
  mailstream * stream;
  ssize_t written;
  int result;

  result = 0;
  data = NULL;
  stream = scripted_stream_new(writes, sizeof(writes) / sizeof(writes[0]),
      &data);
  TEST_CHECK(stream != NULL, "mailstream_new failed");

  written = mailstream_write(stream, "abcdef", 6);
  TEST_CHECK(written == 6, "mailstream_write did not retry zero writes");
  TEST_CHECK(data->output_len == 6, "unexpected output length");
  TEST_CHECK(memcmp(data->output, "abcdef", 6) == 0,
      "unexpected output data");

 cleanup:
  if (stream != NULL)
    mailstream_close(stream);
  return result;
}

static int check_mailstream_zero_write_limit(
    test_failure_callback failure_callback, void * context)
{
  struct scripted_stream * data;
  mailstream * stream;
  ssize_t written;
  int result;

  result = 0;
  data = NULL;
  stream = scripted_stream_new(NULL, 0, &data);
  TEST_CHECK(stream != NULL, "mailstream_new failed");

  written = mailstream_write(stream, "abcde", 5);
  TEST_CHECK(written == -1, "mailstream_write did not fail after no progress");
  TEST_CHECK(data->output_len == 0, "zero writes unexpectedly produced data");
  TEST_CHECK(data->writes_index > 1, "zero write was not retried");

 cleanup:
  if (stream != NULL)
    mailstream_close(stream);
  return result;
}

struct mailstream_case {
  const char * name;
  int (* run)(test_failure_callback failure_callback, void * context);
};

static const struct mailstream_case cases[] = {
  { "mailstream retries zero writes", check_mailstream_retries_zero_writes },
  { "mailstream fails after zero-write limit", check_mailstream_zero_write_limit },
};

size_t mailstream_test_count(void)
{
  return sizeof(cases) / sizeof(cases[0]);
}

const char * mailstream_test_name(size_t index)
{
  if (index >= mailstream_test_count())
    return NULL;
  return cases[index].name;
}

int mailstream_test_run_case(size_t index,
    test_failure_callback failure_callback, void * context)
{
  if (index >= mailstream_test_count()) {
    if (failure_callback != NULL)
      failure_callback(__FILE__, __LINE__, "index < mailstream_test_count()",
          "test case index is out of range", context);
    return -1;
  }

  return cases[index].run(failure_callback, context);
}
