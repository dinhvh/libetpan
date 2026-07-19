/*
 * libEtPan! -- a mail stuff library
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailactivesync_wbxml.h"

#include <libetpan/mmapstring.h>

#include <stdlib.h>
#include <string.h>

struct wbxml_decode_context {
  const unsigned char * data;
  size_t len;
  size_t pos;
  const unsigned char * string_table;
  size_t string_table_len;
  uint8_t current_code_page;
  uint32_t node_count;
};

static int append_byte(MMAPString * buffer, uint8_t value)
{
  if (mmap_string_append_c(buffer, (char) value) == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  return MAILACTIVESYNC_NO_ERROR;
}

static int append_bytes(MMAPString * buffer, const void * data, size_t len)
{
  if (len == 0)
    return MAILACTIVESYNC_NO_ERROR;

  if (mmap_string_append_len(buffer, data, len) == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  return MAILACTIVESYNC_NO_ERROR;
}

static int append_mb_uint(MMAPString * buffer, uint32_t value)
{
  unsigned char bytes[5];
  unsigned int count;
  int i;

  count = 0;
  bytes[count ++] = (unsigned char) (value & 0x7F);
  value >>= 7;

  while (value != 0) {
    if (count == sizeof(bytes))
      return MAILACTIVESYNC_ERROR_PROTOCOL;
    bytes[count ++] = (unsigned char) (0x80 | (value & 0x7F));
    value >>= 7;
  }

  for (i = (int) count - 1; i >= 0; i --) {
    int r;

    r = append_byte(buffer, bytes[i]);
    if (r != MAILACTIVESYNC_NO_ERROR)
      return r;
  }

  return MAILACTIVESYNC_NO_ERROR;
}

static int read_byte(struct wbxml_decode_context * ctx, uint8_t * value)
{
  if (ctx->pos >= ctx->len)
    return MAILACTIVESYNC_ERROR_PARSE;

  * value = ctx->data[ctx->pos ++];
  return MAILACTIVESYNC_NO_ERROR;
}

static int read_mb_uint(struct wbxml_decode_context * ctx, uint32_t * value)
{
  uint32_t result;
  unsigned int count;

  result = 0;
  count = 0;

  while (1) {
    uint8_t byte;
    int r;

    if (count == 5)
      return MAILACTIVESYNC_ERROR_PARSE;

    r = read_byte(ctx, &byte);
    if (r != MAILACTIVESYNC_NO_ERROR)
      return r;

    if (result > ((uint32_t) -1 >> 7))
      return MAILACTIVESYNC_ERROR_PARSE;

    result = (result << 7) | (byte & 0x7F);
    count ++;

    if ((byte & 0x80) == 0)
      break;
  }

  * value = result;
  return MAILACTIVESYNC_NO_ERROR;
}

static char * dup_string(const char * value)
{
  if (value == NULL)
    return NULL;

  return strdup(value);
}

static int node_set_name(struct mailactivesync_wbxml_node * node)
{
  const char * name;

  name = mailactivesync_wbxml_tag_name(node->code_page, node->token);
  if (name == NULL)
    return MAILACTIVESYNC_NO_ERROR;

  node->name = strdup(name);
  if (node->name == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  return MAILACTIVESYNC_NO_ERROR;
}

struct mailactivesync_wbxml_document *
mailactivesync_wbxml_document_new(void)
{
  struct mailactivesync_wbxml_document * document;

  document = malloc(sizeof(* document));
  if (document == NULL)
    return NULL;

  document->version = MAILACTIVESYNC_WBXML_VERSION;
  document->public_id = MAILACTIVESYNC_WBXML_PUBLIC_ID;
  document->charset = MAILACTIVESYNC_WBXML_CHARSET;
  document->root = NULL;

  return document;
}

void mailactivesync_wbxml_document_free(
    struct mailactivesync_wbxml_document * document)
{
  if (document == NULL)
    return;

  mailactivesync_wbxml_node_free(document->root);
  free(document);
}

struct mailactivesync_wbxml_node *
mailactivesync_wbxml_node_new(uint8_t code_page, uint8_t token)
{
  struct mailactivesync_wbxml_node * node;

  node = malloc(sizeof(* node));
  if (node == NULL)
    return NULL;

  node->code_page = code_page;
  node->token = token;
  node->name = NULL;
  node->text = NULL;
  node->opaque = NULL;
  node->opaque_len = 0;
  node->children = NULL;

  if (node_set_name(node) != MAILACTIVESYNC_NO_ERROR) {
    mailactivesync_wbxml_node_free(node);
    return NULL;
  }

  return node;
}

struct mailactivesync_wbxml_node *
mailactivesync_wbxml_node_new_text(uint8_t code_page, uint8_t token,
    const char * text)
{
  struct mailactivesync_wbxml_node * node;

  node = mailactivesync_wbxml_node_new(code_page, token);
  if (node == NULL)
    return NULL;

  node->text = dup_string(text);
  if ((text != NULL) && (node->text == NULL)) {
    mailactivesync_wbxml_node_free(node);
    return NULL;
  }

  return node;
}

struct mailactivesync_wbxml_node *
mailactivesync_wbxml_node_new_opaque(uint8_t code_page, uint8_t token,
    const unsigned char * data, size_t len)
{
  struct mailactivesync_wbxml_node * node;

  node = mailactivesync_wbxml_node_new(code_page, token);
  if (node == NULL)
    return NULL;

  if (len > 0) {
    if (data == NULL) {
      mailactivesync_wbxml_node_free(node);
      return NULL;
    }
    node->opaque = malloc(len);
    if (node->opaque == NULL) {
      mailactivesync_wbxml_node_free(node);
      return NULL;
    }
    memcpy(node->opaque, data, len);
    node->opaque_len = len;
  }

  return node;
}

static void free_node_item(void * value, void * data)
{
  (void) data;
  mailactivesync_wbxml_node_free(value);
}

void mailactivesync_wbxml_node_free(
    struct mailactivesync_wbxml_node * node)
{
  if (node == NULL)
    return;

  free(node->name);
  free(node->text);
  free(node->opaque);
  if (node->children != NULL) {
    clist_foreach(node->children, free_node_item, NULL);
    clist_free(node->children);
  }
  free(node);
}

int mailactivesync_wbxml_node_add_child(
    struct mailactivesync_wbxml_node * parent,
    struct mailactivesync_wbxml_node * child)
{
  if ((parent == NULL) || (child == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  if (parent->children == NULL) {
    parent->children = clist_new();
    if (parent->children == NULL)
      return MAILACTIVESYNC_ERROR_MEMORY;
  }

  if (clist_append(parent->children, child) < 0)
    return MAILACTIVESYNC_ERROR_MEMORY;

  return MAILACTIVESYNC_NO_ERROR;
}

static int node_has_content(struct mailactivesync_wbxml_node * node)
{
  return (node->text != NULL) || (node->opaque != NULL) ||
    ((node->children != NULL) && (clist_count(node->children) > 0));
}

static int encode_node(MMAPString * buffer,
    struct mailactivesync_wbxml_node * node,
    uint8_t * current_code_page)
{
  uint8_t tag;
  int r;

  if (node->code_page != * current_code_page) {
    r = append_byte(buffer, MAILACTIVESYNC_WBXML_SWITCH_PAGE);
    if (r != MAILACTIVESYNC_NO_ERROR)
      return r;
    r = append_byte(buffer, node->code_page);
    if (r != MAILACTIVESYNC_NO_ERROR)
      return r;
    * current_code_page = node->code_page;
  }

  tag = node->token;
  if (node_has_content(node))
    tag |= MAILACTIVESYNC_WBXML_HAS_CONTENT;

  r = append_byte(buffer, tag);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  if (!node_has_content(node))
    return MAILACTIVESYNC_NO_ERROR;

  if (node->text != NULL) {
    r = append_byte(buffer, MAILACTIVESYNC_WBXML_STR_I);
    if (r != MAILACTIVESYNC_NO_ERROR)
      return r;
    r = append_bytes(buffer, node->text, strlen(node->text) + 1);
    if (r != MAILACTIVESYNC_NO_ERROR)
      return r;
  }

  if (node->opaque != NULL) {
    if (node->opaque_len > UINT32_MAX)
      return MAILACTIVESYNC_ERROR_PROTOCOL;

    r = append_byte(buffer, MAILACTIVESYNC_WBXML_OPAQUE);
    if (r != MAILACTIVESYNC_NO_ERROR)
      return r;
    r = append_mb_uint(buffer, (uint32_t) node->opaque_len);
    if (r != MAILACTIVESYNC_NO_ERROR)
      return r;
    r = append_bytes(buffer, node->opaque, node->opaque_len);
    if (r != MAILACTIVESYNC_NO_ERROR)
      return r;
  }

  if (node->children != NULL) {
    clistiter * cur;

    for (cur = clist_begin(node->children); cur != NULL; cur = clist_next(cur)) {
      r = encode_node(buffer, clist_content(cur), current_code_page);
      if (r != MAILACTIVESYNC_NO_ERROR)
        return r;
    }
  }

  return append_byte(buffer, MAILACTIVESYNC_WBXML_END);
}

int mailactivesync_wbxml_encode(
    struct mailactivesync_wbxml_document * document,
    unsigned char ** result,
    size_t * result_len)
{
  MMAPString * buffer;
  uint8_t current_code_page;
  int r;

  if ((document == NULL) || (document->root == NULL) ||
      (result == NULL) || (result_len == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  * result_len = 0;

  buffer = mmap_string_sized_new(256);
  if (buffer == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  r = append_byte(buffer, document->version);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  r = append_mb_uint(buffer, document->public_id);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  r = append_mb_uint(buffer, document->charset);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  r = append_mb_uint(buffer, 0);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;

  current_code_page = 0;
  r = encode_node(buffer, document->root, &current_code_page);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;

  * result = (unsigned char *) malloc(buffer->len);
  if (* result == NULL) {
    r = MAILACTIVESYNC_ERROR_MEMORY;
    goto err;
  }

  memcpy(* result, buffer->str, buffer->len);
  * result_len = buffer->len;
  mmap_string_free(buffer);

  return MAILACTIVESYNC_NO_ERROR;

 err:
  mmap_string_free(buffer);
  return r;
}

static char * read_inline_string(struct wbxml_decode_context * ctx)
{
  size_t start;
  size_t len;
  char * result;

  start = ctx->pos;
  while (ctx->pos < ctx->len) {
    if (ctx->data[ctx->pos] == '\0')
      break;
    ctx->pos ++;
  }

  if (ctx->pos >= ctx->len)
    return NULL;

  len = ctx->pos - start;
  if (len > MAILACTIVESYNC_WBXML_MAX_STRING_SIZE)
    return NULL;

  result = malloc(len + 1);
  if (result == NULL)
    return NULL;

  memcpy(result, ctx->data + start, len);
  result[len] = '\0';
  ctx->pos ++;

  return result;
}

static char * read_table_string(struct wbxml_decode_context * ctx)
{
  uint32_t offset;
  size_t pos;
  size_t len;
  char * result;
  int r;

  r = read_mb_uint(ctx, &offset);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return NULL;

  if (offset >= ctx->string_table_len)
    return NULL;

  pos = offset;
  while (pos < ctx->string_table_len) {
    if (ctx->string_table[pos] == '\0')
      break;
    pos ++;
  }

  if (pos >= ctx->string_table_len)
    return NULL;

  len = pos - offset;
  if (len > MAILACTIVESYNC_WBXML_MAX_STRING_SIZE)
    return NULL;

  result = malloc(len + 1);
  if (result == NULL)
    return NULL;

  memcpy(result, ctx->string_table + offset, len);
  result[len] = '\0';

  return result;
}

static int append_node_text(struct mailactivesync_wbxml_node * node, char * text)
{
  size_t old_len;
  size_t add_len;
  char * new_text;

  if (node->text == NULL) {
    node->text = text;
    return MAILACTIVESYNC_NO_ERROR;
  }

  old_len = strlen(node->text);
  add_len = strlen(text);
  if (old_len + add_len > MAILACTIVESYNC_WBXML_MAX_STRING_SIZE) {
    free(text);
    return MAILACTIVESYNC_ERROR_PARSE;
  }

  new_text = realloc(node->text, old_len + add_len + 1);
  if (new_text == NULL) {
    free(text);
    return MAILACTIVESYNC_ERROR_MEMORY;
  }

  memcpy(new_text + old_len, text, add_len + 1);
  node->text = new_text;
  free(text);

  return MAILACTIVESYNC_NO_ERROR;
}

static int set_node_opaque(struct mailactivesync_wbxml_node * node,
    const unsigned char * data, size_t len)
{
  unsigned char * new_opaque;

  if (len > MAILACTIVESYNC_WBXML_MAX_OPAQUE_SIZE)
    return MAILACTIVESYNC_ERROR_PARSE;

  new_opaque = malloc(len);
  if ((len > 0) && (new_opaque == NULL))
    return MAILACTIVESYNC_ERROR_MEMORY;

  if (len > 0)
    memcpy(new_opaque, data, len);
  free(node->opaque);
  node->opaque = new_opaque;
  node->opaque_len = len;

  return MAILACTIVESYNC_NO_ERROR;
}

static int decode_node(struct wbxml_decode_context * ctx,
    struct mailactivesync_wbxml_node ** result,
    uint32_t depth);

static int decode_node_content(struct wbxml_decode_context * ctx,
    struct mailactivesync_wbxml_node * node,
    uint32_t depth)
{
  while (ctx->pos < ctx->len) {
    uint8_t byte;
    int r;

    r = read_byte(ctx, &byte);
    if (r != MAILACTIVESYNC_NO_ERROR)
      return r;

    switch (byte) {
    case MAILACTIVESYNC_WBXML_END:
      return MAILACTIVESYNC_NO_ERROR;

    case MAILACTIVESYNC_WBXML_SWITCH_PAGE:
      r = read_byte(ctx, &ctx->current_code_page);
      if (r != MAILACTIVESYNC_NO_ERROR)
        return r;
      break;

    case MAILACTIVESYNC_WBXML_STR_I:
      {
        char * text;

        text = read_inline_string(ctx);
        if (text == NULL)
          return MAILACTIVESYNC_ERROR_PARSE;

        r = append_node_text(node, text);
        if (r != MAILACTIVESYNC_NO_ERROR)
          return r;
      }
      break;

    case MAILACTIVESYNC_WBXML_STR_T:
      {
        char * text;

        text = read_table_string(ctx);
        if (text == NULL)
          return MAILACTIVESYNC_ERROR_PARSE;

        r = append_node_text(node, text);
        if (r != MAILACTIVESYNC_NO_ERROR)
          return r;
      }
      break;

    case MAILACTIVESYNC_WBXML_OPAQUE:
      {
        uint32_t opaque_len;

        r = read_mb_uint(ctx, &opaque_len);
        if (r != MAILACTIVESYNC_NO_ERROR)
          return r;
        if (opaque_len > MAILACTIVESYNC_WBXML_MAX_OPAQUE_SIZE)
          return MAILACTIVESYNC_ERROR_PARSE;
        if (ctx->len - ctx->pos < opaque_len)
          return MAILACTIVESYNC_ERROR_PARSE;

        r = set_node_opaque(node, ctx->data + ctx->pos, opaque_len);
        if (r != MAILACTIVESYNC_NO_ERROR)
          return r;

        ctx->pos += opaque_len;
      }
      break;

    default:
      {
        struct mailactivesync_wbxml_node * child;

        ctx->pos --;
        r = decode_node(ctx, &child, depth + 1);
        if (r != MAILACTIVESYNC_NO_ERROR)
          return r;

        r = mailactivesync_wbxml_node_add_child(node, child);
        if (r != MAILACTIVESYNC_NO_ERROR) {
          mailactivesync_wbxml_node_free(child);
          return r;
        }
      }
      break;
    }
  }

  return MAILACTIVESYNC_ERROR_PARSE;
}

static int decode_node(struct wbxml_decode_context * ctx,
    struct mailactivesync_wbxml_node ** result,
    uint32_t depth)
{
  uint8_t byte;
  uint8_t token;
  uint8_t code_page;
  int has_content;
  int r;
  struct mailactivesync_wbxml_node * node;

  if (depth > MAILACTIVESYNC_WBXML_MAX_DEPTH)
    return MAILACTIVESYNC_ERROR_PARSE;

  while (1) {
    r = read_byte(ctx, &byte);
    if (r != MAILACTIVESYNC_NO_ERROR)
      return r;

    if (byte != MAILACTIVESYNC_WBXML_SWITCH_PAGE)
      break;

    r = read_byte(ctx, &ctx->current_code_page);
    if (r != MAILACTIVESYNC_NO_ERROR)
      return r;
  }

  if ((byte == MAILACTIVESYNC_WBXML_END) ||
      (byte == MAILACTIVESYNC_WBXML_ENTITY) ||
      (byte == MAILACTIVESYNC_WBXML_STR_I) ||
      (byte == MAILACTIVESYNC_WBXML_LITERAL) ||
      (byte == MAILACTIVESYNC_WBXML_STR_T) ||
      (byte == MAILACTIVESYNC_WBXML_OPAQUE))
    return MAILACTIVESYNC_ERROR_PARSE;

  if ((byte & MAILACTIVESYNC_WBXML_HAS_ATTRS) != 0)
    return MAILACTIVESYNC_ERROR_PARSE;

  token = byte & MAILACTIVESYNC_WBXML_TOKEN_MASK;
  if (token == 0)
    return MAILACTIVESYNC_ERROR_PARSE;

  code_page = ctx->current_code_page;
  has_content = (byte & MAILACTIVESYNC_WBXML_HAS_CONTENT) != 0;

  node = mailactivesync_wbxml_node_new(code_page, token);
  if (node == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  ctx->node_count ++;
  if (ctx->node_count > MAILACTIVESYNC_WBXML_MAX_NODE_COUNT) {
    mailactivesync_wbxml_node_free(node);
    return MAILACTIVESYNC_ERROR_PARSE;
  }

  if (has_content) {
    r = decode_node_content(ctx, node, depth);
    if (r != MAILACTIVESYNC_NO_ERROR) {
      mailactivesync_wbxml_node_free(node);
      return r;
    }
  }

  * result = node;
  return MAILACTIVESYNC_NO_ERROR;
}

int mailactivesync_wbxml_decode(
    const unsigned char * data,
    size_t data_len,
    struct mailactivesync_wbxml_document ** result)
{
  struct wbxml_decode_context ctx;
  struct mailactivesync_wbxml_document * document;
  uint32_t string_table_len;
  int r;

  if ((data == NULL) || (data_len == 0) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;

  memset(&ctx, 0, sizeof(ctx));
  ctx.data = data;
  ctx.len = data_len;
  ctx.current_code_page = 0;

  document = mailactivesync_wbxml_document_new();
  if (document == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  r = read_byte(&ctx, &document->version);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  r = read_mb_uint(&ctx, &document->public_id);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  r = read_mb_uint(&ctx, &document->charset);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;
  r = read_mb_uint(&ctx, &string_table_len);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;

  if (string_table_len > MAILACTIVESYNC_WBXML_MAX_STRING_TABLE_SIZE) {
    r = MAILACTIVESYNC_ERROR_PARSE;
    goto err;
  }
  if (ctx.len - ctx.pos < string_table_len) {
    r = MAILACTIVESYNC_ERROR_PARSE;
    goto err;
  }

  ctx.string_table = ctx.data + ctx.pos;
  ctx.string_table_len = string_table_len;
  ctx.pos += string_table_len;

  r = decode_node(&ctx, &document->root, 0);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto err;

  if (ctx.pos != ctx.len) {
    r = MAILACTIVESYNC_ERROR_PARSE;
    goto err;
  }

  * result = document;
  return MAILACTIVESYNC_NO_ERROR;

 err:
  mailactivesync_wbxml_document_free(document);
  return r;
}
