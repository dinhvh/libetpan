/*
 * libEtPan! -- a mail stuff library
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailactivesync_http.h"
#include "mailhttp.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static char * dup_string(const char * value)
{
  if (value == NULL)
    return NULL;

  return strdup(value);
}

struct mailactivesync_http_header *
mailactivesync_http_header_new(const char * name, const char * value)
{
  struct mailactivesync_http_header * header;

  if ((name == NULL) || (value == NULL))
    return NULL;

  header = malloc(sizeof(* header));
  if (header == NULL)
    return NULL;

  header->name = dup_string(name);
  header->value = dup_string(value);
  if ((header->name == NULL) || (header->value == NULL)) {
    mailactivesync_http_header_free(header);
    return NULL;
  }

  return header;
}

void mailactivesync_http_header_free(
    struct mailactivesync_http_header * header)
{
  if (header == NULL)
    return;

  free(header->name);
  free(header->value);
  free(header);
}

static void free_header_item(void * value, void * data)
{
  (void) data;
  mailactivesync_http_header_free(value);
}

struct mailactivesync_http_request *
mailactivesync_http_request_new(const char * method, const char * url)
{
  struct mailactivesync_http_request * request;

  if ((method == NULL) || (url == NULL))
    return NULL;

  request = malloc(sizeof(* request));
  if (request == NULL)
    return NULL;

  request->method = dup_string(method);
  request->url = dup_string(url);
  request->headers = clist_new();
  request->body = NULL;
  request->body_len = 0;
  request->timeout = 60;

  if ((request->method == NULL) || (request->url == NULL) ||
      (request->headers == NULL)) {
    mailactivesync_http_request_free(request);
    return NULL;
  }

  return request;
}

void mailactivesync_http_request_free(
    struct mailactivesync_http_request * request)
{
  if (request == NULL)
    return;

  free(request->method);
  free(request->url);
  if (request->headers != NULL) {
    clist_foreach(request->headers, free_header_item, NULL);
    clist_free(request->headers);
  }
  free(request->body);
  free(request);
}

int mailactivesync_http_request_add_header(
    struct mailactivesync_http_request * request,
    const char * name,
    const char * value)
{
  struct mailactivesync_http_header * header;

  if ((request == NULL) || (request->headers == NULL) ||
      (name == NULL) || (value == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  header = mailactivesync_http_header_new(name, value);
  if (header == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  if (clist_append(request->headers, header) < 0) {
    mailactivesync_http_header_free(header);
    return MAILACTIVESYNC_ERROR_MEMORY;
  }

  return MAILACTIVESYNC_NO_ERROR;
}

int mailactivesync_http_request_set_body(
    struct mailactivesync_http_request * request,
    const unsigned char * body,
    size_t body_len)
{
  unsigned char * new_body;

  if (request == NULL)
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  if ((body_len > 0) && (body == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  new_body = NULL;
  if (body_len > 0) {
    new_body = malloc(body_len);
    if (new_body == NULL)
      return MAILACTIVESYNC_ERROR_MEMORY;
    memcpy(new_body, body, body_len);
  }

  free(request->body);
  request->body = new_body;
  request->body_len = body_len;

  return MAILACTIVESYNC_NO_ERROR;
}

struct mailactivesync_http_response *
mailactivesync_http_response_new(int status_code)
{
  struct mailactivesync_http_response * response;

  response = malloc(sizeof(* response));
  if (response == NULL)
    return NULL;

  response->status_code = status_code;
  response->headers = clist_new();
  response->body = NULL;
  response->body_len = 0;

  if (response->headers == NULL) {
    mailactivesync_http_response_free(response);
    return NULL;
  }

  return response;
}

void mailactivesync_http_response_free(
    struct mailactivesync_http_response * response)
{
  if (response == NULL)
    return;

  if (response->headers != NULL) {
    clist_foreach(response->headers, free_header_item, NULL);
    clist_free(response->headers);
  }
  free(response->body);
  free(response);
}

int mailactivesync_http_response_add_header(
    struct mailactivesync_http_response * response,
    const char * name,
    const char * value)
{
  struct mailactivesync_http_header * header;

  if ((response == NULL) || (response->headers == NULL) ||
      (name == NULL) || (value == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  header = mailactivesync_http_header_new(name, value);
  if (header == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  if (clist_append(response->headers, header) < 0) {
    mailactivesync_http_header_free(header);
    return MAILACTIVESYNC_ERROR_MEMORY;
  }

  return MAILACTIVESYNC_NO_ERROR;
}

const char * mailactivesync_http_response_header_value(
    struct mailactivesync_http_response * response,
    const char * name)
{
  clistiter * cur;

  if ((response == NULL) || (response->headers == NULL) || (name == NULL))
    return NULL;

  for (cur = clist_begin(response->headers); cur != NULL;
      cur = clist_next(cur)) {
    struct mailactivesync_http_header * header;

    header = clist_content(cur);
    if ((header != NULL) && (strcasecmp(header->name, name) == 0))
      return header->value;
  }

  return NULL;
}

void mailactivesync_http_transport_free(
    struct mailactivesync_http_transport * transport)
{
  if (transport == NULL)
    return;

  if (transport->free != NULL)
    transport->free(transport);
  free(transport);
}

int mailactivesync_http_perform(
    struct mailactivesync_http_transport * transport,
    struct mailactivesync_http_request * request,
    struct mailactivesync_http_response ** response)
{
  if ((transport == NULL) || (transport->perform == NULL) ||
      (request == NULL) || (response == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * response = NULL;
  return transport->perform(transport, request, response);
}

struct activesync_mailhttp_context {
  struct mailhttp_transport * transport;
};

static int activesync_error_from_mailhttp(int r)
{
  switch (r) {
  case MAILHTTP_NO_ERROR: return MAILACTIVESYNC_NO_ERROR;
  case MAILHTTP_ERROR_MEMORY: return MAILACTIVESYNC_ERROR_MEMORY;
  case MAILHTTP_ERROR_BAD_STATE:
  case MAILHTTP_ERROR_BAD_URL: return MAILACTIVESYNC_ERROR_BAD_STATE;
  case MAILHTTP_ERROR_TLS: return MAILACTIVESYNC_ERROR_SSL;
  case MAILHTTP_ERROR_UNAVAILABLE: return MAILACTIVESYNC_ERROR_HTTP_UNAVAILABLE;
  default: return MAILACTIVESYNC_ERROR_HTTP;
  }
}

static int activesync_mailhttp_perform(
    struct mailactivesync_http_transport * transport,
    struct mailactivesync_http_request * request,
    struct mailactivesync_http_response ** result)
{
  struct activesync_mailhttp_context * context = transport->context;
  struct mailhttp_request * common_request;
  struct mailhttp_response * common_response = NULL;
  struct mailactivesync_http_response * response = NULL;
  clistiter * cur;
  int r;

  common_request = mailhttp_request_new(request->method, request->url);
  if (common_request == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;
  common_request->timeout = request->timeout;
  common_request->max_redirects = 3;
  for (cur = clist_begin(request->headers); cur != NULL;
      cur = clist_next(cur)) {
    struct mailactivesync_http_header * header = clist_content(cur);
    r = mailhttp_request_add_header(common_request, header->name,
        header->value);
    if (r != MAILHTTP_NO_ERROR)
      goto common_error;
  }
  r = mailhttp_request_set_body(common_request, request->body,
      request->body_len);
  if (r != MAILHTTP_NO_ERROR)
    goto common_error;
  r = mailhttp_perform(context->transport, common_request, &common_response);
  if (r != MAILHTTP_NO_ERROR)
    goto common_error;
  response = mailactivesync_http_response_new(common_response->status_code);
  if (response == NULL) {
    r = MAILHTTP_ERROR_MEMORY;
    goto common_error;
  }
  for (cur = clist_begin(common_response->headers); cur != NULL;
      cur = clist_next(cur)) {
    struct mailhttp_header * header = clist_content(cur);
    if (mailactivesync_http_response_add_header(response, header->name,
        header->value) != MAILACTIVESYNC_NO_ERROR) {
      r = MAILHTTP_ERROR_MEMORY;
      goto common_error;
    }
  }
  if (common_response->body_len != 0) {
    response->body = malloc(common_response->body_len);
    if (response->body == NULL) {
      r = MAILHTTP_ERROR_MEMORY;
      goto common_error;
    }
    memcpy(response->body, common_response->body,
        common_response->body_len);
    response->body_len = common_response->body_len;
  }
  mailhttp_response_free(common_response);
  mailhttp_request_free(common_request);
  * result = response;
  return MAILACTIVESYNC_NO_ERROR;

common_error:
  mailactivesync_http_response_free(response);
  mailhttp_response_free(common_response);
  mailhttp_request_free(common_request);
  return activesync_error_from_mailhttp(r);
}

static void activesync_mailhttp_free(
    struct mailactivesync_http_transport * transport)
{
  struct activesync_mailhttp_context * context = transport->context;
  if (context == NULL)
    return;
  mailhttp_transport_free(context->transport);
  free(context);
}

int mailactivesync_http_transport_new_default(
    struct mailactivesync_http_transport ** result)
{
  struct mailhttp_transport * common;
  struct mailactivesync_http_transport * transport;
  struct activesync_mailhttp_context * context;
  int r;

  if (result == NULL)
    return MAILACTIVESYNC_ERROR_BAD_STATE;
  * result = NULL;
  r = mailhttp_transport_new_default(&common);
  if (r != MAILHTTP_NO_ERROR)
    return activesync_error_from_mailhttp(r);
  context = malloc(sizeof(* context));
  transport = malloc(sizeof(* transport));
  if ((context == NULL) || (transport == NULL)) {
    free(context);
    free(transport);
    mailhttp_transport_free(common);
    return MAILACTIVESYNC_ERROR_MEMORY;
  }
  context->transport = common;
  transport->context = context;
  transport->perform = activesync_mailhttp_perform;
  transport->free = activesync_mailhttp_free;
  * result = transport;
  return MAILACTIVESYNC_NO_ERROR;
}

int mailactivesync_http_transport_new_curl(
    struct mailactivesync_http_transport ** result)
{
  struct mailhttp_transport * common;
  struct mailactivesync_http_transport * transport;
  struct activesync_mailhttp_context * context;
  int r;

  if (result == NULL)
    return MAILACTIVESYNC_ERROR_BAD_STATE;
  * result = NULL;
  r = mailhttp_transport_new_curl(&common);
  if (r != MAILHTTP_NO_ERROR)
    return activesync_error_from_mailhttp(r);
  context = malloc(sizeof(* context));
  transport = malloc(sizeof(* transport));
  if ((context == NULL) || (transport == NULL)) {
    free(context);
    free(transport);
    mailhttp_transport_free(common);
    return MAILACTIVESYNC_ERROR_MEMORY;
  }
  context->transport = common;
  transport->context = context;
  transport->perform = activesync_mailhttp_perform;
  transport->free = activesync_mailhttp_free;
  * result = transport;
  return MAILACTIVESYNC_NO_ERROR;
}
