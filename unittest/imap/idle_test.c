#include <assert.h>
#include <setjmp.h>
#include <string.h>
#include <stdio.h>

#include "idle_test.h"

#include "idle.h"
#include "imap_test_utils.h"
#include "mailimap.h"
#include "mailimap_types.h"

static jmp_buf test_abort;
static test_failure_callback active_failure_callback;
static void * active_failure_context;

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

static mailimap * idle_session_new(const char * input, MMAPString ** output)
{
  mailimap * session;

  session = mailimap_new(0, NULL);
  assert(session != NULL);

  session->imap_stream = imap_test_stream_from_string_with_output(input,
      output);
  session->imap_state = MAILIMAP_STATE_SELECTED;
  session->imap_selection_info = mailimap_selection_info_new();
  assert(session->imap_selection_info != NULL);

  return session;
}

static void assert_output_equals(MMAPString * output, const char * expected)
{
  assert(output->len == strlen(expected));
  assert(memcmp(output->str, expected, output->len) == 0);
}

static void check_idle_accepts_immediate_continuation(void)
{
  MMAPString * output;
  mailimap * session;
  int r;

  session = idle_session_new("+ idling\r\n", &output);

  r = mailimap_idle(session);
  assert(r == MAILIMAP_NO_ERROR);
  assert_output_equals(output, "1 IDLE\r\n");

  mailimap_free(session);
  mmap_string_free(output);
}

static void check_idle_accepts_rfc2177_updates_before_continuation(void)
{
  MMAPString * output;
  mailimap * session;
  int r;

  session = idle_session_new(
      "* 2 EXPUNGE\r\n"
      "* 3 EXISTS\r\n"
      "+ idling\r\n",
      &output);

  r = mailimap_idle(session);
  assert(r == MAILIMAP_NO_ERROR);
  assert_output_equals(output, "1 IDLE\r\n");

  mailimap_free(session);
  mmap_string_free(output);
}

static void check_idle_accepts_flags_before_continuation(void)
{
  MMAPString * output;
  mailimap * session;
  int r;

  session = idle_session_new(
      "* FLAGS (\\Answered \\Flagged \\Deleted \\Seen \\Draft)\r\n"
      "+ idling\r\n",
      &output);

  r = mailimap_idle(session);
  assert(r == MAILIMAP_NO_ERROR);
  assert_output_equals(output, "1 IDLE\r\n");

  mailimap_free(session);
  mmap_string_free(output);
}

static void check_idle_done_accepts_queued_updates_before_tagged_ok(void)
{
  MMAPString * output;
  mailimap * session;
  int r;

  session = idle_session_new(
      "+ idling\r\n"
      "* 4 EXISTS\r\n"
      "1 OK IDLE terminated\r\n",
      &output);

  r = mailimap_idle(session);
  assert(r == MAILIMAP_NO_ERROR);

  r = mailimap_idle_done(session);
  assert(r == MAILIMAP_NO_ERROR);
  assert_output_equals(output, "1 IDLE\r\nDONE\r\n");

  mailimap_free(session);
  mmap_string_free(output);
}

static void check_idle_rejection(const char * response)
{
  MMAPString * output;
  mailimap * session;
  int r;

  session = idle_session_new(response, &output);
  r = mailimap_idle(session);
  assert(r != MAILIMAP_NO_ERROR);
  assert_output_equals(output, "1 IDLE\r\n");

  mailimap_free(session);
  mmap_string_free(output);
}

static void check_idle_no_rejection(void)
{
  check_idle_rejection("1 NO IDLE not allowed now\r\n");
}

static void check_idle_bad_rejection(void)
{
  check_idle_rejection("1 BAD unknown command\r\n");
}

struct idle_case {
  const char * name;
  void (* run)(void);
};

static const struct idle_case cases[] = {
  { "immediate continuation", check_idle_accepts_immediate_continuation },
  { "updates before continuation",
    check_idle_accepts_rfc2177_updates_before_continuation },
  { "flags before continuation", check_idle_accepts_flags_before_continuation },
  { "queued updates before tagged OK",
    check_idle_done_accepts_queued_updates_before_tagged_ok },
  { "NO rejection", check_idle_no_rejection },
  { "BAD rejection", check_idle_bad_rejection },
};

size_t imap_idle_test_count(void)
{
  return sizeof(cases) / sizeof(cases[0]);
}

const char * imap_idle_test_name(size_t index)
{
  if (index >= imap_idle_test_count()) return NULL;
  return cases[index].name;
}

int imap_idle_test_run_case(size_t index,
    test_failure_callback failure_callback, void * context)
{
  if (index >= imap_idle_test_count()) return -1;
  active_failure_callback = failure_callback;
  active_failure_context = context;
  if (setjmp(test_abort) != 0) return -1;
  cases[index].run();
  return 0;
}

int imap_idle_test_run(void)
{
  size_t index;

  for (index = 0; index < imap_idle_test_count(); index++) {
    if (imap_idle_test_run_case(index, NULL, NULL) != 0)
      return -1;
  }

  puts("idle_test: ok");
  return 0;
}
