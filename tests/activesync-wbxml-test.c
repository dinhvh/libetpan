/*
 * libEtPan! -- a mail stuff library
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include <libetpan/mailactivesync_codes.h>
#include <libetpan/mailactivesync_wbxml.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int check(int condition, const char * message)
{
  if (!condition) {
    fprintf(stderr, "%s\n", message);
    return 0;
  }

  return 1;
}

static int test_tag_lookup(void)
{
  uint8_t code_page;
  uint8_t token;
  const char * name;

  name = mailactivesync_wbxml_tag_name(
      MAILACTIVESYNC_CP_FOLDERHIERARCHY,
      MAILACTIVESYNC_FOLDER_FOLDER_SYNC);
  if (!check((name != NULL) &&
      (strcmp(name, "FolderHierarchy:FolderSync") == 0),
      "FolderSync tag name lookup failed"))
    return 0;

  if (!check(mailactivesync_wbxml_tag_lookup("AirSync:Sync",
      &code_page, &token), "AirSync:Sync reverse lookup failed"))
    return 0;
  if (!check((code_page == MAILACTIVESYNC_CP_AIRSYNC) &&
      (token == MAILACTIVESYNC_AIRSYNC_SYNC),
      "AirSync:Sync reverse lookup returned wrong token"))
    return 0;

  return 1;
}

static int test_foldersync_golden(void)
{
  static const unsigned char expected[] = {
    0x03, 0x01, 0x6A, 0x00, 0x00, 0x07, 0x56, 0x52,
    0x03, 0x30, 0x00, 0x01, 0x01
  };
  struct mailactivesync_wbxml_document * document;
  struct mailactivesync_wbxml_document * decoded;
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_node * child;
  unsigned char * encoded;
  size_t encoded_len;
  clistiter * first;
  int r;

  document = NULL;
  decoded = NULL;
  root = NULL;
  child = NULL;
  encoded = NULL;
  encoded_len = 0;

  document = mailactivesync_wbxml_document_new();
  if (!check(document != NULL, "document allocation failed"))
    return 0;

  root = mailactivesync_wbxml_node_new(
      MAILACTIVESYNC_CP_FOLDERHIERARCHY,
      MAILACTIVESYNC_FOLDER_FOLDER_SYNC);
  child = mailactivesync_wbxml_node_new_text(
      MAILACTIVESYNC_CP_FOLDERHIERARCHY,
      MAILACTIVESYNC_FOLDER_SYNC_KEY, "0");
  if (!check((root != NULL) && (child != NULL), "node allocation failed"))
    goto err;
  if (!check(mailactivesync_wbxml_node_add_child(root, child) ==
      MAILACTIVESYNC_NO_ERROR, "add child failed"))
    goto err;
  child = NULL;
  document->root = root;
  root = NULL;

  r = mailactivesync_wbxml_encode(document, &encoded, &encoded_len);
  if (!check(r == MAILACTIVESYNC_NO_ERROR, "encode failed"))
    goto err;
  if (!check(encoded_len == sizeof(expected), "encoded length mismatch"))
    goto err;
  if (!check(memcmp(encoded, expected, sizeof(expected)) == 0,
      "encoded FolderSync fixture mismatch"))
    goto err;

  r = mailactivesync_wbxml_decode(encoded, encoded_len, &decoded);
  if (!check(r == MAILACTIVESYNC_NO_ERROR, "decode failed"))
    goto err;
  if (!check(decoded->root != NULL, "decoded root missing"))
    goto err;
  if (!check(decoded->root->code_page == MAILACTIVESYNC_CP_FOLDERHIERARCHY,
      "decoded root code page mismatch"))
    goto err;
  if (!check(decoded->root->token == MAILACTIVESYNC_FOLDER_FOLDER_SYNC,
      "decoded root token mismatch"))
    goto err;
  first = clist_begin(decoded->root->children);
  if (!check(first != NULL, "decoded child missing"))
    goto err;
  child = clist_content(first);
  if (!check((child->text != NULL) && (strcmp(child->text, "0") == 0),
      "decoded SyncKey text mismatch"))
    goto err;

  mailactivesync_wbxml_document_free(decoded);
  mailactivesync_wbxml_document_free(document);
  free(encoded);
  return 1;

 err:
  mailactivesync_wbxml_node_free(child);
  mailactivesync_wbxml_node_free(root);
  mailactivesync_wbxml_document_free(decoded);
  mailactivesync_wbxml_document_free(document);
  free(encoded);
  return 0;
}

static int test_opaque_and_malformed(void)
{
  static const unsigned char payload[] = { 0x00, 0x01, 0x02, 0x03 };
  static const unsigned char malformed[] = {
    0x03, 0x01, 0x6A, 0x00, 0x00, 0x07, 0x56, 0x52,
    0x03, 0x30, 0x00, 0x01
  };
  struct mailactivesync_wbxml_document * document;
  struct mailactivesync_wbxml_document * decoded;
  struct mailactivesync_wbxml_node * root;
  struct mailactivesync_wbxml_node * child;
  unsigned char * encoded;
  size_t encoded_len;
  clistiter * first;
  int r;

  document = NULL;
  decoded = NULL;
  root = NULL;
  child = NULL;
  encoded = NULL;
  encoded_len = 0;

  document = mailactivesync_wbxml_document_new();
  if (!check(document != NULL, "document allocation failed"))
    return 0;

  root = mailactivesync_wbxml_node_new(
      MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_BODY);
  child = mailactivesync_wbxml_node_new_opaque(
      MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_DATA, payload, sizeof(payload));
  if (!check((root != NULL) && (child != NULL), "opaque node allocation failed"))
    goto err;
  if (!check(mailactivesync_wbxml_node_add_child(root, child) ==
      MAILACTIVESYNC_NO_ERROR, "add opaque child failed"))
    goto err;
  child = NULL;
  document->root = root;
  root = NULL;

  r = mailactivesync_wbxml_encode(document, &encoded, &encoded_len);
  if (!check(r == MAILACTIVESYNC_NO_ERROR, "opaque encode failed"))
    goto err;
  r = mailactivesync_wbxml_decode(encoded, encoded_len, &decoded);
  if (!check(r == MAILACTIVESYNC_NO_ERROR, "opaque decode failed"))
    goto err;

  first = clist_begin(decoded->root->children);
  if (!check(first != NULL, "decoded opaque child missing"))
    goto err;
  child = clist_content(first);
  if (!check((child->opaque_len == sizeof(payload)) &&
      (memcmp(child->opaque, payload, sizeof(payload)) == 0),
      "decoded opaque payload mismatch"))
    goto err;

  mailactivesync_wbxml_document_free(decoded);
  decoded = NULL;
  r = mailactivesync_wbxml_decode(malformed, sizeof(malformed), &decoded);
  if (!check(r == MAILACTIVESYNC_ERROR_PARSE,
      "malformed document was not rejected"))
    goto err;

  mailactivesync_wbxml_document_free(document);
  free(encoded);
  return 1;

 err:
  mailactivesync_wbxml_node_free(child);
  mailactivesync_wbxml_node_free(root);
  mailactivesync_wbxml_document_free(decoded);
  mailactivesync_wbxml_document_free(document);
  free(encoded);
  return 0;
}

int main(void)
{
  if (!test_tag_lookup())
    return 1;
  if (!test_foldersync_golden())
    return 1;
  if (!test_opaque_and_malformed())
    return 1;

  return 0;
}
