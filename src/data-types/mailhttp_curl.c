/*
 * libEtPan! -- a mail stuff library
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "mailhttp.h"

#if defined(HAVE_CURL) && !defined(LIBETPAN_DISABLE_CURL)
#include <curl/curl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct curl_context {
  struct mailhttp_request * request;
  struct mailhttp_response * response;
  int callback_error;
};

static int curl_error(CURLcode code)
{
  switch (code) {
  case CURLE_OK:
    return MAILHTTP_NO_ERROR;
  case CURLE_OUT_OF_MEMORY:
    return MAILHTTP_ERROR_MEMORY;
  case CURLE_URL_MALFORMAT:
    return MAILHTTP_ERROR_BAD_URL;
  case CURLE_OPERATION_TIMEDOUT:
    return MAILHTTP_ERROR_TIMEOUT;
  case CURLE_COULDNT_RESOLVE_PROXY:
  case CURLE_COULDNT_RESOLVE_HOST:
    return MAILHTTP_ERROR_RESOLVE;
  case CURLE_COULDNT_CONNECT:
    return MAILHTTP_ERROR_CONNECT;
  case CURLE_SSL_CONNECT_ERROR:
#ifdef CURLE_PEER_FAILED_VERIFICATION
  case CURLE_PEER_FAILED_VERIFICATION:
#endif
    return MAILHTTP_ERROR_TLS;
  case CURLE_ABORTED_BY_CALLBACK:
    return MAILHTTP_ERROR_CANCELLED;
  case CURLE_UNSUPPORTED_PROTOCOL:
    return MAILHTTP_ERROR_PROTOCOL;
  default:
    return MAILHTTP_ERROR_IO;
  }
}

static size_t write_body(char * data, size_t size, size_t count, void * context)
{
  struct curl_context * ctx = context;
  size_t length = size * count;
  int r;

  if (ctx->request->body_sink != NULL) {
    r = ctx->request->body_sink(data, length,
        ctx->request->body_sink_context);
    if (r != 0) {
      ctx->callback_error = MAILHTTP_ERROR_BODY_SINK;
      return 0;
    }
  }
  else {
    r = mailhttp_response_append_body(ctx->response, data, length);
    if (r != MAILHTTP_NO_ERROR) {
      ctx->callback_error = r;
      return 0;
    }
  }
  return length;
}

static size_t write_header(char * data, size_t size, size_t count,
    void * context)
{
  struct curl_context * ctx = context;
  size_t length = size * count;
  const char * colon;
  const char * value;
  const char * end;
  size_t name_len;
  size_t value_len;
  char * name;
  char * value_copy;
  int r;

  colon = memchr(data, ':', length);
  if (colon == NULL)
    return length;
  name_len = (size_t) (colon - data);
  value = colon + 1;
  end = data + length;
  while ((value < end) && ((* value == ' ') || (* value == '\t')))
    value ++;
  while ((end > value) && ((end[-1] == '\r') || (end[-1] == '\n')))
    end --;
  value_len = (size_t) (end - value);

  name = malloc(name_len + 1);
  value_copy = malloc(value_len + 1);
  if ((name == NULL) || (value_copy == NULL)) {
    free(name);
    free(value_copy);
    ctx->callback_error = MAILHTTP_ERROR_MEMORY;
    return 0;
  }
  memcpy(name, data, name_len);
  name[name_len] = '\0';
  memcpy(value_copy, value, value_len);
  value_copy[value_len] = '\0';
  r = mailhttp_response_add_header(ctx->response, name, value_copy);
  free(name);
  free(value_copy);
  if (r != MAILHTTP_NO_ERROR) {
    ctx->callback_error = r;
    return 0;
  }
  return length;
}

static int curl_perform(struct mailhttp_transport * transport,
    struct mailhttp_request * request,
    struct mailhttp_response ** result)
{
  CURL * curl;
  CURLcode curl_result;
  struct curl_slist * headers = NULL;
  clistiter * cur;
  struct mailhttp_response * response;
  struct curl_context context;
  long status_code = 0;
  char * final_url = NULL;
  int r = MAILHTTP_ERROR_IO;

  (void) transport;
  if ((request == NULL) || (result == NULL))
    return MAILHTTP_ERROR_BAD_STATE;
  * result = NULL;
  curl = curl_easy_init();
  if (curl == NULL)
    return MAILHTTP_ERROR_MEMORY;
  response = mailhttp_response_new(0);
  if (response == NULL) {
    curl_easy_cleanup(curl);
    return MAILHTTP_ERROR_MEMORY;
  }
  context.request = request;
  context.response = response;
  context.callback_error = MAILHTTP_NO_ERROR;

  for (cur = clist_begin(request->headers); cur != NULL;
      cur = clist_next(cur)) {
    struct mailhttp_header * header = clist_content(cur);
    size_t length = strlen(header->name) + strlen(header->value) + 3;
    char * line = malloc(length);
    struct curl_slist * new_headers;
    if (line == NULL) {
      r = MAILHTTP_ERROR_MEMORY;
      goto cleanup;
    }
    snprintf(line, length, "%s: %s", header->name, header->value);
    new_headers = curl_slist_append(headers, line);
    free(line);
    if (new_headers == NULL) {
      r = MAILHTTP_ERROR_MEMORY;
      goto cleanup;
    }
    headers = new_headers;
  }

  curl_easy_setopt(curl, CURLOPT_URL, request->url);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_body);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &context);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_header);
  curl_easy_setopt(curl, CURLOPT_HEADERDATA, &context);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION,
      request->follow_redirects ? 1L : 0L);
  curl_easy_setopt(curl, CURLOPT_MAXREDIRS, (long) request->max_redirects);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long) request->timeout);
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);

  if (strcmp(request->method, "GET") != 0) {
    if (strcmp(request->method, "POST") == 0)
      curl_easy_setopt(curl, CURLOPT_POST, 1L);
    else
      curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, request->method);
  }
  if ((request->body != NULL) || (request->body_len != 0)) {
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request->body);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE,
        (curl_off_t) request->body_len);
  }

  curl_result = curl_easy_perform(curl);
  if (context.callback_error != MAILHTTP_NO_ERROR) {
    r = context.callback_error;
    goto cleanup;
  }
  if (curl_result != CURLE_OK) {
    r = curl_error(curl_result);
    goto cleanup;
  }
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
  curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &final_url);
  response->status_code = (int) status_code;
  r = mailhttp_response_set_final_url(response, final_url);
  if (r != MAILHTTP_NO_ERROR)
    goto cleanup;
  * result = response;
  response = NULL;
  r = MAILHTTP_NO_ERROR;

cleanup:
  mailhttp_response_free(response);
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  return r;
}

static const struct mailhttp_transport_driver curl_driver = {
  curl_perform,
  NULL
};

int mailhttp_transport_new_curl(struct mailhttp_transport ** result)
{
  if (result == NULL)
    return MAILHTTP_ERROR_BAD_STATE;
  * result = mailhttp_transport_new(NULL, &curl_driver,
      MAILHTTP_BACKEND_CURL);
  if (* result == NULL)
    return MAILHTTP_ERROR_MEMORY;
  return MAILHTTP_NO_ERROR;
}

#endif /* HAVE_CURL && !LIBETPAN_DISABLE_CURL */
