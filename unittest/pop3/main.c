#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
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

static void run_fake_pop3_server(int fd)
{
  if (write_server_stream(fd, "+OK POP3 ready\r\n") < 0) {
    fprintf(stderr, "server: greeting failed\n");
    _exit(1);
  }
  if (read_client_command(fd) < 0) {
    fprintf(stderr, "server: USER read failed\n");
    _exit(1);
  }
  if (write_server_stream(fd, "+OK user accepted\r\n") < 0) {
    fprintf(stderr, "server: USER response failed\n");
    _exit(1);
  }
  if (read_client_command(fd) < 0) {
    fprintf(stderr, "server: PASS read failed\n");
    _exit(1);
  }
  if (write_server_stream(fd, "+OK pass accepted\r\n") < 0) {
    fprintf(stderr, "server: PASS response failed\n");
    _exit(1);
  }
  if (read_client_command(fd) < 0) {
    fprintf(stderr, "server: LIST read failed\n");
    _exit(1);
  }
  if (write_server_stream(fd,
      "+OK list follows\r\n"
      "0 12345\r\n"
      "1 10\r\n"
      ".\r\n") < 0)
    _exit(1);
  if (read_client_command(fd) < 0)
    _exit(1);
  if (write_server_stream(fd,
      "+OK uidl follows\r\n"
      "0 bad\r\n"
      "1 valid-uid\r\n"
      ".\r\n") < 0)
    _exit(1);
  if (read_client_command(fd) < 0)
    _exit(1);
  if (write_server_stream(fd, "+OK bye\r\n") < 0)
    _exit(1);

  _exit(0);
}

static int test_list_ignores_zero_message_number(void)
{
  int listen_fd;
  int client_fd;
  int server_fd;
  struct sockaddr_in addr;
  socklen_t addr_len;
  pid_t server_pid;
  mailstream * stream;
  mailpop3 * pop3;
  carray * list;
  struct mailpop3_msg_info * msg;
  int connect_r;
  int user_r;
  int pass_r;
  int list_r;
  int ok;
  int status;
  unsigned int list_count = 0;

  listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd < 0) {
    perror("socket");
    return 0;
  }

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = 0;
  if (bind(listen_fd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
    perror("bind");
    close(listen_fd);
    return 0;
  }
  if (listen(listen_fd, 1) < 0) {
    perror("listen");
    close(listen_fd);
    return 0;
  }

  addr_len = sizeof(addr);
  if (getsockname(listen_fd, (struct sockaddr *) &addr, &addr_len) < 0) {
    perror("getsockname");
    close(listen_fd);
    return 0;
  }

  server_pid = fork();
  if (server_pid < 0) {
    close(listen_fd);
    return 0;
  }
  if (server_pid == 0) {
    server_fd = accept(listen_fd, NULL, NULL);
    close(listen_fd);
    if (server_fd < 0)
      _exit(1);
    run_fake_pop3_server(server_fd);
  }

  client_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (client_fd < 0) {
    perror("client socket");
    close(listen_fd);
    return 0;
  }
  if (connect(client_fd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
    perror("connect");
    close(client_fd);
    close(listen_fd);
    return 0;
  }
  close(listen_fd);

  stream = mailstream_socket_open(client_fd);
  if (stream == NULL) {
    close(client_fd);
    return 0;
  }

  pop3 = mailpop3_new(0, NULL);
  if (pop3 == NULL) {
    mailstream_close(stream);
    return 0;
  }

  list = NULL;
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

  mailpop3_free(pop3);
  if (waitpid(server_pid, &status, 0) < 0)
    ok = 0;
  else if (!WIFEXITED(status) || (WEXITSTATUS(status) != 0))
    ok = 0;

  if (!ok) {
    fprintf(stderr, "connect=%d user=%d pass=%d list=%d count=%u\n",
        connect_r, user_r, pass_r, list_r,
        list_count);
  }

  return ok;
}

int pop3_test_run_case(test_failure_callback failure_callback, void * context)
{
  if (!test_list_ignores_zero_message_number()) {
    fprintf(stderr, "POP3 LIST zero message number regression failed\n");
    if (failure_callback != NULL)
      failure_callback(__FILE__, __LINE__,
          "test_list_ignores_zero_message_number()",
          "POP3 LIST must ignore an invalid zero message number", context);
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
