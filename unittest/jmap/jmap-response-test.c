/*
 * libEtPan! -- a mail stuff library
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "../../src/low-level/jmap/mailjmap_response.h"
#include "../../src/low-level/jmap/mailjmap_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char * read_fixture_file(const char * name, size_t * result_len)
{
  const char * prefixes[] = {
    "unittest/jmap/data/response/",
    "../unittest/jmap/data/response/",
    "jmap/data/response/"
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
  if (result_len != NULL)
    * result_len = read_len;
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

static int test_response_parse_success_and_error(void)
{
  struct mailjmap_response * response;
  struct mailjmap_method_response * first;
  struct mailjmap_method_response * second;
  mailjson_value * type_value;
  char * data;
  char * type;
  size_t data_len;
  int r;
  int ok;

  response = NULL;
  type_value = NULL;
  data = NULL;
  type = NULL;
  data_len = 0;
  ok = 0;

  data = read_fixture_file("method-success-and-error.json", &data_len);
  if (!check(data != NULL, "method response fixture read failed"))
    goto cleanup;

  r = mailjmap_response_parse(data, data_len, &response);
  if (!check(r == MAILJMAP_NO_ERROR, "response parse failed"))
    goto cleanup;

  first = clist_content(clist_begin(response->method_responses));
  second = clist_content(clist_next(clist_begin(response->method_responses)));

  r = mailjson_object_get(second->arguments, "type", &type_value);
  if (!check(r == MAILJMAP_NO_ERROR, "error type lookup failed"))
    goto cleanup;
  r = mailjson_string_dup(type_value, &type);
  if (!check(r == MAILJMAP_NO_ERROR, "error type duplication failed"))
    goto cleanup;

  ok = check(str_equal(response->session_state, "state2"),
          "sessionState mismatch") &&
      check(clist_count(response->method_responses) == 2,
          "method response count mismatch") &&
      check(str_equal(first->name, "Mailbox/get"), "first method mismatch") &&
      check(str_equal(first->call_id, "c1"), "first call id mismatch") &&
      check(str_equal(second->name, "error"), "second method mismatch") &&
      check(str_equal(second->call_id, "c2"), "second call id mismatch") &&
      check(str_equal(type, "invalidArguments"),
          "method error type mismatch");

 cleanup:
  free(type);
  free(data);
  mailjson_free(type_value);
  mailjmap_response_free(response);
  return ok;
}

static int test_problem_parse(void)
{
  struct mailjmap_response * response;
  char * data;
  size_t data_len;
  int r;
  int ok;

  response = NULL;
  data = NULL;
  data_len = 0;
  ok = 0;

  data = read_fixture_file("problem-limit.json", &data_len);
  if (!check(data != NULL, "problem fixture read failed"))
    goto cleanup;

  r = mailjmap_response_parse(data, data_len, &response);
  if (!check(r == MAILJMAP_NO_ERROR, "problem parse failed"))
    goto cleanup;

  ok = check(str_equal(response->error_type,
          "urn:ietf:params:jmap:error:limit"), "problem type mismatch") &&
      check(str_equal(response->error_detail, "too many calls"),
          "problem detail mismatch");

 cleanup:
  free(data);
  mailjmap_response_free(response);
  return ok;
}

static int test_malformed_response(void)
{
  static const char data[] =
      "{\"methodResponses\":[[\"Mailbox/get\",[],\"c1\"]],"
      "\"sessionState\":\"s\"}";
  struct mailjmap_response * response;
  int r;

  response = NULL;
  r = mailjmap_response_parse(data, sizeof(data) - 1, &response);
  mailjmap_response_free(response);
  return check(r == MAILJMAP_ERROR_PROTOCOL,
      "malformed response should fail with protocol error");
}

static int test_overlong_method_response_tuple(void)
{
  static const char data[] =
      "{\"methodResponses\":[[\"Mailbox/get\",{},\"c1\",\"extra\"]],"
      "\"sessionState\":\"s\"}";
  struct mailjmap_response * response;
  int r;

  response = NULL;
  r = mailjmap_response_parse(data, sizeof(data) - 1, &response);
  mailjmap_response_free(response);
  return check(r == MAILJMAP_ERROR_PROTOCOL,
      "overlong method response tuple should fail with protocol error");
}

static int test_missing_method_responses(void)
{
  static const char data[] = "{\"sessionState\":\"s\"}";
  struct mailjmap_response * response;
  int r;

  response = NULL;
  r = mailjmap_response_parse(data, sizeof(data) - 1, &response);
  mailjmap_response_free(response);
  return check(r == MAILJMAP_ERROR_PROTOCOL,
      "response without methodResponses should fail with protocol error");
}

static int test_missing_session_state(void)
{
  static const char data[] =
      "{\"methodResponses\":[[\"Mailbox/get\",{},\"c1\"]]}";
  struct mailjmap_response * response;
  int r;

  response = NULL;
  r = mailjmap_response_parse(data, sizeof(data) - 1, &response);
  mailjmap_response_free(response);
  return check(r == MAILJMAP_ERROR_PROTOCOL,
      "response without sessionState should fail with protocol error");
}

int main(void)
{
  if (!test_response_parse_success_and_error())
    return 1;
  if (!test_problem_parse())
    return 1;
  if (!test_malformed_response())
    return 1;
  if (!test_overlong_method_response_tuple())
    return 1;
  if (!test_missing_method_responses())
    return 1;
  if (!test_missing_session_state())
    return 1;

  return 0;
}
