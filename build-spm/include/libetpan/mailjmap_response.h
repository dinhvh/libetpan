/*
 * libEtPan! -- a mail stuff library
 */

#ifndef MAILJMAP_RESPONSE_H

#define MAILJMAP_RESPONSE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libetpan/mailjmap_json.h>

#include <libetpan/clist.h>

#include <stddef.h>

struct mailjmap_method_response {
  char * name;
  mailjmap_json_value * arguments;
  char * call_id;
};

struct mailjmap_response {
  char * session_state;
  clist * method_responses; /* struct mailjmap_method_response * */
  char * error_type;
  char * error_detail;
};

LIBETPAN_EXPORT
struct mailjmap_method_response *
mailjmap_method_response_new(const char * name,
    mailjmap_json_value * arguments, const char * call_id);

LIBETPAN_EXPORT
void mailjmap_method_response_free(
    struct mailjmap_method_response * response);

LIBETPAN_EXPORT
struct mailjmap_response * mailjmap_response_new(void);
LIBETPAN_EXPORT
void mailjmap_response_free(struct mailjmap_response * response);

LIBETPAN_EXPORT
int mailjmap_response_parse(const char * data, size_t data_len,
    struct mailjmap_response ** result);

#ifdef __cplusplus
}
#endif

#endif
