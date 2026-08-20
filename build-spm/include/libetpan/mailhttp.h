/*
 * libEtPan! -- a mail stuff library
 */

#ifndef MAILHTTP_H
#define MAILHTTP_H

#include <libetpan/libetpan-config.h>
#include <libetpan/clist.h>

#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

enum mailhttp_error {
  MAILHTTP_NO_ERROR = 0,
  MAILHTTP_ERROR_BAD_STATE,
  MAILHTTP_ERROR_MEMORY,
  MAILHTTP_ERROR_BAD_URL,
  MAILHTTP_ERROR_TIMEOUT,
  MAILHTTP_ERROR_RESOLVE,
  MAILHTTP_ERROR_CONNECT,
  MAILHTTP_ERROR_TLS,
  MAILHTTP_ERROR_CANCELLED,
  MAILHTTP_ERROR_PROTOCOL,
  MAILHTTP_ERROR_BODY_SINK,
  MAILHTTP_ERROR_UNAVAILABLE,
  MAILHTTP_ERROR_IO
};

enum mailhttp_backend {
  MAILHTTP_BACKEND_CURL,
  MAILHTTP_BACKEND_NSURLSESSION
};

struct mailhttp_header {
  char * name;
  char * value;
};

typedef int (* mailhttp_body_sink)(const void * data, size_t length,
    void * context);

struct mailhttp_request {
  char * method;
  char * url;
  clist * headers; /* struct mailhttp_header * */
  unsigned char * body;
  size_t body_len;
  time_t timeout;
  int follow_redirects;
  unsigned int max_redirects;
  mailhttp_body_sink body_sink;
  void * body_sink_context;
};

struct mailhttp_response {
  int status_code;
  clist * headers; /* struct mailhttp_header * */
  unsigned char * body;
  size_t body_len;
  char * final_url;
};

struct mailhttp_transport;

struct mailhttp_transport_driver {
  int (* perform)(struct mailhttp_transport * transport,
      struct mailhttp_request * request,
      struct mailhttp_response ** response);
  void (* free)(struct mailhttp_transport * transport);
};

struct mailhttp_transport {
  void * data;
  const struct mailhttp_transport_driver * driver;
  enum mailhttp_backend backend;
};

LIBETPAN_EXPORT
struct mailhttp_header * mailhttp_header_new(const char * name,
    const char * value);
LIBETPAN_EXPORT
void mailhttp_header_free(struct mailhttp_header * header);

LIBETPAN_EXPORT
struct mailhttp_request * mailhttp_request_new(const char * method,
    const char * url);
LIBETPAN_EXPORT
void mailhttp_request_free(struct mailhttp_request * request);
LIBETPAN_EXPORT
int mailhttp_request_add_header(struct mailhttp_request * request,
    const char * name, const char * value);
LIBETPAN_EXPORT
int mailhttp_request_set_body(struct mailhttp_request * request,
    const void * body, size_t body_len);
LIBETPAN_EXPORT
void mailhttp_request_set_body_sink(struct mailhttp_request * request,
    mailhttp_body_sink sink, void * context);

LIBETPAN_EXPORT
struct mailhttp_response * mailhttp_response_new(int status_code);
LIBETPAN_EXPORT
void mailhttp_response_free(struct mailhttp_response * response);
LIBETPAN_EXPORT
int mailhttp_response_add_header(struct mailhttp_response * response,
    const char * name, const char * value);
LIBETPAN_EXPORT
const char * mailhttp_response_header_value(
    const struct mailhttp_response * response, const char * name);
LIBETPAN_EXPORT
int mailhttp_response_set_final_url(struct mailhttp_response * response,
    const char * final_url);
LIBETPAN_EXPORT
int mailhttp_response_append_body(struct mailhttp_response * response,
    const void * data, size_t length);

LIBETPAN_EXPORT
struct mailhttp_transport * mailhttp_transport_new(void * data,
    const struct mailhttp_transport_driver * driver,
    enum mailhttp_backend backend);
LIBETPAN_EXPORT
void mailhttp_transport_free(struct mailhttp_transport * transport);
LIBETPAN_EXPORT
int mailhttp_perform(struct mailhttp_transport * transport,
    struct mailhttp_request * request,
    struct mailhttp_response ** response);

LIBETPAN_EXPORT
int mailhttp_backend_is_available(enum mailhttp_backend backend);
LIBETPAN_EXPORT
int mailhttp_set_backend(enum mailhttp_backend backend);
LIBETPAN_EXPORT
enum mailhttp_backend mailhttp_get_backend(void);
LIBETPAN_EXPORT
int mailhttp_transport_new_default(struct mailhttp_transport ** result);
LIBETPAN_EXPORT
int mailhttp_transport_new_curl(struct mailhttp_transport ** result);
LIBETPAN_EXPORT
int mailhttp_transport_new_nsurlsession(struct mailhttp_transport ** result);

#ifdef __cplusplus
}
#endif

#endif
