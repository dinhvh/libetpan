#ifndef MAILIMAP_ID_SENDER_H

#define MAILIMAP_ID_SENDER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libetpan/libetpan-config.h>
#include <libetpan/mailimap_extension.h>
#include <libetpan/mailimap_id_types.h>

LIBETPAN_EXPORT
int mailimap_id_send(mailstream * fd, struct mailimap_id_params_list * client_identification);

#ifdef __cplusplus
}
#endif

#endif
