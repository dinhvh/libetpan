#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libetpan/mailstream.h>
#include <libetpan/mailstream_ssl.h>
#include <libetpan/mailimap.h>
#include <libetpan/mailimap_socket.h>
#include <libetpan/mailpop3.h>
#include <libetpan/mailpop3_socket.h>
#include <libetpan/mailsmtp.h>
#include <libetpan/mailsmtp_socket.h>

#if defined(HAVE_OPENSSL) && !defined(WIN32)
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>

#include <openssl/pem.h>
#include <openssl/ssl.h>

static const char test_cert_pem[] =
"-----BEGIN CERTIFICATE-----\n"
"MIIDCTCCAfGgAwIBAgIUd6kzJAnOYwiPxUhnsJGbwBi+6xEwDQYJKoZIhvcNAQEL\n"
"BQAwFDESMBAGA1UEAwwJbG9jYWxob3N0MB4XDTI2MDgyMTE1MjkxM1oXDTM2MDgx\n"
"ODE1MjkxM1owFDESMBAGA1UEAwwJbG9jYWxob3N0MIIBIjANBgkqhkiG9w0BAQEF\n"
"AAOCAQ8AMIIBCgKCAQEA74jD1LAPCfTzadaAN5eESpjs+MIyiFSkO9FQcIVYP+Dw\n"
"vGvOZr61v3iVVq5EshVsl91GxYnsqi7iD8i4v6127rV8VA6/zzn0I+tMGMcuiVfL\n"
"2JMNVL3SXZwpot7XkWHwRs1vbQ4jmBr2CjW9aRhLy8B9K4zIYIEOYw66CIPvDJsc\n"
"J9Sc2kIOEJ5RjSP5vuRM9Onm1NASg0eeOtM9kqfdn2ey7qdW7If1xifEUqhHHbl/\n"
"VmbaWENai+G9nsbH317q9HxNpdbORxWNTmkNaMbA4CFzlLHRk6VEj9zMNBwm9Q7V\n"
"BNYUxaaauYs88tLleA6yYM+v5o4I+UKxlO7xS0FxFQIDAQABo1MwUTAdBgNVHQ4E\n"
"FgQU4mECoQF9++JGHqp0xVFTljDNlU0wHwYDVR0jBBgwFoAU4mECoQF9++JGHqp0\n"
"xVFTljDNlU0wDwYDVR0TAQH/BAUwAwEB/zANBgkqhkiG9w0BAQsFAAOCAQEAKeU8\n"
"2JLvM85CJ54Cj5sEwNOav9B3LGyskt/yO1IZ2oPGepBBCytnAwALY9OfDMNtBj4D\n"
"zqtznYSkEYhkCoUXriLygrPEKNRn/Bps8X0ht0cJrQ77tsoG+PfPKalIx0Hvffz5\n"
"hDicC8VBkcYDmEOtzVHNaJSE3ayltWSf3mThssZzD9blr3Lv6Kw5NxYXvia0UOQR\n"
"GX70mWitXjSeN+Rm70Iow0VPPA11UPluQRdGcRKU1zl+Cv8h6CQDn4WaBOoiPBnW\n"
"+4ibe0Hmh1CGXDiQQ81+cYtzPSShCwnXet4nbijDhCeuSXltbYSkYDo0Skx5/1XT\n"
"TOzAEWjDjPEvQQ9jHg==\n"
"-----END CERTIFICATE-----\n";

static const char test_key_pem[] =
"-----BEGIN PRIVATE KEY-----\n"
"MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQDviMPUsA8J9PNp\n"
"1oA3l4RKmOz4wjKIVKQ70VBwhVg/4PC8a85mvrW/eJVWrkSyFWyX3UbFieyqLuIP\n"
"yLi/rXbutXxUDr/POfQj60wYxy6JV8vYkw1UvdJdnCmi3teRYfBGzW9tDiOYGvYK\n"
"Nb1pGEvLwH0rjMhggQ5jDroIg+8Mmxwn1JzaQg4QnlGNI/m+5Ez06ebU0BKDR546\n"
"0z2Sp92fZ7Lup1bsh/XGJ8RSqEcduX9WZtpYQ1qL4b2exsffXur0fE2l1s5HFY1O\n"
"aQ1oxsDgIXOUsdGTpUSP3Mw0HCb1DtUE1hTFppq5izzy0uV4DrJgz6/mjgj5QrGU\n"
"7vFLQXEVAgMBAAECggEADmqobK3kWH38GNU5QNSh+J0VJXtcsBILaMfrjpDRU6uT\n"
"GagRvg3W4a1dLEskpSTpylxzShLdIg4HJZo4Al4AmpPk5dNAbfVn5rHepzQfw9I7\n"
"WYTaPczcndfhtU+HtmnoXKk4fl4Q5t1NNyaZCJEBiNkZlNbY6DQER2fSFblJGInv\n"
"Okvm7mBF4ScopiB/8PbcVIHEPTHAZzLaQjpaBtjHwNaYebJtjC03ewRAwSrjWiAU\n"
"fzTXr66kdxb7QPqWVIOdFaAXzIDj6Rm6LH9uWZ1g6tCRYQrKwNYJh+yOo7hZNtYj\n"
"kkJudKzCSVcDcCZETuyhoOEbuxW86sOEERgD4RBJYQKBgQD9QEzm6/15k8SwJvKM\n"
"+9dWnMRSa6t6D6W05c5eQAmSOduEoboU2ppijVW5vlQtVWrEilU6yhLZu9zGq+B8\n"
"VBWBjOkxMpbZMAunrAaOqzVpCcl/HBKPt2rGEv8YO8px6LYlrTS4vd7Mr7Gm93Y3\n"
"SUxFNCYNQ1oDjU6heasNwnmsdQKBgQDyIlmOWuD0jt5kWImtF8KrHCpxNZsncc3U\n"
"xk2xIzhVJVkKGqffeM6wFS36lhUZ8wGthbhhHOHF5toiV/05J3BbdlQMM8Y77lSx\n"
"4G9YPsywOfrO5u7wBYJEmQndVncrsWxL86HwwcZ6E3quaYqRmkSW0bK4RnBXiIO5\n"
"L1o3gFqeIQKBgA35HkUfgDzVCmUtwPRGcPs2ax6hEjUJ5/qzM0+/+Mc4qgQHaFz+\n"
"MsZ9Rd2Zwss9i0aM22cC/0dENko/8YLqoMUlITi1sd0J6Zq2wyXcDHZfM97vjZag\n"
"aCPFqI2Nvv3J+ULa663FzCU/DG5J+RvTz4GB/xd8P9syUMRYEjgQIuaBAoGBAJDX\n"
"EWFBZZ2FNO0rtTeQpgc7MvxDh8tBnVGxSdozakgsriWIKnFYT7MvCDEExBygq5lE\n"
"CzY0U63RWR0AK16cA+8CSmZ+Ng+1kn5Q9eoAruqf1DMeG0IC2Zj1PtxjE6hUWqbY\n"
"Vz+wemyd6F4ajiRo9qyNRe5LXHpQFBObisj2g75hAoGBAJdXj2p5lGN0JTfPmS6D\n"
"YmLjXbjxLHgeznb4+PUUgYTfKRnaUGCUA7WSeT4uUMEkq40jmQw4Gmc9DSPRcF5+\n"
"VqCYBh+v7DozegS1scbjWyBTrSRyADH/G4g3TEJISI9hv79MjsQ8EZwqQYOXbogL\n"
"Hc3dAkPhkFMOUbUmf/KaeQfe\n"
"-----END PRIVATE KEY-----\n";

struct test_server {
  int listen_fd;
  int result;
  pthread_t thread;
  int protocol;
};

enum test_protocol {
  TEST_DIRECT_TLS,
  TEST_IMAP_STARTTLS,
  TEST_POP3_STARTTLS,
  TEST_SMTP_STARTTLS
};

struct callback_state {
  int called;
};

static void ssl_callback(struct mailstream_ssl_context * context, void * data)
{
  struct callback_state * state = data;

  assert(context != NULL);
  assert(mailstream_ssl_get_fd(context) >= 0);
  state->called = 1;
}

static SSL_CTX * server_context_new(void)
{
  BIO * cert_bio;
  BIO * key_bio;
  X509 * cert = NULL;
  EVP_PKEY * key = NULL;
  SSL_CTX * ctx;

  ctx = SSL_CTX_new(TLS_server_method());
  if (ctx == NULL)
    return NULL;

  cert_bio = BIO_new_mem_buf(test_cert_pem, -1);
  key_bio = BIO_new_mem_buf(test_key_pem, -1);
  if (cert_bio == NULL || key_bio == NULL)
    goto err;

  cert = PEM_read_bio_X509(cert_bio, NULL, NULL, NULL);
  key = PEM_read_bio_PrivateKey(key_bio, NULL, NULL, NULL);
  BIO_free(cert_bio);
  BIO_free(key_bio);
  cert_bio = NULL;
  key_bio = NULL;
  if (cert == NULL || key == NULL)
    goto err;

  if (SSL_CTX_use_certificate(ctx, cert) != 1)
    goto free_cert_key;
  if (SSL_CTX_use_PrivateKey(ctx, key) != 1)
    goto free_cert_key;
  if (SSL_CTX_check_private_key(ctx) != 1)
    goto free_cert_key;

  X509_free(cert);
  EVP_PKEY_free(key);
  return ctx;

free_cert_key:
  X509_free(cert);
  EVP_PKEY_free(key);
err:
  if (cert_bio != NULL)
    BIO_free(cert_bio);
  if (key_bio != NULL)
    BIO_free(key_bio);
  SSL_CTX_free(ctx);
  return NULL;
}

static int ssl_read_exact(SSL * ssl, char * buf, size_t len)
{
  size_t done = 0;

  while (done < len) {
    int r = SSL_read(ssl, buf + done, (int) (len - done));
    if (r <= 0)
      return -1;
    done += (size_t) r;
  }

  return 0;
}

static int ssl_write_exact(SSL * ssl, const char * buf, size_t len)
{
  size_t done = 0;

  while (done < len) {
    int r = SSL_write(ssl, buf + done, (int) (len - done));
    if (r <= 0)
      return -1;
    done += (size_t) r;
  }

  return 0;
}

static int socket_write_exact(int fd, const char * buf, size_t len)
{
  size_t done = 0;

  while (done < len) {
    ssize_t r = write(fd, buf + done, len - done);
    if (r <= 0)
      return -1;
    done += (size_t) r;
  }
  return 0;
}

static int socket_read_line(int fd, char * buf, size_t size)
{
  size_t len = 0;

  while (len + 1 < size) {
    ssize_t r = read(fd, buf + len, 1);
    if (r != 1)
      return -1;
    if (buf[len++] == '\n') {
      buf[len] = '\0';
      return 0;
    }
  }
  return -1;
}

static int ssl_read_line(SSL * ssl, char * buf, size_t size)
{
  size_t len = 0;

  while (len + 1 < size) {
    int r = SSL_read(ssl, buf + len, 1);
    if (r != 1)
      return -1;
    if (buf[len++] == '\n') {
      buf[len] = '\0';
      return 0;
    }
  }
  return -1;
}

static int prepare_starttls(int fd, int protocol)
{
  char line[256];
  char response[320];
  char * space;

  switch (protocol) {
  case TEST_IMAP_STARTTLS:
    if (socket_write_exact(fd,
        "* OK [CAPABILITY IMAP4rev1 STARTTLS] Server ready\r\n",
        strlen("* OK [CAPABILITY IMAP4rev1 STARTTLS] Server ready\r\n")) < 0 ||
        socket_read_line(fd, line, sizeof(line)) < 0)
      return -1;
    space = strchr(line, ' ');
    if (space == NULL || strcmp(space + 1, "STARTTLS\r\n") != 0)
      return -1;
    *space = '\0';
    snprintf(response, sizeof(response), "%s OK Begin TLS\r\n", line);
    return socket_write_exact(fd, response, strlen(response));
  case TEST_POP3_STARTTLS:
    if (socket_write_exact(fd, "+OK local POP3 ready\r\n",
        strlen("+OK local POP3 ready\r\n")) < 0 ||
        socket_read_line(fd, line, sizeof(line)) < 0 ||
        strcmp(line, "STLS\r\n") != 0)
      return -1;
    return socket_write_exact(fd, "+OK Begin TLS\r\n",
        strlen("+OK Begin TLS\r\n"));
  case TEST_SMTP_STARTTLS:
    if (socket_write_exact(fd, "220 local SMTP ready\r\n",
        strlen("220 local SMTP ready\r\n")) < 0 ||
        socket_read_line(fd, line, sizeof(line)) < 0 ||
        strncmp(line, "EHLO ", 5) != 0 ||
        socket_write_exact(fd, "250-local\r\n250 STARTTLS\r\n",
            strlen("250-local\r\n250 STARTTLS\r\n")) < 0 ||
        socket_read_line(fd, line, sizeof(line)) < 0 ||
        strcmp(line, "STARTTLS\r\n") != 0)
      return -1;
    return socket_write_exact(fd, "220 Begin TLS\r\n",
        strlen("220 Begin TLS\r\n"));
  default:
    return -1;
  }
}

static int serve_post_starttls(SSL * ssl, int protocol)
{
  char line[256];
  char response[320];
  char * space;

  if (ssl_read_line(ssl, line, sizeof(line)) < 0)
    return -1;
  switch (protocol) {
  case TEST_IMAP_STARTTLS:
    space = strchr(line, ' ');
    if (space == NULL || strcmp(space + 1, "NOOP\r\n") != 0)
      return -1;
    *space = '\0';
    snprintf(response, sizeof(response), "%s OK NOOP completed\r\n", line);
    return ssl_write_exact(ssl, response, strlen(response));
  case TEST_POP3_STARTTLS:
    if (strcmp(line, "CAPA\r\n") != 0)
      return -1;
    return ssl_write_exact(ssl, "+OK\r\nSTLS\r\n.\r\n", 14);
  case TEST_SMTP_STARTTLS:
    if (strcmp(line, "NOOP\r\n") != 0)
      return -1;
    return ssl_write_exact(ssl, "250 OK\r\n", 8);
  default:
    return -1;
  }
}

static void * server_thread_main(void * data)
{
  struct test_server * server = data;
  SSL_CTX * ctx;
  SSL * ssl = NULL;
  int client_fd = -1;
  char input[4];

  ctx = server_context_new();
  if (ctx == NULL)
    goto err;

  client_fd = accept(server->listen_fd, NULL, NULL);
  close(server->listen_fd);
  server->listen_fd = -1;
  if (client_fd < 0)
    goto err_ctx;

  if (server->protocol != TEST_DIRECT_TLS &&
      prepare_starttls(client_fd, server->protocol) < 0)
    goto err_client;

  ssl = SSL_new(ctx);
  if (ssl == NULL)
    goto err_client;
  if (SSL_set_fd(ssl, client_fd) != 1)
    goto err_ssl;
  if (SSL_accept(ssl) != 1)
    goto err_ssl;
  if (server->protocol == TEST_DIRECT_TLS) {
    if (ssl_read_exact(ssl, input, sizeof(input)) < 0 ||
        memcmp(input, "ping", sizeof(input)) != 0 ||
        ssl_write_exact(ssl, "pong", 4) < 0)
      goto err_ssl;
  }
  else if (serve_post_starttls(ssl, server->protocol) < 0)
    goto err_ssl;

  SSL_shutdown(ssl);
  SSL_free(ssl);
  close(client_fd);
  SSL_CTX_free(ctx);
  server->result = 0;
  return NULL;

err_ssl:
  SSL_free(ssl);
err_client:
  close(client_fd);
err_ctx:
  SSL_CTX_free(ctx);
err:
  if (server->listen_fd >= 0) {
    close(server->listen_fd);
    server->listen_fd = -1;
  }
  server->result = -1;
  return NULL;
}

static uint16_t server_start(struct test_server * server, int protocol)
{
  struct sockaddr_in addr;
  socklen_t addr_len;

  memset(server, 0, sizeof(*server));
  server->protocol = protocol;
  server->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  assert(server->listen_fd >= 0);

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = 0;
  assert(bind(server->listen_fd, (struct sockaddr *) &addr, sizeof(addr)) == 0);
  assert(listen(server->listen_fd, 1) == 0);

  addr_len = sizeof(addr);
  assert(getsockname(server->listen_fd, (struct sockaddr *) &addr,
      &addr_len) == 0);
  assert(pthread_create(&server->thread, NULL, server_thread_main,
      server) == 0);

  return ntohs(addr.sin_port);
}

static int loopback_sockets_are_available(void)
{
  struct sockaddr_in addr;
  int fd;

  fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    if (errno == EACCES || errno == EPERM) {
      fputs("loopback sockets are not permitted in this environment\n",
          stderr);
      return 0;
    }
    assert(fd >= 0);
  }

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = 0;
  if (bind(fd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
    if (errno == EACCES || errno == EPERM) {
      close(fd);
      fputs("loopback sockets are not permitted in this environment\n",
          stderr);
      return 0;
    }
    assert(0);
  }
  close(fd);
  return 1;
}

static void server_join(struct test_server * server)
{
  assert(pthread_join(server->thread, NULL) == 0);
  assert(server->result == 0);
}

static void assert_stream_roundtrip(mailstream * stream)
{
  char output[4];
  carray * certificate_chain;

  assert(stream != NULL);
  assert(mailstream_write(stream, "ping", 4) == 4);
  assert(mailstream_flush(stream) == 0);
  assert(mailstream_read(stream, output, sizeof(output)) == 4);
  assert(memcmp(output, "pong", sizeof(output)) == 0);

  certificate_chain = mailstream_get_certificate_chain(stream);
  if (certificate_chain != NULL) {
    assert(carray_count(certificate_chain) > 0);
    mailstream_certificate_chain_free(certificate_chain);
  }

  assert(mailstream_close(stream) == 0);
}

static void test_backend(enum mailstream_ssl_backend backend,
    int expect_callback)
{
  struct test_server server;
  struct callback_state callback;
  enum mailstream_ssl_connect_error error;
  mailstream * stream;
  uint16_t port;

  if (!mailstream_ssl_backend_is_available(backend))
    return;

  assert(mailstream_ssl_set_backend(backend) == 0);
  callback.called = 0;
  port = server_start(&server, TEST_DIRECT_TLS);
  error = MAILSTREAM_SSL_CONNECT_ERROR_CONNECTION_REFUSED;
  stream = mailstream_ssl_connect_timeout("127.0.0.1", port, 5,
      expect_callback ? ssl_callback : NULL, &callback, &error);
  assert(error == MAILSTREAM_SSL_CONNECT_NO_ERROR);
  assert_stream_roundtrip(stream);
  assert(callback.called == expect_callback);
  server_join(&server);
}


static void test_starttls_protocol(enum mailstream_ssl_backend backend,
    int protocol)
{
  struct test_server server;
  struct callback_state callback;
  uint16_t port;
  int r;

  if (!mailstream_ssl_backend_is_available(backend))
    return;
  /* CFStream performs the upgrade in-place and does not use this callback. */
  assert(mailstream_ssl_set_backend(backend) == 0);
  callback.called = 0;
  port = server_start(&server, protocol);

  if (protocol == TEST_IMAP_STARTTLS) {
    mailimap * session = mailimap_new(0, NULL);
    mailstream_low * old_low;
    assert(session != NULL);
    r = mailimap_socket_connect(session, "127.0.0.1", port);
    assert(r == MAILIMAP_NO_ERROR_NON_AUTHENTICATED);
    old_low = mailstream_get_low(session->imap_stream);
    r = mailimap_socket_starttls_with_callback(session, ssl_callback, &callback);
    assert(r == MAILIMAP_NO_ERROR);
    assert(mailstream_get_low(session->imap_stream) != old_low ||
        backend == MAILSTREAM_SSL_BACKEND_CFNETWORK);
    assert(mailimap_noop(session) == MAILIMAP_NO_ERROR);
    mailimap_free(session);
  }
  else if (protocol == TEST_POP3_STARTTLS) {
    mailpop3 * session = mailpop3_new(0, NULL);
    mailstream_low * old_low;
    clist * capa_list;
    assert(session != NULL);
    r = mailpop3_socket_connect(session, "127.0.0.1", port);
    assert(r == MAILPOP3_NO_ERROR);
    old_low = mailstream_get_low(session->pop3_stream);
    r = mailpop3_socket_starttls_with_callback(session, ssl_callback, &callback);
    assert(r == MAILPOP3_NO_ERROR);
    assert(mailstream_get_low(session->pop3_stream) != old_low ||
        backend == MAILSTREAM_SSL_BACKEND_CFNETWORK);
    capa_list = NULL;
    assert(mailpop3_capa(session, &capa_list) == MAILPOP3_NO_ERROR);
    mailpop3_capa_resp_free(capa_list);
    mailpop3_free(session);
  }
  else {
    mailsmtp * session = mailsmtp_new(0, NULL);
    mailstream_low * old_low;
    assert(session != NULL);
    r = mailsmtp_socket_connect(session, "127.0.0.1", port);
    assert(r == MAILSMTP_NO_ERROR);
    assert(mailesmtp_ehlo(session) == MAILSMTP_NO_ERROR);
    old_low = mailstream_get_low(session->stream);
    r = mailsmtp_socket_starttls_with_callback(session, ssl_callback, &callback);
    assert(r == MAILSMTP_NO_ERROR);
    assert(mailstream_get_low(session->stream) != old_low ||
        backend == MAILSTREAM_SSL_BACKEND_CFNETWORK);
    assert(mailsmtp_noop(session) == MAILSMTP_NO_ERROR);
    mailsmtp_free(session);
  }

  assert(callback.called == (backend != MAILSTREAM_SSL_BACKEND_CFNETWORK));
  server_join(&server);
}

static void test_starttls_backend(enum mailstream_ssl_backend backend)
{
  test_starttls_protocol(backend, TEST_IMAP_STARTTLS);
  test_starttls_protocol(backend, TEST_POP3_STARTTLS);
  test_starttls_protocol(backend, TEST_SMTP_STARTTLS);
}

int mailstream_tls_backend_roundtrip_test(void)
{
  if (!loopback_sockets_are_available())
    return 77;

  SSL_library_init();
  SSL_load_error_strings();

  test_backend(MAILSTREAM_SSL_BACKEND_OPENSSL, 1);
  test_backend(MAILSTREAM_SSL_BACKEND_GNUTLS, 1);
  test_starttls_backend(MAILSTREAM_SSL_BACKEND_OPENSSL);
  test_starttls_backend(MAILSTREAM_SSL_BACKEND_GNUTLS);

  test_backend(MAILSTREAM_SSL_BACKEND_CFNETWORK, 0);

  return 0;
}

#ifndef MAILSTREAM_TLS_NO_MAIN
int main(void)
{
  return mailstream_tls_backend_roundtrip_test();
}
#endif

#else
#ifndef MAILSTREAM_TLS_NO_MAIN
int main(void)
{
  fputs("OpenSSL and POSIX sockets are required for this fixture\n", stderr);
  return 77;
}
#endif
#endif
