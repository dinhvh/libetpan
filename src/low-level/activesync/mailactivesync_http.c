/*
 * libEtPan! -- a mail stuff library
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailactivesync_http.h"

#include <libetpan/mmapstring.h>

#ifdef HAVE_CURL
#include <curl/curl.h>
#endif

#include <ctype.h>
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

#ifdef HAVE_CURL

static size_t curl_write_body(char * ptr, size_t size, size_t nmemb,
    void * userdata)
{
  struct mailactivesync_http_response * response;
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
  struct mailactivesync_http_response * response;
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

  if (mailactivesync_http_response_add_header(response, name, value) !=
      MAILACTIVESYNC_NO_ERROR) {
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
    return MAILACTIVESYNC_NO_ERROR;
  case CURLE_OUT_OF_MEMORY:
    return MAILACTIVESYNC_ERROR_MEMORY;
  case CURLE_SSL_CONNECT_ERROR:
#ifdef CURLE_PEER_FAILED_VERIFICATION
  case CURLE_PEER_FAILED_VERIFICATION:
#endif
    return MAILACTIVESYNC_ERROR_SSL;
  default:
    return MAILACTIVESYNC_ERROR_HTTP;
  }
}

static int curl_perform(struct mailactivesync_http_transport * transport,
    struct mailactivesync_http_request * request,
    struct mailactivesync_http_response ** result)
{
  CURL * curl;
  CURLcode curl_res;
  struct curl_slist * headers;
  struct mailactivesync_http_response * response;
  clistiter * cur;
  long status_code;
  int r;

  (void) transport;

  curl = curl_easy_init();
  if (curl == NULL)
    return MAILACTIVESYNC_ERROR_HTTP_UNAVAILABLE;

  headers = NULL;
  response = mailactivesync_http_response_new(0);
  if (response == NULL) {
    curl_easy_cleanup(curl);
    return MAILACTIVESYNC_ERROR_MEMORY;
  }

  for (cur = clist_begin(request->headers); cur != NULL;
      cur = clist_next(cur)) {
    struct mailactivesync_http_header * header;
    MMAPString * header_line;

    header = clist_content(cur);
    header_line = mmap_string_sized_new(strlen(header->name) +
        strlen(header->value) + 3);
    if (header_line == NULL) {
      r = MAILACTIVESYNC_ERROR_MEMORY;
      goto err;
    }
    if ((mmap_string_append(header_line, header->name) == NULL) ||
        (mmap_string_append(header_line, ": ") == NULL) ||
        (mmap_string_append(header_line, header->value) == NULL)) {
      mmap_string_free(header_line);
      r = MAILACTIVESYNC_ERROR_MEMORY;
      goto err;
    }
    headers = curl_slist_append(headers, header_line->str);
    mmap_string_free(header_line);
    if (headers == NULL) {
      r = MAILACTIVESYNC_ERROR_MEMORY;
      goto err;
    }
  }

  curl_easy_setopt(curl, CURLOPT_URL, request->url);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_body);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, curl_write_header);
  curl_easy_setopt(curl, CURLOPT_HEADERDATA, response);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
  curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 3);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, request->timeout);
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "libEtPan ActiveSync");

  if (strcasecmp(request->method, "OPTIONS") == 0) {
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "OPTIONS");
  }
  else if (strcasecmp(request->method, "POST") == 0) {
    curl_easy_setopt(curl, CURLOPT_POST, 1);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request->body);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long) request->body_len);
  }
  else {
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, request->method);
  }

  curl_res = curl_easy_perform(curl);
  if (curl_res != CURLE_OK) {
    r = curl_error_convert(curl_res);
    goto err;
  }

  status_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
  response->status_code = (int) status_code;

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);

  * result = response;
  return MAILACTIVESYNC_NO_ERROR;

 err:
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  mailactivesync_http_response_free(response);
  return r;
}

int mailactivesync_http_transport_new_curl(
    struct mailactivesync_http_transport ** result)
{
  struct mailactivesync_http_transport * transport;

  if (result == NULL)
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;

  transport = malloc(sizeof(* transport));
  if (transport == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  transport->context = NULL;
  transport->perform = curl_perform;
  transport->free = NULL;

  * result = transport;
  return MAILACTIVESYNC_NO_ERROR;
}

#else

int mailactivesync_http_transport_new_curl(
    struct mailactivesync_http_transport ** result)
{
  if (result == NULL)
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  return MAILACTIVESYNC_ERROR_HTTP_UNAVAILABLE;
}

#endif
