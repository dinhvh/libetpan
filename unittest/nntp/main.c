#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <libetpan/mailstream_socket.h>
#include <libetpan/newsnntp.h>

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

static void * run_fake_nntp_server(void * context)
{
  int fd = (int) (intptr_t) context;
  int failed = 0;

  if (write_server_stream(fd, "200 ready\r\n") < 0) {
    failed = 1;
    goto cleanup;
  }
  if (read_client_command(fd) < 0) {
    failed = 1;
    goto cleanup;
  }
  if (write_server_stream(fd, "111\r\n") < 0)
    failed = 1;
  if (read_client_command(fd) < 0) {
    failed = 1;
    goto cleanup;
  }
  if (write_server_stream(fd, "205 bye\r\n") < 0)
    failed = 1;

cleanup:
  close(fd);
  return (void *) (intptr_t) failed;
}

static int test_date_rejects_short_response(void)
{
  int sockets[2] = { -1, -1 };
  pthread_t server_thread;
  int server_thread_started = 0;
  void * server_result = NULL;
  mailstream * stream = NULL;
  newsnntp * nntp = NULL;
  struct tm date;
  int connect_r = -1;
  int date_r = -1;
  int ok = 0;

  if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) < 0) {
    perror("socketpair");
    goto cleanup;
  }

  if (pthread_create(&server_thread, NULL, run_fake_nntp_server,
          (void *) (intptr_t) sockets[1]) != 0)
    goto cleanup;
  server_thread_started = 1;
  sockets[1] = -1;

  stream = mailstream_socket_open(sockets[0]);
  if (stream == NULL)
    goto cleanup;
  mailstream_socket_set_use_read(stream, 1);
  sockets[0] = -1;

  nntp = newsnntp_new(0, NULL);
  if (nntp == NULL)
    goto cleanup;

  connect_r = newsnntp_connect(nntp, stream);
  date_r = newsnntp_date(nntp, &date);
  ok = (connect_r == NEWSNNTP_NO_ERROR) &&
    (date_r == NEWSNNTP_ERROR_INVALID_RESPONSE);

cleanup:
  if (nntp != NULL)
    newsnntp_free(nntp);
  else if (stream != NULL)
    mailstream_close(stream);
  if (sockets[0] >= 0)
    close(sockets[0]);
  if (sockets[1] >= 0)
    close(sockets[1]);
  if (server_thread_started) {
    if (pthread_join(server_thread, &server_result) != 0)
      ok = 0;
    else if ((intptr_t) server_result != 0)
      ok = 0;
  }

  if (!ok) {
    fprintf(stderr, "connect=%d date=%d\n", connect_r, date_r);
    return -1;
  }

  return 0;
}

int main(void)
{
  if (test_date_rejects_short_response() != 0)
    return 1;

  puts("nntp_test: ok");
  return 0;
}
