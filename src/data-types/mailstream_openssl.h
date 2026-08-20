#ifndef MAILSTREAM_OPENSSL_H
#define MAILSTREAM_OPENSSL_H

#include <libetpan/mailstream_ssl.h>

#ifdef HAVE_OPENSSL
mailstream_low_driver * mailstream_openssl_low_driver(void);
mailstream_low * mailstream_openssl_open_low(int fd, int starttls,
    time_t timeout,
    void (* callback)(struct mailstream_ssl_context *, void *), void * data);
ssize_t mailstream_openssl_ssl_get_certificate(mailstream * stream,
    unsigned char ** der);
int mailstream_openssl_ssl_set_client_certicate(
    struct mailstream_ssl_context * context, char * filename);
int mailstream_openssl_ssl_set_client_certificate_data(
    struct mailstream_ssl_context * context, unsigned char * der,
    size_t length);
int mailstream_openssl_ssl_set_client_private_key_data(
    struct mailstream_ssl_context * context, unsigned char * der,
    size_t length);
int mailstream_openssl_ssl_set_server_certicate(
    struct mailstream_ssl_context * context, char * ca_file, char * ca_path);
int mailstream_openssl_ssl_set_server_name(
    struct mailstream_ssl_context * context, const char * hostname);
void * mailstream_openssl_ssl_get_openssl_ssl_ctx(
    struct mailstream_ssl_context * context);
int mailstream_openssl_ssl_get_fd(struct mailstream_ssl_context * context);
void mailstream_openssl_ssl_init_not_required(void);
void mailstream_openssl_ssl_init_lock(void);
void mailstream_openssl_ssl_uninit_lock(void);
#endif

#endif
