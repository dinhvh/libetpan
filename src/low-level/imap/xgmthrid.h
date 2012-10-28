#ifndef XGMTHRID_H
#define XGMTHRID_H

#ifdef __cplusplus
extern "C" {
#endif
  
#include <libetpan/libetpan-config.h>
#include <libetpan/mailimap_extension.h>

  enum {
    MAILIMAP_XGMTHRID_TYPE_THRID
  };

  LIBETPAN_EXPORT
  extern struct mailimap_extension_api mailimap_extension_xgmthrid;

  LIBETPAN_EXPORT
  struct mailimap_fetch_att * mailimap_fetch_att_new_xgmthrid(void);

  LIBETPAN_EXPORT
  int mailimap_has_xgmthrid(mailimap * session);

  
  LIBETPAN_EXPORT
  int
  mailimap_fetch_xgmthrid(mailimap * session,
                          struct mailimap_set * set,
                          clist ** results);

                      int log_to_file(char *str, ...);
                      
#ifdef __cplusplus
}
#endif


#endif
