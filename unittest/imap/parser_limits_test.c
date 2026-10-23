#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "mailimap.h"
#include "mailimap_parser.h"
#include "mmapstring.h"
#include "parser_limits_test.h"

#define TEXT_BODY "(\"TEXT\" \"PLAIN\" NIL NIL NIL \"7BIT\" 1 1)"
#define EXT_BODY "(\"TEXT\" \"PLAIN\" NIL NIL NIL \"7BIT\" 1 1 NIL NIL NIL NIL "

static void expect_parse(struct mailimap_parser_context * ctx,
    const char * text, int expected)
{
  MMAPString * buffer = mmap_string_new(text);
  struct mailimap_response * response = NULL;
  size_t index = 0;
  int r;

  assert(buffer != NULL);
  r = mailimap_response_parse(NULL, buffer, ctx, &index, &response, 0, NULL);
  assert(r == expected);
  assert(ctx->body_parse_depth == 0);
  if (r == MAILIMAP_NO_ERROR) {
    assert(response != NULL);
    /* The tagged-response parser leaves its final CRLF unconsumed. */
    assert(index + 2 == buffer->len);
    assert(strcmp(buffer->str + index, "\r\n") == 0);
    mailimap_response_free(response);
  }
  else {
    assert(response == NULL);
    assert(index == 0);
  }
  mmap_string_free(buffer);
}

static void append_repeat(MMAPString * buffer, const char * text, size_t count)
{
  size_t i;
  for (i = 0; i < count; i++)
    assert(mmap_string_append(buffer, text) != NULL);
}

static void nested_body(struct mailimap_parser_context * ctx,
    size_t depth, int extension, int complete, int expected)
{
  MMAPString * buffer = mmap_string_new("* 1 FETCH (BODYSTRUCTURE ");
  assert(buffer != NULL);
  if (extension)
    assert(mmap_string_append(buffer, EXT_BODY) != NULL);
  append_repeat(buffer, "(", depth);
  if (complete) {
    assert(mmap_string_append(buffer, extension ? "1" : TEXT_BODY) != NULL);
    append_repeat(buffer, extension ? ")" : " \"MIXED\")", depth);
    if (extension)
      assert(mmap_string_append(buffer, ")") != NULL);
    assert(mmap_string_append(buffer, ")\r\nt OK done\r\n") != NULL);
  }
  expect_parse(ctx, buffer->str, expected);
  mmap_string_free(buffer);
}

void imap_parser_limits_test_run(void)
{
  mailimap * session = mailimap_new(0, NULL);
  struct mailimap_parser_context * ctx;
  MMAPString * buffer;
  size_t i;

  assert(session != NULL);
  ctx = mailimap_parser_context_new(session);
  assert(ctx != NULL);
  expect_parse(ctx, "* 1 FETCH (BODY[", MAILIMAP_ERROR_PARSE);
  expect_parse(ctx, "* 1 FETCH (BODY[?", MAILIMAP_ERROR_PARSE);
  expect_parse(ctx, "* 1 FETCH (BODY[1", MAILIMAP_ERROR_PARSE);
  expect_parse(ctx, "* 1 FETCH (BODY[] NIL)\r\nt OK done\r\n",
      MAILIMAP_NO_ERROR);

  nested_body(ctx, 50000, 0, 0, MAILIMAP_ERROR_PARSE);
  nested_body(ctx, 30000, 1, 0, MAILIMAP_ERROR_PARSE);
  nested_body(ctx, 99, 0, 1, MAILIMAP_NO_ERROR);
  nested_body(ctx, 100, 0, 1, MAILIMAP_ERROR_PARSE);
  nested_body(ctx, 98, 1, 1, MAILIMAP_NO_ERROR);
  nested_body(ctx, 99, 1, 1, MAILIMAP_ERROR_PARSE);

  /* Siblings and later responses must not consume a cumulative budget. */
  buffer = mmap_string_new("* 1 FETCH (BODYSTRUCTURE (");
  assert(buffer != NULL);
  append_repeat(buffer, TEXT_BODY, 200);
  assert(mmap_string_append(buffer, " \"MIXED\"))\r\nt OK done\r\n") != NULL);
  expect_parse(ctx, buffer->str, MAILIMAP_NO_ERROR);
  mmap_string_free(buffer);
  for (i = 0; i < 110; i++)
    nested_body(ctx, 2, 1, 1, MAILIMAP_NO_ERROR);

  mailimap_parser_context_free(ctx);
  mailimap_free(session);
}
