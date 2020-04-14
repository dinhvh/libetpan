#include "extensions.h"

struct domain_pindata_pair_entry * domain_pindata_pair_entry_block = 0;
int domain_pindata_pair_entry_count = 0;

void set_domain_pindata_pairs(struct domain_pindata_pair_entry * pair_entries, int entry_count)
{
  domain_pindata_pair_entry_block = pair_entries;
  domain_pindata_pair_entry_count = entry_count;
}

void set_client_certificate(char * certificate)
{
  
}


