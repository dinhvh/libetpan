#ifndef XGMTHRID_H
#define XGMTHRID_H

#ifdef __cplusplus
extern "C" {
#endif
  
#include <libetpan/libetpan-config.h>
#include <libetpan/mailimap_extension.h>

  LIBETPAN_EXPORT
  extern struct mailimap_extension_api mailimap_extension_xgmthrid;
  
  LIBETPAN_EXPORT
  int
  mailimap_fetch_xgmthrid(mailimap * session,
                          struct mailimap_set * set,
                          clist ** results);

#ifdef __cplusplus
}
#endif


#endif
