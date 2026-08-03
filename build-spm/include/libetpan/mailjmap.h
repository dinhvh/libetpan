/*
 * libEtPan! -- a mail stuff library
 */

#ifndef MAILJMAP_H

#define MAILJMAP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libetpan/mailjmap_types.h>
#include <libetpan/mailjmap_http.h>
#include <libetpan/mailjmap_request.h>
#include <libetpan/mailjmap_response.h>
#include <libetpan/mailjmap_mail.h>
#include <libetpan/mailjmap_blob.h>

#include <stddef.h>
#include <time.h>

LIBETPAN_EXPORT
mailjmap * mailjmap_new(int cached, const char * cache_directory);

LIBETPAN_EXPORT
void mailjmap_free(mailjmap * session);

LIBETPAN_EXPORT
int mailjmap_set_http_transport(mailjmap * session,
    struct mailjmap_http_transport * transport);

LIBETPAN_EXPORT
int mailjmap_connect(mailjmap * session, const char * session_url);

LIBETPAN_EXPORT
int mailjmap_discover(mailjmap * session,
    const char * domain_or_email,
    struct mailjmap_session ** result);

LIBETPAN_EXPORT
int mailjmap_login_oauth2(mailjmap * session,
    const char * user, const char * access_token);

LIBETPAN_EXPORT
int mailjmap_set_oauth2_token(mailjmap * session,
    const char * access_token);

LIBETPAN_EXPORT
int mailjmap_set_timeout(mailjmap * session, time_t timeout);

LIBETPAN_EXPORT
int mailjmap_get_last_http_status(mailjmap * session);

LIBETPAN_EXPORT
const char * mailjmap_get_last_error_message(mailjmap * session);

LIBETPAN_EXPORT
const char * mailjmap_get_last_problem_type(mailjmap * session);

LIBETPAN_EXPORT
const char * mailjmap_get_last_method_name(mailjmap * session);

LIBETPAN_EXPORT
const char * mailjmap_get_last_method_call_id(mailjmap * session);

LIBETPAN_EXPORT
const char * mailjmap_get_last_method_error_type(mailjmap * session);

LIBETPAN_EXPORT
const char * mailjmap_get_last_method_error_description(mailjmap * session);

LIBETPAN_EXPORT
int mailjmap_get_session(mailjmap * session,
    struct mailjmap_session ** result);

LIBETPAN_EXPORT
int mailjmap_call(mailjmap * session,
    struct mailjmap_request * request,
    struct mailjmap_response ** result);

#ifdef __cplusplus
}
#endif

#endif
