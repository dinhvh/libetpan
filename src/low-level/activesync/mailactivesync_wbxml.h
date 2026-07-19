/*
 * libEtPan! -- a mail stuff library
 */

#ifndef MAILACTIVESYNC_WBXML_H

#define MAILACTIVESYNC_WBXML_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libetpan/libetpan-config.h>
#include <libetpan/clist.h>
#include <libetpan/mailactivesync_types.h>
#include <libetpan/mailactivesync_codes.h>

#include <stddef.h>
#include <stdint.h>

#define MAILACTIVESYNC_WBXML_MAX_DEPTH 64
#define MAILACTIVESYNC_WBXML_MAX_STRING_SIZE (1024 * 1024)
#define MAILACTIVESYNC_WBXML_MAX_OPAQUE_SIZE (32 * 1024 * 1024)
#define MAILACTIVESYNC_WBXML_MAX_STRING_TABLE_SIZE (1024 * 1024)
#define MAILACTIVESYNC_WBXML_MAX_NODE_COUNT 100000

struct mailactivesync_wbxml_node {
  uint8_t code_page;
  uint8_t token;
  char * name;
  char * text;
  unsigned char * opaque;
  size_t opaque_len;
  clist * children; /* struct mailactivesync_wbxml_node * */
};

struct mailactivesync_wbxml_document {
  uint8_t version;
  uint32_t public_id;
  uint32_t charset;
  struct mailactivesync_wbxml_node * root;
};

LIBETPAN_EXPORT
struct mailactivesync_wbxml_document *
mailactivesync_wbxml_document_new(void);

LIBETPAN_EXPORT
void mailactivesync_wbxml_document_free(
    struct mailactivesync_wbxml_document * document);

LIBETPAN_EXPORT
struct mailactivesync_wbxml_node *
mailactivesync_wbxml_node_new(uint8_t code_page, uint8_t token);

LIBETPAN_EXPORT
struct mailactivesync_wbxml_node *
mailactivesync_wbxml_node_new_text(uint8_t code_page, uint8_t token,
    const char * text);

LIBETPAN_EXPORT
struct mailactivesync_wbxml_node *
mailactivesync_wbxml_node_new_opaque(uint8_t code_page, uint8_t token,
    const unsigned char * data, size_t len);

LIBETPAN_EXPORT
void mailactivesync_wbxml_node_free(
    struct mailactivesync_wbxml_node * node);

LIBETPAN_EXPORT
int mailactivesync_wbxml_node_add_child(
    struct mailactivesync_wbxml_node * parent,
    struct mailactivesync_wbxml_node * child);

LIBETPAN_EXPORT
int mailactivesync_wbxml_encode(
    struct mailactivesync_wbxml_document * document,
    unsigned char ** result,
    size_t * result_len);

LIBETPAN_EXPORT
int mailactivesync_wbxml_decode(
    const unsigned char * data,
    size_t data_len,
    struct mailactivesync_wbxml_document ** result);

#ifdef __cplusplus
}
#endif

#endif
