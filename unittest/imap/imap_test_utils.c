#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "imap_test_utils.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zlib.h>

#include "mailimap.h"
#include "mailimap_parser.h"
#include "mailstream.h"
#include "mailstream_compress.h"
#include "mailstream_low.h"
#include "mmapstring.h"

struct memory_stream {
  unsigned char * input;
  size_t input_len;
  size_t input_pos;
};

struct capture_stream {
  MMAPString * output;
};

struct script_stream {
  struct memory_stream memory;
  MMAPString * output;
};

static ssize_t memory_read(mailstream_low * s, void * buf, size_t count)
{
  struct memory_stream * data = s->data;
  size_t remaining = data->input_len - data->input_pos;
  size_t chunk = remaining < count ? remaining : count;

  if (chunk == 0)
    return 0;

  memcpy(buf, data->input + data->input_pos, chunk);
  data->input_pos += chunk;
  return (ssize_t) chunk;
}

static ssize_t memory_write(mailstream_low * s, const void * buf, size_t count)
{
  (void) s;
  (void) buf;
  return (ssize_t) count;
}

static int memory_close(mailstream_low * s)
{
  (void) s;
  return 0;
}

static int memory_get_fd(mailstream_low * s)
{
  (void) s;
  return -1;
}

static void memory_free(mailstream_low * s)
{
  struct memory_stream * data = s->data;

  free(data->input);
  free(data);
  free(s);
}

static void memory_cancel(mailstream_low * s)
{
  (void) s;
}

static carray * memory_get_certificate_chain(mailstream_low * s)
{
  (void) s;
  return NULL;
}

static int memory_idle(mailstream_low * s)
{
  (void) s;
  return -1;
}

static mailstream_low_driver memory_driver = {
  memory_read,
  memory_write,
  memory_close,
  memory_get_fd,
  memory_free,
  memory_cancel,
  NULL,
  memory_get_certificate_chain,
  memory_idle,
  memory_idle,
  memory_idle
};

static ssize_t capture_read(mailstream_low * s, void * buf, size_t count)
{
  (void) s;
  (void) buf;
  (void) count;
  return 0;
}

static ssize_t capture_write(mailstream_low * s, const void * buf,
    size_t count)
{
  struct capture_stream * data = s->data;

  assert(mmap_string_append_len(data->output, buf, count) != NULL);
  return (ssize_t) count;
}

static int capture_close(mailstream_low * s)
{
  (void) s;
  return 0;
}

static int capture_get_fd(mailstream_low * s)
{
  (void) s;
  return -1;
}

static void capture_free(mailstream_low * s)
{
  struct capture_stream * data = s->data;

  mmap_string_free(data->output);
  free(data);
  free(s);
}

static void capture_cancel(mailstream_low * s)
{
  (void) s;
}

static carray * capture_get_certificate_chain(mailstream_low * s)
{
  (void) s;
  return NULL;
}

static mailstream_low_driver capture_driver = {
  capture_read,
  capture_write,
  capture_close,
  capture_get_fd,
  capture_free,
  capture_cancel,
  NULL,
  capture_get_certificate_chain,
  memory_idle,
  memory_idle,
  memory_idle
};

static ssize_t script_read(mailstream_low * s, void * buf, size_t count)
{
  struct script_stream * data = s->data;
  size_t remaining = data->memory.input_len - data->memory.input_pos;
  size_t chunk = remaining < count ? remaining : count;

  if (chunk == 0)
    return 0;

  memcpy(buf, data->memory.input + data->memory.input_pos, chunk);
  data->memory.input_pos += chunk;
  return (ssize_t) chunk;
}

static ssize_t script_write(mailstream_low * s, const void * buf,
    size_t count)
{
  struct script_stream * data = s->data;

  assert(mmap_string_append_len(data->output, buf, count) != NULL);
  return (ssize_t) count;
}

static void script_free(mailstream_low * s)
{
  struct script_stream * data = s->data;

  free(data->memory.input);
  free(data);
  free(s);
}

static mailstream_low_driver script_driver = {
  script_read,
  script_write,
  memory_close,
  memory_get_fd,
  script_free,
  memory_cancel,
  NULL,
  memory_get_certificate_chain,
  memory_idle,
  memory_idle,
  memory_idle
};

static MMAPString * read_fixture(const char * path)
{
  FILE * f;
  MMAPString * result;
  int ch;
  int previous = 0;

  f = fopen(path, "rb");
  assert(f != NULL);

  result = mmap_string_new("");
  assert(result != NULL);

  while ((ch = fgetc(f)) != EOF) {
    if (ch == '\n' && previous != '\r')
      assert(mmap_string_append_c(result, '\r') != NULL);
    assert(mmap_string_append_c(result, (char) ch) != NULL);
    previous = ch;
  }

  fclose(f);
  return result;
}

static mailstream_low * memory_stream_low_new(const unsigned char * input,
    size_t input_len)
{
  struct memory_stream * data = calloc(1, sizeof(* data));
  mailstream_low * low;

  assert(data != NULL);
  data->input = malloc(input_len);
  assert(data->input != NULL);
  memcpy(data->input, input, input_len);
  data->input_len = input_len;

  low = mailstream_low_new(data, &memory_driver);
  assert(low != NULL);
  return low;
}

static unsigned char * deflate_raw(const unsigned char * input, size_t input_len,
    size_t * output_len)
{
  z_stream strm;
  unsigned char * output;
  size_t output_cap = compressBound(input_len) + 32;
  int zr;

  output = malloc(output_cap);
  assert(output != NULL);

  memset(&strm, 0, sizeof(strm));
  zr = deflateInit2(&strm, Z_BEST_SPEED, Z_DEFLATED, -15, 8,
      Z_DEFAULT_STRATEGY);
  assert(zr == Z_OK);

  strm.next_in = (Bytef *) input;
  strm.avail_in = (uInt) input_len;
  strm.next_out = output;
  strm.avail_out = (uInt) output_cap;

  zr = deflate(&strm, Z_FINISH);
  assert(zr == Z_STREAM_END);
  *output_len = output_cap - strm.avail_out;

  deflateEnd(&strm);
  return output;
}

static mailstream * stream_from_fixture(MMAPString * fixture, bool compressed)
{
  const unsigned char * bytes = (const unsigned char *) fixture->str;
  size_t len = fixture->len;
  unsigned char * compressed_bytes = NULL;
  size_t compressed_len = 0;
  mailstream_low * low;
  mailstream * stream;

  if (compressed) {
    compressed_bytes = deflate_raw(bytes, len, &compressed_len);
    bytes = compressed_bytes;
    len = compressed_len;
  }

  low = memory_stream_low_new(bytes, len);
  if (compressed) {
    low = mailstream_low_compress_open(low);
    assert(low != NULL);
  }

  stream = mailstream_new(low, 128);
  assert(stream != NULL);
  free(compressed_bytes);
  return stream;
}

mailstream * imap_test_stream_from_string(const char * input)
{
  mailstream_low * low;
  mailstream * stream;

  low = memory_stream_low_new((const unsigned char *) input, strlen(input));
  stream = mailstream_new(low, 128);
  assert(stream != NULL);
  return stream;
}

mailstream * imap_test_stream_from_string_with_output(const char * input,
    MMAPString ** output)
{
  struct script_stream * data;
  mailstream_low * low;
  mailstream * stream;
  size_t input_len;

  data = calloc(1, sizeof(* data));
  assert(data != NULL);

  input_len = strlen(input);
  data->memory.input = malloc(input_len);
  assert(data->memory.input != NULL);
  memcpy(data->memory.input, input, input_len);
  data->memory.input_len = input_len;
  data->output = mmap_string_new("");
  assert(data->output != NULL);

  low = mailstream_low_new(data, &script_driver);
  assert(low != NULL);
  stream = mailstream_new(low, 128);
  assert(stream != NULL);

  *output = data->output;
  return stream;
}

static MMAPString * read_stream(mailstream * stream)
{
  MMAPString * buffer;
  char read_buffer[64];

  buffer = mmap_string_new("");
  assert(buffer != NULL);

  while (1) {
    ssize_t read_count = mailstream_read(stream, read_buffer,
        sizeof(read_buffer));

    assert(read_count >= 0);
    if (read_count == 0)
      break;
    assert(mmap_string_append_len(buffer, read_buffer,
          (size_t) read_count) != NULL);
  }

  return buffer;
}

static int parse_buffer_as_response(MMAPString * buffer,
    struct mailimap_response ** result)
{
  mailimap * session;
  struct mailimap_parser_context * parser_ctx;
  size_t indx = 0;
  int r;

  session = mailimap_new(0, NULL);
  assert(session != NULL);

  parser_ctx = mailimap_parser_context_new(session);
  assert(parser_ctx != NULL);

  r = mailimap_response_parse(NULL, buffer, parser_ctx, &indx, result, 0, NULL);

  mailimap_parser_context_free(parser_ctx);
  mailimap_free(session);
  return r;
}

static int parse_buffer_as_response_data(MMAPString * buffer,
    struct mailimap_response_data ** result)
{
  mailimap * session;
  struct mailimap_parser_context * parser_ctx;
  size_t indx = 0;
  int r;

  session = mailimap_new(0, NULL);
  assert(session != NULL);

  parser_ctx = mailimap_parser_context_new(session);
  assert(parser_ctx != NULL);

  r = mailimap_response_data_parse(NULL, buffer, parser_ctx, &indx, result,
      0, NULL);

  mailimap_parser_context_free(parser_ctx);
  mailimap_free(session);
  return r;
}

int imap_test_parse_response_file(const char * path, bool compressed,
    struct mailimap_response ** result)
{
  MMAPString * fixture;
  MMAPString * buffer;
  mailstream * stream;
  int r;

  printf("testing %s (%s)\n", path, compressed ? "compressed" : "plain");

  fixture = read_fixture(path);
  stream = stream_from_fixture(fixture, compressed);
  buffer = read_stream(stream);

  *result = NULL;
  r = parse_buffer_as_response(buffer, result);

  mmap_string_free(buffer);
  mailstream_close(stream);
  mmap_string_free(fixture);
  return r;
}

int imap_test_parse_response_data_file(const char * path, bool compressed,
    struct mailimap_response_data ** result)
{
  MMAPString * fixture;
  MMAPString * buffer;
  mailstream * stream;
  int r;

  printf("testing %s (%s)\n", path, compressed ? "compressed" : "plain");

  fixture = read_fixture(path);
  stream = stream_from_fixture(fixture, compressed);
  buffer = read_stream(stream);

  *result = NULL;
  r = parse_buffer_as_response_data(buffer, result);

  mmap_string_free(buffer);
  mailstream_close(stream);
  mmap_string_free(fixture);
  return r;
}

void imap_test_expect_send_file(const char * path, imap_test_sender * sender,
    void * context)
{
  MMAPString * expected;
  struct capture_stream * data;
  mailstream_low * low;
  mailstream * stream;
  int r;

  printf("testing %s\n", path);

  expected = read_fixture(path);
  data = calloc(1, sizeof(* data));
  assert(data != NULL);
  data->output = mmap_string_new("");
  assert(data->output != NULL);

  low = mailstream_low_new(data, &capture_driver);
  assert(low != NULL);

  stream = mailstream_new(low, 128);
  assert(stream != NULL);

  r = sender(stream, context);
  assert(r == MAILIMAP_NO_ERROR);
  assert(mailstream_flush(stream) == 0);

  if ((data->output->len != expected->len) ||
      (memcmp(data->output->str, expected->str, expected->len) != 0)) {
    fprintf(stderr, "sender output mismatch for %s\n", path);
    fprintf(stderr, "expected: %s\n", expected->str);
    fprintf(stderr, "actual:   %s\n", data->output->str);
    assert(0);
  }

  mailstream_close(stream);
  mmap_string_free(expected);
}
