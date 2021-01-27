
#ifndef EXTENSIONS_H

#define EXTENSIONS_H

#ifdef __cplusplus
extern "C" {
#endif

  
struct domain_pindata_pair_entry
{
    char* domain;
    char* pin_data;
};

void set_domain_pindata_pairs(struct domain_pindata_pair_entry * pair_entries, int entry_count);
void set_client_certificate(char * certificate);

#ifdef __cplusplus
}
#endif


#endif


