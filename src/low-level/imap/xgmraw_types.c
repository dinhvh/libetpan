#include "xgmraw_types.h"
#include "mmapstring.h"
#include "mailimap_extension.h"

#include <stdlib.h>

struct xgmraw_search_key *
xgmraw_search_key_new(int sk_type,
                      char * sk_bcc, struct mailimap_date * sk_before, char * sk_cc,
                      char * sk_filename,
                      char * sk_from, char * sk_keyword, struct mailimap_date * sk_since,
                      char * sk_subject, char * sk_to, struct xgmraw_search_key * sk_not,
                      struct xgmraw_search_key * sk_or1,
                      struct xgmraw_search_key * sk_or2,
                      clist * sk_multiple)
{
  struct xgmraw_search_key * key;
  
  key = malloc(sizeof(* key));
  if (key == NULL)
    return NULL;
  
  key->sk_type = sk_type;
  switch (sk_type) {
    case XGMRAW_SEARCH_KEY_BCC:
      key->sk_data.sk_bcc = sk_bcc;
      break;
    case XGMRAW_SEARCH_KEY_BEFORE:
      key->sk_data.sk_before = sk_before;
      break;
    case XGMRAW_SEARCH_KEY_CC:
      key->sk_data.sk_cc = sk_cc;
      break;
    case XGMRAW_SEARCH_KEY_FILENAME:
      key->sk_data.sk_filename = sk_filename;
      break;
    case XGMRAW_SEARCH_KEY_FROM:
      key->sk_data.sk_from = sk_from;
      break;
    case XGMRAW_SEARCH_KEY_KEYWORD:
      key->sk_data.sk_keyword = sk_keyword;
      break;
    case XGMRAW_SEARCH_KEY_SINCE:
      key->sk_data.sk_since = sk_since;
      break;
    case XGMRAW_SEARCH_KEY_SUBJECT:
      key->sk_data.sk_subject = sk_subject;
      break;
    case XGMRAW_SEARCH_KEY_TO:
      key->sk_data.sk_to = sk_to;
      break;
    case XGMRAW_SEARCH_KEY_NOT:
      key->sk_data.sk_not = sk_not;
      break;
    case XGMRAW_SEARCH_KEY_OR:
      key->sk_data.sk_or.sk_or1 = sk_or1;
      key->sk_data.sk_or.sk_or2 = sk_or2;
      break;
    case XGMRAW_SEARCH_KEY_MULTIPLE:
      key->sk_data.sk_multiple = sk_multiple;
      break;
  }
  return key;
}

void xgmraw_search_key_free(struct xgmraw_search_key * key)
{
  switch (key->sk_type) {
    case XGMRAW_SEARCH_KEY_BCC:
      mailimap_astring_free(key->sk_data.sk_bcc);
      break;
    case XGMRAW_SEARCH_KEY_BEFORE:
      mailimap_date_free(key->sk_data.sk_before);
      break;
    case XGMRAW_SEARCH_KEY_CC:
      mailimap_astring_free(key->sk_data.sk_cc);
      break;
    case XGMRAW_SEARCH_KEY_FROM:
      mailimap_astring_free(key->sk_data.sk_from);
      break;
    case XGMRAW_SEARCH_KEY_KEYWORD:
      mailimap_flag_keyword_free(key->sk_data.sk_keyword);
      break;
    case XGMRAW_SEARCH_KEY_SINCE:
      mailimap_date_free(key->sk_data.sk_since);
      break;
    case XGMRAW_SEARCH_KEY_SUBJECT:
      mailimap_astring_free(key->sk_data.sk_subject);
      break;
    case XGMRAW_SEARCH_KEY_TO:
      mailimap_astring_free(key->sk_data.sk_to);
      break;
    case XGMRAW_SEARCH_KEY_NOT:
      xgmraw_search_key_free(key->sk_data.sk_not);
      break;
    case XGMRAW_SEARCH_KEY_OR:
      xgmraw_search_key_free(key->sk_data.sk_or.sk_or1);
      xgmraw_search_key_free(key->sk_data.sk_or.sk_or2);
      break;
    case XGMRAW_SEARCH_KEY_MULTIPLE:
      clist_foreach(key->sk_data.sk_multiple,
                    (clist_func) xgmraw_search_key_free, NULL);
      clist_free(key->sk_data.sk_multiple);
      break;
  }
  
  free(key);
}
