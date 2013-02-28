#ifndef CONDSTORE_TYPE_H

#define CONDSTORE_TYPE_H

#include <libetpan/mailimap_types.h>

struct mailimap_condstore_fetch_mod_resp {
  uint64_t cs_modseq_value;
};

enum {
  MAILIMAP_CONDSTORE_RESPTEXTCODE_HIGHESTMODSEQ,
  MAILIMAP_CONDSTORE_RESPTEXTCODE_NOMODSEQ,
  MAILIMAP_CONDSTORE_RESPTEXTCODE_MODIFIED
};

struct mailimap_msg_att_modseq {
  uint64_t att_modseq;
};

struct mailimap_condstore_resptextcode {
  int cs_type;
  union {
    uint64_t cs_modseq_value;
    struct mailimap_set * cs_modified_set;
  } cs_data;
};

struct mailimap_condstore_search {
  clist * cs_search_result; /* uint32_t */
  uint64_t cs_modseq_value;
};

struct mailimap_condstore_status_info {
  uint64_t cs_highestmodseq_value;
};

struct mailimap_condstore_fetch_mod_resp * mailimap_condstore_fetch_mod_resp_new(uint64_t cs_modseq_value);
void mailimap_condstore_fetch_mod_resp_free(struct mailimap_condstore_fetch_mod_resp * fetch_data);

struct mailimap_condstore_resptextcode * mailimap_condstore_resptextcode_new(int cs_type,
  uint64_t cs_modseq_value, struct mailimap_set * cs_modified_set);
void mailimap_condstore_resptextcode_free(struct mailimap_condstore_resptextcode * resptextcode);

struct mailimap_condstore_search * mailimap_condstore_search_new(clist * cs_search_result, uint64_t cs_modseq_value);
void mailimap_condstore_search_free(struct mailimap_condstore_search * search_data);

struct mailimap_condstore_status_info * mailimap_condstore_status_info_new(uint64_t cs_highestmodseq_value);
void mailimap_condstore_status_info_free(struct mailimap_condstore_status_info * status_info);

#endif
