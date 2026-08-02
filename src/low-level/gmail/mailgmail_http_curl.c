/*
 * libEtPan! -- a mail stuff library
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailgmail_http.h"

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
  struct mailgmail_http_response * response;
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
  struct mailgmail_http_response * response;
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

  if (mailgmail_http_response_add_header(response, name, value) !=
      MAILGMAIL_NO_ERROR) {
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
    return MAILGMAIL_NO_ERROR;
  case CURLE_OUT_OF_MEMORY:
    return MAILGMAIL_ERROR_MEMORY;
  case CURLE_SSL_CONNECT_ERROR:
#ifdef CURLE_PEER_FAILED_VERIFICATION
  case CURLE_PEER_FAILED_VERIFICATION:
#endif
    return MAILGMAIL_ERROR_SSL;
  case CURLE_OPERATION_TIMEDOUT:
    return MAILGMAIL_ERROR_HTTP;
  default:
    return MAILGMAIL_ERROR_HTTP;
  }
}

static int debug_enabled(void)
{
  const char * value;

  value = getenv("LIBETPAN_GMAIL_HTTP_DEBUG");
  return (value != NULL) && (value[0] != '\0') && (strcmp(value, "0") != 0);
}

static void debug_write_line(const char * prefix,
    const char * data, size_t size)
{
  if ((size >= 14) && (strncasecmp(data, "Authorization:", 14) == 0)) {
    fprintf(stderr, "gmail-http: %s Authorization: <redacted>\n", prefix);
    return;
  }

  fprintf(stderr, "gmail-http: %s %.*s\n", prefix, (int) size, data);
}

static void debug_write(const char * prefix, char * data, size_t size)
{
  size_t start;
  size_t i;

  start = 0;
  for (i = 0; i < size; i ++) {
    if (data[i] == '\n') {
      size_t end;

      end = i;
      if ((end > start) && (data[end - 1] == '\r'))
        end --;
      debug_write_line(prefix, data + start, end - start);
      start = i + 1;
    }
  }

  if (start < size)
    debug_write_line(prefix, data + start, size - start);
}

static int gmail_curl_debug_callback(CURL * curl, curl_infotype type,
    char * data, size_t size, void * userdata)
{
  (void) curl;
  (void) userdata;

  switch (type) {
  case CURLINFO_HEADER_IN:
    debug_write("<", data, size);
    break;
  default:
    break;
  }

  return 0;
}

static void debug_request(struct mailgmail_http_request * request)
{
  clistiter * cur;

  if (!debug_enabled() || (request == NULL))
    return;

  fprintf(stderr, "gmail-http: > %s %s\n", request->method, request->url);
  for (cur = request->headers != NULL ? clist_begin(request->headers) : NULL;
      cur != NULL; cur = clist_next(cur)) {
    struct mailgmail_http_header * header;

    header = clist_content(cur);
    if (strcasecmp(header->name, "Authorization") == 0)
      fprintf(stderr, "gmail-http: > Authorization: <redacted>\n");
    else
      fprintf(stderr, "gmail-http: > %s: %s\n", header->name, header->value);
  }
  if (request->body_len > 0)
    fprintf(stderr, "gmail-http: > body: <%lu bytes>\n",
        (unsigned long) request->body_len);
}

static void debug_response(struct mailgmail_http_response * response)
{
  if (!debug_enabled() || (response == NULL))
    return;

  fprintf(stderr, "gmail-http: < status=%d body_bytes=%lu\n",
      response->status_code, (unsigned long) response->body_len);
}

static int curl_perform(struct mailgmail_http_transport * transport,
    struct mailgmail_http_request * request,
    struct mailgmail_http_response ** result)
{
  CURL * curl;
  CURLcode curl_res;
  struct curl_slist * headers;
  struct mailgmail_http_response * response;
  clistiter * cur;
  long status_code;
  int r;

  (void) transport;

  curl = curl_easy_init();
  if (curl == NULL)
    return MAILGMAIL_ERROR_HTTP_UNAVAILABLE;

  headers = NULL;
  response = mailgmail_http_response_new(0);
  if (response == NULL) {
    curl_easy_cleanup(curl);
    return MAILGMAIL_ERROR_MEMORY;
  }

  for (cur = clist_begin(request->headers); cur != NULL;
      cur = clist_next(cur)) {
    struct mailgmail_http_header * header;
    char * header_line;
    size_t header_line_len;

    header = clist_content(cur);
    header_line_len = strlen(header->name) + strlen(header->value) + 3;
    header_line = malloc(header_line_len);
    if (header_line == NULL) {
      r = MAILGMAIL_ERROR_MEMORY;
      goto err;
    }
    snprintf(header_line, header_line_len, "%s: %s", header->name,
        header->value);
    headers = curl_slist_append(headers, header_line);
    free(header_line);
    if (headers == NULL) {
      r = MAILGMAIL_ERROR_MEMORY;
      goto err;
    }
  }

  debug_request(request);

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
  curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
  if (debug_enabled()) {
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
    curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, gmail_curl_debug_callback);
  }

  if (strcasecmp(request->method, "GET") == 0) {
  }
  else if (strcasecmp(request->method, "POST") == 0) {
    curl_easy_setopt(curl, CURLOPT_POST, 1);
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
  debug_response(response);

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);

  * result = response;
  return MAILGMAIL_NO_ERROR;

 err:
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  mailgmail_http_response_free(response);
  return r;
}

int mailgmail_http_transport_new_curl(
    struct mailgmail_http_transport ** result)
{
  struct mailgmail_http_transport * transport;

  if (result == NULL)
    return MAILGMAIL_ERROR_BAD_STATE;

  * result = NULL;

  transport = malloc(sizeof(* transport));
  if (transport == NULL)
    return MAILGMAIL_ERROR_MEMORY;

  transport->context = NULL;
  transport->perform = curl_perform;
  transport->free = NULL;

  * result = transport;
  return MAILGMAIL_NO_ERROR;
}

#else

int mailgmail_http_transport_new_curl(
    struct mailgmail_http_transport ** result)
{
  if (result == NULL)
    return MAILGMAIL_ERROR_BAD_STATE;

  * result = NULL;
  return MAILGMAIL_ERROR_HTTP_UNAVAILABLE;
}

#endif
