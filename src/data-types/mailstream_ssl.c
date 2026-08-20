/*
 * Provider-neutral TLS stream dispatch for libEtPan.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "mailstream_ssl.h"
#include "mailstream_ssl_backend.h"
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
  const struct mailstream_ssl_backend_driver * driver;
  void * provider_context;
};

struct mailstream_ssl_callback_data {
  const struct mailstream_ssl_backend_driver * driver;
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
  return mailstream_ssl_connect_voip_timeout(server, port, timeout, 0,
      callback, callback_data, error);
}

mailstream * mailstream_ssl_connect_voip_timeout(const char * server,
    uint16_t port, time_t timeout, int voip_enabled,
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

    stream = mailstream_cfstream_open_voip_timeout(server, port, voip_enabled,
        timeout);
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
#else
  (void) voip_enabled;
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

static const struct mailstream_ssl_backend_driver *
mailstream_ssl_backend_driver_for_type(enum mailstream_ssl_backend backend)
{
  switch (backend) {
#ifdef HAVE_OPENSSL
  case MAILSTREAM_SSL_BACKEND_OPENSSL:
    return mailstream_openssl_backend_driver();
#endif
#ifdef HAVE_GNUTLS
  case MAILSTREAM_SSL_BACKEND_GNUTLS:
    return mailstream_gnutls_backend_driver();
#endif
  default:
    return NULL;
  }
}

static const struct mailstream_ssl_backend_driver *
mailstream_ssl_default_socket_backend_driver(void)
{
#ifdef HAVE_OPENSSL
  return mailstream_openssl_backend_driver();
#elif defined(HAVE_GNUTLS)
  return mailstream_gnutls_backend_driver();
#else
  return NULL;
#endif
}

static const struct mailstream_ssl_backend_driver *
mailstream_ssl_selected_socket_backend_driver(void)
{
  const struct mailstream_ssl_backend_driver * driver;

  driver = mailstream_ssl_backend_driver_for_type(selected_backend);
  if (driver != NULL)
    return driver;
  return mailstream_ssl_default_socket_backend_driver();
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
  return mailstream_ssl_backend_driver_for_type(backend) != NULL;
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

static void mailstream_ssl_provider_callback(void * provider_context,
    void * data)
{
  struct mailstream_ssl_callback_data * callback_data = data;
  struct mailstream_ssl_context context;

  context.backend = callback_data->driver->backend;
  context.driver = callback_data->driver;
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
  const struct mailstream_ssl_backend_driver * driver;
  struct mailstream_ssl_callback_data callback_data;

  driver = mailstream_ssl_selected_socket_backend_driver();
  if (driver == NULL)
    return NULL;

  callback_data.driver = driver;
  callback_data.server_name = server_name;
  callback_data.callback = callback;
  callback_data.callback_data = data;

  return driver->open_low(fd, starttls, timeout,
      mailstream_ssl_provider_callback, &callback_data);
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

static const struct mailstream_ssl_backend_driver *
mailstream_ssl_driver_for_stream(mailstream * stream)
{
  mailstream_low * low;

  if (stream == NULL)
    return NULL;
  low = mailstream_get_low(stream);
  if (low == NULL)
    return NULL;
#ifdef HAVE_OPENSSL
  if (low->driver == mailstream_openssl_backend_driver()->low_driver)
    return mailstream_openssl_backend_driver();
#endif
#ifdef HAVE_GNUTLS
  if (low->driver == mailstream_gnutls_backend_driver()->low_driver)
    return mailstream_gnutls_backend_driver();
#endif
  return NULL;
}

ssize_t mailstream_ssl_get_certificate(mailstream * stream,
    unsigned char ** cert_der)
{
  const struct mailstream_ssl_backend_driver * driver;

  driver = mailstream_ssl_driver_for_stream(stream);
  if (driver == NULL)
    return -1;
  return driver->copy_certificate(stream, cert_der);
}

int mailstream_ssl_set_client_certicate(struct mailstream_ssl_context * context,
    char * filename)
{
  if (context == NULL || context->driver->set_client_certificate_file == NULL)
    return -1;
  return context->driver->set_client_certificate_file(
      context->provider_context, filename);
}

int mailstream_ssl_set_client_certificate_data(
    struct mailstream_ssl_context * context, unsigned char * der, size_t length)
{
  if (context == NULL || context->driver->set_client_certificate_data == NULL)
    return -1;
  return context->driver->set_client_certificate_data(
      context->provider_context, der, length);
}

int mailstream_ssl_set_client_private_key_data(
    struct mailstream_ssl_context * context, unsigned char * der, size_t length)
{
  if (context == NULL || context->driver->set_client_private_key_data == NULL)
    return -1;
  return context->driver->set_client_private_key_data(
      context->provider_context, der, length);
}

int mailstream_ssl_set_server_certicate(struct mailstream_ssl_context * context,
    char * ca_file, char * ca_path)
{
  if (context == NULL || context->driver->set_server_certificate == NULL)
    return -1;
  return context->driver->set_server_certificate(
      context->provider_context, ca_file, ca_path);
}

int mailstream_ssl_set_server_name(struct mailstream_ssl_context * context,
    const char * hostname)
{
  if (context == NULL || context->driver->set_server_name == NULL)
    return -1;
  return context->driver->set_server_name(context->provider_context, hostname);
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
      context->backend != MAILSTREAM_SSL_BACKEND_OPENSSL ||
      context->driver->get_native_context == NULL)
    return NULL;
  return context->driver->get_native_context(context->provider_context);
}

int mailstream_ssl_get_fd(struct mailstream_ssl_context * context)
{
  if (context == NULL || context->driver->get_context_fd == NULL)
    return -1;
  return context->driver->get_context_fd(context->provider_context);
}

void mailstream_openssl_init_not_required(void)
{
#ifdef HAVE_OPENSSL
  mailstream_openssl_backend_driver()->init_not_required();
#endif
}

void mailstream_gnutls_init_not_required(void)
{
#ifdef HAVE_GNUTLS
  mailstream_gnutls_backend_driver()->init_not_required();
#endif
}

void mailstream_ssl_init_not_required(void)
{
  const struct mailstream_ssl_backend_driver * driver;

  driver = mailstream_ssl_selected_socket_backend_driver();
  if (driver != NULL)
    driver->init_not_required();
}

void mailstream_ssl_init_lock(void)
{
  const struct mailstream_ssl_backend_driver * driver;

  driver = mailstream_ssl_selected_socket_backend_driver();
  if (driver != NULL && driver->init_lock != NULL)
    driver->init_lock();
}

void mailstream_ssl_uninit_lock(void)
{
  const struct mailstream_ssl_backend_driver * driver;

  driver = mailstream_ssl_selected_socket_backend_driver();
  if (driver != NULL && driver->uninit_lock != NULL)
    driver->uninit_lock();
}

carray * mailstream_low_ssl_get_certificate_chain(mailstream_low * stream)
{
  return mailstream_low_get_certificate_chain(stream);
}
