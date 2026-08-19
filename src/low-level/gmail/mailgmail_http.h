/*
 * libEtPan! -- a mail stuff library
 */

#ifndef MAILGMAIL_HTTP_H

#define MAILGMAIL_HTTP_H

#include "mailgmail_types.h"

#include <stddef.h>
#include <time.h>

struct mailgmail_http_header {
  char * name;
  char * value;
};

struct mailgmail_http_request {
  char * method;
  char * url;
  clist * headers; /* struct mailgmail_http_header * */
  unsigned char * body;
  size_t body_len;
  time_t timeout;
};

struct mailgmail_http_response {
  int status_code;
  clist * headers; /* struct mailgmail_http_header * */
  unsigned char * body;
  size_t body_len;
};

struct mailgmail_http_transport {
  void * context;
  int (*perform)(struct mailgmail_http_transport * transport,
      struct mailgmail_http_request * request,
      struct mailgmail_http_response ** response);
  void (*free)(struct mailgmail_http_transport * transport);
};

struct mailhttp_transport;

struct mailgmail_http_header *
mailgmail_http_header_new(const char * name, const char * value);

void mailgmail_http_header_free(struct mailgmail_http_header * header);

struct mailgmail_http_request *
mailgmail_http_request_new(const char * method, const char * url);

void mailgmail_http_request_free(struct mailgmail_http_request * request);

int mailgmail_http_request_add_header(
    struct mailgmail_http_request * request,
    const char * name,
    const char * value);

int mailgmail_http_request_set_body(
    struct mailgmail_http_request * request,
    const unsigned char * body,
    size_t body_len);

struct mailgmail_http_response *
mailgmail_http_response_new(int status_code);

void mailgmail_http_response_free(struct mailgmail_http_response * response);

int mailgmail_http_response_add_header(
    struct mailgmail_http_response * response,
    const char * name,
    const char * value);

const char * mailgmail_http_response_header_value(
    struct mailgmail_http_response * response,
    const char * name);

int mailgmail_http_transport_new_default(
    struct mailgmail_http_transport ** result);

int mailgmail_http_transport_new_curl(
    struct mailgmail_http_transport ** result);

int mailgmail_http_transport_new_nsurlsession(
    struct mailgmail_http_transport ** result);

int mailgmail_http_transport_new_mailhttp(
    struct mailhttp_transport * common,
    struct mailgmail_http_transport ** result);

void mailgmail_http_transport_free(
    struct mailgmail_http_transport * transport);

int mailgmail_http_perform(
    struct mailgmail_http_transport * transport,
    struct mailgmail_http_request * request,
    struct mailgmail_http_response ** response);

#endif
