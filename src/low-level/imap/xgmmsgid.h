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
  struct mailimap_fetch_att * mailimap_fetch_att_new_xgmmsgid(void);

#ifdef __cplusplus
}
#endif


#endif
