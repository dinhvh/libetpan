/*
 * libEtPan! -- a mail stuff library
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailjmap_http.h"
#include "mailhttp.h"

#include <stdlib.h>
#include <string.h>
#include <strings.h>

struct jmap_mailhttp_context {
  struct mailhttp_transport * transport;
};

static int jmap_error_from_mailhttp(int r)
{
  switch (r) {
  case MAILHTTP_NO_ERROR: return MAILJMAP_NO_ERROR;
  case MAILHTTP_ERROR_MEMORY: return MAILJMAP_ERROR_MEMORY;
  case MAILHTTP_ERROR_BAD_STATE:
  case MAILHTTP_ERROR_BAD_URL: return MAILJMAP_ERROR_BAD_STATE;
  default: return MAILJMAP_ERROR_HTTP;
  }
}

static int jmap_mailhttp_perform(struct mailjmap_http_transport * transport,
    struct mailjmap_http_request * request,
    struct mailjmap_http_response ** result)
{
  struct jmap_mailhttp_context * context = transport->context;
  struct mailhttp_request * common_request;
  struct mailhttp_response * common_response = NULL;
  struct mailjmap_http_response * response = NULL;
  clistiter * cur;
  int r;

  common_request = mailhttp_request_new(request->method, request->url);
  if (common_request == NULL)
    return MAILJMAP_ERROR_MEMORY;
  common_request->timeout = request->timeout;
  for (cur = clist_begin(request->headers); cur != NULL;
      cur = clist_next(cur)) {
    struct mailjmap_http_header * header = clist_content(cur);
    r = mailhttp_request_add_header(common_request, header->name,
        header->value);
    if (r != MAILHTTP_NO_ERROR)
      goto common_error;
  }
  if (request->content_type != NULL) {
    r = mailhttp_request_add_header(common_request, "Content-Type",
        request->content_type);
    if (r != MAILHTTP_NO_ERROR)
      goto common_error;
  }
  if (request->accept_type != NULL) {
    r = mailhttp_request_add_header(common_request, "Accept",
        request->accept_type);
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
  response = mailjmap_http_response_new(common_response->status_code);
  if (response == NULL) {
    r = MAILHTTP_ERROR_MEMORY;
    goto common_error;
  }
  for (cur = clist_begin(common_response->headers); cur != NULL;
      cur = clist_next(cur)) {
    struct mailhttp_header * header = clist_content(cur);
    if (mailjmap_http_response_add_header(response, header->name,
        header->value) != MAILJMAP_NO_ERROR) {
      r = MAILHTTP_ERROR_MEMORY;
      goto common_error;
    }
  }
  if (mailjmap_http_response_set_final_url(response,
      common_response->final_url) != MAILJMAP_NO_ERROR) {
    r = MAILHTTP_ERROR_MEMORY;
    goto common_error;
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
  return MAILJMAP_NO_ERROR;

common_error:
  mailjmap_http_response_free(response);
  mailhttp_response_free(common_response);
  mailhttp_request_free(common_request);
  return jmap_error_from_mailhttp(r);
}

static void jmap_mailhttp_free(struct mailjmap_http_transport * transport)
{
  struct jmap_mailhttp_context * context = transport->context;
  if (context == NULL)
    return;
  mailhttp_transport_free(context->transport);
  free(context);
}

static int jmap_transport_wrap(struct mailhttp_transport * common,
    struct mailjmap_http_transport ** result)
{
  struct mailjmap_http_transport * transport;
  struct jmap_mailhttp_context * context;

  context = malloc(sizeof(* context));
  transport = malloc(sizeof(* transport));
  if ((context == NULL) || (transport == NULL)) {
    free(context);
    free(transport);
    mailhttp_transport_free(common);
    return MAILJMAP_ERROR_MEMORY;
  }
  context->transport = common;
  transport->context = context;
  transport->perform = jmap_mailhttp_perform;
  transport->free = jmap_mailhttp_free;
  * result = transport;
  return MAILJMAP_NO_ERROR;
}

static char * dup_string(const char * value)
{
  if (value == NULL)
    return NULL;

  return strdup(value);
}

static int replace_string(char ** target, const char * value)
{
  char * copy;

  if (target == NULL)
    return MAILJMAP_ERROR_BAD_STATE;

  copy = NULL;
  if (value != NULL) {
    copy = dup_string(value);
    if (copy == NULL)
      return MAILJMAP_ERROR_MEMORY;
  }

  free(* target);
  * target = copy;
  return MAILJMAP_NO_ERROR;
}

struct mailjmap_http_header *
mailjmap_http_header_new(const char * name, const char * value)
{
  struct mailjmap_http_header * header;

  if ((name == NULL) || (value == NULL))
    return NULL;

  header = malloc(sizeof(* header));
  if (header == NULL)
    return NULL;

  header->name = dup_string(name);
  header->value = dup_string(value);
  if ((header->name == NULL) || (header->value == NULL)) {
    mailjmap_http_header_free(header);
    return NULL;
  }

  return header;
}

void mailjmap_http_header_free(struct mailjmap_http_header * header)
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
  mailjmap_http_header_free(value);
}

struct mailjmap_http_request *
mailjmap_http_request_new(const char * method, const char * url)
{
  struct mailjmap_http_request * request;

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
  request->content_type = NULL;
  request->accept_type = NULL;
  request->timeout = 60;
  if ((request->method == NULL) || (request->url == NULL) ||
      (request->headers == NULL)) {
    mailjmap_http_request_free(request);
    return NULL;
  }

  return request;
}

void mailjmap_http_request_free(struct mailjmap_http_request * request)
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
  free(request->content_type);
  free(request->accept_type);
  free(request);
}

int mailjmap_http_request_add_header(
    struct mailjmap_http_request * request,
    const char * name,
    const char * value)
{
  struct mailjmap_http_header * header;

  if ((request == NULL) || (request->headers == NULL) ||
      (name == NULL) || (value == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  header = mailjmap_http_header_new(name, value);
  if (header == NULL)
    return MAILJMAP_ERROR_MEMORY;

  if (clist_append(request->headers, header) < 0) {
    mailjmap_http_header_free(header);
    return MAILJMAP_ERROR_MEMORY;
  }

  return MAILJMAP_NO_ERROR;
}

int mailjmap_http_request_set_body(
    struct mailjmap_http_request * request,
    const unsigned char * body,
    size_t body_len)
{
  unsigned char * new_body;

  if (request == NULL)
    return MAILJMAP_ERROR_BAD_STATE;

  if ((body_len > 0) && (body == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  new_body = NULL;
  if (body_len > 0) {
    new_body = malloc(body_len);
    if (new_body == NULL)
      return MAILJMAP_ERROR_MEMORY;
    memcpy(new_body, body, body_len);
  }

  free(request->body);
  request->body = new_body;
  request->body_len = body_len;
  return MAILJMAP_NO_ERROR;
}

int mailjmap_http_request_set_content_type(
    struct mailjmap_http_request * request,
    const char * content_type)
{
  if (request == NULL)
    return MAILJMAP_ERROR_BAD_STATE;

  return replace_string(&request->content_type, content_type);
}

int mailjmap_http_request_set_accept_type(
    struct mailjmap_http_request * request,
    const char * accept_type)
{
  if (request == NULL)
    return MAILJMAP_ERROR_BAD_STATE;

  return replace_string(&request->accept_type, accept_type);
}

struct mailjmap_http_response *
mailjmap_http_response_new(int status_code)
{
  struct mailjmap_http_response * response;

  response = malloc(sizeof(* response));
  if (response == NULL)
    return NULL;

  response->status_code = status_code;
  response->headers = clist_new();
  response->body = NULL;
  response->body_len = 0;
  response->final_url = NULL;
  if (response->headers == NULL) {
    mailjmap_http_response_free(response);
    return NULL;
  }

  return response;
}

void mailjmap_http_response_free(struct mailjmap_http_response * response)
{
  if (response == NULL)
    return;

  if (response->headers != NULL) {
    clist_foreach(response->headers, free_header_item, NULL);
    clist_free(response->headers);
  }
  free(response->body);
  free(response->final_url);
  free(response);
}

int mailjmap_http_response_add_header(
    struct mailjmap_http_response * response,
    const char * name,
    const char * value)
{
  struct mailjmap_http_header * header;

  if ((response == NULL) || (response->headers == NULL) ||
      (name == NULL) || (value == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  header = mailjmap_http_header_new(name, value);
  if (header == NULL)
    return MAILJMAP_ERROR_MEMORY;

  if (clist_append(response->headers, header) < 0) {
    mailjmap_http_header_free(header);
    return MAILJMAP_ERROR_MEMORY;
  }

  return MAILJMAP_NO_ERROR;
}

const char * mailjmap_http_response_header_value(
    struct mailjmap_http_response * response,
    const char * name)
{
  clistiter * cur;

  if ((response == NULL) || (response->headers == NULL) || (name == NULL))
    return NULL;

  for (cur = clist_begin(response->headers); cur != NULL;
      cur = clist_next(cur)) {
    struct mailjmap_http_header * header;

    header = clist_content(cur);
    if ((header != NULL) && (strcasecmp(header->name, name) == 0))
      return header->value;
  }

  return NULL;
}

int mailjmap_http_response_set_final_url(
    struct mailjmap_http_response * response,
    const char * final_url)
{
  if (response == NULL)
    return MAILJMAP_ERROR_BAD_STATE;

  return replace_string(&response->final_url, final_url);
}

void mailjmap_http_transport_free(
    struct mailjmap_http_transport * transport)
{
  if (transport == NULL)
    return;

  if (transport->free != NULL)
    transport->free(transport);
  free(transport);
}

int mailjmap_http_perform(
    struct mailjmap_http_transport * transport,
    struct mailjmap_http_request * request,
    struct mailjmap_http_response ** response)
{
  if ((transport == NULL) || (transport->perform == NULL) ||
      (request == NULL) || (response == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  * response = NULL;
  return transport->perform(transport, request, response);
}

int mailjmap_http_transport_new_default(
    struct mailjmap_http_transport ** result)
{
  if (result == NULL)
    return MAILJMAP_ERROR_BAD_STATE;

  * result = NULL;

  {
    struct mailhttp_transport * common;
    int r = mailhttp_transport_new_default(&common);
    if (r != MAILHTTP_NO_ERROR)
      return jmap_error_from_mailhttp(r);
    return jmap_transport_wrap(common, result);
  }
}

int mailjmap_http_transport_new_curl(
    struct mailjmap_http_transport ** result)
{
  struct mailhttp_transport * common;
  int r;
  if (result == NULL)
    return MAILJMAP_ERROR_BAD_STATE;
  * result = NULL;
  r = mailhttp_transport_new_curl(&common);
  if (r != MAILHTTP_NO_ERROR)
    return jmap_error_from_mailhttp(r);
  return jmap_transport_wrap(common, result);
}

int mailjmap_http_transport_new_nsurlsession(
    struct mailjmap_http_transport ** result)
{
  struct mailhttp_transport * common;
  int r;
  if (result == NULL)
    return MAILJMAP_ERROR_BAD_STATE;
  * result = NULL;
  r = mailhttp_transport_new_nsurlsession(&common);
  if (r != MAILHTTP_NO_ERROR)
    return jmap_error_from_mailhttp(r);
  return jmap_transport_wrap(common, result);
}
