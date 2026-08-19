/*
 * libEtPan! -- a mail stuff library
 */

#ifndef MAILJMAP_HTTP_H

#define MAILJMAP_HTTP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libetpan/clist.h>
#include <libetpan/mailjmap_types.h>

#include <stddef.h>
#include <time.h>

struct mailjmap_http_header {
  char * name;
  char * value;
};

struct mailjmap_http_request {
  char * method;
  char * url;
  clist * headers; /* struct mailjmap_http_header * */
  unsigned char * body;
  size_t body_len;
  char * content_type;
  char * accept_type;
  time_t timeout;
};

struct mailjmap_http_response {
  int status_code;
  clist * headers; /* struct mailjmap_http_header * */
  unsigned char * body;
  size_t body_len;
  char * final_url;
};

struct mailjmap_http_transport {
  void * context;
  int (*perform)(struct mailjmap_http_transport * transport,
      struct mailjmap_http_request * request,
      struct mailjmap_http_response ** response);
  void (*free)(struct mailjmap_http_transport * transport);
};

LIBETPAN_EXPORT
struct mailjmap_http_header *
mailjmap_http_header_new(const char * name, const char * value);

LIBETPAN_EXPORT
void mailjmap_http_header_free(struct mailjmap_http_header * header);

LIBETPAN_EXPORT
struct mailjmap_http_request *
mailjmap_http_request_new(const char * method, const char * url);

LIBETPAN_EXPORT
void mailjmap_http_request_free(struct mailjmap_http_request * request);

LIBETPAN_EXPORT
int mailjmap_http_request_add_header(
    struct mailjmap_http_request * request,
    const char * name,
    const char * value);

LIBETPAN_EXPORT
int mailjmap_http_request_set_body(
    struct mailjmap_http_request * request,
    const unsigned char * body,
    size_t body_len);

LIBETPAN_EXPORT
int mailjmap_http_request_set_content_type(
    struct mailjmap_http_request * request,
    const char * content_type);

LIBETPAN_EXPORT
int mailjmap_http_request_set_accept_type(
    struct mailjmap_http_request * request,
    const char * accept_type);

LIBETPAN_EXPORT
struct mailjmap_http_response *
mailjmap_http_response_new(int status_code);

LIBETPAN_EXPORT
void mailjmap_http_response_free(struct mailjmap_http_response * response);

LIBETPAN_EXPORT
int mailjmap_http_response_add_header(
    struct mailjmap_http_response * response,
    const char * name,
    const char * value);

LIBETPAN_EXPORT
const char * mailjmap_http_response_header_value(
    struct mailjmap_http_response * response,
    const char * name);

LIBETPAN_EXPORT
int mailjmap_http_response_set_final_url(
    struct mailjmap_http_response * response,
    const char * final_url);

LIBETPAN_EXPORT
int mailjmap_http_transport_new_default(
    struct mailjmap_http_transport ** result);

LIBETPAN_EXPORT
int mailjmap_http_transport_new_curl(
    struct mailjmap_http_transport ** result);

LIBETPAN_EXPORT
int mailjmap_http_transport_new_nsurlsession(
    struct mailjmap_http_transport ** result);

LIBETPAN_EXPORT
void mailjmap_http_transport_free(
    struct mailjmap_http_transport * transport);

LIBETPAN_EXPORT
int mailjmap_http_perform(
    struct mailjmap_http_transport * transport,
    struct mailjmap_http_request * request,
    struct mailjmap_http_response ** response);

#ifdef __cplusplus
}
#endif

#endif
