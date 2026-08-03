/*
 * libEtPan! -- a mail stuff library
 */

#ifndef MAILJMAP_TYPES_H

#define MAILJMAP_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libetpan/libetpan-config.h>
#include <libetpan/clist.h>

#include <stddef.h>

enum {
  MAILJMAP_NO_ERROR = 0,
  MAILJMAP_ERROR_BAD_STATE,
  MAILJMAP_ERROR_AUTHENTICATION,
  MAILJMAP_ERROR_DISCOVERY,
  MAILJMAP_ERROR_HTTP,
  MAILJMAP_ERROR_JSON_PARSE,
  MAILJMAP_ERROR_PROTOCOL,
  MAILJMAP_ERROR_CAPABILITY,
  MAILJMAP_ERROR_METHOD,
  MAILJMAP_ERROR_LIMIT,
  MAILJMAP_ERROR_STREAM,
  MAILJMAP_ERROR_MEMORY
};

typedef struct mailjmap mailjmap;

struct mailjmap_session_capability {
  char * capability;
  char * json;
};

struct mailjmap_session_account {
  char * account_id;
  char * name;
  int is_personal;
  int is_read_only;
  clist * capabilities; /* char * */
  clist * capability_details; /* struct mailjmap_session_capability * */
};

struct mailjmap_session_primary_account {
  char * capability;
  char * account_id;
};

struct mailjmap_session {
  char * api_url;
  char * upload_url;
  char * download_url;
  char * event_source_url;
  char * session_state;
  clist * capabilities; /* char * */
  clist * capability_details; /* struct mailjmap_session_capability * */
  clist * accounts; /* struct mailjmap_session_account * */
  clist * primary_accounts; /* struct mailjmap_session_primary_account * */
};

struct mailjmap_blob_upload {
  char * account_id;
  char * blob_id;
  char * type;
  char * name;
  size_t size;
};

LIBETPAN_EXPORT
struct mailjmap_session_capability *
mailjmap_session_capability_new(const char * capability,
    const char * json);

LIBETPAN_EXPORT
void mailjmap_session_capability_free(
    struct mailjmap_session_capability * capability);

LIBETPAN_EXPORT
struct mailjmap_session_account *
mailjmap_session_account_new(const char * account_id);

LIBETPAN_EXPORT
void mailjmap_session_account_free(
    struct mailjmap_session_account * account);

LIBETPAN_EXPORT
struct mailjmap_session_primary_account *
mailjmap_session_primary_account_new(const char * capability,
    const char * account_id);

LIBETPAN_EXPORT
void mailjmap_session_primary_account_free(
    struct mailjmap_session_primary_account * primary_account);

LIBETPAN_EXPORT
struct mailjmap_session * mailjmap_session_new(void);

LIBETPAN_EXPORT
void mailjmap_session_free(struct mailjmap_session * session);

LIBETPAN_EXPORT
struct mailjmap_blob_upload * mailjmap_blob_upload_new(void);

LIBETPAN_EXPORT
void mailjmap_blob_upload_free(struct mailjmap_blob_upload * upload);

#ifdef __cplusplus
}
#endif

#endif
