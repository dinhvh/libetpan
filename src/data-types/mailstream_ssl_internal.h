#ifndef MAILSTREAM_SSL_INTERNAL_H
#define MAILSTREAM_SSL_INTERNAL_H

#include <time.h>

/* Private helpers shared by the OpenSSL and GnuTLS implementations. */
#if defined(__GNUC__) || defined(__clang__)
#define MAILSTREAM_SSL_INTERNAL __attribute__((visibility("hidden")))
#else
#define MAILSTREAM_SSL_INTERNAL
#endif

MAILSTREAM_SSL_INTERNAL int mailstream_ssl_internal_prepare_fd(int fd);
MAILSTREAM_SSL_INTERNAL void mailstream_ssl_internal_close_fd(int fd);
MAILSTREAM_SSL_INTERNAL int mailstream_ssl_internal_wait_fd(int fd,
    int want_read, time_t timeout);

#endif
