#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "idle_test.h"

#include "idle.h"
#include "imap_test_utils.h"
#include "mailimap.h"
#include "mailimap_types.h"

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

static void check_idle_fails_when_server_rejects_command(void)
{
  static const char * const responses[] = {
    "1 NO IDLE not allowed now\r\n",
    "1 BAD unknown command\r\n"
  };
  size_t i;

  for (i = 0; i < sizeof(responses) / sizeof(responses[0]); i++) {
    MMAPString * output;
    mailimap * session;
    int r;

    session = idle_session_new(responses[i], &output);

    r = mailimap_idle(session);
    assert(r != MAILIMAP_NO_ERROR);
    assert_output_equals(output, "1 IDLE\r\n");

    mailimap_free(session);
    mmap_string_free(output);
  }
}

int imap_idle_test_run(void)
{
  check_idle_accepts_immediate_continuation();
  check_idle_accepts_rfc2177_updates_before_continuation();
  check_idle_accepts_flags_before_continuation();
  check_idle_done_accepts_queued_updates_before_tagged_ok();
  check_idle_fails_when_server_rejects_command();

  puts("idle_test: ok");
  return 0;
}
