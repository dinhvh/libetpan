/*
 * libEtPan! -- a mail stuff library
 */

#ifndef MAILJMAP_BLOB_H

#define MAILJMAP_BLOB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libetpan/mailjmap_types.h>

#include <stddef.h>

LIBETPAN_EXPORT
int mailjmap_upload(mailjmap * session,
    const char * account_id,
    const char * content_type,
    const char * data,
    size_t data_len,
    struct mailjmap_blob_upload ** result);

LIBETPAN_EXPORT
int mailjmap_download(mailjmap * session,
    const char * account_id,
    const char * blob_id,
    const char * name,
    const char * accept,
    char ** data,
    size_t * data_len);

#ifdef __cplusplus
}
#endif

#endif
