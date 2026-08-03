/*
 * libEtPan! -- a mail stuff library
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailjmap_json.h"

#include <jansson.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>

struct mailjmap_json_value {
  json_t * json;
};

static mailjmap_json_value * value_new(json_t * json)
{
  mailjmap_json_value * value;

  if (json == NULL)
    return NULL;

  value = malloc(sizeof(* value));
  if (value == NULL) {
    json_decref(json);
    return NULL;
  }

  value->json = json;
  return value;
}

static int wrap_new(json_t * json, mailjmap_json_value ** result)
{
  mailjmap_json_value * value;

  if (result == NULL) {
    if (json != NULL)
      json_decref(json);
    return MAILJMAP_ERROR_BAD_STATE;
  }

  value = value_new(json);
  if (value == NULL)
    return MAILJMAP_ERROR_MEMORY;

  * result = value;
  return MAILJMAP_NO_ERROR;
}

static char * dup_string(const char * value)
{
  if (value == NULL)
    return NULL;

  return strdup(value);
}

int mailjmap_json_parse(const char * data, size_t len,
    mailjmap_json_value ** result)
{
  json_error_t error;
  json_t * json;

  if ((data == NULL) || (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  * result = NULL;
  json = json_loadb(data, len, JSON_DECODE_ANY | JSON_REJECT_DUPLICATES,
      &error);
  if (json == NULL)
    return MAILJMAP_ERROR_JSON_PARSE;

  return wrap_new(json, result);
}

int mailjmap_json_serialize(mailjmap_json_value * value, int flags,
    char ** result, size_t * result_len)
{
  size_t json_flags;
  char * data;

  if ((value == NULL) || (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  json_flags = 0;
  if ((flags & MAILJMAP_JSON_SERIALIZE_COMPACT) != 0)
    json_flags |= JSON_COMPACT;
  if ((flags & MAILJMAP_JSON_SERIALIZE_SORT_KEYS) != 0)
    json_flags |= JSON_SORT_KEYS;

  data = json_dumps(value->json, json_flags);
  if (data == NULL)
    return MAILJMAP_ERROR_MEMORY;

  * result = data;
  if (result_len != NULL)
    * result_len = strlen(data);

  return MAILJMAP_NO_ERROR;
}

void mailjmap_json_free(mailjmap_json_value * value)
{
  if (value == NULL)
    return;

  if (value->json != NULL)
    json_decref(value->json);
  free(value);
}

int mailjmap_json_deep_copy(mailjmap_json_value * value,
    mailjmap_json_value ** result)
{
  json_t * copy;

  if ((value == NULL) || (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  copy = json_deep_copy(value->json);
  if (copy == NULL)
    return MAILJMAP_ERROR_MEMORY;

  return wrap_new(copy, result);
}

int mailjmap_json_new_object(mailjmap_json_value ** result)
{
  return wrap_new(json_object(), result);
}

int mailjmap_json_new_array(mailjmap_json_value ** result)
{
  return wrap_new(json_array(), result);
}

int mailjmap_json_new_string(const char * string,
    mailjmap_json_value ** result)
{
  if (string == NULL)
    return MAILJMAP_ERROR_BAD_STATE;

  return wrap_new(json_string(string), result);
}

int mailjmap_json_new_integer(int64_t integer,
    mailjmap_json_value ** result)
{
  if (result == NULL)
    return MAILJMAP_ERROR_BAD_STATE;

  * result = NULL;
  if ((sizeof(json_int_t) < sizeof(int64_t)) &&
      ((integer < (int64_t) LONG_MIN) || (integer > (int64_t) LONG_MAX)))
    return MAILJMAP_ERROR_BAD_STATE;

  return wrap_new(json_integer((json_int_t) integer), result);
}

int mailjmap_json_new_boolean(int boolean,
    mailjmap_json_value ** result)
{
  return wrap_new(json_boolean(boolean != 0), result);
}

int mailjmap_json_new_null(mailjmap_json_value ** result)
{
  return wrap_new(json_null(), result);
}

int mailjmap_json_is_object(mailjmap_json_value * value)
{
  return (value != NULL) && json_is_object(value->json);
}

int mailjmap_json_is_array(mailjmap_json_value * value)
{
  return (value != NULL) && json_is_array(value->json);
}

int mailjmap_json_is_string(mailjmap_json_value * value)
{
  return (value != NULL) && json_is_string(value->json);
}

int mailjmap_json_is_integer(mailjmap_json_value * value)
{
  return (value != NULL) && json_is_integer(value->json);
}

int mailjmap_json_is_boolean(mailjmap_json_value * value)
{
  return (value != NULL) && json_is_boolean(value->json);
}

int mailjmap_json_is_null(mailjmap_json_value * value)
{
  return (value != NULL) && json_is_null(value->json);
}

int mailjmap_json_object_set_new(mailjmap_json_value * object,
    const char * key, mailjmap_json_value * value)
{
  int r;

  if ((object == NULL) || (key == NULL) || (value == NULL) ||
      !json_is_object(object->json))
    return MAILJMAP_ERROR_BAD_STATE;

  r = json_object_set_new(object->json, key, value->json);
  if (r < 0)
    return MAILJMAP_ERROR_BAD_STATE;

  value->json = NULL;
  mailjmap_json_free(value);
  return MAILJMAP_NO_ERROR;
}

int mailjmap_json_object_get(mailjmap_json_value * object,
    const char * key, mailjmap_json_value ** result)
{
  json_t * child;

  if ((object == NULL) || (key == NULL) || (result == NULL) ||
      !json_is_object(object->json))
    return MAILJMAP_ERROR_BAD_STATE;

  child = json_object_get(object->json, key);
  if (child == NULL) {
    * result = NULL;
    return MAILJMAP_NO_ERROR;
  }

  json_incref(child);
  return wrap_new(child, result);
}

int mailjmap_json_object_get_string_dup(mailjmap_json_value * object,
    const char * key, char ** result)
{
  mailjmap_json_value * value;
  int r;

  if (result == NULL)
    return MAILJMAP_ERROR_BAD_STATE;

  * result = NULL;
  value = NULL;
  r = mailjmap_json_object_get(object, key, &value);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  if (value == NULL)
    return MAILJMAP_NO_ERROR;

  r = mailjmap_json_string_dup(value, result);
  mailjmap_json_free(value);
  return r;
}

int mailjmap_json_object_get_integer(mailjmap_json_value * object,
    const char * key, int64_t * result)
{
  mailjmap_json_value * value;
  int r;

  value = NULL;
  r = mailjmap_json_object_get(object, key, &value);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  if (value == NULL)
    return MAILJMAP_NO_ERROR;

  r = mailjmap_json_integer_value(value, result);
  mailjmap_json_free(value);
  return r;
}

int mailjmap_json_object_get_boolean(mailjmap_json_value * object,
    const char * key, int * result)
{
  mailjmap_json_value * value;
  int r;

  value = NULL;
  r = mailjmap_json_object_get(object, key, &value);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  if (value == NULL)
    return MAILJMAP_NO_ERROR;

  r = mailjmap_json_boolean_value(value, result);
  mailjmap_json_free(value);
  return r;
}

int mailjmap_json_object_foreach(mailjmap_json_value * object,
    mailjmap_json_object_iter_func func, void * context)
{
  const char * key;
  json_t * child;
  int r;

  if ((object == NULL) || (func == NULL) || !json_is_object(object->json))
    return MAILJMAP_ERROR_BAD_STATE;

  json_object_foreach(object->json, key, child) {
    mailjmap_json_value wrapped;

    wrapped.json = child;
    r = func(key, &wrapped, context);
    if (r != MAILJMAP_NO_ERROR)
      return r;
  }

  return MAILJMAP_NO_ERROR;
}

int mailjmap_json_array_append_new(mailjmap_json_value * array,
    mailjmap_json_value * value)
{
  int r;

  if ((array == NULL) || (value == NULL) || !json_is_array(array->json))
    return MAILJMAP_ERROR_BAD_STATE;

  r = json_array_append_new(array->json, value->json);
  if (r < 0)
    return MAILJMAP_ERROR_BAD_STATE;

  value->json = NULL;
  mailjmap_json_free(value);
  return MAILJMAP_NO_ERROR;
}

size_t mailjmap_json_array_size(mailjmap_json_value * array)
{
  if ((array == NULL) || !json_is_array(array->json))
    return 0;

  return json_array_size(array->json);
}

int mailjmap_json_array_get(mailjmap_json_value * array,
    size_t index, mailjmap_json_value ** result)
{
  json_t * child;

  if ((array == NULL) || (result == NULL) || !json_is_array(array->json))
    return MAILJMAP_ERROR_BAD_STATE;

  child = json_array_get(array->json, index);
  if (child == NULL) {
    * result = NULL;
    return MAILJMAP_NO_ERROR;
  }

  json_incref(child);
  return wrap_new(child, result);
}

int mailjmap_json_string_dup(mailjmap_json_value * value, char ** result)
{
  const char * string;

  if ((value == NULL) || (result == NULL) || !json_is_string(value->json))
    return MAILJMAP_ERROR_BAD_STATE;

  string = json_string_value(value->json);
  * result = dup_string(string);
  if (* result == NULL)
    return MAILJMAP_ERROR_MEMORY;

  return MAILJMAP_NO_ERROR;
}

int mailjmap_json_integer_value(mailjmap_json_value * value,
    int64_t * result)
{
  if ((value == NULL) || (result == NULL) || !json_is_integer(value->json))
    return MAILJMAP_ERROR_BAD_STATE;

  * result = (int64_t) json_integer_value(value->json);
  return MAILJMAP_NO_ERROR;
}

int mailjmap_json_boolean_value(mailjmap_json_value * value, int * result)
{
  if ((value == NULL) || (result == NULL) || !json_is_boolean(value->json))
    return MAILJMAP_ERROR_BAD_STATE;

  * result = json_is_true(value->json);
  return MAILJMAP_NO_ERROR;
}
