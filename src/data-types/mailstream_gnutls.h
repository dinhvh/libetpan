#ifndef MAILSTREAM_GNUTLS_H
#define MAILSTREAM_GNUTLS_H

#include "mailstream_ssl_backend.h"

#ifdef HAVE_GNUTLS
const struct mailstream_ssl_backend_driver *
mailstream_gnutls_backend_driver(void);
#endif

#endif
