/*
 * libEtPan! -- a mail stuff library
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailjmap_blob.h"
#include <libetpan/mailjson.h>
#include "mailjmap_private.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int is_url_unreserved(unsigned char ch)
{
  if ((ch >= 'A') && (ch <= 'Z'))
    return 1;
  if ((ch >= 'a') && (ch <= 'z'))
    return 1;
  if ((ch >= '0') && (ch <= '9'))
    return 1;

  return (ch == '-') || (ch == '.') || (ch == '_') || (ch == '~');
}

static size_t url_encoded_len(const char * value)
{
  size_t len;
  const unsigned char * cur;

  len = 0;
  for (cur = (const unsigned char *) value; * cur != '\0'; cur ++) {
    if (is_url_unreserved(* cur))
      len ++;
    else
      len += 3;
  }

  return len;
}

static void append_url_encoded(char ** cursor, const char * value)
{
  static const char hex[] = "0123456789ABCDEF";
  const unsigned char * cur;

  for (cur = (const unsigned char *) value; * cur != '\0'; cur ++) {
    if (is_url_unreserved(* cur)) {
      ** cursor = (char) * cur;
      (* cursor) ++;
    }
    else {
      ** cursor = '%';
      (* cursor) ++;
      ** cursor = hex[* cur >> 4];
      (* cursor) ++;
      ** cursor = hex[* cur & 0x0f];
      (* cursor) ++;
    }
  }
}

static const char * template_value_for_name(const char * name,
    const char * account_id, const char * blob_id, const char * type,
    const char * value_name)
{
  if (strcmp(name, "accountId") == 0)
    return account_id;
  if (strcmp(name, "blobId") == 0)
    return blob_id;
  if (strcmp(name, "type") == 0)
    return type;
  if (strcmp(name, "name") == 0)
    return value_name;

  return NULL;
}

static int append_template_value_len(size_t * result_len,
    const char * name, size_t name_len, const char * account_id,
    const char * blob_id, const char * type, const char * value_name)
{
  char placeholder[16];
  const char * value;

  if ((name_len + 1) > sizeof(placeholder))
    return MAILJMAP_ERROR_PROTOCOL;

  memcpy(placeholder, name, name_len);
  placeholder[name_len] = '\0';
  value = template_value_for_name(placeholder, account_id, blob_id, type,
      value_name);
  if (value == NULL)
    return MAILJMAP_ERROR_PROTOCOL;

  * result_len += url_encoded_len(value);
  return MAILJMAP_NO_ERROR;
}

static int expand_jmap_url_template(const char * url_template,
    const char * account_id, const char * blob_id, const char * type,
    const char * value_name, char ** result)
{
  size_t result_len;
  const char * cur;
  char * expanded;
  char * out;
  int r;

  if ((url_template == NULL) || (account_id == NULL) || (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  result_len = 0;
  for (cur = url_template; * cur != '\0'; cur ++) {
    if (* cur == '{') {
      const char * end;

      end = strchr(cur + 1, '}');
      if (end == NULL)
        return MAILJMAP_ERROR_PROTOCOL;
      r = append_template_value_len(&result_len, cur + 1,
          (size_t) (end - cur - 1), account_id, blob_id, type, value_name);
      if (r != MAILJMAP_NO_ERROR)
        return r;
      cur = end;
    }
    else {
      result_len ++;
    }
  }

  expanded = malloc(result_len + 1);
  if (expanded == NULL)
    return MAILJMAP_ERROR_MEMORY;

  out = expanded;
  for (cur = url_template; * cur != '\0'; cur ++) {
    if (* cur == '{') {
      const char * end;
      char placeholder[16];
      const char * value;
      size_t name_len;

      end = strchr(cur + 1, '}');
      name_len = (size_t) (end - cur - 1);
      memcpy(placeholder, cur + 1, name_len);
      placeholder[name_len] = '\0';
      value = template_value_for_name(placeholder, account_id, blob_id, type,
          value_name);
      append_url_encoded(&out, value);
      cur = end;
    }
    else {
      * out = * cur;
      out ++;
    }
  }
  * out = '\0';

  * result = expanded;
  return MAILJMAP_NO_ERROR;
}

static int object_get_optional_size(mailjson_value * object,
    const char * key, size_t * result)
{
  mailjson_value * value;
  int64_t integer;
  int r;

  value = NULL;
  r = mailjson_object_get(object, key, &value);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  if (value == NULL)
    return MAILJMAP_NO_ERROR;

  r = mailjson_integer_value(value, &integer);
  mailjson_free(value);
  if (r != MAILJMAP_NO_ERROR)
    return MAILJMAP_ERROR_PROTOCOL;
  if (integer < 0)
    return MAILJMAP_ERROR_PROTOCOL;

  * result = (size_t) integer;
  return MAILJMAP_NO_ERROR;
}

static int parse_upload_response(const char * data, size_t data_len,
    struct mailjmap_blob_upload ** result)
{
  mailjson_value * root;
  struct mailjmap_blob_upload * upload;
  int r;

  if ((data == NULL) || (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  root = NULL;
  upload = NULL;

  r = mailjson_parse(data, data_len, &root);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  if (!mailjson_is_object(root)) {
    r = MAILJMAP_ERROR_PROTOCOL;
    goto cleanup;
  }

  upload = mailjmap_blob_upload_new();
  if (upload == NULL) {
    r = MAILJMAP_ERROR_MEMORY;
    goto cleanup;
  }

  r = mailjson_object_get_string_dup(root, "accountId",
      &upload->account_id);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = mailjson_object_get_string_dup(root, "blobId", &upload->blob_id);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = mailjson_object_get_string_dup(root, "type", &upload->type);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = mailjson_object_get_string_dup(root, "name", &upload->name);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = object_get_optional_size(root, "size", &upload->size);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;

  if ((upload->account_id == NULL) || (upload->blob_id == NULL) ||
      (upload->type == NULL)) {
    r = MAILJMAP_ERROR_PROTOCOL;
    goto cleanup;
  }

  mailjson_free(root);
  * result = upload;
  return MAILJMAP_NO_ERROR;

 cleanup:
  mailjson_free(root);
  mailjmap_blob_upload_free(upload);
  return mailjmap_private_normalize_parse_error(r);
}

static int copy_response_body(struct mailjmap_http_response * response,
    char ** data, size_t * data_len)
{
  char * copy;

  if ((response == NULL) || (data == NULL) || (data_len == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  copy = malloc(response->body_len + 1);
  if (copy == NULL)
    return MAILJMAP_ERROR_MEMORY;
  if (response->body_len > 0)
    memcpy(copy, response->body, response->body_len);
  copy[response->body_len] = '\0';

  * data = copy;
  * data_len = response->body_len;
  return MAILJMAP_NO_ERROR;
}

int mailjmap_upload(mailjmap * session,
    const char * account_id,
    const char * content_type,
    const char * data,
    size_t data_len,
    struct mailjmap_blob_upload ** result)
{
  struct mailjmap_http_request * request;
  struct mailjmap_http_response * response;
  char * url;
  int r;

  if ((session == NULL) || (account_id == NULL) || (content_type == NULL) ||
      (data == NULL && data_len > 0) || (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  * result = NULL;
  mailjmap_private_clear_last_error(session);

  if ((session->jmap_upload_url == NULL) ||
      (* session->jmap_upload_url == '\0'))
    return MAILJMAP_ERROR_BAD_STATE;
  if (session->jmap_http_transport == NULL)
    return MAILJMAP_ERROR_HTTP;

  request = NULL;
  response = NULL;
  url = NULL;

  r = expand_jmap_url_template(session->jmap_upload_url, account_id, NULL,
      content_type, NULL, &url);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;

  request = mailjmap_http_request_new("POST", url);
  if (request == NULL) {
    r = MAILJMAP_ERROR_MEMORY;
    goto cleanup;
  }
  request->timeout = session->jmap_timeout;

  r = mailjmap_http_request_set_content_type(request, content_type);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = mailjmap_http_request_set_accept_type(request, "application/json");
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = mailjmap_http_request_set_body(request, (const unsigned char *) data,
      data_len);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = mailjmap_private_add_auth_header(session, request);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;

  r = mailjmap_http_perform(session->jmap_http_transport, request, &response);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;

  session->jmap_last_http_status = response->status_code;
  if (response->status_code < 200 || response->status_code >= 300) {
    r = mailjmap_private_remember_http_problem_diagnostics(session, response);
    if (r != MAILJMAP_NO_ERROR)
      goto cleanup;
  }
  if (response->status_code == 401 || response->status_code == 403) {
    mailjmap_private_set_last_error_message_if_empty(session,
        "JMAP authentication failed");
    r = MAILJMAP_ERROR_AUTHENTICATION;
    goto cleanup;
  }
  if (response->status_code == 413) {
    mailjmap_private_set_last_error_message_if_empty(session,
        "JMAP upload too large");
    r = MAILJMAP_ERROR_LIMIT;
    goto cleanup;
  }
  if (response->status_code < 200 || response->status_code >= 300) {
    mailjmap_private_set_last_error_message_if_empty(session,
        "JMAP upload request failed");
    r = MAILJMAP_ERROR_HTTP;
    goto cleanup;
  }

  r = parse_upload_response((const char *) response->body,
      response->body_len, result);

 cleanup:
  free(url);
  mailjmap_http_response_free(response);
  mailjmap_http_request_free(request);
  return r;
}

int mailjmap_download(mailjmap * session,
    const char * account_id,
    const char * blob_id,
    const char * name,
    const char * accept,
    char ** data,
    size_t * data_len)
{
  struct mailjmap_http_request * request;
  struct mailjmap_http_response * response;
  char * url;
  int r;

  if ((session == NULL) || (account_id == NULL) || (blob_id == NULL) ||
      (data == NULL) || (data_len == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  * data = NULL;
  * data_len = 0;
  mailjmap_private_clear_last_error(session);

  if ((session->jmap_download_url == NULL) ||
      (* session->jmap_download_url == '\0'))
    return MAILJMAP_ERROR_BAD_STATE;
  if (session->jmap_http_transport == NULL)
    return MAILJMAP_ERROR_HTTP;

  request = NULL;
  response = NULL;
  url = NULL;

  r = expand_jmap_url_template(session->jmap_download_url, account_id,
      blob_id, accept, name, &url);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;

  request = mailjmap_http_request_new("GET", url);
  if (request == NULL) {
    r = MAILJMAP_ERROR_MEMORY;
    goto cleanup;
  }
  request->timeout = session->jmap_timeout;

  if (accept != NULL) {
    r = mailjmap_http_request_set_accept_type(request, accept);
    if (r != MAILJMAP_NO_ERROR)
      goto cleanup;
  }
  r = mailjmap_private_add_auth_header(session, request);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;

  r = mailjmap_http_perform(session->jmap_http_transport, request, &response);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;

  session->jmap_last_http_status = response->status_code;
  if (response->status_code < 200 || response->status_code >= 300) {
    r = mailjmap_private_remember_http_problem_diagnostics(session, response);
    if (r != MAILJMAP_NO_ERROR)
      goto cleanup;
  }
  if (response->status_code == 401 || response->status_code == 403) {
    mailjmap_private_set_last_error_message_if_empty(session,
        "JMAP authentication failed");
    r = MAILJMAP_ERROR_AUTHENTICATION;
    goto cleanup;
  }
  if (response->status_code < 200 || response->status_code >= 300) {
    mailjmap_private_set_last_error_message_if_empty(session,
        "JMAP download request failed");
    r = MAILJMAP_ERROR_HTTP;
    goto cleanup;
  }

  r = copy_response_body(response, data, data_len);

 cleanup:
  free(url);
  mailjmap_http_response_free(response);
  mailjmap_http_request_free(request);
  return r;
}
