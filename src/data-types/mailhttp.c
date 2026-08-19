/*
 * libEtPan! -- a mail stuff library
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "mailhttp.h"

#include <stdlib.h>
#include <string.h>
#ifndef WIN32
#include <strings.h>
#endif

static enum mailhttp_backend selected_backend =
#if defined(__APPLE__)
  MAILHTTP_BACKEND_NSURLSESSION;
#else
  MAILHTTP_BACKEND_CURL;
#endif

static char * mailhttp_strdup(const char * value)
{
  if (value == NULL)
    return NULL;
  return strdup(value);
}

static int replace_string(char ** target, const char * value)
{
  char * copy = NULL;

  if (target == NULL)
    return MAILHTTP_ERROR_BAD_STATE;
  if (value != NULL) {
    copy = mailhttp_strdup(value);
    if (copy == NULL)
      return MAILHTTP_ERROR_MEMORY;
  }
  free(* target);
  * target = copy;
  return MAILHTTP_NO_ERROR;
}

struct mailhttp_header * mailhttp_header_new(const char * name,
    const char * value)
{
  struct mailhttp_header * header;

  if ((name == NULL) || (value == NULL))
    return NULL;
  header = calloc(1, sizeof(* header));
  if (header == NULL)
    return NULL;
  header->name = mailhttp_strdup(name);
  header->value = mailhttp_strdup(value);
  if ((header->name == NULL) || (header->value == NULL)) {
    mailhttp_header_free(header);
    return NULL;
  }
  return header;
}

void mailhttp_header_free(struct mailhttp_header * header)
{
  if (header == NULL)
    return;
  free(header->name);
  free(header->value);
  free(header);
}

static void free_header(void * value, void * context)
{
  (void) context;
  mailhttp_header_free(value);
}

struct mailhttp_request * mailhttp_request_new(const char * method,
    const char * url)
{
  struct mailhttp_request * request;

  if ((method == NULL) || (url == NULL))
    return NULL;
  request = calloc(1, sizeof(* request));
  if (request == NULL)
    return NULL;
  request->method = mailhttp_strdup(method);
  request->url = mailhttp_strdup(url);
  request->headers = clist_new();
  request->timeout = 60;
  request->follow_redirects = 1;
  request->max_redirects = 5;
  if ((request->method == NULL) || (request->url == NULL) ||
      (request->headers == NULL)) {
    mailhttp_request_free(request);
    return NULL;
  }
  return request;
}

void mailhttp_request_free(struct mailhttp_request * request)
{
  if (request == NULL)
    return;
  free(request->method);
  free(request->url);
  if (request->headers != NULL) {
    clist_foreach(request->headers, free_header, NULL);
    clist_free(request->headers);
  }
  free(request->body);
  free(request);
}

int mailhttp_request_add_header(struct mailhttp_request * request,
    const char * name, const char * value)
{
  struct mailhttp_header * header;

  if ((request == NULL) || (request->headers == NULL) ||
      (name == NULL) || (value == NULL))
    return MAILHTTP_ERROR_BAD_STATE;
  header = mailhttp_header_new(name, value);
  if (header == NULL)
    return MAILHTTP_ERROR_MEMORY;
  if (clist_append(request->headers, header) < 0) {
    mailhttp_header_free(header);
    return MAILHTTP_ERROR_MEMORY;
  }
  return MAILHTTP_NO_ERROR;
}

int mailhttp_request_set_body(struct mailhttp_request * request,
    const void * body, size_t body_len)
{
  unsigned char * copy = NULL;

  if ((request == NULL) || ((body == NULL) && (body_len != 0)))
    return MAILHTTP_ERROR_BAD_STATE;
  if (body_len != 0) {
    copy = malloc(body_len);
    if (copy == NULL)
      return MAILHTTP_ERROR_MEMORY;
    memcpy(copy, body, body_len);
  }
  free(request->body);
  request->body = copy;
  request->body_len = body_len;
  return MAILHTTP_NO_ERROR;
}

void mailhttp_request_set_body_sink(struct mailhttp_request * request,
    mailhttp_body_sink sink, void * context)
{
  if (request == NULL)
    return;
  request->body_sink = sink;
  request->body_sink_context = context;
}

struct mailhttp_response * mailhttp_response_new(int status_code)
{
  struct mailhttp_response * response;

  response = calloc(1, sizeof(* response));
  if (response == NULL)
    return NULL;
  response->status_code = status_code;
  response->headers = clist_new();
  if (response->headers == NULL) {
    mailhttp_response_free(response);
    return NULL;
  }
  return response;
}

void mailhttp_response_free(struct mailhttp_response * response)
{
  if (response == NULL)
    return;
  if (response->headers != NULL) {
    clist_foreach(response->headers, free_header, NULL);
    clist_free(response->headers);
  }
  free(response->body);
  free(response->final_url);
  free(response);
}

int mailhttp_response_add_header(struct mailhttp_response * response,
    const char * name, const char * value)
{
  struct mailhttp_header * header;

  if ((response == NULL) || (response->headers == NULL) ||
      (name == NULL) || (value == NULL))
    return MAILHTTP_ERROR_BAD_STATE;
  header = mailhttp_header_new(name, value);
  if (header == NULL)
    return MAILHTTP_ERROR_MEMORY;
  if (clist_append(response->headers, header) < 0) {
    mailhttp_header_free(header);
    return MAILHTTP_ERROR_MEMORY;
  }
  return MAILHTTP_NO_ERROR;
}

const char * mailhttp_response_header_value(
    const struct mailhttp_response * response, const char * name)
{
  clistiter * cur;

  if ((response == NULL) || (response->headers == NULL) || (name == NULL))
    return NULL;
  for (cur = clist_begin(response->headers); cur != NULL;
      cur = clist_next(cur)) {
    struct mailhttp_header * header = clist_content(cur);
    if ((header != NULL) && (strcasecmp(header->name, name) == 0))
      return header->value;
  }
  return NULL;
}

int mailhttp_response_set_final_url(struct mailhttp_response * response,
    const char * final_url)
{
  if (response == NULL)
    return MAILHTTP_ERROR_BAD_STATE;
  return replace_string(&response->final_url, final_url);
}

int mailhttp_response_append_body(struct mailhttp_response * response,
    const void * data, size_t length)
{
  unsigned char * body;

  if ((response == NULL) || ((data == NULL) && (length != 0)))
    return MAILHTTP_ERROR_BAD_STATE;
  if (length == 0)
    return MAILHTTP_NO_ERROR;
  if (response->body_len > ((size_t) -1) - length)
    return MAILHTTP_ERROR_MEMORY;
  body = realloc(response->body, response->body_len + length);
  if (body == NULL)
    return MAILHTTP_ERROR_MEMORY;
  memcpy(body + response->body_len, data, length);
  response->body = body;
  response->body_len += length;
  return MAILHTTP_NO_ERROR;
}

struct mailhttp_transport * mailhttp_transport_new(void * data,
    const struct mailhttp_transport_driver * driver,
    enum mailhttp_backend backend)
{
  struct mailhttp_transport * transport;

  if ((driver == NULL) || (driver->perform == NULL))
    return NULL;
  transport = malloc(sizeof(* transport));
  if (transport == NULL)
    return NULL;
  transport->data = data;
  transport->driver = driver;
  transport->backend = backend;
  return transport;
}

void mailhttp_transport_free(struct mailhttp_transport * transport)
{
  if (transport == NULL)
    return;
  if ((transport->driver != NULL) && (transport->driver->free != NULL))
    transport->driver->free(transport);
  free(transport);
}

int mailhttp_perform(struct mailhttp_transport * transport,
    struct mailhttp_request * request,
    struct mailhttp_response ** response)
{
  if ((transport == NULL) || (transport->driver == NULL) ||
      (transport->driver->perform == NULL) || (request == NULL) ||
      (response == NULL))
    return MAILHTTP_ERROR_BAD_STATE;
  * response = NULL;
  return transport->driver->perform(transport, request, response);
}

int mailhttp_backend_is_available(enum mailhttp_backend backend)
{
  switch (backend) {
  case MAILHTTP_BACKEND_CURL:
#if defined(HAVE_CURL) && !defined(LIBETPAN_DISABLE_CURL)
    return 1;
#else
    return 0;
#endif
  case MAILHTTP_BACKEND_NSURLSESSION:
#if defined(HAVE_NSURLSESSION) || defined(__APPLE__)
    return 1;
#else
    return 0;
#endif
  default:
    return 0;
  }
}

int mailhttp_set_backend(enum mailhttp_backend backend)
{
  if (!mailhttp_backend_is_available(backend))
    return -1;
  selected_backend = backend;
  return 0;
}

enum mailhttp_backend mailhttp_get_backend(void)
{
  return selected_backend;
}

int mailhttp_transport_new_default(struct mailhttp_transport ** result)
{
  if (result == NULL)
    return MAILHTTP_ERROR_BAD_STATE;
  * result = NULL;
  switch (mailhttp_get_backend()) {
  case MAILHTTP_BACKEND_CURL:
    return mailhttp_transport_new_curl(result);
  case MAILHTTP_BACKEND_NSURLSESSION:
    return mailhttp_transport_new_nsurlsession(result);
  default:
    return MAILHTTP_ERROR_UNAVAILABLE;
  }
}

#if !defined(HAVE_CURL) || defined(LIBETPAN_DISABLE_CURL)
int mailhttp_transport_new_curl(struct mailhttp_transport ** result)
{
  if (result == NULL)
    return MAILHTTP_ERROR_BAD_STATE;
  * result = NULL;
  return MAILHTTP_ERROR_UNAVAILABLE;
}
#endif

#if !defined(HAVE_NSURLSESSION) && !defined(__APPLE__)
int mailhttp_transport_new_nsurlsession(struct mailhttp_transport ** result)
{
  if (result == NULL)
    return MAILHTTP_ERROR_BAD_STATE;
  * result = NULL;
  return MAILHTTP_ERROR_UNAVAILABLE;
}
#endif
