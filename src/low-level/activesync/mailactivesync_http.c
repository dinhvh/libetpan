/*
 * libEtPan! -- a mail stuff library
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailactivesync_http.h"
#include "mailactivesync_wbxml.h"

#include <libetpan/mmapstring.h>

#ifdef HAVE_CURL
#include <curl/curl.h>
#endif

#include <ctype.h>
#include <stdio.h>
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

static int curl_debug_enabled(void)
{
  const char * value;

  value = getenv("LIBETPAN_ACTIVESYNC_HTTP_DEBUG");
  return (value != NULL) && (value[0] != '\0') && (strcmp(value, "0") != 0);
}

static int wbxml_debug_enabled(void)
{
  const char * value;

  value = getenv("LIBETPAN_ACTIVESYNC_WBXML_DEBUG");
  return (value != NULL) && (value[0] != '\0') && (strcmp(value, "0") != 0);
}

static int debug_xml_append_escaped(MMAPString * buffer, const char * value)
{
  const char * cur;

  if (value == NULL)
    return MAILACTIVESYNC_NO_ERROR;

  for (cur = value; * cur != '\0'; cur ++) {
    const char * escaped;

    escaped = NULL;
    switch (* cur) {
    case '&':
      escaped = "&amp;";
      break;
    case '<':
      escaped = "&lt;";
      break;
    case '>':
      escaped = "&gt;";
      break;
    case '"':
      escaped = "&quot;";
      break;
    case '\'':
      escaped = "&apos;";
      break;
    default:
      break;
    }

    if (escaped != NULL) {
      if (mmap_string_append(buffer, escaped) == NULL)
        return MAILACTIVESYNC_ERROR_MEMORY;
    }
    else if (mmap_string_append_c(buffer, * cur) == NULL)
      return MAILACTIVESYNC_ERROR_MEMORY;
  }

  return MAILACTIVESYNC_NO_ERROR;
}

static int debug_wbxml_dump_node(MMAPString * buffer,
    struct mailactivesync_wbxml_node * node, unsigned int depth)
{
  clistiter * cur;
  const char * name;
  unsigned int i;

  if ((buffer == NULL) || (node == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  name = node->name != NULL ? node->name : "Unknown";
  for (i = 0; i < depth; i ++) {
    if (mmap_string_append(buffer, "  ") == NULL)
      return MAILACTIVESYNC_ERROR_MEMORY;
  }
  if ((mmap_string_append_c(buffer, '<') == NULL) ||
      (mmap_string_append(buffer, name) == NULL) ||
      (mmap_string_append_c(buffer, '>') == NULL))
    return MAILACTIVESYNC_ERROR_MEMORY;

  if (node->text != NULL) {
    int r;

    r = debug_xml_append_escaped(buffer, node->text);
    if (r != MAILACTIVESYNC_NO_ERROR)
      return r;
  }
  if (node->opaque != NULL) {
    char opaque[64];

    snprintf(opaque, sizeof(opaque), "<opaque bytes=\"%lu\"/>",
        (unsigned long) node->opaque_len);
    if (mmap_string_append(buffer, opaque) == NULL)
      return MAILACTIVESYNC_ERROR_MEMORY;
  }

  if ((node->children != NULL) && (clist_count(node->children) > 0)) {
    if (mmap_string_append_c(buffer, '\n') == NULL)
      return MAILACTIVESYNC_ERROR_MEMORY;
    for (cur = clist_begin(node->children); cur != NULL;
        cur = clist_next(cur)) {
      int r;

      r = debug_wbxml_dump_node(buffer, clist_content(cur), depth + 1);
      if (r != MAILACTIVESYNC_NO_ERROR)
        return r;
    }
    for (i = 0; i < depth; i ++) {
      if (mmap_string_append(buffer, "  ") == NULL)
        return MAILACTIVESYNC_ERROR_MEMORY;
    }
  }

  if ((mmap_string_append(buffer, "</") == NULL) ||
      (mmap_string_append(buffer, name) == NULL) ||
      (mmap_string_append(buffer, ">\n") == NULL))
    return MAILACTIVESYNC_ERROR_MEMORY;

  return MAILACTIVESYNC_NO_ERROR;
}

static int debug_summary_interesting_node(
    struct mailactivesync_wbxml_node * node)
{
  if ((node == NULL) || (node->name == NULL) || (node->text == NULL))
    return 0;

  return (strstr(node->name, ":Status") != NULL) ||
      (strstr(node->name, ":SyncKey") != NULL) ||
      (strstr(node->name, ":PolicyKey") != NULL) ||
      (strstr(node->name, ":Estimate") != NULL);
}

static int debug_wbxml_append_summary_node(MMAPString * buffer,
    struct mailactivesync_wbxml_node * node)
{
  clistiter * cur;

  if ((buffer == NULL) || (node == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  if (debug_summary_interesting_node(node)) {
    int r;

    if ((mmap_string_append_c(buffer, ' ') == NULL) ||
        (mmap_string_append(buffer, node->name) == NULL) ||
        (mmap_string_append_c(buffer, '=') == NULL))
      return MAILACTIVESYNC_ERROR_MEMORY;
    if (strstr(node->name, ":PolicyKey") != NULL) {
      if (mmap_string_append(buffer, "<present>") == NULL)
        return MAILACTIVESYNC_ERROR_MEMORY;
    }
    else {
      r = debug_xml_append_escaped(buffer, node->text);
      if (r != MAILACTIVESYNC_NO_ERROR)
        return r;
    }
  }

  for (cur = node->children != NULL ? clist_begin(node->children) : NULL;
      cur != NULL; cur = clist_next(cur)) {
    int r;

    r = debug_wbxml_append_summary_node(buffer, clist_content(cur));
    if (r != MAILACTIVESYNC_NO_ERROR)
      return r;
  }

  return MAILACTIVESYNC_NO_ERROR;
}

static void debug_wbxml_summary(const char * prefix,
    struct mailactivesync_wbxml_document * document)
{
  MMAPString * buffer;
  int r;

  if ((document == NULL) || (document->root == NULL))
    return;

  buffer = mmap_string_sized_new(256);
  if (buffer == NULL)
    return;

  if ((mmap_string_append(buffer, "root=") == NULL) ||
      (mmap_string_append(buffer,
          document->root->name != NULL ? document->root->name : "Unknown") ==
          NULL)) {
    mmap_string_free(buffer);
    return;
  }

  r = debug_wbxml_append_summary_node(buffer, document->root);
  if (r == MAILACTIVESYNC_NO_ERROR)
    fprintf(stderr, "activesync-wbxml: %s summary %s\n", prefix,
        buffer->str);

  mmap_string_free(buffer);
}

static void debug_wbxml_body(const char * prefix, const unsigned char * body,
    size_t body_len)
{
  struct mailactivesync_wbxml_document * document;
  MMAPString * buffer;
  int r;

  if (!wbxml_debug_enabled() || (body == NULL) || (body_len == 0))
    return;

  document = NULL;
  r = mailactivesync_wbxml_decode(body, body_len, &document);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    fprintf(stderr, "activesync-wbxml: %s decode failed: %d\n", prefix, r);
    return;
  }

  buffer = mmap_string_sized_new(1024);
  if (buffer == NULL) {
    mailactivesync_wbxml_document_free(document);
    return;
  }

  r = debug_wbxml_dump_node(buffer, document->root, 0);
  if (r == MAILACTIVESYNC_NO_ERROR) {
    debug_wbxml_summary(prefix, document);
    fprintf(stderr, "activesync-wbxml: %s\n%s", prefix, buffer->str);
  }
  else
    fprintf(stderr, "activesync-wbxml: %s dump failed: %d\n", prefix, r);

  mmap_string_free(buffer);
  mailactivesync_wbxml_document_free(document);
}

static void curl_debug_write_line(const char * prefix,
    const char * data, size_t size)
{
  if ((size >= 14) && (strncasecmp(data, "Authorization:", 14) == 0)) {
    fprintf(stderr, "activesync-http: %s Authorization: <redacted>\n",
        prefix);
    return;
  }

  fprintf(stderr, "activesync-http: %s %.*s\n", prefix, (int) size, data);
}

static void curl_debug_write(const char * prefix, char * data, size_t size)
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
      curl_debug_write_line(prefix, data + start, end - start);
      start = i + 1;
    }
  }

  if (start < size)
    curl_debug_write_line(prefix, data + start, size - start);
}

static int active_sync_curl_debug_callback(CURL * curl, curl_infotype type,
    char * data, size_t size, void * userdata)
{
  (void) curl;
  (void) userdata;

  switch (type) {
  case CURLINFO_HEADER_IN:
    curl_debug_write("<", data, size);
    break;
  default:
    break;
  }

  return 0;
}

static void debug_request(struct mailactivesync_http_request * request)
{
  clistiter * cur;

  if (!curl_debug_enabled() || (request == NULL))
    return;

  fprintf(stderr, "activesync-http: > %s %s\n", request->method,
      request->url);
  for (cur = request->headers != NULL ? clist_begin(request->headers) : NULL;
      cur != NULL; cur = clist_next(cur)) {
    struct mailactivesync_http_header * header;

    header = clist_content(cur);
    if (strcasecmp(header->name, "Authorization") == 0)
      fprintf(stderr, "activesync-http: > Authorization: <redacted>\n");
    else
      fprintf(stderr, "activesync-http: > %s: %s\n",
          header->name, header->value);
  }
  if (request->body_len > 0)
    fprintf(stderr, "activesync-http: > body: <%lu bytes>\n",
        (unsigned long) request->body_len);
  debug_wbxml_body(">", request->body, request->body_len);
}

static int buffer_is_mostly_text(const unsigned char * data, size_t len)
{
  size_t i;
  size_t text_count;

  if (len == 0)
    return 1;

  text_count = 0;
  for (i = 0; i < len; i ++) {
    if ((data[i] == '\r') || (data[i] == '\n') || (data[i] == '\t') ||
        isprint(data[i]))
      text_count ++;
  }

  return text_count >= ((len * 3) / 4);
}

static void debug_response_body(struct mailactivesync_http_response * response)
{
  size_t i;
  size_t len;

  if ((response == NULL) || (response->body == NULL) ||
      (response->body_len == 0))
    return;

  len = response->body_len;
  if (len > 512)
    len = 512;

  if (buffer_is_mostly_text(response->body, len)) {
    fprintf(stderr, "activesync-http: < body: ");
    for (i = 0; i < len; i ++) {
      unsigned char ch;

      ch = response->body[i];
      if ((ch == '\r') || (ch == '\n'))
        fputc(' ', stderr);
      else if ((ch == '\t') || isprint(ch))
        fputc(ch, stderr);
      else
        fputc('.', stderr);
    }
    if (response->body_len > len)
      fprintf(stderr, "...");
    fprintf(stderr, "\n");
  }
  else {
    fprintf(stderr, "activesync-http: < body-hex:");
    for (i = 0; i < len; i ++)
      fprintf(stderr, " %02x", response->body[i]);
    if (response->body_len > len)
      fprintf(stderr, " ...");
    fprintf(stderr, "\n");
  }
}

static void debug_response_summary(
    struct mailactivesync_http_response * response)
{
  if (!curl_debug_enabled() || (response == NULL))
    return;

  fprintf(stderr, "activesync-http: < status=%d body_bytes=%lu\n",
      response->status_code, (unsigned long) response->body_len);
  debug_wbxml_body("<", response->body, response->body_len);
  if (response->status_code >= 400)
    debug_response_body(response);
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
  if (curl_debug_enabled()) {
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
    curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION,
        active_sync_curl_debug_callback);
  }

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
  debug_response_summary(response);

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
