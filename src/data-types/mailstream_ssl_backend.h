#ifndef MAILSTREAM_SSL_BACKEND_H
#define MAILSTREAM_SSL_BACKEND_H

#include <libetpan/mailstream_ssl.h>
#include <libetpan/mailstream_types.h>

struct mailstream_ssl_backend_driver {
  enum mailstream_ssl_backend backend;
  const char * name;
  mailstream_low_driver * low_driver;

  mailstream_low * (*open_low)(int fd, int starttls, time_t timeout,
      void (* callback)(void * provider_context, void * data), void * data);
  ssize_t (*copy_certificate)(mailstream * stream, unsigned char ** der);

  int (*set_client_certificate_file)(void * context, char * filename);
  int (*set_client_certificate_data)(void * context,
      unsigned char * der, size_t length);
  int (*set_client_private_key_data)(void * context,
      unsigned char * der, size_t length);
  int (*set_server_certificate)(void * context,
      char * ca_file, char * ca_path);
  int (*set_server_name)(void * context, const char * hostname);
  void * (*get_native_context)(void * context);
  int (*get_context_fd)(void * context);

  void (*init_not_required)(void);
  void (*init_lock)(void);
  void (*uninit_lock)(void);
};

#ifdef HAVE_OPENSSL
const struct mailstream_ssl_backend_driver *
mailstream_openssl_backend_driver(void);
#endif

#ifdef HAVE_GNUTLS
const struct mailstream_ssl_backend_driver *
mailstream_gnutls_backend_driver(void);
#endif

#endif
