/*
 * libEtPan! -- a mail stuff library
 */

#ifndef MAILJMAP_REQUEST_H

#define MAILJMAP_REQUEST_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libetpan/mailjson.h>

struct mailjmap_request;

LIBETPAN_EXPORT
struct mailjmap_request * mailjmap_request_new(void);
LIBETPAN_EXPORT
void mailjmap_request_free(struct mailjmap_request * request);

LIBETPAN_EXPORT
int mailjmap_request_add_capability(struct mailjmap_request * request,
    const char * capability);

LIBETPAN_EXPORT
int mailjmap_request_add_call_json(struct mailjmap_request * request,
    const char * name, mailjson_value * arguments,
    const char * call_id);

LIBETPAN_EXPORT
int mailjmap_request_add_call_empty(struct mailjmap_request * request,
    const char * name,
    const char * call_id);

LIBETPAN_EXPORT
int mailjmap_request_add_string_argument(mailjson_value * arguments,
    const char * key,
    const char * value);

LIBETPAN_EXPORT
int mailjmap_request_arguments_set_result_reference(
    mailjson_value * arguments,
    const char * key,
    const char * result_of,
    const char * name,
    const char * path);

LIBETPAN_EXPORT
const char * mailjmap_request_method_name_for_call_id(
    struct mailjmap_request * request,
    const char * call_id);

LIBETPAN_EXPORT
int mailjmap_request_serialize(struct mailjmap_request * request,
    mailjson_value ** result);

#ifdef __cplusplus
}
#endif

#endif
