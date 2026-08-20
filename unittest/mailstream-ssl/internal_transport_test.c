#include "mailstream_ssl_internal.h"

#include <assert.h>
#ifndef WIN32
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

int main(void)
{
#ifndef WIN32
  int sockets[2];
  int flags;
  char byte = 'x';

  assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
  assert(mailstream_ssl_internal_prepare_fd(sockets[0]) == 0);

  flags = fcntl(sockets[0], F_GETFL, 0);
  assert(flags >= 0);
  assert((flags & O_NONBLOCK) != 0);
  assert(mailstream_ssl_internal_wait_fd(sockets[0], 0, 1) == 0);
  assert(write(sockets[1], &byte, 1) == 1);
  assert(mailstream_ssl_internal_wait_fd(sockets[0], 1, 1) == 0);

  mailstream_ssl_internal_close_fd(sockets[0]);
  assert(fcntl(sockets[0], F_GETFD) == -1);
  close(sockets[1]);
#endif
  return 0;
}
