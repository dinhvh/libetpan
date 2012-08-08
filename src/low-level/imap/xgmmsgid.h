#ifndef XGMMSGID_H
#define XGMMSGID_H

#ifdef __cplusplus
extern "C" {
#endif
  
#include <libetpan/libetpan-config.h>
#include <libetpan/mailimap_extension.h>

  LIBETPAN_EXPORT
  extern struct mailimap_extension_api mailimap_extension_xgmmsgid;
  
  LIBETPAN_EXPORT
  int
  mailimap_fetch_xgmmsgid(mailimap * session,
                          struct mailimap_set * set,
                          clist ** results);

#ifdef __cplusplus
}
#endif


#endif
