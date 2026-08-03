/*
 * libEtPan! -- a mail stuff library
 */

#ifndef MAILJMAP_JSON_H

#define MAILJMAP_JSON_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libetpan/mailjmap_types.h>

#include <stddef.h>
#include <stdint.h>

#define MAILJMAP_JSON_SERIALIZE_COMPACT 1
#define MAILJMAP_JSON_SERIALIZE_SORT_KEYS 2

typedef struct mailjmap_json_value mailjmap_json_value;

typedef int (* mailjmap_json_object_iter_func)(const char * key,
    mailjmap_json_value * value, void * context);

LIBETPAN_EXPORT
int mailjmap_json_parse(const char * data, size_t len,
    mailjmap_json_value ** result);

LIBETPAN_EXPORT
int mailjmap_json_serialize(mailjmap_json_value * value, int flags,
    char ** result, size_t * result_len);

LIBETPAN_EXPORT
void mailjmap_json_free(mailjmap_json_value * value);
LIBETPAN_EXPORT
int mailjmap_json_deep_copy(mailjmap_json_value * value,
    mailjmap_json_value ** result);

LIBETPAN_EXPORT
int mailjmap_json_new_object(mailjmap_json_value ** result);
LIBETPAN_EXPORT
int mailjmap_json_new_array(mailjmap_json_value ** result);
LIBETPAN_EXPORT
int mailjmap_json_new_string(const char * value,
    mailjmap_json_value ** result);
LIBETPAN_EXPORT
int mailjmap_json_new_integer(int64_t value,
    mailjmap_json_value ** result);
LIBETPAN_EXPORT
int mailjmap_json_new_boolean(int value,
    mailjmap_json_value ** result);
LIBETPAN_EXPORT
int mailjmap_json_new_null(mailjmap_json_value ** result);

LIBETPAN_EXPORT
int mailjmap_json_is_object(mailjmap_json_value * value);
LIBETPAN_EXPORT
int mailjmap_json_is_array(mailjmap_json_value * value);
LIBETPAN_EXPORT
int mailjmap_json_is_string(mailjmap_json_value * value);
LIBETPAN_EXPORT
int mailjmap_json_is_integer(mailjmap_json_value * value);
LIBETPAN_EXPORT
int mailjmap_json_is_boolean(mailjmap_json_value * value);
LIBETPAN_EXPORT
int mailjmap_json_is_null(mailjmap_json_value * value);

LIBETPAN_EXPORT
int mailjmap_json_object_set_new(mailjmap_json_value * object,
    const char * key, mailjmap_json_value * value);
LIBETPAN_EXPORT
int mailjmap_json_object_get(mailjmap_json_value * object,
    const char * key, mailjmap_json_value ** result);
LIBETPAN_EXPORT
int mailjmap_json_object_get_string_dup(mailjmap_json_value * object,
    const char * key, char ** result);
LIBETPAN_EXPORT
int mailjmap_json_object_get_integer(mailjmap_json_value * object,
    const char * key, int64_t * result);
LIBETPAN_EXPORT
int mailjmap_json_object_get_boolean(mailjmap_json_value * object,
    const char * key, int * result);
LIBETPAN_EXPORT
int mailjmap_json_object_foreach(mailjmap_json_value * object,
    mailjmap_json_object_iter_func func, void * context);

LIBETPAN_EXPORT
int mailjmap_json_array_append_new(mailjmap_json_value * array,
    mailjmap_json_value * value);
LIBETPAN_EXPORT
size_t mailjmap_json_array_size(mailjmap_json_value * array);
LIBETPAN_EXPORT
int mailjmap_json_array_get(mailjmap_json_value * array,
    size_t index, mailjmap_json_value ** result);

LIBETPAN_EXPORT
int mailjmap_json_string_dup(mailjmap_json_value * value, char ** result);
LIBETPAN_EXPORT
int mailjmap_json_integer_value(mailjmap_json_value * value,
    int64_t * result);
LIBETPAN_EXPORT
int mailjmap_json_boolean_value(mailjmap_json_value * value, int * result);

#ifdef __cplusplus
}
#endif

#endif
