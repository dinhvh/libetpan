#ifndef MAILSTREAM_OPENSSL_H
#define MAILSTREAM_OPENSSL_H

#include "mailstream_ssl_backend.h"

#ifdef HAVE_OPENSSL
const struct mailstream_ssl_backend_driver *
mailstream_openssl_backend_driver(void);
#endif

#endif
