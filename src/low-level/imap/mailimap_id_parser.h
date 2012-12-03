#ifndef MAILIMAP_ID_PARSER_H

#define MAILIMAP_ID_PARSER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libetpan/libetpan-config.h>
#include <libetpan/mailimap_extension.h>
#include <libetpan/mailimap_id_types.h>

int mailimap_id_parse(int calling_parser, mailstream * fd,
    MMAPString * buffer, size_t * indx,
    struct mailimap_extension_data ** result,
    size_t progr_rate,
    progress_function * progr_fun);

#ifdef __cplusplus
}
#endif

#endif
