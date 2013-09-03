#ifndef XGMMSGID_H
#define XGMMSGID_H

#ifdef __cplusplus
extern "C" {
#endif
  
#include <libetpan/libetpan-config.h>
#include <libetpan/mailimap_extension.h>

  enum {
    MAILIMAP_XGMMSGID_TYPE_MSGID
  };

  LIBETPAN_EXPORT
  extern struct mailimap_extension_api mailimap_extension_xgmmsgid;

  LIBETPAN_EXPORT
  struct mailimap_fetch_att * mailimap_fetch_att_new_xgmmsgid(void);

  LIBETPAN_EXPORT
  int mailimap_has_xgmmsgid(mailimap * session);

  LIBETPAN_EXPORT
  int
  mailimap_fetch_xgmmsgid(mailimap * session,
                          struct mailimap_set * set,
                          clist ** results);

#ifdef __cplusplus
}
#endif


#endif
