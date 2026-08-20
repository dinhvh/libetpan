/*
 * libEtPan! -- a mail stuff library
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "mailjson.h"

#include <json-c/json.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>

struct mailjson_value {
  struct json_object * json;
};

struct json_scan {
  const char * data;
  size_t len;
  size_t pos;
};

static mailjson_value * value_new(struct json_object * json)
{
  mailjson_value * value;

  value = malloc(sizeof(* value));
  if (value == NULL) {
    json_object_put(json);
    return NULL;
  }
  value->json = json;
  return value;
}

static int wrap_new(struct json_object * json, mailjson_value ** result)
{
  mailjson_value * value;

  if (result == NULL) {
    json_object_put(json);
    return MAILJSON_ERROR_BAD_STATE;
  }
  value = value_new(json);
  if (value == NULL)
    return MAILJSON_ERROR_MEMORY;
  * result = value;
  return MAILJSON_NO_ERROR;
}

static char * dup_string(const char * string)
{
  if (string == NULL)
    return NULL;
  return strdup(string);
}

static void scan_whitespace(struct json_scan * scan)
{
  while (scan->pos < scan->len) {
    char ch;

    ch = scan->data[scan->pos];
    if ((ch != ' ') && (ch != '\t') && (ch != '\r') && (ch != '\n'))
      break;
    scan->pos ++;
  }
}

static int scan_string(struct json_scan * scan, size_t * start, size_t * len)
{
  size_t begin;

  if ((scan->pos >= scan->len) || (scan->data[scan->pos] != '"'))
    return -1;
  begin = scan->pos ++;
  while (scan->pos < scan->len) {
    char ch;

    ch = scan->data[scan->pos ++];
    if (ch == '"') {
      if (start != NULL)
        * start = begin;
      if (len != NULL)
        * len = scan->pos - begin;
      return 0;
    }
    if (ch == '\\') {
      if (scan->pos >= scan->len)
        return -1;
      if (scan->data[scan->pos ++] == 'u') {
        if (scan->len - scan->pos < 4)
          return -1;
        scan->pos += 4;
      }
    }
  }
  return -1;
}

static char * decode_key(const char * data, size_t len)
{
  struct json_tokener * tokener;
  struct json_object * value;
  char * key;

  if (len >= INT_MAX)
    return NULL;
  tokener = json_tokener_new();
  if (tokener == NULL)
    return NULL;
  json_tokener_set_flags(tokener,
      JSON_TOKENER_STRICT | JSON_TOKENER_VALIDATE_UTF8);
  value = json_tokener_parse_ex(tokener, data, (int) len);
  if ((json_tokener_get_error(tokener) != json_tokener_success) ||
      (json_tokener_get_parse_end(tokener) != len) ||
      !json_object_is_type(value, json_type_string)) {
    json_object_put(value);
    json_tokener_free(tokener);
    return NULL;
  }
  key = dup_string(json_object_get_string(value));
  json_object_put(value);
  json_tokener_free(tokener);
  return key;
}

static int scan_value(struct json_scan * scan);

static int scan_array(struct json_scan * scan)
{
  scan->pos ++;
  scan_whitespace(scan);
  if ((scan->pos < scan->len) && (scan->data[scan->pos] == ']')) {
    scan->pos ++;
    return 0;
  }
  for (;;) {
    if (scan_value(scan) < 0)
      return -1;
    scan_whitespace(scan);
    if ((scan->pos < scan->len) && (scan->data[scan->pos] == ']')) {
      scan->pos ++;
      return 0;
    }
    if ((scan->pos >= scan->len) || (scan->data[scan->pos] != ','))
      return -1;
    scan->pos ++;
    scan_whitespace(scan);
  }
}

static int scan_object(struct json_scan * scan)
{
  char ** keys;
  size_t count;
  size_t capacity;
  int result;

  keys = NULL;
  count = 0;
  capacity = 0;
  result = -1;
  scan->pos ++;
  scan_whitespace(scan);
  if ((scan->pos < scan->len) && (scan->data[scan->pos] == '}')) {
    scan->pos ++;
    return 0;
  }
  for (;;) {
    size_t start;
    size_t len;
    size_t index;
    char * key;

    if (scan_string(scan, &start, &len) < 0)
      goto cleanup;
    key = decode_key(scan->data + start, len);
    if (key == NULL)
      goto cleanup;
    for (index = 0; index < count; index ++) {
      if (strcmp(keys[index], key) == 0) {
        free(key);
        goto cleanup;
      }
    }
    if (count == capacity) {
      char ** resized;
      size_t new_capacity;

      new_capacity = (capacity == 0) ? 8 : capacity * 2;
      resized = realloc(keys, new_capacity * sizeof(* keys));
      if (resized == NULL) {
        free(key);
        goto cleanup;
      }
      keys = resized;
      capacity = new_capacity;
    }
    keys[count ++] = key;
    scan_whitespace(scan);
    if ((scan->pos >= scan->len) || (scan->data[scan->pos] != ':'))
      goto cleanup;
    scan->pos ++;
    if (scan_value(scan) < 0)
      goto cleanup;
    scan_whitespace(scan);
    if ((scan->pos < scan->len) && (scan->data[scan->pos] == '}')) {
      scan->pos ++;
      result = 0;
      goto cleanup;
    }
    if ((scan->pos >= scan->len) || (scan->data[scan->pos] != ','))
      goto cleanup;
    scan->pos ++;
    scan_whitespace(scan);
  }

 cleanup:
  while (count > 0)
    free(keys[-- count]);
  free(keys);
  return result;
}

static int scan_value(struct json_scan * scan)
{
  scan_whitespace(scan);
  if (scan->pos >= scan->len)
    return -1;
  switch (scan->data[scan->pos]) {
  case '{':
    return scan_object(scan);
  case '[':
    return scan_array(scan);
  case '"':
    return scan_string(scan, NULL, NULL);
  default:
    while (scan->pos < scan->len) {
      char ch;

      ch = scan->data[scan->pos];
      if ((ch == ',') || (ch == ']') || (ch == '}') || (ch == ' ') ||
          (ch == '\t') || (ch == '\r') || (ch == '\n'))
        break;
      scan->pos ++;
    }
    return 0;
  }
}

static int duplicate_keys_found(const char * data, size_t len)
{
  struct json_scan scan;

  scan.data = data;
  scan.len = len;
  scan.pos = 0;
  if (scan_value(&scan) < 0)
    return 1;
  scan_whitespace(&scan);
  return scan.pos != scan.len;
}

static int compare_keys(const void * left, const void * right)
{
  const char * const * left_key;
  const char * const * right_key;

  left_key = left;
  right_key = right;
  return strcmp(* left_key, * right_key);
}

static struct json_object * sorted_copy(struct json_object * value)
{
  enum json_type type;

  if (value == NULL)
    return NULL;
  type = json_object_get_type(value);
  if (type == json_type_object) {
    struct json_object * copy;
    char ** keys;
    size_t count;
    size_t index;

    count = json_object_object_length(value);
    keys = (count == 0) ? NULL : malloc(count * sizeof(* keys));
    if ((count != 0) && (keys == NULL))
      return NULL;
    index = 0;
    json_object_object_foreach(value, key, child) {
      (void) child;
      keys[index ++] = (char *) key;
    }
    qsort(keys, count, sizeof(* keys), compare_keys);
    copy = json_object_new_object();
    if (copy == NULL) {
      free(keys);
      return NULL;
    }
    for (index = 0; index < count; index ++) {
      struct json_object * child;
      struct json_object * child_copy;

      if (!json_object_object_get_ex(value, keys[index], &child)) {
        json_object_put(copy);
        free(keys);
        return NULL;
      }
      child_copy = sorted_copy(child);
      if ((child != NULL) && (child_copy == NULL)) {
        json_object_put(copy);
        free(keys);
        return NULL;
      }
      if (json_object_object_add(copy, keys[index], child_copy) < 0) {
        json_object_put(child_copy);
        json_object_put(copy);
        free(keys);
        return NULL;
      }
    }
    free(keys);
    return copy;
  }
  if (type == json_type_array) {
    struct json_object * copy;
    size_t count;
    size_t index;

    count = json_object_array_length(value);
    copy = json_object_new_array();
    if (copy == NULL)
      return NULL;
    for (index = 0; index < count; index ++) {
      struct json_object * child;
      struct json_object * child_copy;

      child = json_object_array_get_idx(value, index);
      child_copy = sorted_copy(child);
      if ((child != NULL) && (child_copy == NULL)) {
        json_object_put(copy);
        return NULL;
      }
      if (json_object_array_add(copy, child_copy) < 0) {
        json_object_put(child_copy);
        json_object_put(copy);
        return NULL;
      }
    }
    return copy;
  }
  return json_object_get(value);
}

int mailjson_parse(const char * data, size_t len, mailjson_value ** result)
{
  struct json_tokener * tokener;
  struct json_object * json;
  enum json_tokener_error error;
  size_t parsed;
  char * terminated;

  if ((data == NULL) || (result == NULL))
    return MAILJSON_ERROR_BAD_STATE;
  * result = NULL;
  if (len >= INT_MAX)
    return MAILJSON_ERROR_PARSE;
  terminated = malloc(len + 1);
  if (terminated == NULL)
    return MAILJSON_ERROR_MEMORY;
  memcpy(terminated, data, len);
  terminated[len] = '\0';
  tokener = json_tokener_new();
  if (tokener == NULL) {
    free(terminated);
    return MAILJSON_ERROR_MEMORY;
  }
  json_tokener_set_flags(tokener,
      JSON_TOKENER_STRICT | JSON_TOKENER_VALIDATE_UTF8);
  json = json_tokener_parse_ex(tokener, terminated, (int) len + 1);
  error = json_tokener_get_error(tokener);
  parsed = json_tokener_get_parse_end(tokener);
  json_tokener_free(tokener);
  free(terminated);
  if ((error != json_tokener_success) || (parsed != len) ||
      duplicate_keys_found(data, len)) {
    json_object_put(json);
    return MAILJSON_ERROR_PARSE;
  }
  return wrap_new(json, result);
}

int mailjson_serialize(mailjson_value * value, int flags,
    char ** result, size_t * result_len)
{
  struct json_object * serialized_value;
  const char * serialized;
  char * data;
  int json_flags;

  if ((value == NULL) || (result == NULL))
    return MAILJSON_ERROR_BAD_STATE;
  serialized_value = value->json;
  if ((flags & MAILJSON_SERIALIZE_SORT_KEYS) != 0) {
    serialized_value = sorted_copy(value->json);
    if ((value->json != NULL) && (serialized_value == NULL))
      return MAILJSON_ERROR_MEMORY;
  }
  json_flags = ((flags & MAILJSON_SERIALIZE_COMPACT) != 0) ?
      JSON_C_TO_STRING_PLAIN : JSON_C_TO_STRING_SPACED;
  json_flags |= JSON_C_TO_STRING_NOSLASHESCAPE;
  serialized = json_object_to_json_string_ext(serialized_value, json_flags);
  data = dup_string(serialized);
  if ((flags & MAILJSON_SERIALIZE_SORT_KEYS) != 0)
    json_object_put(serialized_value);
  if (data == NULL)
    return MAILJSON_ERROR_MEMORY;
  * result = data;
  if (result_len != NULL)
    * result_len = strlen(data);
  return MAILJSON_NO_ERROR;
}

void mailjson_free(mailjson_value * value)
{
  if (value == NULL)
    return;
  json_object_put(value->json);
  free(value);
}

int mailjson_deep_copy(mailjson_value * value, mailjson_value ** result)
{
  struct json_object * copy;

  if ((value == NULL) || (result == NULL))
    return MAILJSON_ERROR_BAD_STATE;
  if (value->json == NULL)
    return wrap_new(NULL, result);
  copy = NULL;
  if (json_object_deep_copy(value->json, &copy, NULL) < 0)
    return MAILJSON_ERROR_MEMORY;
  return wrap_new(copy, result);
}

int mailjson_new_object(mailjson_value ** result)
{
  struct json_object * json;

  if (result == NULL)
    return MAILJSON_ERROR_BAD_STATE;
  json = json_object_new_object();
  if (json == NULL)
    return MAILJSON_ERROR_MEMORY;
  return wrap_new(json, result);
}

int mailjson_new_array(mailjson_value ** result)
{
  struct json_object * json;

  if (result == NULL)
    return MAILJSON_ERROR_BAD_STATE;
  json = json_object_new_array();
  if (json == NULL)
    return MAILJSON_ERROR_MEMORY;
  return wrap_new(json, result);
}

int mailjson_new_string(const char * string, mailjson_value ** result)
{
  struct json_object * json;

  if ((string == NULL) || (result == NULL))
    return MAILJSON_ERROR_BAD_STATE;
  json = json_object_new_string(string);
  if (json == NULL)
    return MAILJSON_ERROR_MEMORY;
  return wrap_new(json, result);
}

int mailjson_new_integer(int64_t integer, mailjson_value ** result)
{
  struct json_object * json;

  if (result == NULL)
    return MAILJSON_ERROR_BAD_STATE;
  * result = NULL;
  json = json_object_new_int64(integer);
  if (json == NULL)
    return MAILJSON_ERROR_MEMORY;
  return wrap_new(json, result);
}

int mailjson_new_boolean(int boolean, mailjson_value ** result)
{
  struct json_object * json;

  if (result == NULL)
    return MAILJSON_ERROR_BAD_STATE;
  json = json_object_new_boolean(boolean != 0);
  if (json == NULL)
    return MAILJSON_ERROR_MEMORY;
  return wrap_new(json, result);
}

int mailjson_new_null(mailjson_value ** result)
{
  if (result == NULL)
    return MAILJSON_ERROR_BAD_STATE;
  return wrap_new(NULL, result);
}

int mailjson_is_object(mailjson_value * value)
{
  return (value != NULL) && json_object_is_type(value->json, json_type_object);
}

int mailjson_is_array(mailjson_value * value)
{
  return (value != NULL) && json_object_is_type(value->json, json_type_array);
}

int mailjson_is_string(mailjson_value * value)
{
  return (value != NULL) && json_object_is_type(value->json, json_type_string);
}

int mailjson_is_integer(mailjson_value * value)
{
  return (value != NULL) && json_object_is_type(value->json, json_type_int);
}

int mailjson_is_boolean(mailjson_value * value)
{
  return (value != NULL) && json_object_is_type(value->json, json_type_boolean);
}

int mailjson_is_null(mailjson_value * value)
{
  return (value != NULL) && json_object_is_type(value->json, json_type_null);
}

int mailjson_object_set_new(mailjson_value * object,
    const char * key, mailjson_value * value)
{
  if ((object == NULL) || (key == NULL) || (value == NULL) ||
      !mailjson_is_object(object))
    return MAILJSON_ERROR_BAD_STATE;
  if (json_object_object_add(object->json, key, value->json) < 0)
    return MAILJSON_ERROR_BAD_STATE;
  value->json = NULL;
  mailjson_free(value);
  return MAILJSON_NO_ERROR;
}

int mailjson_object_get(mailjson_value * object,
    const char * key, mailjson_value ** result)
{
  struct json_object * child;

  if ((object == NULL) || (key == NULL) || (result == NULL) ||
      !mailjson_is_object(object))
    return MAILJSON_ERROR_BAD_STATE;
  if (!json_object_object_get_ex(object->json, key, &child)) {
    * result = NULL;
    return MAILJSON_NO_ERROR;
  }
  return wrap_new(json_object_get(child), result);
}

int mailjson_object_get_string_dup(mailjson_value * object,
    const char * key, char ** result)
{
  mailjson_value * value;
  int r;

  if (result == NULL)
    return MAILJSON_ERROR_BAD_STATE;
  * result = NULL;
  value = NULL;
  r = mailjson_object_get(object, key, &value);
  if ((r != MAILJSON_NO_ERROR) || (value == NULL))
    return r;
  r = mailjson_string_dup(value, result);
  mailjson_free(value);
  return r;
}

int mailjson_object_get_integer(mailjson_value * object,
    const char * key, int64_t * result)
{
  mailjson_value * value;
  int r;

  value = NULL;
  r = mailjson_object_get(object, key, &value);
  if ((r != MAILJSON_NO_ERROR) || (value == NULL))
    return r;
  r = mailjson_integer_value(value, result);
  mailjson_free(value);
  return r;
}

int mailjson_object_get_boolean(mailjson_value * object,
    const char * key, int * result)
{
  mailjson_value * value;
  int r;

  value = NULL;
  r = mailjson_object_get(object, key, &value);
  if ((r != MAILJSON_NO_ERROR) || (value == NULL))
    return r;
  r = mailjson_boolean_value(value, result);
  mailjson_free(value);
  return r;
}

int mailjson_object_foreach(mailjson_value * object,
    mailjson_object_iter_func func, void * context)
{
  int r;

  if ((object == NULL) || (func == NULL) || !mailjson_is_object(object))
    return MAILJSON_ERROR_BAD_STATE;
  json_object_object_foreach(object->json, key, child) {
    mailjson_value wrapped;

    wrapped.json = child;
    r = func(key, &wrapped, context);
    if (r != MAILJSON_NO_ERROR)
      return r;
  }
  return MAILJSON_NO_ERROR;
}

int mailjson_array_append_new(mailjson_value * array, mailjson_value * value)
{
  if ((array == NULL) || (value == NULL) || !mailjson_is_array(array))
    return MAILJSON_ERROR_BAD_STATE;
  if (json_object_array_add(array->json, value->json) < 0)
    return MAILJSON_ERROR_BAD_STATE;
  value->json = NULL;
  mailjson_free(value);
  return MAILJSON_NO_ERROR;
}

size_t mailjson_array_size(mailjson_value * array)
{
  if ((array == NULL) || !mailjson_is_array(array))
    return 0;
  return json_object_array_length(array->json);
}

int mailjson_array_get(mailjson_value * array,
    size_t index, mailjson_value ** result)
{
  struct json_object * child;

  if ((array == NULL) || (result == NULL) || !mailjson_is_array(array))
    return MAILJSON_ERROR_BAD_STATE;
  if (index >= json_object_array_length(array->json)) {
    * result = NULL;
    return MAILJSON_NO_ERROR;
  }
  child = json_object_array_get_idx(array->json, index);
  return wrap_new(json_object_get(child), result);
}

int mailjson_string_dup(mailjson_value * value, char ** result)
{
  const char * string;

  if ((value == NULL) || (result == NULL) || !mailjson_is_string(value))
    return MAILJSON_ERROR_BAD_STATE;
  string = json_object_get_string(value->json);
  * result = dup_string(string);
  if (* result == NULL)
    return MAILJSON_ERROR_MEMORY;
  return MAILJSON_NO_ERROR;
}

int mailjson_integer_value(mailjson_value * value, int64_t * result)
{
  if ((value == NULL) || (result == NULL) || !mailjson_is_integer(value))
    return MAILJSON_ERROR_BAD_STATE;
  * result = json_object_get_int64(value->json);
  return MAILJSON_NO_ERROR;
}

int mailjson_boolean_value(mailjson_value * value, int * result)
{
  if ((value == NULL) || (result == NULL) || !mailjson_is_boolean(value))
    return MAILJSON_ERROR_BAD_STATE;
  * result = json_object_get_boolean(value->json);
  return MAILJSON_NO_ERROR;
}
