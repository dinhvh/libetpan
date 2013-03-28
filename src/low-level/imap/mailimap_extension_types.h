#ifndef MAILIMAP_EXTENSION_TYPES_H

#define MAILIMAP_EXTENSION_TYPES_H

#include <libetpan/mailstream.h>

struct mailimap_extension_data;

/*
  this is the list of known extensions with the purpose to
  get integer identifers for the extensions.
*/

enum {
  MAILIMAP_EXTENSION_ANNOTATEMORE,  /* the annotatemore-draft */
  MAILIMAP_EXTENSION_ACL,           /* the acl capability */
  MAILIMAP_EXTENSION_UIDPLUS,       /* UIDPLUS */
  MAILIMAP_EXTENSION_QUOTA,         /* quota */
  MAILIMAP_EXTENSION_NAMESPACE,     /* namespace */
  MAILIMAP_EXTENSION_XLIST,         /* XLIST (Gmail and Zimbra have this) */
  MAILIMAP_EXTENSION_XGMLABELS,     /* X-GM-LABELS (Gmail) */
  MAILIMAP_EXTENSION_XGMMSGID,      /* X-GM-MSGID (Gmail) */
  MAILIMAP_EXTENSION_XGMTHRID,      /* X-GM-THRID (Gmail) */
  MAILIMAP_EXTENSION_ID,            /* ID */
  MAILIMAP_EXTENSION_ENABLE,        /* ENABLE */
  MAILIMAP_EXTENSION_CONDSTORE,     /* CONDSTORE */
  MAILIMAP_EXTENSION_QRESYNC,       /* QRESYNC */
  MAILIMAP_EXTENSION_SORT           /* SORT */
};


/*
  this is a list of extended parser functions. The extended parser
  passes its identifier to the extension parser.
*/

enum {
  MAILIMAP_EXTENDED_PARSER_RESPONSE_DATA,
  MAILIMAP_EXTENDED_PARSER_RESP_TEXT_CODE,
  MAILIMAP_EXTENDED_PARSER_MAILBOX_DATA,
  MAILIMAP_EXTENDED_PARSER_FETCH_DATA,
  MAILIMAP_EXTENDED_PARSER_STATUS_ATT
};

/*
  this is the extension interface. each extension consists
  of a initial parser and an initial free. the parser is
  passed the calling parser's identifier. based on this
  identifier the initial parser can then decide which
  actual parser to call. free has mailimap_extension_data
  as parameter. if you look at mailimap_extension_data
  you'll see that it contains "type" as one of its
  elements. thus an extension's initial free can call
  the correct actual free to free its data.
*/
struct mailimap_extension_api {
  char * ext_name;
  int ext_id; /* use -1 if this is an extension outside libetpan */

  int (* ext_parser)(int calling_parser, mailstream * fd,
            MMAPString * buffer, size_t * indx,
            struct mailimap_extension_data ** result,
            size_t progr_rate,
            progress_function * progr_fun);

  void (* ext_free)(struct mailimap_extension_data * ext_data);
};

/*
  mailimap_extension_data is a wrapper for values parsed by extensions

  - extension is an identifier for the extension that parsed the value.

  - type is an identifier for the real type of the data.

  - data is a pointer to the real data.
*/
struct mailimap_extension_data {
  struct mailimap_extension_api * ext_extension;
  int ext_type;
  void * ext_data;
};

#endif
