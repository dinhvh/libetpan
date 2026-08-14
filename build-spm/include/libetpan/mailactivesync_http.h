/*
 * libEtPan! -- a mail stuff library
 */

#ifndef MAILACTIVESYNC_HTTP_H

#define MAILACTIVESYNC_HTTP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libetpan/libetpan-config.h>
#include <libetpan/clist.h>
#include <libetpan/mailactivesync_types.h>

#include <stddef.h>
#include <time.h>

struct mailactivesync_http_header {
  char * name;
  char * value;
};

struct mailactivesync_http_request {
  char * method;
  char * url;
  clist * headers; /* struct mailactivesync_http_header * */
  unsigned char * body;
  size_t body_len;
  time_t timeout;
};

struct mailactivesync_http_response {
  int status_code;
  clist * headers; /* struct mailactivesync_http_header * */
  unsigned char * body;
  size_t body_len;
};

struct mailactivesync_http_transport {
  void * context;
  int (*perform)(struct mailactivesync_http_transport * transport,
      struct mailactivesync_http_request * request,
      struct mailactivesync_http_response ** response);
  void (*free)(struct mailactivesync_http_transport * transport);
};

LIBETPAN_EXPORT
struct mailactivesync_http_header *
mailactivesync_http_header_new(const char * name, const char * value);

LIBETPAN_EXPORT
void mailactivesync_http_header_free(
    struct mailactivesync_http_header * header);

LIBETPAN_EXPORT
struct mailactivesync_http_request *
mailactivesync_http_request_new(const char * method, const char * url);

LIBETPAN_EXPORT
void mailactivesync_http_request_free(
    struct mailactivesync_http_request * request);

LIBETPAN_EXPORT
int mailactivesync_http_request_add_header(
    struct mailactivesync_http_request * request,
    const char * name,
    const char * value);

LIBETPAN_EXPORT
int mailactivesync_http_request_set_body(
    struct mailactivesync_http_request * request,
    const unsigned char * body,
    size_t body_len);

LIBETPAN_EXPORT
struct mailactivesync_http_response *
mailactivesync_http_response_new(int status_code);

LIBETPAN_EXPORT
void mailactivesync_http_response_free(
    struct mailactivesync_http_response * response);

LIBETPAN_EXPORT
int mailactivesync_http_response_add_header(
    struct mailactivesync_http_response * response,
    const char * name,
    const char * value);

LIBETPAN_EXPORT
const char * mailactivesync_http_response_header_value(
    struct mailactivesync_http_response * response,
    const char * name);

LIBETPAN_EXPORT
int mailactivesync_http_transport_new_curl(
    struct mailactivesync_http_transport ** result);

LIBETPAN_EXPORT
void mailactivesync_http_transport_free(
    struct mailactivesync_http_transport * transport);

LIBETPAN_EXPORT
int mailactivesync_http_perform(
    struct mailactivesync_http_transport * transport,
    struct mailactivesync_http_request * request,
    struct mailactivesync_http_response ** response);

#ifdef __cplusplus
}
#endif

#endif
