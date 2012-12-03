#ifndef MAILIMAP_ID_H

#define MAILIMAP_ID_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libetpan/libetpan-config.h>
#include <libetpan/mailimap_extension.h>
#include <libetpan/mailimap_id_types.h>

extern struct mailimap_extension_api mailimap_extension_id;

LIBETPAN_EXPORT
int mailimap_id(mailimap * session, struct mailimap_id_params_list * client_identification,
                struct mailimap_id_params_list ** result);

/* Helpers */

/* result must be freed */
LIBETPAN_EXPORT
int mailimap_id_basic(mailimap * session, const char * name, const char * version,
                      char ** p_server_name, char ** p_server_version);

#ifdef __cplusplus
}
#endif

#endif
