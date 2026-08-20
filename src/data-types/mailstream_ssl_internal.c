#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "mailstream_ssl_internal.h"
#include "mailstream.h"

#ifdef WIN32
#include <win_etpan.h>
#else
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#if USE_POLL
#ifdef HAVE_SYS_POLL_H
#include <sys/poll.h>
#endif
#elif defined(HAVE_SYS_SELECT_H)
#include <sys/select.h>
#endif
#endif

int mailstream_ssl_internal_prepare_fd(int fd)
{
#ifndef WIN32
  int flags;

  flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0)
    return -1;
  if (fcntl(fd, F_SETFL, flags | O_NDELAY) < 0)
    return -1;
#else
  (void) fd;
#endif
  return 0;
}

void mailstream_ssl_internal_close_fd(int fd)
{
#ifdef WIN32
  closesocket(fd);
#else
  close(fd);
#endif
}

int mailstream_ssl_internal_wait_fd(int fd, int want_read, time_t timeout_value)
{
  struct timeval timeout;
  int r;
#if defined(WIN32) || !USE_POLL
  fd_set fds;
#else
  struct pollfd pfd;
#endif

  if (timeout_value == 0) {
    timeout = mailstream_network_delay;
  }
  else {
    timeout.tv_sec = timeout_value;
    timeout.tv_usec = 0;
  }

#if defined(WIN32) || !USE_POLL
  FD_ZERO(&fds);
  FD_SET(fd, &fds);
  if (want_read)
    r = select(fd + 1, &fds, NULL, NULL, &timeout);
  else
    r = select(fd + 1, NULL, &fds, NULL, &timeout);
  if (r <= 0 || !FD_ISSET(fd, &fds))
    return -1;
#else
  pfd.fd = fd;
  pfd.events = want_read ? POLLIN : POLLOUT;
  pfd.revents = 0;
  r = poll(&pfd, 1, timeout.tv_sec * 1000 + timeout.tv_usec / 1000);
  if (r <= 0 || (pfd.revents & pfd.events) != pfd.events)
    return -1;
#endif
  return 0;
}
