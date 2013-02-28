#ifndef CONDSTORE_PRIVATE_H

#define CONDSTORE_PRIVATE_H

int mailimap_examine_condstore_optional(mailimap * session, const char * mb,
  int condstore, uint64_t * p_mod_sequence_value);

int mailimap_select_condstore_optional(mailimap * session, const char * mb,
	int condstore, uint64_t * p_mod_sequence_value);

int mailimap_store_unchangedsince_optional(mailimap * session,
	struct mailimap_set * set, int use_unchangedsince, uint64_t mod_sequence_valzer,
  struct mailimap_store_att_flags * store_att_flags);

int mailimap_uid_store_unchangedsince_optional(mailimap * session,
	struct mailimap_set * set, int use_unchangedsince, uint64_t mod_sequence_valzer,
  struct mailimap_store_att_flags * store_att_flags);

#endif
