/*
 * libEtPan! -- a mail stuff library
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailjmap_http.h"

#ifdef HAVE_CURL
#include <curl/curl.h>
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#ifdef HAVE_CURL

static size_t curl_write_body(char * ptr, size_t size, size_t nmemb,
    void * userdata)
{
  struct mailjmap_http_response * response;
  unsigned char * new_body;
  size_t len;

  response = userdata;
  len = size * nmemb;
  if (len == 0)
    return 0;

  new_body = realloc(response->body, response->body_len + len);
  if (new_body == NULL)
    return 0;

  memcpy(new_body + response->body_len, ptr, len);
  response->body = new_body;
  response->body_len += len;
  return len;
}

static char * copy_trimmed(const char * str, size_t len)
{
  size_t start;
  char * result;

  start = 0;
  while ((start < len) && isspace((unsigned char) str[start]))
    start ++;
  while ((len > start) && isspace((unsigned char) str[len - 1]))
    len --;

  result = malloc(len - start + 1);
  if (result == NULL)
    return NULL;

  memcpy(result, str + start, len - start);
  result[len - start] = '\0';
  return result;
}

static size_t curl_write_header(char * ptr, size_t size, size_t nmemb,
    void * userdata)
{
  struct mailjmap_http_response * response;
  char * colon;
  char * name;
  char * value;
  size_t len;

  response = userdata;
  len = size * nmemb;
  if (len == 0)
    return 0;

  colon = memchr(ptr, ':', len);
  if (colon == NULL)
    return len;

  name = copy_trimmed(ptr, (size_t) (colon - ptr));
  value = copy_trimmed(colon + 1, len - (size_t) (colon - ptr) - 1);
  if ((name == NULL) || (value == NULL)) {
    free(name);
    free(value);
    return 0;
  }

  if (mailjmap_http_response_add_header(response, name, value) !=
      MAILJMAP_NO_ERROR) {
    free(name);
    free(value);
    return 0;
  }

  free(name);
  free(value);
  return len;
}

static int curl_error_convert(CURLcode curl_res)
{
  switch (curl_res) {
  case CURLE_OK:
    return MAILJMAP_NO_ERROR;
  case CURLE_OUT_OF_MEMORY:
    return MAILJMAP_ERROR_MEMORY;
  case CURLE_SSL_CONNECT_ERROR:
#ifdef CURLE_PEER_FAILED_VERIFICATION
  case CURLE_PEER_FAILED_VERIFICATION:
#endif
    return MAILJMAP_ERROR_STREAM;
  case CURLE_OPERATION_TIMEDOUT:
    return MAILJMAP_ERROR_HTTP;
  default:
    return MAILJMAP_ERROR_HTTP;
  }
}

static int debug_enabled(void)
{
  const char * value;

  value = getenv("LIBETPAN_JMAP_HTTP_DEBUG");
  return (value != NULL) && (value[0] != '\0') && (strcmp(value, "0") != 0);
}

static void debug_header_line(const char * prefix,
    const char * name, const char * value)
{
  if (strcasecmp(name, "Authorization") == 0)
    fprintf(stderr, "jmap-http: %s Authorization: <redacted>\n", prefix);
  else
    fprintf(stderr, "jmap-http: %s %s: %s\n", prefix, name, value);
}

static void debug_request(struct mailjmap_http_request * request)
{
  clistiter * cur;

  if (!debug_enabled() || (request == NULL))
    return;

  fprintf(stderr, "jmap-http: > %s %s\n", request->method, request->url);
  if (request->content_type != NULL)
    debug_header_line(">", "Content-Type", request->content_type);
  if (request->accept_type != NULL)
    debug_header_line(">", "Accept", request->accept_type);
  for (cur = request->headers != NULL ? clist_begin(request->headers) : NULL;
      cur != NULL; cur = clist_next(cur)) {
    struct mailjmap_http_header * header;

    header = clist_content(cur);
    if (header != NULL)
      debug_header_line(">", header->name, header->value);
  }
  if (request->body_len > 0)
    fprintf(stderr, "jmap-http: > body: <%lu bytes>\n",
        (unsigned long) request->body_len);
}

static void debug_response(struct mailjmap_http_response * response)
{
  if (!debug_enabled() || (response == NULL))
    return;

  fprintf(stderr, "jmap-http: < status=%d body_bytes=%lu final_url=%s\n",
      response->status_code, (unsigned long) response->body_len,
      response->final_url != NULL ? response->final_url : "");
}

static int append_header(struct curl_slist ** headers,
    const char * name, const char * value)
{
  struct curl_slist * new_headers;
  char * header_line;
  size_t header_line_len;

  header_line_len = strlen(name) + strlen(value) + 3;
  header_line = malloc(header_line_len);
  if (header_line == NULL)
    return MAILJMAP_ERROR_MEMORY;

  snprintf(header_line, header_line_len, "%s: %s", name, value);
  new_headers = curl_slist_append(* headers, header_line);
  free(header_line);
  if (new_headers == NULL)
    return MAILJMAP_ERROR_MEMORY;

  * headers = new_headers;
  return MAILJMAP_NO_ERROR;
}

static int add_request_headers(struct curl_slist ** headers,
    struct mailjmap_http_request * request)
{
  clistiter * cur;
  int r;

  if (request->content_type != NULL) {
    r = append_header(headers, "Content-Type", request->content_type);
    if (r != MAILJMAP_NO_ERROR)
      return r;
  }
  if (request->accept_type != NULL) {
    r = append_header(headers, "Accept", request->accept_type);
    if (r != MAILJMAP_NO_ERROR)
      return r;
  }

  for (cur = clist_begin(request->headers); cur != NULL;
      cur = clist_next(cur)) {
    struct mailjmap_http_header * header;

    header = clist_content(cur);
    if (header == NULL)
      continue;
    r = append_header(headers, header->name, header->value);
    if (r != MAILJMAP_NO_ERROR)
      return r;
  }

  return MAILJMAP_NO_ERROR;
}

static int curl_perform(struct mailjmap_http_transport * transport,
    struct mailjmap_http_request * request,
    struct mailjmap_http_response ** result)
{
  CURL * curl;
  CURLcode curl_res;
  struct curl_slist * headers;
  struct mailjmap_http_response * response;
  long status_code;
  char * final_url;
  int r;

  (void) transport;

  curl = curl_easy_init();
  if (curl == NULL)
    return MAILJMAP_ERROR_HTTP;

  headers = NULL;
  response = mailjmap_http_response_new(0);
  if (response == NULL) {
    curl_easy_cleanup(curl);
    return MAILJMAP_ERROR_MEMORY;
  }

  r = add_request_headers(&headers, request);
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  debug_request(request);

  curl_easy_setopt(curl, CURLOPT_URL, request->url);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_body);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, curl_write_header);
  curl_easy_setopt(curl, CURLOPT_HEADERDATA, response);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, request->timeout);
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);

  if (strcasecmp(request->method, "GET") == 0) {
  }
  else if (strcasecmp(request->method, "POST") == 0) {
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request->body);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long) request->body_len);
  }
  else {
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, request->method);
    if (request->body_len > 0) {
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request->body);
      curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long) request->body_len);
    }
  }

  curl_res = curl_easy_perform(curl);
  if (curl_res != CURLE_OK) {
    r = curl_error_convert(curl_res);
    goto err;
  }

  status_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
  response->status_code = (int) status_code;

  final_url = NULL;
  curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &final_url);
  if (final_url != NULL) {
    r = mailjmap_http_response_set_final_url(response, final_url);
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }

  debug_response(response);

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);

  * result = response;
  return MAILJMAP_NO_ERROR;

 err:
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  mailjmap_http_response_free(response);
  return r;
}

int mailjmap_http_transport_new_curl(
    struct mailjmap_http_transport ** result)
{
  struct mailjmap_http_transport * transport;

  if (result == NULL)
    return MAILJMAP_ERROR_BAD_STATE;

  transport = malloc(sizeof(* transport));
  if (transport == NULL)
    return MAILJMAP_ERROR_MEMORY;

  transport->context = NULL;
  transport->perform = curl_perform;
  transport->free = NULL;

  * result = transport;
  return MAILJMAP_NO_ERROR;
}

#else

int mailjmap_http_transport_new_curl(
    struct mailjmap_http_transport ** result)
{
  if (result == NULL)
    return MAILJMAP_ERROR_BAD_STATE;

  * result = NULL;
  return MAILJMAP_ERROR_HTTP;
}

#endif
