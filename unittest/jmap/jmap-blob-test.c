/*
 * libEtPan! -- a mail stuff library
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include <libetpan/mailjmap.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct fake_context {
  int calls;
  char * method;
  char * url;
  char * accept_type;
  char * content_type;
  char * authorization;
  unsigned char * body;
  size_t body_len;
  time_t timeout;
  char * upload_response_body;
  size_t upload_response_body_len;
  unsigned char * download_response_body;
  size_t download_response_body_len;
};

static const char session_json[] =
    "{"
    "\"capabilities\":{\"urn:ietf:params:jmap:core\":{}},"
    "\"accounts\":{},"
    "\"primaryAccounts\":{},"
    "\"apiUrl\":\"https://example.com/jmap/api\","
    "\"downloadUrl\":\"https://example.com/download/{accountId}/{blobId}/{name}?accept={type}\","
    "\"uploadUrl\":\"https://example.com/upload/{accountId}?type={type}\","
    "\"sessionState\":\"state1\""
    "}";

static int check(int condition, const char * message)
{
  if (!condition) {
    fprintf(stderr, "%s\n", message);
    return 0;
  }

  return 1;
}

static int str_equal(const char * left, const char * right)
{
  return (left != NULL) && (right != NULL) && (strcmp(left, right) == 0);
}

static char * dup_string(const char * value)
{
  if (value == NULL)
    return NULL;

  return strdup(value);
}

static char * read_fixture_file(const char * directory, const char * name,
    size_t * result_len)
{
  char path[512];
  char * data;
  FILE * f;
  long len;
  size_t read_len;
  const char * prefixes[] = {
    "unittest/jmap/data/",
    "../unittest/jmap/data/",
    "jmap/data/"
  };
  size_t i;

  for (i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); i ++) {
    snprintf(path, sizeof(path), "%s%s/%s", prefixes[i], directory, name);
    f = fopen(path, "rb");
    if (f != NULL)
      break;
  }
  if (f == NULL)
    return NULL;

  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return NULL;
  }
  len = ftell(f);
  if (len < 0) {
    fclose(f);
    return NULL;
  }
  if (fseek(f, 0, SEEK_SET) != 0) {
    fclose(f);
    return NULL;
  }

  data = malloc((size_t) len + 1);
  if (data == NULL) {
    fclose(f);
    return NULL;
  }

  read_len = fread(data, 1, (size_t) len, f);
  fclose(f);
  if (read_len != (size_t) len) {
    free(data);
    return NULL;
  }

  data[read_len] = '\0';
  if (result_len != NULL)
    * result_len = read_len;
  return data;
}

static int remember_string(char ** target, const char * value)
{
  char * copy;

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

static void fake_context_clear(struct fake_context * context)
{
  if (context == NULL)
    return;

  free(context->method);
  free(context->url);
  free(context->accept_type);
  free(context->content_type);
  free(context->authorization);
  free(context->body);
  free(context->upload_response_body);
  free(context->download_response_body);
  memset(context, 0, sizeof(* context));
}

static int capture_request(struct fake_context * context,
    struct mailjmap_http_request * request)
{
  clistiter * cur;
  int r;

  r = remember_string(&context->method, request->method);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  r = remember_string(&context->url, request->url);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  r = remember_string(&context->accept_type, request->accept_type);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  r = remember_string(&context->content_type, request->content_type);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  for (cur = clist_begin(request->headers); cur != NULL;
      cur = clist_next(cur)) {
    struct mailjmap_http_header * header;

    header = clist_content(cur);
    if ((header != NULL) && (strcmp(header->name, "Authorization") == 0)) {
      r = remember_string(&context->authorization, header->value);
      if (r != MAILJMAP_NO_ERROR)
        return r;
    }
  }

  free(context->body);
  context->body = NULL;
  context->body_len = request->body_len;
  if (request->body_len > 0) {
    context->body = malloc(request->body_len);
    if (context->body == NULL)
      return MAILJMAP_ERROR_MEMORY;
    memcpy(context->body, request->body, request->body_len);
  }

  context->timeout = request->timeout;
  return MAILJMAP_NO_ERROR;
}

static int set_response_body(struct mailjmap_http_response * response,
    const unsigned char * body, size_t body_len)
{
  response->body = malloc(body_len);
  if (response->body == NULL)
    return MAILJMAP_ERROR_MEMORY;

  memcpy(response->body, body, body_len);
  response->body_len = body_len;
  return MAILJMAP_NO_ERROR;
}

static int fake_perform(struct mailjmap_http_transport * transport,
    struct mailjmap_http_request * request,
    struct mailjmap_http_response ** result)
{
  struct fake_context * context;
  struct mailjmap_http_response * response;
  int r;

  context = transport->context;
  context->calls ++;

  r = capture_request(context, request);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  response = mailjmap_http_response_new(200);
  if (response == NULL)
    return MAILJMAP_ERROR_MEMORY;

  if (context->calls == 1) {
    r = set_response_body(response, (const unsigned char *) session_json,
        strlen(session_json));
  }
  else if (context->calls == 2) {
    r = set_response_body(response,
        (const unsigned char *) context->upload_response_body,
        context->upload_response_body_len);
  }
  else {
    r = set_response_body(response, context->download_response_body,
        context->download_response_body_len);
  }
  if (r != MAILJMAP_NO_ERROR) {
    mailjmap_http_response_free(response);
    return r;
  }

  * result = response;
  return MAILJMAP_NO_ERROR;
}

static struct mailjmap_http_transport * fake_transport_new(
    struct fake_context * context)
{
  struct mailjmap_http_transport * transport;

  transport = malloc(sizeof(* transport));
  if (transport == NULL)
    return NULL;

  transport->context = context;
  transport->perform = fake_perform;
  transport->free = NULL;
  return transport;
}

static int test_upload_and_download(void)
{
  mailjmap * client;
  struct fake_context context;
  struct mailjmap_session * session_object;
  struct mailjmap_blob_upload * upload;
  char * download;
  size_t download_len;
  const char upload_body[] = "raw email";
  int r;
  int ok;

  memset(&context, 0, sizeof(context));
  client = NULL;
  session_object = NULL;
  upload = NULL;
  download = NULL;
  download_len = 0;
  ok = 0;

  context.upload_response_body = read_fixture_file("blob",
      "upload-response.json", &context.upload_response_body_len);
  if (!check(context.upload_response_body != NULL,
      "upload response fixture load failed"))
    goto cleanup;
  context.download_response_body = (unsigned char *) read_fixture_file("blob",
      "download-message.eml", &context.download_response_body_len);
  if (!check(context.download_response_body != NULL,
      "download response fixture load failed"))
    goto cleanup;

  client = mailjmap_new(0, NULL);
  if (!check(client != NULL, "client allocation failed"))
    goto cleanup;
  r = mailjmap_set_http_transport(client, fake_transport_new(&context));
  if (!check(r == MAILJMAP_NO_ERROR, "transport set failed"))
    goto cleanup;
  r = mailjmap_connect(client, "https://example.com/jmap/session");
  if (!check(r == MAILJMAP_NO_ERROR, "connect failed"))
    goto cleanup;
  r = mailjmap_login_oauth2(client, "user@example.com", "token");
  if (!check(r == MAILJMAP_NO_ERROR, "login failed"))
    goto cleanup;
  r = mailjmap_set_timeout(client, 11);
  if (!check(r == MAILJMAP_NO_ERROR, "timeout set failed"))
    goto cleanup;
  r = mailjmap_get_session(client, &session_object);
  if (!check(r == MAILJMAP_NO_ERROR, "get session failed"))
    goto cleanup;

  r = mailjmap_upload(client, "acc 1", "message/rfc822", upload_body,
      strlen(upload_body), &upload);
  if (!check(r == MAILJMAP_NO_ERROR, "upload failed"))
    goto cleanup;

  if (!check(str_equal(context.method, "POST"), "upload method mismatch"))
    goto cleanup;
  if (!check(str_equal(context.url,
      "https://example.com/upload/acc%201?type=message%2Frfc822"),
      "upload URL mismatch"))
    goto cleanup;
  if (!check(str_equal(context.content_type, "message/rfc822"),
      "upload content type mismatch"))
    goto cleanup;
  if (!check(str_equal(context.accept_type, "application/json"),
      "upload accept type mismatch"))
    goto cleanup;
  if (!check(str_equal(context.authorization, "Bearer token"),
      "auth header mismatch"))
    goto cleanup;
  if (!check(context.body_len == strlen(upload_body),
      "upload body length mismatch"))
    goto cleanup;
  if (!check(memcmp(context.body, upload_body, strlen(upload_body)) == 0,
      "upload body mismatch"))
    goto cleanup;

  r = mailjmap_download(client, "acc 1", "blob/1", "message 1.eml",
      "message/rfc822", &download, &download_len);
  if (!check(r == MAILJMAP_NO_ERROR, "download failed"))
    goto cleanup;

  ok = check(context.calls == 3, "call count mismatch") &&
      check(str_equal(context.method, "GET"), "download method mismatch") &&
      check(str_equal(context.url,
          "https://example.com/download/acc%201/blob%2F1/message%201.eml?accept=message%2Frfc822"),
          "download URL mismatch") &&
      check(str_equal(context.accept_type, "message/rfc822"),
          "download accept type mismatch") &&
      check(str_equal(upload->account_id, "acc 1"),
          "upload accountId mismatch") &&
      check(str_equal(upload->blob_id, "blob/1"),
          "upload blobId mismatch") &&
      check(str_equal(upload->type, "message/rfc822"),
          "upload type mismatch") &&
      check(str_equal(upload->name, "message.eml"),
          "upload name mismatch") &&
      check(upload->size == 9, "upload size mismatch") &&
      check(download_len == context.download_response_body_len,
          "download body length mismatch") &&
      check(memcmp(download, context.download_response_body,
          context.download_response_body_len) == 0,
          "download body mismatch") &&
      check(mailjmap_get_last_http_status(client) == 200,
          "last HTTP status mismatch");

 cleanup:
  free(download);
  mailjmap_blob_upload_free(upload);
  mailjmap_session_free(session_object);
  mailjmap_free(client);
  fake_context_clear(&context);
  return ok;
}

int main(void)
{
  return test_upload_and_download() ? 0 : 1;
}
