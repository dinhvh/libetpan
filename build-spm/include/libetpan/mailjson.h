/*
 * libEtPan! -- a mail stuff library
 */

#ifndef MAILJSON_H

#define MAILJSON_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libetpan/libetpan-config.h>

#include <stddef.h>
#include <stdint.h>

enum {
  MAILJSON_NO_ERROR = 0,
  MAILJSON_ERROR_BAD_STATE,
  MAILJSON_ERROR_PARSE = 5,
  MAILJSON_ERROR_MEMORY = 11
};

#define MAILJSON_SERIALIZE_COMPACT 1
#define MAILJSON_SERIALIZE_SORT_KEYS 2

typedef struct mailjson_value mailjson_value;

typedef int (* mailjson_object_iter_func)(const char * key,
    mailjson_value * value, void * context);

LIBETPAN_EXPORT
int mailjson_parse(const char * data, size_t len, mailjson_value ** result);

LIBETPAN_EXPORT
int mailjson_serialize(mailjson_value * value, int flags,
    char ** result, size_t * result_len);

LIBETPAN_EXPORT
void mailjson_free(mailjson_value * value);
LIBETPAN_EXPORT
int mailjson_deep_copy(mailjson_value * value, mailjson_value ** result);

LIBETPAN_EXPORT
int mailjson_new_object(mailjson_value ** result);
LIBETPAN_EXPORT
int mailjson_new_array(mailjson_value ** result);
LIBETPAN_EXPORT
int mailjson_new_string(const char * value, mailjson_value ** result);
LIBETPAN_EXPORT
int mailjson_new_integer(int64_t value, mailjson_value ** result);
LIBETPAN_EXPORT
int mailjson_new_boolean(int value, mailjson_value ** result);
LIBETPAN_EXPORT
int mailjson_new_null(mailjson_value ** result);

LIBETPAN_EXPORT
int mailjson_is_object(mailjson_value * value);
LIBETPAN_EXPORT
int mailjson_is_array(mailjson_value * value);
LIBETPAN_EXPORT
int mailjson_is_string(mailjson_value * value);
LIBETPAN_EXPORT
int mailjson_is_integer(mailjson_value * value);
LIBETPAN_EXPORT
int mailjson_is_boolean(mailjson_value * value);
LIBETPAN_EXPORT
int mailjson_is_null(mailjson_value * value);

LIBETPAN_EXPORT
int mailjson_object_set_new(mailjson_value * object,
    const char * key, mailjson_value * value);
LIBETPAN_EXPORT
int mailjson_object_get(mailjson_value * object,
    const char * key, mailjson_value ** result);
LIBETPAN_EXPORT
int mailjson_object_get_string_dup(mailjson_value * object,
    const char * key, char ** result);
LIBETPAN_EXPORT
int mailjson_object_get_integer(mailjson_value * object,
    const char * key, int64_t * result);
LIBETPAN_EXPORT
int mailjson_object_get_boolean(mailjson_value * object,
    const char * key, int * result);
LIBETPAN_EXPORT
int mailjson_object_foreach(mailjson_value * object,
    mailjson_object_iter_func func, void * context);

LIBETPAN_EXPORT
int mailjson_array_append_new(mailjson_value * array, mailjson_value * value);
LIBETPAN_EXPORT
size_t mailjson_array_size(mailjson_value * array);
LIBETPAN_EXPORT
int mailjson_array_get(mailjson_value * array,
    size_t index, mailjson_value ** result);

LIBETPAN_EXPORT
int mailjson_string_dup(mailjson_value * value, char ** result);
LIBETPAN_EXPORT
int mailjson_integer_value(mailjson_value * value, int64_t * result);
LIBETPAN_EXPORT
int mailjson_boolean_value(mailjson_value * value, int * result);

#ifdef __cplusplus
}
#endif

#endif
