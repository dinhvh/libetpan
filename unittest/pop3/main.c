#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <libetpan/carray.h>
#include <libetpan/mailpop3.h>
#include <libetpan/mailstream_socket.h>

#include "pop3_tests.h"

static int write_server_stream(int fd, const char * stream)
{
  size_t left;
  const char * cur;

  left = strlen(stream);
  cur = stream;
  while (left > 0) {
    ssize_t written;

    written = write(fd, cur, left);
    if (written <= 0)
      return -1;

    cur += written;
    left -= (size_t) written;
  }

  return 0;
}

static int read_client_command(int fd)
{
  char ch;

  do {
    ssize_t read_count;

    read_count = read(fd, &ch, 1);
    if (read_count <= 0)
      return -1;
  }
  while (ch != '\n');

  return 0;
}

static void * run_fake_pop3_server(void * context)
{
  int fd = (int) (intptr_t) context;
  int failed = 0;

  if (write_server_stream(fd, "+OK POP3 ready\r\n") < 0) {
    fprintf(stderr, "server: greeting failed\n");
    failed = 1;
    goto cleanup;
  }
  if (read_client_command(fd) < 0) {
    fprintf(stderr, "server: USER read failed\n");
    failed = 1;
    goto cleanup;
  }
  if (write_server_stream(fd, "+OK user accepted\r\n") < 0) {
    fprintf(stderr, "server: USER response failed\n");
    failed = 1;
    goto cleanup;
  }
  if (read_client_command(fd) < 0) {
    fprintf(stderr, "server: PASS read failed\n");
    failed = 1;
    goto cleanup;
  }
  if (write_server_stream(fd, "+OK pass accepted\r\n") < 0) {
    fprintf(stderr, "server: PASS response failed\n");
    failed = 1;
    goto cleanup;
  }
  if (read_client_command(fd) < 0) {
    fprintf(stderr, "server: LIST read failed\n");
    failed = 1;
    goto cleanup;
  }
  if (write_server_stream(fd,
      "+OK list follows\r\n"
      "0 12345\r\n"
      "1 10\r\n"
      ".\r\n") < 0) {
    failed = 1;
    goto cleanup;
  }
  if (read_client_command(fd) < 0) {
    failed = 1;
    goto cleanup;
  }
  if (write_server_stream(fd,
      "+OK uidl follows\r\n"
      "0 bad\r\n"
      "1 valid-uid\r\n"
      ".\r\n") < 0) {
    failed = 1;
    goto cleanup;
  }
  if (read_client_command(fd) < 0) {
    failed = 1;
    goto cleanup;
  }
  if (write_server_stream(fd, "+OK bye\r\n") < 0)
    failed = 1;

cleanup:
  close(fd);
  return (void *) (intptr_t) failed;
}

static int test_list_ignores_zero_message_number(const char ** failure_message)
{
  int sockets[2] = { -1, -1 };
  pthread_t server_thread;
  int server_thread_started = 0;
  void * server_result = NULL;
  mailstream * stream = NULL;
  mailpop3 * pop3 = NULL;
  carray * list = NULL;
  struct mailpop3_msg_info * msg;
  int connect_r = -1;
  int user_r = -1;
  int pass_r = -1;
  int list_r = -1;
  int ok = 0;
  unsigned int list_count = 0;

  *failure_message = "POP3 LIST result did not match the expected messages";
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) < 0) {
    perror("socketpair");
    *failure_message = "could not create the fake POP3 socket pair";
    goto cleanup;
  }

  if (pthread_create(&server_thread, NULL, run_fake_pop3_server,
          (void *) (intptr_t) sockets[1]) != 0) {
    *failure_message = "could not start the fake POP3 server thread";
    goto cleanup;
  }
  server_thread_started = 1;
  sockets[1] = -1;

  stream = mailstream_socket_open(sockets[0]);
  if (stream == NULL) {
    *failure_message = "could not open the POP3 client mailstream";
    goto cleanup;
  }
  sockets[0] = -1;

  pop3 = mailpop3_new(0, NULL);
  if (pop3 == NULL) {
    *failure_message = "could not allocate the POP3 client";
    goto cleanup;
  }

  ok = 1;
  connect_r = mailpop3_connect(pop3, stream);
  ok = ok && (connect_r == MAILPOP3_NO_ERROR);
  user_r = mailpop3_user(pop3, "user");
  ok = ok && (user_r == MAILPOP3_NO_ERROR);
  pass_r = mailpop3_pass(pop3, "pass");
  ok = ok && (pass_r == MAILPOP3_NO_ERROR);
  list_r = mailpop3_list(pop3, &list);
  ok = ok && (list_r == MAILPOP3_NO_ERROR);
  ok = ok && (list != NULL);
  ok = ok && (carray_count(list) == 1);

  if (ok) {
    msg = carray_get(list, 0);
    ok = ok && (msg != NULL);
    ok = ok && (msg->msg_index == 1);
    ok = ok && (msg->msg_size == 10);
    ok = ok && (msg->msg_uidl != NULL);
    ok = ok && (strcmp(msg->msg_uidl, "valid-uid") == 0);
  }

  if (list != NULL)
    list_count = carray_count(list);

cleanup:
  if (pop3 != NULL)
    mailpop3_free(pop3);
  else if (stream != NULL)
    mailstream_close(stream);
  if (sockets[0] >= 0)
    close(sockets[0]);
  if (sockets[1] >= 0)
    close(sockets[1]);
  if (server_thread_started) {
    if (pthread_join(server_thread, &server_result) != 0) {
      *failure_message = "could not join the fake POP3 server thread";
      ok = 0;
    }
    else if ((intptr_t) server_result != 0) {
      *failure_message = "the fake POP3 server conversation failed";
      ok = 0;
    }
  }

  if (!ok) {
    fprintf(stderr, "connect=%d user=%d pass=%d list=%d count=%u\n",
        connect_r, user_r, pass_r, list_r,
        list_count);
  }

  return ok;
}

int pop3_test_run_case(test_failure_callback failure_callback, void * context)
{
  const char * failure_message;

  if (!test_list_ignores_zero_message_number(&failure_message)) {
    fprintf(stderr, "POP3 LIST zero message number regression failed\n");
    if (failure_callback != NULL)
      failure_callback(__FILE__, __LINE__,
          "test_list_ignores_zero_message_number()",
          failure_message, context);
    return -1;
  }

  return 0;
}

int pop3_test_run(void)
{
  return pop3_test_run_case(NULL, NULL);
}

#ifndef POP3_NO_MAIN
int main(void)
{
  return pop3_test_run() == 0 ? 0 : 1;
}
#endif
