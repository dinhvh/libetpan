/*
 * libEtPan! -- a mail stuff library
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailjson.h"

#include <jansson.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>

struct mailjson_value {
  json_t * json;
};

static mailjson_value * value_new(json_t * json)
{
  mailjson_value * value;

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

static int wrap_new(json_t * json, mailjson_value ** result)
{
  mailjson_value * value;

  if (result == NULL) {
    if (json != NULL)
      json_decref(json);
    return MAILJSON_ERROR_BAD_STATE;
  }

  value = value_new(json);
  if (value == NULL)
    return MAILJSON_ERROR_MEMORY;

  * result = value;
  return MAILJSON_NO_ERROR;
}

static char * dup_string(const char * value)
{
  if (value == NULL)
    return NULL;

  return strdup(value);
}

int mailjson_parse(const char * data, size_t len, mailjson_value ** result)
{
  json_error_t error;
  json_t * json;

  if ((data == NULL) || (result == NULL))
    return MAILJSON_ERROR_BAD_STATE;

  * result = NULL;
  json = json_loadb(data, len, JSON_DECODE_ANY | JSON_REJECT_DUPLICATES,
      &error);
  if (json == NULL)
    return MAILJSON_ERROR_PARSE;

  return wrap_new(json, result);
}

int mailjson_serialize(mailjson_value * value, int flags,
    char ** result, size_t * result_len)
{
  size_t json_flags;
  char * data;

  if ((value == NULL) || (result == NULL))
    return MAILJSON_ERROR_BAD_STATE;

  json_flags = 0;
  if ((flags & MAILJSON_SERIALIZE_COMPACT) != 0)
    json_flags |= JSON_COMPACT;
  if ((flags & MAILJSON_SERIALIZE_SORT_KEYS) != 0)
    json_flags |= JSON_SORT_KEYS;

  data = json_dumps(value->json, json_flags);
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

  if (value->json != NULL)
    json_decref(value->json);
  free(value);
}

int mailjson_deep_copy(mailjson_value * value, mailjson_value ** result)
{
  json_t * copy;

  if ((value == NULL) || (result == NULL))
    return MAILJSON_ERROR_BAD_STATE;

  copy = json_deep_copy(value->json);
  if (copy == NULL)
    return MAILJSON_ERROR_MEMORY;

  return wrap_new(copy, result);
}

int mailjson_new_object(mailjson_value ** result)
{
  return wrap_new(json_object(), result);
}

int mailjson_new_array(mailjson_value ** result)
{
  return wrap_new(json_array(), result);
}

int mailjson_new_string(const char * string, mailjson_value ** result)
{
  if (string == NULL)
    return MAILJSON_ERROR_BAD_STATE;

  return wrap_new(json_string(string), result);
}

int mailjson_new_integer(int64_t integer, mailjson_value ** result)
{
  if (result == NULL)
    return MAILJSON_ERROR_BAD_STATE;

  * result = NULL;
  if ((sizeof(json_int_t) < sizeof(int64_t)) &&
      ((integer < (int64_t) LONG_MIN) || (integer > (int64_t) LONG_MAX)))
    return MAILJSON_ERROR_BAD_STATE;

  return wrap_new(json_integer((json_int_t) integer), result);
}

int mailjson_new_boolean(int boolean, mailjson_value ** result)
{
  return wrap_new(json_boolean(boolean != 0), result);
}

int mailjson_new_null(mailjson_value ** result)
{
  return wrap_new(json_null(), result);
}

int mailjson_is_object(mailjson_value * value)
{
  return (value != NULL) && json_is_object(value->json);
}

int mailjson_is_array(mailjson_value * value)
{
  return (value != NULL) && json_is_array(value->json);
}

int mailjson_is_string(mailjson_value * value)
{
  return (value != NULL) && json_is_string(value->json);
}

int mailjson_is_integer(mailjson_value * value)
{
  return (value != NULL) && json_is_integer(value->json);
}

int mailjson_is_boolean(mailjson_value * value)
{
  return (value != NULL) && json_is_boolean(value->json);
}

int mailjson_is_null(mailjson_value * value)
{
  return (value != NULL) && json_is_null(value->json);
}

int mailjson_object_set_new(mailjson_value * object,
    const char * key, mailjson_value * value)
{
  int r;

  if ((object == NULL) || (key == NULL) || (value == NULL) ||
      !mailjson_is_object(object))
    return MAILJSON_ERROR_BAD_STATE;

  r = json_object_set_new(object->json, key, value->json);
  if (r < 0)
    return MAILJSON_ERROR_BAD_STATE;

  value->json = NULL;
  mailjson_free(value);
  return MAILJSON_NO_ERROR;
}

int mailjson_object_get(mailjson_value * object,
    const char * key, mailjson_value ** result)
{
  json_t * child;

  if ((object == NULL) || (key == NULL) || (result == NULL) ||
      !mailjson_is_object(object))
    return MAILJSON_ERROR_BAD_STATE;

  child = json_object_get(object->json, key);
  if (child == NULL) {
    * result = NULL;
    return MAILJSON_NO_ERROR;
  }

  json_incref(child);
  return wrap_new(child, result);
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
  if (r != MAILJSON_NO_ERROR)
    return r;
  if (value == NULL)
    return MAILJSON_NO_ERROR;

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
  if (r != MAILJSON_NO_ERROR)
    return r;
  if (value == NULL)
    return MAILJSON_NO_ERROR;

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
  if (r != MAILJSON_NO_ERROR)
    return r;
  if (value == NULL)
    return MAILJSON_NO_ERROR;

  r = mailjson_boolean_value(value, result);
  mailjson_free(value);
  return r;
}

int mailjson_object_foreach(mailjson_value * object,
    mailjson_object_iter_func func, void * context)
{
  const char * key;
  json_t * child;
  int r;

  if ((object == NULL) || (func == NULL) || !mailjson_is_object(object))
    return MAILJSON_ERROR_BAD_STATE;

  json_object_foreach(object->json, key, child) {
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
  int r;

  if ((array == NULL) || (value == NULL) || !mailjson_is_array(array))
    return MAILJSON_ERROR_BAD_STATE;

  r = json_array_append_new(array->json, value->json);
  if (r < 0)
    return MAILJSON_ERROR_BAD_STATE;

  value->json = NULL;
  mailjson_free(value);
  return MAILJSON_NO_ERROR;
}

size_t mailjson_array_size(mailjson_value * array)
{
  if ((array == NULL) || !mailjson_is_array(array))
    return 0;

  return json_array_size(array->json);
}

int mailjson_array_get(mailjson_value * array,
    size_t index, mailjson_value ** result)
{
  json_t * child;

  if ((array == NULL) || (result == NULL) || !mailjson_is_array(array))
    return MAILJSON_ERROR_BAD_STATE;

  child = json_array_get(array->json, index);
  if (child == NULL) {
    * result = NULL;
    return MAILJSON_NO_ERROR;
  }

  json_incref(child);
  return wrap_new(child, result);
}

int mailjson_string_dup(mailjson_value * value, char ** result)
{
  const char * string;

  if ((value == NULL) || (result == NULL) || !mailjson_is_string(value))
    return MAILJSON_ERROR_BAD_STATE;

  string = json_string_value(value->json);
  * result = dup_string(string);
  if (* result == NULL)
    return MAILJSON_ERROR_MEMORY;

  return MAILJSON_NO_ERROR;
}

int mailjson_integer_value(mailjson_value * value, int64_t * result)
{
  if ((value == NULL) || (result == NULL) || !mailjson_is_integer(value))
    return MAILJSON_ERROR_BAD_STATE;

  * result = (int64_t) json_integer_value(value->json);
  return MAILJSON_NO_ERROR;
}

int mailjson_boolean_value(mailjson_value * value, int * result)
{
  if ((value == NULL) || (result == NULL) || !mailjson_is_boolean(value))
    return MAILJSON_ERROR_BAD_STATE;

  * result = json_is_true(value->json);
  return MAILJSON_NO_ERROR;
}
