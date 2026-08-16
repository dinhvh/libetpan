/*
 * libEtPan! -- a mail stuff library
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "../../src/data-types/mailjson.h"
#include "../../src/low-level/jmap/mailjmap_request.h"
#include "../../src/low-level/jmap/mailjmap_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char * read_expected_file(const char * name)
{
  const char * prefixes[] = {
    "unittest/jmap/data/request/",
    "../unittest/jmap/data/request/",
    "jmap/data/request/"
  };
  char path[512];
  char * data;
  FILE * f;
  long len;
  size_t read_len;
  size_t i;

  for (i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); i ++) {
    snprintf(path, sizeof(path), "%s%s", prefixes[i], name);
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
  while ((read_len > 0) &&
      ((data[read_len - 1] == '\n') || (data[read_len - 1] == '\r'))) {
    read_len --;
    data[read_len] = '\0';
  }

  return data;
}

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

static int add_string_property(mailjson_value * object,
    const char * key, const char * value)
{
  mailjson_value * string_value;
  int r;

  string_value = NULL;
  r = mailjson_new_string(value, &string_value);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  r = mailjson_object_set_new(object, key, string_value);
  if (r != MAILJMAP_NO_ERROR) {
    mailjson_free(string_value);
    return r;
  }

  return MAILJMAP_NO_ERROR;
}

static int test_request_convenience_helpers(void)
{
  struct mailjmap_request * request;
  mailjson_value * arguments;
  mailjson_value * envelope;
  char * data;
  size_t data_len;
  int r;
  int ok;

  request = NULL;
  arguments = NULL;
  envelope = NULL;
  data = NULL;
  data_len = 0;
  ok = 0;

  request = mailjmap_request_new();
  if (!check(request != NULL, "convenience request allocation failed"))
    goto cleanup;

  r = mailjmap_request_add_capability(request,
      "urn:example:jmap:extension");
  if (!check(r == MAILJMAP_NO_ERROR, "extension capability add failed"))
    goto cleanup;

  r = mailjson_new_object(&arguments);
  if (!check(r == MAILJMAP_NO_ERROR, "convenience args allocation failed"))
    goto cleanup;
  r = mailjmap_request_add_string_argument(arguments, "accountId", "acc1");
  if (!check(r == MAILJMAP_NO_ERROR, "string argument add failed"))
    goto cleanup;
  r = mailjmap_request_add_call_json(request, "Example/echo", arguments,
      "c1");
  if (!check(r == MAILJMAP_NO_ERROR, "convenience call add failed"))
    goto cleanup;
  arguments = NULL;

  r = mailjmap_request_add_call_empty(request, "Example/ping", "c2");
  if (!check(r == MAILJMAP_NO_ERROR, "empty call add failed"))
    goto cleanup;

  r = mailjmap_request_serialize(request, &envelope);
  if (!check(r == MAILJMAP_NO_ERROR, "convenience serialization failed"))
    goto cleanup;
  r = mailjson_serialize(envelope,
      MAILJSON_SERIALIZE_COMPACT |
      MAILJSON_SERIALIZE_SORT_KEYS,
      &data, &data_len);
  if (!check(r == MAILJMAP_NO_ERROR, "convenience JSON serialization failed"))
    goto cleanup;

  ok = check(strstr(data, "\"Example/echo\"") != NULL,
          "convenience envelope missing echo call") &&
      check(strstr(data, "\"Example/ping\",{},\"c2\"") != NULL,
          "convenience envelope missing empty call") &&
      check(strstr(data, "\"accountId\":\"acc1\"") != NULL,
          "convenience envelope missing string argument") &&
      check(strstr(data, "\"urn:example:jmap:extension\"") != NULL,
          "convenience envelope missing extension capability") &&
      check(data_len == strlen(data), "convenience request length mismatch");

 cleanup:
  free(data);
  mailjson_free(envelope);
  mailjson_free(arguments);
  mailjmap_request_free(request);
  return ok;
}

static int test_request_serialize(void)
{
  struct mailjmap_request * request;
  mailjson_value * arguments;
  mailjson_value * envelope;
  char * data;
  char * expected;
  size_t data_len;
  int r;
  int ok;

  request = NULL;
  arguments = NULL;
  envelope = NULL;
  data = NULL;
  expected = NULL;
  data_len = 0;
  ok = 0;

  request = mailjmap_request_new();
  if (!check(request != NULL, "request allocation failed"))
    goto cleanup;

  r = mailjmap_request_add_capability(request,
      "urn:ietf:params:jmap:core");
  if (!check(r == MAILJMAP_NO_ERROR, "core capability add failed"))
    goto cleanup;

  r = mailjmap_request_add_capability(request,
      "urn:ietf:params:jmap:mail");
  if (!check(r == MAILJMAP_NO_ERROR, "mail capability add failed"))
    goto cleanup;

  r = mailjson_new_object(&arguments);
  if (!check(r == MAILJMAP_NO_ERROR, "arguments allocation failed"))
    goto cleanup;

  r = add_string_property(arguments, "accountId", "acc1");
  if (!check(r == MAILJMAP_NO_ERROR, "accountId add failed"))
    goto cleanup;

  r = mailjmap_request_add_call_json(request, "Mailbox/get", arguments, "c1");
  if (!check(r == MAILJMAP_NO_ERROR, "method call add failed"))
    goto cleanup;
  arguments = NULL;

  r = mailjmap_request_serialize(request, &envelope);
  if (!check(r == MAILJMAP_NO_ERROR, "request serialization failed"))
    goto cleanup;

  r = mailjson_serialize(envelope,
      MAILJSON_SERIALIZE_COMPACT |
      MAILJSON_SERIALIZE_SORT_KEYS,
      &data, &data_len);
  if (!check(r == MAILJMAP_NO_ERROR, "JSON serialization failed"))
    goto cleanup;

  expected = read_expected_file("mailbox-get.json");
  if (!check(expected != NULL, "mailbox-get golden file read failed"))
    goto cleanup;

  ok = check(str_equal(data, expected),
      "request envelope mismatch") &&
      check(data_len == strlen(data), "request length mismatch");

 cleanup:
  free(expected);
  free(data);
  mailjson_free(envelope);
  mailjson_free(arguments);
  mailjmap_request_free(request);
  return ok;
}

static int test_request_result_reference(void)
{
  struct mailjmap_request * request;
  mailjson_value * query_args;
  mailjson_value * get_args;
  mailjson_value * envelope;
  char * data;
  char * expected;
  size_t data_len;
  int r;
  int ok;

  request = NULL;
  query_args = NULL;
  get_args = NULL;
  envelope = NULL;
  data = NULL;
  expected = NULL;
  data_len = 0;
  ok = 0;

  request = mailjmap_request_new();
  if (!check(request != NULL, "request allocation failed"))
    goto cleanup;

  r = mailjmap_request_add_capability(request,
      "urn:ietf:params:jmap:mail");
  if (!check(r == MAILJMAP_NO_ERROR, "mail capability add failed"))
    goto cleanup;

  r = mailjson_new_object(&query_args);
  if (!check(r == MAILJMAP_NO_ERROR, "query args allocation failed"))
    goto cleanup;
  r = add_string_property(query_args, "accountId", "acc1");
  if (!check(r == MAILJMAP_NO_ERROR, "query accountId add failed"))
    goto cleanup;
  r = mailjmap_request_add_call_json(request, "Email/query", query_args,
      "c1");
  if (!check(r == MAILJMAP_NO_ERROR, "query call add failed"))
    goto cleanup;
  query_args = NULL;

  r = mailjson_new_object(&get_args);
  if (!check(r == MAILJMAP_NO_ERROR, "get args allocation failed"))
    goto cleanup;
  r = add_string_property(get_args, "accountId", "acc1");
  if (!check(r == MAILJMAP_NO_ERROR, "get accountId add failed"))
    goto cleanup;
  r = mailjmap_request_arguments_set_result_reference(get_args, "#ids",
      "c1", "Email/query", "/ids");
  if (!check(r == MAILJMAP_NO_ERROR, "result reference add failed"))
    goto cleanup;
  r = mailjmap_request_add_call_json(request, "Email/get", get_args, "c2");
  if (!check(r == MAILJMAP_NO_ERROR, "get call add failed"))
    goto cleanup;
  get_args = NULL;

  r = mailjmap_request_serialize(request, &envelope);
  if (!check(r == MAILJMAP_NO_ERROR, "request serialization failed"))
    goto cleanup;

  r = mailjson_serialize(envelope,
      MAILJSON_SERIALIZE_COMPACT |
      MAILJSON_SERIALIZE_SORT_KEYS,
      &data, &data_len);
  if (!check(r == MAILJMAP_NO_ERROR, "JSON serialization failed"))
    goto cleanup;

  expected = read_expected_file("email-query-get-result-reference.json");
  if (!check(expected != NULL, "result reference golden file read failed"))
    goto cleanup;

  ok = check(str_equal(data, expected),
      "result reference envelope mismatch") &&
      check(data_len == strlen(data), "request length mismatch");

 cleanup:
  free(expected);
  free(data);
  mailjson_free(envelope);
  mailjson_free(get_args);
  mailjson_free(query_args);
  mailjmap_request_free(request);
  return ok;
}

int main(void)
{
  return (test_request_convenience_helpers() &&
      test_request_serialize() &&
      test_request_result_reference()) ? 0 : 1;
}
