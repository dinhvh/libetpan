#include <assert.h>

#include <libetpan/mailstream_ssl.h>

int main(void)
{
#if HAVE_CFNETWORK
  assert(mailstream_ssl_backend_is_available(
      MAILSTREAM_SSL_BACKEND_CFNETWORK));
  assert(mailstream_ssl_get_backend() == MAILSTREAM_SSL_BACKEND_CFNETWORK);
#else
  assert(!mailstream_ssl_backend_is_available(
      MAILSTREAM_SSL_BACKEND_CFNETWORK));
#endif

#ifdef HAVE_OPENSSL
  assert(mailstream_ssl_backend_is_available(MAILSTREAM_SSL_BACKEND_OPENSSL));
#else
  assert(!mailstream_ssl_backend_is_available(MAILSTREAM_SSL_BACKEND_OPENSSL));
#endif

#ifdef HAVE_GNUTLS
  assert(mailstream_ssl_backend_is_available(MAILSTREAM_SSL_BACKEND_GNUTLS));
#else
  assert(!mailstream_ssl_backend_is_available(MAILSTREAM_SSL_BACKEND_GNUTLS));
#endif

#ifdef HAVE_GNUTLS
  assert(mailstream_ssl_set_backend(MAILSTREAM_SSL_BACKEND_GNUTLS) == 0);
  assert(mailstream_ssl_get_backend() == MAILSTREAM_SSL_BACKEND_GNUTLS);
#endif

#ifdef HAVE_OPENSSL
  assert(mailstream_ssl_set_backend(MAILSTREAM_SSL_BACKEND_OPENSSL) == 0);
  assert(mailstream_ssl_get_backend() == MAILSTREAM_SSL_BACKEND_OPENSSL);
#endif

#if HAVE_CFNETWORK
  assert(mailstream_ssl_set_backend(MAILSTREAM_SSL_BACKEND_CFNETWORK) == 0);
  assert(mailstream_ssl_get_backend() == MAILSTREAM_SSL_BACKEND_CFNETWORK);
#endif

  return 0;
}
