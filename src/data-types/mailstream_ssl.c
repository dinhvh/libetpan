/*
 * Provider-neutral TLS stream dispatch for libEtPan.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "mailstream_ssl.h"
#include "mailstream_openssl.h"
#include "mailstream_gnutls.h"
#include "mailstream_cfstream.h"
#include "mailstream.h"
#include "mailstream_low.h"
#include "mailstream_types.h"
#include "connect.h"

#include <stdlib.h>

#ifdef WIN32
#include <win_etpan.h>
#elif defined(HAVE_UNISTD_H)
#include <unistd.h>
#endif

struct mailstream_ssl_context {
  enum mailstream_ssl_backend backend;
  void * provider_context;
};

struct mailstream_ssl_callback_data {
  enum mailstream_ssl_backend backend;
  const char * server_name;
  void (* callback)(struct mailstream_ssl_context *, void *);
  void * callback_data;
};

static int selected_backend_was_set = 0;
static enum mailstream_ssl_backend selected_backend =
#if HAVE_CFNETWORK
  MAILSTREAM_SSL_BACKEND_CFNETWORK;
#elif defined(HAVE_OPENSSL)
  MAILSTREAM_SSL_BACKEND_OPENSSL;
#else
  MAILSTREAM_SSL_BACKEND_GNUTLS;
#endif

mailstream * mailstream_ssl_connect_timeout(const char * server,
    uint16_t port, time_t timeout,
    void (* callback)(struct mailstream_ssl_context *, void *),
    void * callback_data, enum mailstream_ssl_connect_error * error)
{
  mailstream * stream;
  int fd;

  if (error != NULL)
    * error = MAILSTREAM_SSL_CONNECT_ERROR_CONNECTION_REFUSED;

#if HAVE_CFNETWORK
  if (mailstream_ssl_get_backend() == MAILSTREAM_SSL_BACKEND_CFNETWORK &&
      callback == NULL) {
    int r;

    stream = mailstream_cfstream_open_timeout(server, port, timeout);
    if (stream == NULL)
      return NULL;

    mailstream_cfstream_set_ssl_level(stream,
        MAILSTREAM_CFSTREAM_SSL_LEVEL_NEGOCIATED_SSL);
    mailstream_cfstream_set_ssl_peer_name(stream, server);
    mailstream_cfstream_set_ssl_verification_mask(stream,
        MAILSTREAM_CFSTREAM_SSL_NO_VERIFICATION);
    r = mailstream_cfstream_set_ssl_enabled(stream, 1);
    if (r < 0) {
      mailstream_close(stream);
      if (error != NULL)
        * error = MAILSTREAM_SSL_CONNECT_ERROR_SSL;
      return NULL;
    }

    if (error != NULL)
      * error = MAILSTREAM_SSL_CONNECT_NO_ERROR;
    return stream;
  }
#endif

  fd = mail_tcp_connect_timeout(server, port, timeout);
  if (fd == -1)
    return NULL;

  stream = mailstream_ssl_open_with_server_name_callback_timeout(fd, timeout,
      server, callback, callback_data);
  if (stream == NULL) {
#ifdef WIN32
    closesocket(fd);
#else
    close(fd);
#endif
    if (error != NULL)
      * error = MAILSTREAM_SSL_CONNECT_ERROR_SSL;
    return NULL;
  }

  if (error != NULL)
    * error = MAILSTREAM_SSL_CONNECT_NO_ERROR;
  return stream;
}

static int mailstream_ssl_socket_backend_is_available(
    enum mailstream_ssl_backend backend)
{
  switch (backend) {
#ifdef HAVE_OPENSSL
  case MAILSTREAM_SSL_BACKEND_OPENSSL:
    return 1;
#endif
#ifdef HAVE_GNUTLS
  case MAILSTREAM_SSL_BACKEND_GNUTLS:
    return 1;
#endif
  default:
    return 0;
  }
}

static enum mailstream_ssl_backend mailstream_ssl_default_socket_backend(void)
{
#ifdef HAVE_OPENSSL
  return MAILSTREAM_SSL_BACKEND_OPENSSL;
#elif defined(HAVE_GNUTLS)
  return MAILSTREAM_SSL_BACKEND_GNUTLS;
#else
  return MAILSTREAM_SSL_BACKEND_CFNETWORK;
#endif
}

static enum mailstream_ssl_backend mailstream_ssl_selected_socket_backend(void)
{
  if (mailstream_ssl_socket_backend_is_available(selected_backend))
    return selected_backend;
  return mailstream_ssl_default_socket_backend();
}

int mailstream_ssl_backend_is_available(enum mailstream_ssl_backend backend)
{
  if (backend == MAILSTREAM_SSL_BACKEND_CFNETWORK) {
#if HAVE_CFNETWORK
    return 1;
#else
    return 0;
#endif
  }
  return mailstream_ssl_socket_backend_is_available(backend);
}

int mailstream_ssl_set_backend(enum mailstream_ssl_backend backend)
{
  if (!mailstream_ssl_backend_is_available(backend))
    return -1;

  selected_backend = backend;
  selected_backend_was_set = 1;
#if HAVE_CFNETWORK
  mailstream_cfstream_enabled =
    (backend == MAILSTREAM_SSL_BACKEND_CFNETWORK);
#endif
  return 0;
}

enum mailstream_ssl_backend mailstream_ssl_get_backend(void)
{
#if HAVE_CFNETWORK
  if (!selected_backend_was_set && mailstream_cfstream_enabled)
    return MAILSTREAM_SSL_BACKEND_CFNETWORK;
#endif
  return selected_backend;
}

static void mailstream_ssl_provider_callback(
    struct mailstream_ssl_context * provider_context,
    void * data)
{
  struct mailstream_ssl_callback_data * callback_data = data;
  struct mailstream_ssl_context context;

  context.backend = callback_data->backend;
  context.provider_context = provider_context;

  if (callback_data->server_name != NULL)
    mailstream_ssl_set_server_name(&context, callback_data->server_name);
  if (callback_data->callback != NULL)
    callback_data->callback(&context, callback_data->callback_data);
}

static mailstream_low * mailstream_low_ssl_open_full(int fd, int starttls,
    time_t timeout, const char * server_name,
    void (* callback)(struct mailstream_ssl_context *, void *), void * data)
{
  enum mailstream_ssl_backend backend;
  struct mailstream_ssl_callback_data callback_data;

  backend = mailstream_ssl_selected_socket_backend();
  callback_data.backend = backend;
  callback_data.server_name = server_name;
  callback_data.callback = callback;
  callback_data.callback_data = data;

  switch (backend) {
#ifdef HAVE_OPENSSL
  case MAILSTREAM_SSL_BACKEND_OPENSSL:
    return mailstream_openssl_open_low(fd, starttls, timeout,
        mailstream_ssl_provider_callback, &callback_data);
#endif
#ifdef HAVE_GNUTLS
  case MAILSTREAM_SSL_BACKEND_GNUTLS:
    return mailstream_gnutls_open_low(fd, starttls, timeout,
        mailstream_ssl_provider_callback, &callback_data);
#endif
  default:
    return NULL;
  }
}

mailstream_low * mailstream_low_ssl_open(int fd)
{
  return mailstream_low_ssl_open_timeout(fd, 0);
}

mailstream_low * mailstream_low_tls_open(int fd)
{
  return mailstream_low_tls_open_timeout(fd, 0);
}

mailstream_low * mailstream_low_ssl_open_timeout(int fd, time_t timeout)
{
  return mailstream_low_ssl_open_full(fd, 0, timeout, NULL, NULL, NULL);
}

mailstream_low * mailstream_low_tls_open_timeout(int fd, time_t timeout)
{
  return mailstream_low_ssl_open_full(fd, 1, timeout, NULL, NULL, NULL);
}

mailstream_low * mailstream_low_ssl_open_with_callback(int fd,
    void (* callback)(struct mailstream_ssl_context *, void *), void * data)
{
  return mailstream_low_ssl_open_with_callback_timeout(fd, 0, callback, data);
}

mailstream_low * mailstream_low_ssl_open_with_callback_timeout(int fd,
    time_t timeout,
    void (* callback)(struct mailstream_ssl_context *, void *), void * data)
{
  return mailstream_low_ssl_open_full(fd, 0, timeout, NULL, callback, data);
}

mailstream_low * mailstream_low_tls_open_with_callback(int fd,
    void (* callback)(struct mailstream_ssl_context *, void *), void * data)
{
  return mailstream_low_tls_open_with_callback_timeout(fd, 0, callback, data);
}

mailstream_low * mailstream_low_tls_open_with_callback_timeout(int fd,
    time_t timeout,
    void (* callback)(struct mailstream_ssl_context *, void *), void * data)
{
  return mailstream_low_ssl_open_full(fd, 1, timeout, NULL, callback, data);
}

mailstream_low * mailstream_low_ssl_open_with_server_name_callback_timeout(
    int fd, time_t timeout, const char * server_name,
    void (* callback)(struct mailstream_ssl_context *, void *), void * data)
{
  return mailstream_low_ssl_open_full(fd, 0, timeout, server_name,
      callback, data);
}

mailstream_low * mailstream_low_tls_open_with_server_name_callback_timeout(
    int fd, time_t timeout, const char * server_name,
    void (* callback)(struct mailstream_ssl_context *, void *), void * data)
{
  return mailstream_low_ssl_open_full(fd, 1, timeout, server_name,
      callback, data);
}

mailstream * mailstream_ssl_open(int fd)
{
  return mailstream_ssl_open_timeout(fd, 0);
}

mailstream * mailstream_ssl_open_timeout(int fd, time_t timeout)
{
  return mailstream_ssl_open_with_callback_timeout(fd, timeout, NULL, NULL);
}

mailstream * mailstream_ssl_open_with_callback(int fd,
    void (* callback)(struct mailstream_ssl_context *, void *), void * data)
{
  return mailstream_ssl_open_with_callback_timeout(fd, 0, callback, data);
}

mailstream * mailstream_ssl_open_with_callback_timeout(int fd,
    time_t timeout,
    void (* callback)(struct mailstream_ssl_context *, void *), void * data)
{
  mailstream_low * low;
  mailstream * stream;

  low = mailstream_low_ssl_open_with_callback_timeout(fd, timeout,
      callback, data);
  if (low == NULL)
    return NULL;

  stream = mailstream_new(low, 8192);
  if (stream == NULL)
    mailstream_low_close(low);
  return stream;
}

mailstream * mailstream_ssl_open_with_server_name_callback_timeout(int fd,
    time_t timeout, const char * server_name,
    void (* callback)(struct mailstream_ssl_context *, void *), void * data)
{
  mailstream_low * low;
  mailstream * stream;

  low = mailstream_low_ssl_open_with_server_name_callback_timeout(fd,
      timeout, server_name, callback, data);
  if (low == NULL)
    return NULL;

  stream = mailstream_new(low, 8192);
  if (stream == NULL)
    mailstream_low_close(low);
  return stream;
}

static enum mailstream_ssl_backend mailstream_ssl_backend_for_stream(
    mailstream * stream)
{
  mailstream_low * low;

  if (stream == NULL)
    return MAILSTREAM_SSL_BACKEND_CFNETWORK;
  low = mailstream_get_low(stream);
  if (low == NULL)
    return MAILSTREAM_SSL_BACKEND_CFNETWORK;
#ifdef HAVE_OPENSSL
  if (low->driver == mailstream_openssl_low_driver())
    return MAILSTREAM_SSL_BACKEND_OPENSSL;
#endif
#ifdef HAVE_GNUTLS
  if (low->driver == mailstream_gnutls_low_driver())
    return MAILSTREAM_SSL_BACKEND_GNUTLS;
#endif
  return MAILSTREAM_SSL_BACKEND_CFNETWORK;
}

ssize_t mailstream_ssl_get_certificate(mailstream * stream,
    unsigned char ** cert_der)
{
  switch (mailstream_ssl_backend_for_stream(stream)) {
#ifdef HAVE_OPENSSL
  case MAILSTREAM_SSL_BACKEND_OPENSSL:
    return mailstream_openssl_ssl_get_certificate(stream, cert_der);
#endif
#ifdef HAVE_GNUTLS
  case MAILSTREAM_SSL_BACKEND_GNUTLS:
    return mailstream_gnutls_ssl_get_certificate(stream, cert_der);
#endif
  default:
    return -1;
  }
}

int mailstream_ssl_set_client_certicate(struct mailstream_ssl_context * context,
    char * filename)
{
  if (context == NULL)
    return -1;
  switch (context->backend) {
#ifdef HAVE_OPENSSL
  case MAILSTREAM_SSL_BACKEND_OPENSSL:
    return mailstream_openssl_ssl_set_client_certicate(
        context->provider_context, filename);
#endif
#ifdef HAVE_GNUTLS
  case MAILSTREAM_SSL_BACKEND_GNUTLS:
    return mailstream_gnutls_ssl_set_client_certicate(
        context->provider_context, filename);
#endif
  default:
    return -1;
  }
}

int mailstream_ssl_set_client_certificate_data(
    struct mailstream_ssl_context * context, unsigned char * der, size_t length)
{
  if (context == NULL)
    return -1;
  switch (context->backend) {
#ifdef HAVE_OPENSSL
  case MAILSTREAM_SSL_BACKEND_OPENSSL:
    return mailstream_openssl_ssl_set_client_certificate_data(
        context->provider_context, der, length);
#endif
#ifdef HAVE_GNUTLS
  case MAILSTREAM_SSL_BACKEND_GNUTLS:
    return mailstream_gnutls_ssl_set_client_certificate_data(
        context->provider_context, der, length);
#endif
  default:
    return -1;
  }
}

int mailstream_ssl_set_client_private_key_data(
    struct mailstream_ssl_context * context, unsigned char * der, size_t length)
{
  if (context == NULL)
    return -1;
  switch (context->backend) {
#ifdef HAVE_OPENSSL
  case MAILSTREAM_SSL_BACKEND_OPENSSL:
    return mailstream_openssl_ssl_set_client_private_key_data(
        context->provider_context, der, length);
#endif
#ifdef HAVE_GNUTLS
  case MAILSTREAM_SSL_BACKEND_GNUTLS:
    return mailstream_gnutls_ssl_set_client_private_key_data(
        context->provider_context, der, length);
#endif
  default:
    return -1;
  }
}

int mailstream_ssl_set_server_certicate(struct mailstream_ssl_context * context,
    char * ca_file, char * ca_path)
{
  if (context == NULL)
    return -1;
  switch (context->backend) {
#ifdef HAVE_OPENSSL
  case MAILSTREAM_SSL_BACKEND_OPENSSL:
    return mailstream_openssl_ssl_set_server_certicate(
        context->provider_context, ca_file, ca_path);
#endif
#ifdef HAVE_GNUTLS
  case MAILSTREAM_SSL_BACKEND_GNUTLS:
    return mailstream_gnutls_ssl_set_server_certicate(
        context->provider_context, ca_file, ca_path);
#endif
  default:
    return -1;
  }
}

int mailstream_ssl_set_server_name(struct mailstream_ssl_context * context,
    const char * hostname)
{
  if (context == NULL)
    return -1;
  switch (context->backend) {
#ifdef HAVE_OPENSSL
  case MAILSTREAM_SSL_BACKEND_OPENSSL:
    return mailstream_openssl_ssl_set_server_name(
        context->provider_context, hostname);
#endif
#ifdef HAVE_GNUTLS
  case MAILSTREAM_SSL_BACKEND_GNUTLS:
    return mailstream_gnutls_ssl_set_server_name(
        context->provider_context, hostname);
#endif
  default:
    return -1;
  }
}

void mailstream_ssl_set_server_name_callback(
    struct mailstream_ssl_context * context, void * data)
{
  mailstream_ssl_set_server_name(context, data);
}

void * mailstream_ssl_get_openssl_ssl_ctx(
    struct mailstream_ssl_context * context)
{
  if (context == NULL ||
      context->backend != MAILSTREAM_SSL_BACKEND_OPENSSL)
    return NULL;
#ifdef HAVE_OPENSSL
  return mailstream_openssl_ssl_get_openssl_ssl_ctx(
      context->provider_context);
#else
  return NULL;
#endif
}

int mailstream_ssl_get_fd(struct mailstream_ssl_context * context)
{
  if (context == NULL)
    return -1;
  switch (context->backend) {
#ifdef HAVE_OPENSSL
  case MAILSTREAM_SSL_BACKEND_OPENSSL:
    return mailstream_openssl_ssl_get_fd(context->provider_context);
#endif
#ifdef HAVE_GNUTLS
  case MAILSTREAM_SSL_BACKEND_GNUTLS:
    return mailstream_gnutls_ssl_get_fd(context->provider_context);
#endif
  default:
    return -1;
  }
}

void mailstream_openssl_init_not_required(void)
{
#ifdef HAVE_OPENSSL
  mailstream_openssl_ssl_init_not_required();
#endif
}

void mailstream_gnutls_init_not_required(void)
{
#ifdef HAVE_GNUTLS
  mailstream_gnutls_ssl_init_not_required();
#endif
}

void mailstream_ssl_init_not_required(void)
{
  switch (mailstream_ssl_selected_socket_backend()) {
#ifdef HAVE_OPENSSL
  case MAILSTREAM_SSL_BACKEND_OPENSSL:
    mailstream_openssl_ssl_init_not_required();
    break;
#endif
#ifdef HAVE_GNUTLS
  case MAILSTREAM_SSL_BACKEND_GNUTLS:
    mailstream_gnutls_ssl_init_not_required();
    break;
#endif
  default:
    break;
  }
}

void mailstream_ssl_init_lock(void)
{
  switch (mailstream_ssl_selected_socket_backend()) {
#ifdef HAVE_OPENSSL
  case MAILSTREAM_SSL_BACKEND_OPENSSL:
    mailstream_openssl_ssl_init_lock();
    break;
#endif
#ifdef HAVE_GNUTLS
  case MAILSTREAM_SSL_BACKEND_GNUTLS:
    mailstream_gnutls_ssl_init_lock();
    break;
#endif
  default:
    break;
  }
}

void mailstream_ssl_uninit_lock(void)
{
  switch (mailstream_ssl_selected_socket_backend()) {
#ifdef HAVE_OPENSSL
  case MAILSTREAM_SSL_BACKEND_OPENSSL:
    mailstream_openssl_ssl_uninit_lock();
    break;
#endif
#ifdef HAVE_GNUTLS
  case MAILSTREAM_SSL_BACKEND_GNUTLS:
    mailstream_gnutls_ssl_uninit_lock();
    break;
#endif
  default:
    break;
  }
}

carray * mailstream_low_ssl_get_certificate_chain(mailstream_low * stream)
{
  return mailstream_low_get_certificate_chain(stream);
}
