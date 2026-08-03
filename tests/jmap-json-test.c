/*
 * libEtPan! -- a mail stuff library
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "../src/low-level/jmap/mailjmap_json.h"

#include <jansson.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static int test_parse_invalid_json(void)
{
  mailjmap_json_value * sentinel;
  mailjmap_json_value * value;
  int r;

  sentinel = (mailjmap_json_value *) 1;
  value = sentinel;
  r = mailjmap_json_parse("{\"bad\"", 6, &value);

  if (value != sentinel)
    mailjmap_json_free(value);
  return check(r == MAILJMAP_ERROR_JSON_PARSE,
      "invalid JSON was not rejected") &&
      check(value == NULL, "invalid JSON did not clear result");
}

static int test_parse_duplicate_keys(void)
{
  mailjmap_json_value * value;
  int r;
  int ok;

  value = NULL;
  r = mailjmap_json_parse("{\"id\":\"a\",\"id\":\"b\"}", 19, &value);

  mailjmap_json_free(value);
  ok = check(r == MAILJMAP_ERROR_JSON_PARSE,
      "duplicate JSON object keys were not rejected");

  value = NULL;
  r = mailjmap_json_parse("{\"outer\":{\"id\":\"a\",\"id\":\"b\"}}",
      strlen("{\"outer\":{\"id\":\"a\",\"id\":\"b\"}}"), &value);

  mailjmap_json_free(value);
  return check(r == MAILJMAP_ERROR_JSON_PARSE,
      "nested duplicate JSON object keys were not rejected") && ok;
}

static int test_parse_trailing_data(void)
{
  mailjmap_json_value * sentinel;
  mailjmap_json_value * value;
  int r;

  sentinel = (mailjmap_json_value *) 1;
  value = sentinel;
  r = mailjmap_json_parse("{}{}", 4, &value);

  if (value != sentinel)
    mailjmap_json_free(value);
  return check(r == MAILJMAP_ERROR_JSON_PARSE,
      "JSON with trailing data was not rejected") &&
      check(value == NULL, "trailing JSON did not clear result");
}

static int test_parse_and_lookup(void)
{
  static const char json[] =
      "{"
      "\"name\":\"Inbox\","
      "\"count\":9223372036854775807,"
      "\"enabled\":true,"
      "\"label\":\"caf\\u00e9\","
      "\"ids\":[\"a\",\"b\"],"
      "\"none\":null"
      "}";
  mailjmap_json_value * root;
  mailjmap_json_value * ids;
  mailjmap_json_value * first;
  mailjmap_json_value * none;
  char * name;
  char * label;
  char * id;
  int64_t count;
  int enabled;
  int r;
  int ok;

  root = NULL;
  ids = NULL;
  first = NULL;
  none = NULL;
  name = NULL;
  label = NULL;
  id = NULL;
  count = 0;
  enabled = 0;
  ok = 0;

  r = mailjmap_json_parse(json, strlen(json), &root);
  if (!check(r == MAILJMAP_NO_ERROR, "JSON parse failed"))
    goto cleanup;

  r = mailjmap_json_object_get_string_dup(root, "name", &name);
  if (!check(r == MAILJMAP_NO_ERROR, "string lookup failed"))
    goto cleanup;

  r = mailjmap_json_object_get_integer(root, "count", &count);
  if (!check(r == MAILJMAP_NO_ERROR, "integer lookup failed"))
    goto cleanup;

  r = mailjmap_json_object_get_string_dup(root, "label", &label);
  if (!check(r == MAILJMAP_NO_ERROR, "UTF-8 string lookup failed"))
    goto cleanup;

  r = mailjmap_json_object_get_boolean(root, "enabled", &enabled);
  if (!check(r == MAILJMAP_NO_ERROR, "boolean lookup failed"))
    goto cleanup;

  r = mailjmap_json_object_get(root, "ids", &ids);
  if (!check(r == MAILJMAP_NO_ERROR, "array lookup failed"))
    goto cleanup;

  r = mailjmap_json_array_get(ids, 0, &first);
  if (!check(r == MAILJMAP_NO_ERROR, "array item lookup failed"))
    goto cleanup;

  r = mailjmap_json_string_dup(first, &id);
  if (!check(r == MAILJMAP_NO_ERROR, "array string dup failed"))
    goto cleanup;

  r = mailjmap_json_object_get(root, "none", &none);
  if (!check(r == MAILJMAP_NO_ERROR, "null lookup failed"))
    goto cleanup;

  ok = check(str_equal(name, "Inbox"), "name mismatch") &&
      check(count == INT64_C(9223372036854775807), "count mismatch") &&
      check(str_equal(label, "caf" "\xC3" "\xA9"), "UTF-8 label mismatch") &&
      check(enabled == 1, "enabled mismatch") &&
      check(mailjmap_json_array_size(ids) == 2, "array size mismatch") &&
      check(str_equal(id, "a"), "array value mismatch") &&
      check(mailjmap_json_is_null(none), "null type mismatch");

 cleanup:
  free(name);
  free(label);
  free(id);
  mailjmap_json_free(none);
  mailjmap_json_free(first);
  mailjmap_json_free(ids);
  mailjmap_json_free(root);
  return ok;
}

static int test_build_and_serialize(void)
{
  mailjmap_json_value * root;
  mailjmap_json_value * using_array;
  mailjmap_json_value * value;
  char * data;
  size_t data_len;
  int r;
  int ok;

  root = NULL;
  using_array = NULL;
  value = NULL;
  data = NULL;
  data_len = 0;
  ok = 0;

  r = mailjmap_json_new_object(&root);
  if (!check(r == MAILJMAP_NO_ERROR, "object allocation failed"))
    goto cleanup;

  r = mailjmap_json_new_array(&using_array);
  if (!check(r == MAILJMAP_NO_ERROR, "array allocation failed"))
    goto cleanup;

  r = mailjmap_json_new_string("urn:ietf:params:jmap:core", &value);
  if (!check(r == MAILJMAP_NO_ERROR, "string allocation failed"))
    goto cleanup;
  r = mailjmap_json_array_append_new(using_array, value);
  value = NULL;
  if (!check(r == MAILJMAP_NO_ERROR, "array append failed"))
    goto cleanup;

  r = mailjmap_json_object_set_new(root, "using", using_array);
  using_array = NULL;
  if (!check(r == MAILJMAP_NO_ERROR, "object set failed"))
    goto cleanup;

  r = mailjmap_json_new_boolean(1, &value);
  if (!check(r == MAILJMAP_NO_ERROR, "boolean allocation failed"))
    goto cleanup;
  r = mailjmap_json_object_set_new(root, "ok", value);
  value = NULL;
  if (!check(r == MAILJMAP_NO_ERROR, "boolean object set failed"))
    goto cleanup;

  r = mailjmap_json_serialize(root,
      MAILJMAP_JSON_SERIALIZE_COMPACT |
      MAILJMAP_JSON_SERIALIZE_SORT_KEYS,
      &data, &data_len);
  if (!check(r == MAILJMAP_NO_ERROR, "serialization failed"))
    goto cleanup;

  ok = check(str_equal(data,
      "{\"ok\":true,\"using\":[\"urn:ietf:params:jmap:core\"]}"),
      "serialized JSON mismatch") &&
      check(data_len == strlen(data), "serialized length mismatch");

 cleanup:
  free(data);
  mailjmap_json_free(value);
  mailjmap_json_free(using_array);
  mailjmap_json_free(root);
  return ok;
}

static int test_build_scalars_and_sorted_serialize(void)
{
  mailjmap_json_value * root;
  mailjmap_json_value * value;
  char * data;
  size_t data_len;
  int r;
  int ok;

  root = NULL;
  value = NULL;
  data = NULL;
  data_len = 0;
  ok = 0;

  r = mailjmap_json_new_object(&root);
  if (!check(r == MAILJMAP_NO_ERROR, "object allocation failed"))
    goto cleanup;

  r = mailjmap_json_new_string("z", &value);
  if (!check(r == MAILJMAP_NO_ERROR, "string allocation failed"))
    goto cleanup;
  r = mailjmap_json_object_set_new(root, "zeta", value);
  value = NULL;
  if (!check(r == MAILJMAP_NO_ERROR, "zeta set failed"))
    goto cleanup;

  r = mailjmap_json_new_null(&value);
  if (!check(r == MAILJMAP_NO_ERROR, "null allocation failed"))
    goto cleanup;
  r = mailjmap_json_object_set_new(root, "middle", value);
  value = NULL;
  if (!check(r == MAILJMAP_NO_ERROR, "null set failed"))
    goto cleanup;

  r = mailjmap_json_new_integer(7, &value);
  if (!check(r == MAILJMAP_NO_ERROR, "integer allocation failed"))
    goto cleanup;
  r = mailjmap_json_object_set_new(root, "alpha", value);
  value = NULL;
  if (!check(r == MAILJMAP_NO_ERROR, "integer set failed"))
    goto cleanup;

  r = mailjmap_json_serialize(root,
      MAILJMAP_JSON_SERIALIZE_COMPACT |
      MAILJMAP_JSON_SERIALIZE_SORT_KEYS,
      &data, &data_len);
  if (!check(r == MAILJMAP_NO_ERROR, "serialization failed"))
    goto cleanup;

  ok = check(str_equal(data, "{\"alpha\":7,\"middle\":null,\"zeta\":\"z\"}"),
      "sorted scalar serialization mismatch") &&
      check(data_len == strlen(data), "serialized length mismatch");

 cleanup:
  free(data);
  mailjmap_json_free(value);
  mailjmap_json_free(root);
  return ok;
}

static int test_integer_creation_bounds(void)
{
  mailjmap_json_value * value;
  int64_t parsed;
  int r;
  int ok;

  value = NULL;
  parsed = 0;
  ok = 0;

  r = mailjmap_json_new_integer(INT64_C(9223372036854775807), &value);
  if (sizeof(json_int_t) >= sizeof(int64_t)) {
    if (!check(r == MAILJMAP_NO_ERROR, "INT64_MAX allocation failed"))
      goto cleanup;
    r = mailjmap_json_integer_value(value, &parsed);
    if (!check(r == MAILJMAP_NO_ERROR, "INT64_MAX lookup failed"))
      goto cleanup;
    ok = check(parsed == INT64_C(9223372036854775807),
        "INT64_MAX value mismatch");
  }
  else {
    ok = check(r == MAILJMAP_ERROR_BAD_STATE,
        "out-of-range integer was not rejected") &&
        check(value == NULL, "out-of-range integer did not clear result");
  }

 cleanup:
  mailjmap_json_free(value);
  return ok;
}

int main(void)
{
  int ok;

  ok = 1;
  ok = test_parse_invalid_json() && ok;
  ok = test_parse_duplicate_keys() && ok;
  ok = test_parse_trailing_data() && ok;
  ok = test_parse_and_lookup() && ok;
  ok = test_build_and_serialize() && ok;
  ok = test_build_scalars_and_sorted_serialize() && ok;
  ok = test_integer_creation_bounds() && ok;

  return ok ? 0 : 1;
}
