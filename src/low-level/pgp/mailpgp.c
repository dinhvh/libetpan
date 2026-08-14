/*
 * libEtPan! -- a mail stuff library
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "mailpgp_private.h"

#include <libetpan/clist.h>
#include <libetpan/mailimf.h>
#include <libetpan/mailmime_content.h>
#include <libetpan/mailmime_types_helper.h>
#include <libetpan/mailmime_write_mem.h>
#include <libetpan/mmapstring.h>

#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define PGP_SIGNED_ARMOR "-----BEGIN PGP SIGNED MESSAGE-----"
#define PGP_MESSAGE_ARMOR "-----BEGIN PGP MESSAGE-----"
#define PGP_PUBLIC_KEY_ARMOR "-----BEGIN PGP PUBLIC KEY BLOCK-----"
#define PGP_PUBLIC_KEY_ARMOR_END "-----END PGP PUBLIC KEY BLOCK-----"

static clist * mailpgp_mime_owners = NULL;

static int str_case_equal(const char * a, const char * b)
{
  if ((a == NULL) || (b == NULL))
    return 0;
  return strcasecmp(a, b) == 0;
}

static int has_armor(const char * data, size_t length, const char * marker)
{
  size_t marker_len;
  size_t i;

  if ((data == NULL) || (marker == NULL))
    return 0;

  marker_len = strlen(marker);
  if (length < marker_len)
    return 0;

  for (i = 0; i <= length - marker_len; i ++) {
    if (memcmp(data + i, marker, marker_len) == 0)
      return 1;
  }

  return 0;
}

static char * str_dup(const char * str)
{
  char * result;
  size_t len;

  if (str == NULL)
    return NULL;

  len = strlen(str);
  result = malloc(len + 1);
  if (result == NULL)
    return NULL;
  memcpy(result, str, len + 1);

  return result;
}

static void free_func(void * data, void * context)
{
  (void) context;
  free(data);
}

static void mime_owner_free(struct mailpgp_mime_owner * owner)
{
  if (owner == NULL)
    return;
  if (owner->buffers != NULL) {
    clist_foreach(owner->buffers, free_func, NULL);
    clist_free(owner->buffers);
  }
  free(owner);
}

static struct mailpgp_mime_owner * mime_owner_find(struct mailmime * mime)
{
  clistiter * cur;

  if (mailpgp_mime_owners == NULL)
    return NULL;

  for (cur = clist_begin(mailpgp_mime_owners); cur != NULL;
      cur = clist_next(cur)) {
    struct mailpgp_mime_owner * owner;

    owner = clist_content(cur);
    if ((owner != NULL) && (owner->mime == mime))
      return owner;
  }

  return NULL;
}

static int mime_register_owned_buffer(struct mailmime * mime, char * buffer)
{
  struct mailpgp_mime_owner * owner;

  if ((mime == NULL) || (buffer == NULL))
    return MAILPGP_ERROR_INVAL;

  if (mailpgp_mime_owners == NULL) {
    mailpgp_mime_owners = clist_new();
    if (mailpgp_mime_owners == NULL)
      return MAILPGP_ERROR_MEMORY;
  }

  owner = mime_owner_find(mime);
  if (owner == NULL) {
    owner = calloc(1, sizeof(* owner));
    if (owner == NULL)
      return MAILPGP_ERROR_MEMORY;
    owner->mime = mime;
    owner->buffers = clist_new();
    if (owner->buffers == NULL) {
      mime_owner_free(owner);
      return MAILPGP_ERROR_MEMORY;
    }
    if (clist_append(mailpgp_mime_owners, owner) < 0) {
      mime_owner_free(owner);
      return MAILPGP_ERROR_MEMORY;
    }
  }

  if (clist_append(owner->buffers, buffer) < 0)
    return MAILPGP_ERROR_MEMORY;

  return MAILPGP_NO_ERROR;
}

static void mime_unregister_owned_buffers(struct mailmime * mime)
{
  clistiter * cur;

  if (mailpgp_mime_owners == NULL)
    return;

  for (cur = clist_begin(mailpgp_mime_owners); cur != NULL;
      cur = clist_next(cur)) {
    struct mailpgp_mime_owner * owner;

    owner = clist_content(cur);
    if ((owner != NULL) && (owner->mime == mime)) {
      clist_delete(mailpgp_mime_owners, cur);
      mime_owner_free(owner);
      return;
    }
  }
}

static void mime_unregister_owned_buffers_recursive(struct mailmime * mime)
{
  clistiter * cur;

  if (mime == NULL)
    return;

  if (mime->mm_type == MAILMIME_MULTIPLE) {
    for (cur = clist_begin(mime->mm_data.mm_multipart.mm_mp_list);
        cur != NULL; cur = clist_next(cur))
      mime_unregister_owned_buffers_recursive(clist_content(cur));
  }
  else if ((mime->mm_type == MAILMIME_MESSAGE) &&
      (mime->mm_data.mm_message.mm_msg_mime != NULL)) {
    mime_unregister_owned_buffers_recursive(
        mime->mm_data.mm_message.mm_msg_mime);
  }

  mime_unregister_owned_buffers(mime);
}

static char * mem_dup_to_string(const char * data, size_t length)
{
  char * result;

  result = malloc(length + 1);
  if (result == NULL)
    return NULL;

  if (length != 0)
    memcpy(result, data, length);
  result[length] = '\0';

  return result;
}

static void key_data_free(struct mailpgp_key_data * key)
{
  if (key == NULL)
    return;
  free(key->data);
  free(key->filename);
  free(key->passphrase);
  free(key);
}

static void key_data_free_func(void * data, void * context)
{
  (void) context;
  key_data_free(data);
}

void mailpgp_key_free_internal(struct mailpgp_key * key)
{
  if (key == NULL)
    return;
  free(key->email);
  free(key->name);
  free(key->user_id);
  free(key->fingerprint);
  free(key->key_id);
  free(key->algorithm);
  free(key->armored);
  free(key);
}

static int str_clone_field(char ** dest, const char * src)
{
  if (src == NULL)
    return MAILPGP_NO_ERROR;

  * dest = str_dup(src);
  if (* dest == NULL)
    return MAILPGP_ERROR_MEMORY;

  return MAILPGP_NO_ERROR;
}

static struct mailpgp_key * key_clone(struct mailpgp_key * key)
{
  struct mailpgp_key * clone;

  if (key == NULL)
    return NULL;

  clone = calloc(1, sizeof(* clone));
  if (clone == NULL)
    return NULL;

  if (str_clone_field(&clone->email, key->email) != MAILPGP_NO_ERROR)
    goto free;
  if (str_clone_field(&clone->name, key->name) != MAILPGP_NO_ERROR)
    goto free;
  if (str_clone_field(&clone->user_id, key->user_id) != MAILPGP_NO_ERROR)
    goto free;
  if (str_clone_field(&clone->fingerprint, key->fingerprint) !=
      MAILPGP_NO_ERROR)
    goto free;
  if (str_clone_field(&clone->key_id, key->key_id) != MAILPGP_NO_ERROR)
    goto free;
  if (str_clone_field(&clone->algorithm, key->algorithm) != MAILPGP_NO_ERROR)
    goto free;
  if (key->armored != NULL) {
    clone->armored = mem_dup_to_string(key->armored, key->armored_len);
    if (clone->armored == NULL)
      goto free;
    clone->armored_len = key->armored_len;
  }
  clone->expired = key->expired;
  clone->revoked = key->revoked;

  return clone;

 free:
  mailpgp_key_free_internal(clone);
  return NULL;
}

static void key_free_func(void * data, void * context)
{
  (void) context;
  mailpgp_key_free_internal(data);
}

static void fingerprint_free(struct mailpgp_fingerprint * fingerprint)
{
  if (fingerprint == NULL)
    return;
  free(fingerprint->value);
  free(fingerprint->key_id);
  free(fingerprint);
}

static void fingerprint_free_func(void * data, void * context)
{
  (void) context;
  fingerprint_free(data);
}

struct mailpgp_result * mailpgp_result_new(int status)
{
  struct mailpgp_result * result;

  result = calloc(1, sizeof(* result));
  if (result == NULL)
    return NULL;

  result->status = status;
  result->signers = clist_new();
  if (result->signers == NULL) {
    free(result);
    return NULL;
  }

  return result;
}

static struct mailpgp_fingerprint_result * fingerprint_result_new(void)
{
  struct mailpgp_fingerprint_result * result;

  result = calloc(1, sizeof(* result));
  if (result == NULL)
    return NULL;

  result->fingerprints = clist_new();
  if (result->fingerprints == NULL) {
    free(result);
    return NULL;
  }

  return result;
}

static struct mailpgp_key_list * key_list_new(void)
{
  struct mailpgp_key_list * list;

  list = calloc(1, sizeof(* list));
  if (list == NULL)
    return NULL;

  list->keys = clist_new();
  if (list->keys == NULL) {
    free(list);
    return NULL;
  }

  return list;
}

static int content_type_is(struct mailmime_content * content,
    int discrete_type, int composite_type, const char * subtype)
{
  if ((content == NULL) || (content->ct_type == NULL))
    return 0;

  if (!str_case_equal(content->ct_subtype, subtype))
    return 0;

  switch (content->ct_type->tp_type) {
  case MAILMIME_TYPE_DISCRETE_TYPE:
    if (content->ct_type->tp_data.tp_discrete_type == NULL)
      return 0;
    return content->ct_type->tp_data.tp_discrete_type->dt_type ==
        discrete_type;

  case MAILMIME_TYPE_COMPOSITE_TYPE:
    if (content->ct_type->tp_data.tp_composite_type == NULL)
      return 0;
    return content->ct_type->tp_data.tp_composite_type->ct_type ==
        composite_type;
  }

  return 0;
}

static int content_type_is_application(struct mailmime_content * content,
    const char * subtype)
{
  return content_type_is(content, MAILMIME_DISCRETE_TYPE_APPLICATION,
      MAILMIME_COMPOSITE_TYPE_ERROR, subtype);
}

static int content_type_is_text(struct mailmime_content * content,
    const char * subtype)
{
  return content_type_is(content, MAILMIME_DISCRETE_TYPE_TEXT,
      MAILMIME_COMPOSITE_TYPE_ERROR, subtype);
}

static int content_type_is_multipart(struct mailmime_content * content,
    const char * subtype)
{
  return content_type_is(content, MAILMIME_DISCRETE_TYPE_ERROR,
      MAILMIME_COMPOSITE_TYPE_MULTIPART, subtype);
}

static int content_param_equal(struct mailmime_content * content,
    const char * name, const char * value)
{
  clistiter * cur;

  if ((content == NULL) || (content->ct_parameters == NULL))
    return 0;

  for (cur = clist_begin(content->ct_parameters); cur != NULL;
      cur = clist_next(cur)) {
    struct mailmime_parameter * param;

    param = clist_content(cur);
    if ((param != NULL) && str_case_equal(param->pa_name, name) &&
        str_case_equal(param->pa_value, value))
      return 1;
  }

  return 0;
}

static int mime_body_has_armor(struct mailmime * mime, const char * marker)
{
  struct mailmime_single_fields fields;
  int encoding;
  char * decoded;
  size_t decoded_len;
  size_t index;
  int r;
  int found;

  if ((mime == NULL) || (mime->mm_body == NULL) ||
      (mime->mm_body->dt_type != MAILMIME_DATA_TEXT))
    return 0;

  mailmime_single_fields_init(&fields, mime->mm_mime_fields,
      mime->mm_content_type);
  if (fields.fld_encoding != NULL)
    encoding = fields.fld_encoding->enc_type;
  else
    encoding = MAILMIME_MECHANISM_8BIT;

  decoded = NULL;
  decoded_len = 0;
  index = 0;
  r = mailmime_part_parse(mime->mm_body->dt_data.dt_text.dt_data,
      mime->mm_body->dt_data.dt_text.dt_length, &index, encoding, &decoded,
      &decoded_len);
  if (r != MAILIMF_NO_ERROR)
    return 0;

  found = has_armor(decoded, decoded_len, marker);
  mmap_string_unref(decoded);

  return found;
}

static int mime_body_decode(struct mailmime * mime, char ** result,
    size_t * result_len)
{
  struct mailmime_single_fields fields;
  int encoding;
  size_t index;
  int r;

  if ((mime == NULL) || (result == NULL) || (result_len == NULL) ||
      (mime->mm_body == NULL) ||
      (mime->mm_body->dt_type != MAILMIME_DATA_TEXT))
    return MAILPGP_ERROR_INVAL;

  mailmime_single_fields_init(&fields, mime->mm_mime_fields,
      mime->mm_content_type);
  if (fields.fld_encoding != NULL)
    encoding = fields.fld_encoding->enc_type;
  else
    encoding = MAILMIME_MECHANISM_8BIT;

  index = 0;
  r = mailmime_part_parse(mime->mm_body->dt_data.dt_text.dt_data,
      mime->mm_body->dt_data.dt_text.dt_length, &index, encoding, result,
      result_len);
  if (r != MAILIMF_NO_ERROR)
    return MAILPGP_ERROR_PARSE;

  return MAILPGP_NO_ERROR;
}

static int message_body_bytes(const char * message, size_t length,
    const char ** body, size_t * body_len)
{
  size_t i;

  if ((message == NULL) || (body == NULL) || (body_len == NULL))
    return MAILPGP_ERROR_INVAL;

  for (i = 0; i + 3 < length; i ++) {
    if ((message[i] == '\r') && (message[i + 1] == '\n') &&
        (message[i + 2] == '\r') && (message[i + 3] == '\n')) {
      * body = message + i + 4;
      * body_len = length - i - 4;
      return MAILPGP_NO_ERROR;
    }
  }

  for (i = 0; i + 1 < length; i ++) {
    if ((message[i] == '\n') && (message[i + 1] == '\n')) {
      * body = message + i + 2;
      * body_len = length - i - 2;
      return MAILPGP_NO_ERROR;
    }
  }

  return MAILPGP_ERROR_PARSE;
}

static int mime_is_text_like(struct mailmime * mime)
{
  if (mime == NULL)
    return 0;

  if (mime->mm_content_type == NULL)
    return 1;

  if (content_type_is_text(mime->mm_content_type, "plain"))
    return 1;

  if (content_type_is_application(mime->mm_content_type, "octet-stream"))
    return 1;

  return 0;
}

static int add_key_data(clist * list, const char * data, size_t length,
    const char * filename, const char * passphrase)
{
  struct mailpgp_key_data * key;

  if (list == NULL)
    return MAILPGP_ERROR_INVAL;

  key = calloc(1, sizeof(* key));
  if (key == NULL)
    return MAILPGP_ERROR_MEMORY;

  if (data != NULL) {
    key->data = mem_dup_to_string(data, length);
    if (key->data == NULL) {
      key_data_free(key);
      return MAILPGP_ERROR_MEMORY;
    }
    key->length = length;
  }

  if (filename != NULL) {
    key->filename = str_dup(filename);
    if (key->filename == NULL) {
      key_data_free(key);
      return MAILPGP_ERROR_MEMORY;
    }
  }

  if (passphrase != NULL) {
    key->passphrase = str_dup(passphrase);
    if (key->passphrase == NULL) {
      key_data_free(key);
      return MAILPGP_ERROR_MEMORY;
    }
  }

  if (clist_append(list, key) < 0) {
    key_data_free(key);
    return MAILPGP_ERROR_MEMORY;
  }

  return MAILPGP_NO_ERROR;
}

static int parse_message_mime(const char * message, size_t length,
    struct mailmime ** result)
{
  size_t index;
  int r;

  if (result == NULL)
    return MAILPGP_ERROR_INVAL;

  index = 0;
  r = mailmime_parse(message, length, &index, result);
  if (r != MAILIMF_NO_ERROR)
    return MAILPGP_ERROR_PARSE;

  return MAILPGP_NO_ERROR;
}

static int mmap_to_alloc(MMAPString * mmapstr, char ** result,
    size_t * result_len)
{
  char * str;

  str = malloc(mmapstr->len + 1);
  if (str == NULL)
    return MAILPGP_ERROR_MEMORY;

  memcpy(str, mmapstr->str, mmapstr->len);
  str[mmapstr->len] = '\0';
  * result = str;
  * result_len = mmapstr->len;

  return MAILPGP_NO_ERROR;
}

static int write_mime_to_mem(struct mailmime * mime, char ** result,
    size_t * result_len)
{
  MMAPString * mmapstr;
  int col;
  int r;

  if ((mime == NULL) || (result == NULL) || (result_len == NULL))
    return MAILPGP_ERROR_INVAL;

  mmapstr = mmap_string_new("");
  if (mmapstr == NULL)
    return MAILPGP_ERROR_MEMORY;

  col = 0;
  r = mailmime_write_mem(mmapstr, &col, mime);
  if (r != MAILIMF_NO_ERROR) {
    mmap_string_free(mmapstr);
    return MAILPGP_ERROR_PARSE;
  }

  r = mmap_to_alloc(mmapstr, result, result_len);
  mmap_string_free(mmapstr);

  return r;
}

static int write_mime_sub_to_mem(struct mailmime * mime, char ** result,
    size_t * result_len)
{
  struct mailmime * saved_parent;
  int saved_parent_type;
  int r;

  if (mime == NULL)
    return MAILPGP_ERROR_INVAL;

  saved_parent = mime->mm_parent;
  saved_parent_type = mime->mm_parent_type;
  if (mime->mm_parent == NULL) {
    mime->mm_parent = mime;
    mime->mm_parent_type = MAILMIME_MULTIPLE;
  }

  r = write_mime_to_mem(mime, result, result_len);

  mime->mm_parent = saved_parent;
  mime->mm_parent_type = saved_parent_type;

  return r;
}

static int parse_mime_from_mem(char * data, size_t len,
    struct mailmime ** result)
{
  size_t index;
  int r;

  index = 0;
  r = mailmime_parse(data, len, &index, result);
  if (r != MAILIMF_NO_ERROR)
    return MAILPGP_ERROR_PARSE;

  r = mime_register_owned_buffer(* result, data);
  if (r != MAILPGP_NO_ERROR) {
    mailmime_free(* result);
    * result = NULL;
    return r;
  }

  return MAILPGP_NO_ERROR;
}

static int add_param(struct mailmime_content * content, const char * name,
    const char * value)
{
  struct mailmime_parameter * param;

  if ((content == NULL) || (content->ct_parameters == NULL))
    return MAILPGP_ERROR_INVAL;

  param = mailmime_param_new_with_data((char *) name, (char *) value);
  if (param == NULL)
    return MAILPGP_ERROR_MEMORY;

  if (clist_append(content->ct_parameters, param) < 0) {
    mailmime_parameter_free(param);
    return MAILPGP_ERROR_MEMORY;
  }

  return MAILPGP_NO_ERROR;
}

static struct mailmime_fields * fields_with_encoding_and_disposition(
    int encoding_type, int disposition_type, const char * filename)
{
  struct mailmime_mechanism * encoding;
  struct mailmime_disposition * disposition;
  struct mailmime_fields * fields;

  encoding = mailmime_mechanism_new(encoding_type, NULL);
  if (encoding == NULL)
    return NULL;

  disposition = mailmime_disposition_new_filename(disposition_type,
      filename != NULL ? str_dup(filename) : NULL);
  if (disposition == NULL)
    goto free_encoding;

  fields = mailmime_fields_new_with_data(encoding, NULL, NULL, disposition,
      NULL);
  if (fields == NULL)
    goto free_disposition;

  return fields;

 free_disposition:
  mailmime_disposition_free(disposition);
 free_encoding:
  mailmime_encoding_free(encoding);
  return NULL;
}

static int make_single_body(const char * content_type,
    struct mailmime_fields * fields, int body_encoding, const char * body,
    size_t body_len, struct mailmime ** result)
{
  struct mailmime * mime;
  struct mailmime_data * data;
  char * body_copy;
  int r;

  if ((content_type == NULL) || (body == NULL) || (result == NULL))
    return MAILPGP_ERROR_INVAL;

  r = mailmime_new_with_content(content_type, fields, &mime);
  if (r != MAILIMF_NO_ERROR)
    return MAILPGP_ERROR_MEMORY;

  body_copy = mem_dup_to_string(body, body_len);
  if (body_copy == NULL) {
    mailmime_free(mime);
    return MAILPGP_ERROR_MEMORY;
  }

  data = mailmime_data_new_data(body_encoding,
      body_encoding == MAILMIME_MECHANISM_BASE64 ? 0 : 1, body_copy,
      body_len);
  if (data == NULL) {
    free(body_copy);
    mailmime_free(mime);
    return MAILPGP_ERROR_MEMORY;
  }

  mime->mm_data.mm_single = data;
  mime->mm_body = data;
  r = mime_register_owned_buffer(mime, body_copy);
  if (r != MAILPGP_NO_ERROR) {
    mailmime_free(mime);
    free(body_copy);
    return r;
  }

  * result = mime;
  return MAILPGP_NO_ERROR;
}

static int copy_mime(struct mailmime * mime, struct mailmime ** result)
{
  char * data;
  size_t len;
  int r;

  r = write_mime_to_mem(mime, &data, &len);
  if (r != MAILPGP_NO_ERROR)
    return r;

  r = parse_mime_from_mem(data, len, result);
  if (r != MAILPGP_NO_ERROR)
    free(data);

  return r;
}

static struct mailmime * multipart_part(struct mailmime * mime,
    unsigned int index)
{
  if ((mime == NULL) || (mime->mm_type == MAILMIME_MESSAGE))
    return mime != NULL ? multipart_part(
        mime->mm_data.mm_message.mm_msg_mime, index) : NULL;

  if ((mime == NULL) || (mime->mm_type != MAILMIME_MULTIPLE) ||
      (mime->mm_data.mm_multipart.mm_mp_list == NULL))
    return NULL;

  return clist_nth_data(mime->mm_data.mm_multipart.mm_mp_list, index);
}

static int make_signature_part(const char * armor, size_t armor_len,
    struct mailmime ** result)
{
  struct mailmime_fields * fields;
  struct mailmime * mime;
  int r;

  fields = fields_with_encoding_and_disposition(MAILMIME_MECHANISM_7BIT,
      MAILMIME_DISPOSITION_TYPE_ATTACHMENT, "signature.asc");
  if (fields == NULL)
    return MAILPGP_ERROR_MEMORY;

  r = make_single_body("application/pgp-signature", fields,
      MAILMIME_MECHANISM_7BIT, armor, armor_len, &mime);
  if (r != MAILPGP_NO_ERROR) {
    mailmime_fields_free(fields);
    return r;
  }

  r = add_param(mime->mm_content_type, "name", "signature.asc");
  if (r != MAILPGP_NO_ERROR) {
    mailpgp_mime_free(mime);
    return r;
  }

  * result = mime;
  return MAILPGP_NO_ERROR;
}

static int make_multipart_signed_with_part(struct mailmime * original_part,
    const char * sig_armor, size_t sig_armor_len, struct mailmime ** result)
{
  struct mailmime * multipart;
  struct mailmime * signature;
  int r;

  if ((original_part == NULL) || (sig_armor == NULL) || (result == NULL))
    return MAILPGP_ERROR_INVAL;

  r = mailmime_new_with_content("multipart/signed", NULL, &multipart);
  if (r != MAILIMF_NO_ERROR)
    return MAILPGP_ERROR_MEMORY;

  r = add_param(multipart->mm_content_type, "protocol",
      "application/pgp-signature");
  if (r != MAILPGP_NO_ERROR)
    goto free_multipart;

  r = add_param(multipart->mm_content_type, "micalg", "pgp-sha256");
  if (r != MAILPGP_NO_ERROR)
    goto free_multipart;

  r = make_signature_part(sig_armor, sig_armor_len, &signature);
  if (r != MAILPGP_NO_ERROR)
    goto free_multipart;

  r = mailmime_smart_add_part(multipart, original_part);
  if (r != MAILIMF_NO_ERROR)
    goto free_signature;

  r = mailmime_smart_add_part(multipart, signature);
  if (r != MAILIMF_NO_ERROR) {
    mailpgp_mime_free(signature);
    goto free_multipart;
  }

  * result = multipart;
  return MAILPGP_NO_ERROR;

 free_signature:
  mailpgp_mime_free(signature);
 free_multipart:
  mailpgp_mime_free(multipart);
  return MAILPGP_ERROR_MEMORY;
}

static int make_multipart_signed(struct mailmime * original,
    const char * sig_armor, size_t sig_armor_len, struct mailmime ** result)
{
  struct mailmime * original_copy;
  int r;

  r = copy_mime(original, &original_copy);
  if (r != MAILPGP_NO_ERROR)
    return r;

  r = make_multipart_signed_with_part(original_copy, sig_armor,
      sig_armor_len, result);
  if (r != MAILPGP_NO_ERROR)
    mailpgp_mime_free(original_copy);

  return r;
}

static int make_encrypted_version_part(struct mailmime ** result)
{
  struct mailmime * mime;
  int r;

  r = make_single_body("application/pgp-encrypted", NULL,
      MAILMIME_MECHANISM_7BIT, "Version: 1\r\n",
      strlen("Version: 1\r\n"), &mime);
  if (r != MAILPGP_NO_ERROR)
    return r;

  * result = mime;
  return MAILPGP_NO_ERROR;
}

static int make_encrypted_data_part(const char * armor, size_t armor_len,
    struct mailmime ** result)
{
  struct mailmime_fields * fields;
  int r;

  fields = fields_with_encoding_and_disposition(MAILMIME_MECHANISM_7BIT,
      MAILMIME_DISPOSITION_TYPE_ATTACHMENT, "encrypted.asc");
  if (fields == NULL)
    return MAILPGP_ERROR_MEMORY;

  r = make_single_body("application/octet-stream", fields,
      MAILMIME_MECHANISM_7BIT, armor, armor_len, result);
  if (r != MAILPGP_NO_ERROR) {
    mailmime_fields_free(fields);
    return r;
  }

  return MAILPGP_NO_ERROR;
}

static int make_multipart_encrypted(const char * armor, size_t armor_len,
    struct mailmime ** result)
{
  struct mailmime * multipart;
  struct mailmime * version;
  struct mailmime * encrypted;
  int r;

  r = mailmime_new_with_content("multipart/encrypted", NULL, &multipart);
  if (r != MAILIMF_NO_ERROR)
    return MAILPGP_ERROR_MEMORY;

  r = add_param(multipart->mm_content_type, "protocol",
      "application/pgp-encrypted");
  if (r != MAILPGP_NO_ERROR)
    goto free_multipart;

  r = make_encrypted_version_part(&version);
  if (r != MAILPGP_NO_ERROR)
    goto free_multipart;

  r = make_encrypted_data_part(armor, armor_len, &encrypted);
  if (r != MAILPGP_NO_ERROR)
    goto free_version;

  r = mailmime_smart_add_part(multipart, version);
  if (r != MAILIMF_NO_ERROR)
    goto free_encrypted;

  r = mailmime_smart_add_part(multipart, encrypted);
  if (r != MAILIMF_NO_ERROR) {
    mailpgp_mime_free(encrypted);
    goto free_multipart;
  }

  * result = multipart;
  return MAILPGP_NO_ERROR;

 free_encrypted:
  mailpgp_mime_free(encrypted);
 free_version:
  mailpgp_mime_free(version);
 free_multipart:
  mailpgp_mime_free(multipart);
  return MAILPGP_ERROR_MEMORY;
}

int mailpgp_fingerprint_result_add(struct mailpgp_fingerprint_result * result,
    const char * text, size_t length, const char * key_id, int source,
    int verified);

#ifdef USE_PGP_RNP
static int set_field_from_rnp_buffer(char ** field, char * value)
{
  char * copy;

  if (value == NULL)
    return MAILPGP_NO_ERROR;

  copy = str_dup(value);
  if (value != NULL)
    rnp_buffer_destroy(value);
  if (copy == NULL)
    return MAILPGP_ERROR_MEMORY;

  free(* field);
  * field = copy;

  return MAILPGP_NO_ERROR;
}

static char * email_from_user_id(const char * user_id)
{
  const char * begin;
  const char * end;

  if (user_id == NULL)
    return NULL;

  begin = strchr(user_id, '<');
  end = strchr(user_id, '>');
  if ((begin == NULL) || (end == NULL) || (end <= begin + 1))
    return NULL;

  return mem_dup_to_string(begin + 1, (size_t) (end - begin - 1));
}

static char * name_from_user_id(const char * user_id)
{
  const char * lt;
  size_t len;

  if (user_id == NULL)
    return NULL;

  lt = strchr(user_id, '<');
  if (lt == NULL)
    return NULL;

  len = (size_t) (lt - user_id);
  while ((len != 0) && isspace((unsigned char) user_id[len - 1]))
    len --;
  if (len == 0)
    return NULL;

  return mem_dup_to_string(user_id, len);
}

int mailpgp_key_fill_from_rnp_handle(struct mailpgp_key * key,
    rnp_key_handle_t handle)
{
  char * value;
  bool boolean_value;
  int r;

  if ((key == NULL) || (handle == NULL))
    return MAILPGP_ERROR_INVAL;

  value = NULL;
  if (rnp_key_get_primary_uid(handle, &value) == RNP_SUCCESS) {
    r = set_field_from_rnp_buffer(&key->user_id, value);
    if (r != MAILPGP_NO_ERROR)
      return r;

    key->email = email_from_user_id(key->user_id);
    key->name = name_from_user_id(key->user_id);
  }

  value = NULL;
  if (rnp_key_get_fprint(handle, &value) == RNP_SUCCESS) {
    r = set_field_from_rnp_buffer(&key->fingerprint, value);
    if (r != MAILPGP_NO_ERROR)
      return r;
  }

  value = NULL;
  if (rnp_key_get_keyid(handle, &value) == RNP_SUCCESS) {
    r = set_field_from_rnp_buffer(&key->key_id, value);
    if (r != MAILPGP_NO_ERROR)
      return r;
  }

  value = NULL;
  if (rnp_key_get_alg(handle, &value) == RNP_SUCCESS) {
    r = set_field_from_rnp_buffer(&key->algorithm, value);
    if (r != MAILPGP_NO_ERROR)
      return r;
  }

  boolean_value = false;
  if (rnp_key_is_expired(handle, &boolean_value) == RNP_SUCCESS)
    key->expired = boolean_value ? 1 : 0;

  boolean_value = false;
  if (rnp_key_is_revoked(handle, &boolean_value) == RNP_SUCCESS)
    key->revoked = boolean_value ? 1 : 0;

  return MAILPGP_NO_ERROR;
}

#endif

static int add_public_key_block(struct mailpgp_key_list * list,
    const char * data, size_t length)
{
  struct mailpgp_key * key;

  if ((list == NULL) || (data == NULL))
    return MAILPGP_ERROR_INVAL;

  key = calloc(1, sizeof(* key));
  if (key == NULL)
    return MAILPGP_ERROR_MEMORY;

  key->armored = mem_dup_to_string(data, length);
  if (key->armored == NULL) {
    mailpgp_key_free_internal(key);
    return MAILPGP_ERROR_MEMORY;
  }
  key->armored_len = length;

#ifdef USE_PGP_RNP
  (void) mailpgp_rnp_enrich_key(key, data, length);
#endif

  if (clist_append(list->keys, key) < 0) {
    mailpgp_key_free_internal(key);
    return MAILPGP_ERROR_MEMORY;
  }

  return MAILPGP_NO_ERROR;
}

static int scan_public_key_blocks(struct mailpgp_key_list * list,
    const char * data, size_t length)
{
  size_t begin_len;
  size_t end_len;
  size_t i;

  if ((list == NULL) || (data == NULL))
    return MAILPGP_ERROR_INVAL;

  begin_len = strlen(PGP_PUBLIC_KEY_ARMOR);
  end_len = strlen(PGP_PUBLIC_KEY_ARMOR_END);
  if (length < begin_len + end_len)
    return MAILPGP_NO_ERROR;

  i = 0;
  while (i <= length - begin_len) {
    size_t block_start;
    size_t j;

    if (memcmp(data + i, PGP_PUBLIC_KEY_ARMOR, begin_len) != 0) {
      i ++;
      continue;
    }

    block_start = i;
    j = i + begin_len;
    while (j <= length - end_len) {
      if (memcmp(data + j, PGP_PUBLIC_KEY_ARMOR_END, end_len) == 0) {
        size_t block_end;
        int r;

        block_end = j + end_len;
        while ((block_end < length) &&
            ((data[block_end] == '\r') || (data[block_end] == '\n')))
          block_end ++;

        r = add_public_key_block(list, data + block_start,
            block_end - block_start);
        if (r != MAILPGP_NO_ERROR)
          return r;

        i = block_end;
        break;
      }
      j ++;
    }

    if (j > length - end_len)
      break;
  }

  return MAILPGP_NO_ERROR;
}

static int scan_public_keys_mime(struct mailpgp_key_list * list,
    struct mailmime * mime)
{
  clistiter * cur;

  if ((list == NULL) || (mime == NULL))
    return MAILPGP_ERROR_INVAL;

  switch (mime->mm_type) {
  case MAILMIME_SINGLE:
    if (mailpgp_is_key(mime)) {
      char * decoded;
      size_t decoded_len;
      int r;

      decoded = NULL;
      decoded_len = 0;
      r = mime_body_decode(mime, &decoded, &decoded_len);
      if (r == MAILPGP_NO_ERROR) {
        r = scan_public_key_blocks(list, decoded, decoded_len);
        mmap_string_unref(decoded);
      }
      if ((r != MAILPGP_NO_ERROR) && (r != MAILPGP_ERROR_INVAL))
        return r;
    }
    break;

  case MAILMIME_MULTIPLE:
    for (cur = clist_begin(mime->mm_data.mm_multipart.mm_mp_list);
        cur != NULL; cur = clist_next(cur)) {
      int r;

      r = scan_public_keys_mime(list, clist_content(cur));
      if (r != MAILPGP_NO_ERROR)
        return r;
    }
    break;

  case MAILMIME_MESSAGE:
    if (mime->mm_data.mm_message.mm_msg_mime != NULL)
      return scan_public_keys_mime(list,
          mime->mm_data.mm_message.mm_msg_mime);
    break;
  }

  return MAILPGP_NO_ERROR;
}

static int maybe_hex_fingerprint(const char * text, size_t length)
{
  size_t hex_count;
  size_t i;

  hex_count = 0;
  for (i = 0; i < length; i ++) {
    if (isxdigit((unsigned char) text[i]))
      hex_count ++;
    else if ((text[i] == ' ') || (text[i] == ':'))
      continue;
    else
      return 0;
  }

  return (hex_count == 40) || (hex_count == 64);
}

static char * normalize_fingerprint(const char * text, size_t length)
{
  char * result;
  size_t i;
  size_t j;

  result = malloc(length + 1);
  if (result == NULL)
    return NULL;

  j = 0;
  for (i = 0; i < length; i ++) {
    if (isxdigit((unsigned char) text[i])) {
      result[j] = (char) toupper((unsigned char) text[i]);
      j ++;
    }
  }
  result[j] = '\0';

  return result;
}

static int fingerprint_exists(struct mailpgp_fingerprint_result * result,
    const char * value)
{
  clistiter * cur;

  if ((result == NULL) || (value == NULL))
    return 0;

  for (cur = clist_begin(result->fingerprints); cur != NULL;
      cur = clist_next(cur)) {
    struct mailpgp_fingerprint * fingerprint;

    fingerprint = clist_content(cur);
    if ((fingerprint != NULL) && str_case_equal(fingerprint->value, value))
      return 1;
  }

  return 0;
}

static int key_id_exists(struct mailpgp_fingerprint_result * result,
    const char * key_id, int source)
{
  clistiter * cur;

  if ((result == NULL) || (key_id == NULL))
    return 0;

  for (cur = clist_begin(result->fingerprints); cur != NULL;
      cur = clist_next(cur)) {
    struct mailpgp_fingerprint * fingerprint;

    fingerprint = clist_content(cur);
    if ((fingerprint != NULL) && (fingerprint->source == source) &&
        str_case_equal(fingerprint->key_id, key_id))
      return 1;
  }

  return 0;
}

static char * normalize_key_id(const char * text)
{
  char * result;
  size_t length;
  size_t i;

  if (text == NULL)
    return NULL;

  length = strlen(text);
  if ((length != 16) && (length != 40))
    return NULL;

  result = malloc(length + 1);
  if (result == NULL)
    return NULL;

  for (i = 0; i < length; i ++) {
    if (!isxdigit((unsigned char) text[i])) {
      free(result);
      return NULL;
    }
    result[i] = (char) toupper((unsigned char) text[i]);
  }
  result[length] = '\0';

  return result;
}

int mailpgp_fingerprint_result_add_key_id(
    struct mailpgp_fingerprint_result * result, const char * key_id,
    int source)
{
  struct mailpgp_fingerprint * fingerprint;
  char * normalized;

  if ((result == NULL) || (key_id == NULL))
    return MAILPGP_ERROR_INVAL;

  normalized = normalize_key_id(key_id);
  if (normalized == NULL)
    return MAILPGP_NO_ERROR;

  if (key_id_exists(result, normalized, source)) {
    free(normalized);
    return MAILPGP_NO_ERROR;
  }

  fingerprint = calloc(1, sizeof(* fingerprint));
  if (fingerprint == NULL) {
    free(normalized);
    return MAILPGP_ERROR_MEMORY;
  }

  fingerprint->key_id = normalized;
  fingerprint->source = source;
  fingerprint->verified = 0;

  if (clist_append(result->fingerprints, fingerprint) < 0) {
    fingerprint_free(fingerprint);
    return MAILPGP_ERROR_MEMORY;
  }

  return MAILPGP_NO_ERROR;
}

int mailpgp_fingerprint_result_add(struct mailpgp_fingerprint_result * result,
    const char * text, size_t length, const char * key_id, int source,
    int verified)
{
  struct mailpgp_fingerprint * fingerprint;
  char * normalized;

  if ((result == NULL) || (text == NULL))
    return MAILPGP_ERROR_INVAL;

  if (!maybe_hex_fingerprint(text, length))
    return MAILPGP_NO_ERROR;

  normalized = normalize_fingerprint(text, length);
  if (normalized == NULL)
    return MAILPGP_ERROR_MEMORY;

  if (fingerprint_exists(result, normalized)) {
    free(normalized);
    return MAILPGP_NO_ERROR;
  }

  fingerprint = calloc(1, sizeof(* fingerprint));
  if (fingerprint == NULL) {
    free(normalized);
    return MAILPGP_ERROR_MEMORY;
  }

  fingerprint->value = normalized;
  if (key_id != NULL) {
    fingerprint->key_id = str_dup(key_id);
    if (fingerprint->key_id == NULL) {
      fingerprint_free(fingerprint);
      return MAILPGP_ERROR_MEMORY;
    }
  }
  fingerprint->source = source;
  fingerprint->verified = verified;

  if (clist_append(result->fingerprints, fingerprint) < 0) {
    fingerprint_free(fingerprint);
    return MAILPGP_ERROR_MEMORY;
  }

  return MAILPGP_NO_ERROR;
}

static int scan_header_hint_line(struct mailpgp_fingerprint_result * result,
    const char * line, size_t length)
{
  size_t i;
  int r;

  for (i = 0; i < length; i ++) {
    size_t start;
    size_t token_len;

    while ((i < length) &&
        !isxdigit((unsigned char) line[i]))
      i ++;
    start = i;
    while ((i < length) &&
        (isxdigit((unsigned char) line[i]) || (line[i] == ' ') ||
        (line[i] == ':')))
      i ++;
    token_len = i - start;
    if (token_len != 0) {
      r = mailpgp_fingerprint_result_add(result, line + start, token_len, NULL,
          MAILPGP_FINGERPRINT_SOURCE_HEADER_HINT, 0);
      if (r != MAILPGP_NO_ERROR)
        return r;
    }
  }

  return MAILPGP_NO_ERROR;
}

static int scan_message_header_hints(struct mailpgp_fingerprint_result * result,
    const char * message, size_t length)
{
  size_t i;
  size_t line_start;
  int interesting;
  int r;

  if ((message == NULL) || (result == NULL))
    return MAILPGP_ERROR_INVAL;

  i = 0;
  line_start = 0;
  interesting = 0;
  while (line_start < length) {
    size_t line_end;
    size_t line_len;

    line_end = line_start;
    while ((line_end < length) && (message[line_end] != '\n'))
      line_end ++;

    line_len = line_end - line_start;
    if ((line_len != 0) && (message[line_start + line_len - 1] == '\r'))
      line_len --;

    if (line_len == 0)
      break;

    if ((message[line_start] != ' ') && (message[line_start] != '\t')) {
      interesting = 0;
      if ((line_len >= 8) &&
          (strncasecmp(message + line_start, "OpenPGP:", 8) == 0))
        interesting = 1;
      else if ((line_len >= 10) &&
          (strncasecmp(message + line_start, "Autocrypt:", 10) == 0))
        interesting = 1;
      else if ((line_len >= 10) &&
          (strncasecmp(message + line_start, "X-PGP-Key:", 10) == 0))
        interesting = 1;
      else if ((line_len >= 18) &&
          (strncasecmp(message + line_start, "X-PGP-Fingerprint:", 18) == 0))
        interesting = 1;
    }

    if (interesting) {
      r = scan_header_hint_line(result, message + line_start, line_len);
      if (r != MAILPGP_NO_ERROR)
        return r;
    }

    i = line_end;
    if ((i < length) && (message[i] == '\n'))
      i ++;
    line_start = i;
  }

  return MAILPGP_NO_ERROR;
}

static int add_public_key_fingerprints(
    struct mailpgp_fingerprint_result * fingerprints,
    struct mailpgp_key_list * keys)
{
  clistiter * cur;

  if ((fingerprints == NULL) || (keys == NULL) || (keys->keys == NULL))
    return MAILPGP_NO_ERROR;

  for (cur = clist_begin(keys->keys); cur != NULL; cur = clist_next(cur)) {
    struct mailpgp_key * key;
    int r;

    key = clist_content(cur);
    if ((key == NULL) || (key->fingerprint == NULL))
      continue;

    r = mailpgp_fingerprint_result_add(fingerprints, key->fingerprint,
        strlen(key->fingerprint), key->key_id,
        MAILPGP_FINGERPRINT_SOURCE_PUBLIC_KEY, 1);
    if (r != MAILPGP_NO_ERROR)
      return r;
  }

  return MAILPGP_NO_ERROR;
}

struct mailpgp * mailpgp_new(void)
{
  struct mailpgp * pgp;

  pgp = calloc(1, sizeof(* pgp));
  if (pgp == NULL)
    return NULL;

  pgp->public_keys = clist_new();
  pgp->secret_keys = clist_new();
  if ((pgp->public_keys == NULL) || (pgp->secret_keys == NULL)) {
    mailpgp_free(pgp);
    return NULL;
  }

#ifdef USE_PGP_RNP
  if (mailpgp_rnp_init(pgp) != MAILPGP_NO_ERROR) {
    mailpgp_free(pgp);
    return NULL;
  }
#endif

  return pgp;
}

void mailpgp_free(struct mailpgp * pgp)
{
  if (pgp == NULL)
    return;

  if (pgp->public_keys != NULL) {
    clist_foreach(pgp->public_keys, key_data_free_func, NULL);
    clist_free(pgp->public_keys);
  }
  if (pgp->secret_keys != NULL) {
    clist_foreach(pgp->secret_keys, key_data_free_func, NULL);
    clist_free(pgp->secret_keys);
  }
#ifdef USE_PGP_RNP
  mailpgp_rnp_done(pgp);
#endif
  free(pgp);
}

int mailpgp_set_passphrase_callback(struct mailpgp * pgp,
    mailpgp_passphrase_callback callback, void * context)
{
  if (pgp == NULL)
    return MAILPGP_ERROR_INVAL;

  pgp->passphrase_callback = callback;
  pgp->passphrase_context = context;

  return MAILPGP_NO_ERROR;
}

int mailpgp_add_public_key_file(struct mailpgp * pgp, const char * filename)
{
  int r;

  if ((pgp == NULL) || (filename == NULL))
    return MAILPGP_ERROR_INVAL;

  r = add_key_data(pgp->public_keys, NULL, 0, filename, NULL);
  if (r != MAILPGP_NO_ERROR)
    return r;

#ifdef USE_PGP_RNP
  r = mailpgp_rnp_import_public_key_file(pgp, filename);
  if (r != MAILPGP_NO_ERROR)
    return r;
#endif

  return MAILPGP_NO_ERROR;
}

int mailpgp_add_secret_key_file(struct mailpgp * pgp, const char * filename,
    const char * passphrase)
{
  int r;

  if ((pgp == NULL) || (filename == NULL))
    return MAILPGP_ERROR_INVAL;

  r = add_key_data(pgp->secret_keys, NULL, 0, filename, passphrase);
  if (r != MAILPGP_NO_ERROR)
    return r;

#ifdef USE_PGP_RNP
  r = mailpgp_rnp_import_secret_key_file(pgp, filename);
  if (r != MAILPGP_NO_ERROR)
    return r;
#endif

  return MAILPGP_NO_ERROR;
}

int mailpgp_add_public_key(struct mailpgp * pgp, const char * data,
    size_t length)
{
  int r;

  if ((pgp == NULL) || ((data == NULL) && (length != 0)))
    return MAILPGP_ERROR_INVAL;

  r = add_key_data(pgp->public_keys, data, length, NULL, NULL);
  if (r != MAILPGP_NO_ERROR)
    return r;

#ifdef USE_PGP_RNP
  r = mailpgp_rnp_import_public_key(pgp, data, length);
  if (r != MAILPGP_NO_ERROR)
    return r;
#endif

  return MAILPGP_NO_ERROR;
}

int mailpgp_add_secret_key(struct mailpgp * pgp, const char * data,
    size_t length, const char * passphrase)
{
  int r;

  if ((pgp == NULL) || ((data == NULL) && (length != 0)))
    return MAILPGP_ERROR_INVAL;

  r = add_key_data(pgp->secret_keys, data, length, NULL, passphrase);
  if (r != MAILPGP_NO_ERROR)
    return r;

#ifdef USE_PGP_RNP
  r = mailpgp_rnp_import_secret_key(pgp, data, length);
  if (r != MAILPGP_NO_ERROR)
    return r;
#endif

  return MAILPGP_NO_ERROR;
}

int mailpgp_is_signed(struct mailmime * mime)
{
  if ((mime != NULL) && (mime->mm_type == MAILMIME_MESSAGE))
    return mailpgp_is_signed(mime->mm_data.mm_message.mm_msg_mime);

  if ((mime == NULL) || (mime->mm_content_type == NULL))
    return 0;

  if (content_type_is_multipart(mime->mm_content_type, "signed") &&
      content_param_equal(mime->mm_content_type, "protocol",
      "application/pgp-signature"))
    return 1;

  if (content_type_is_application(mime->mm_content_type, "pgp-signature"))
    return 1;

  if ((mime->mm_type == MAILMIME_SINGLE) && mime_is_text_like(mime) &&
      mime_body_has_armor(mime, PGP_SIGNED_ARMOR))
    return 1;

  return 0;
}

int mailpgp_is_encrypted(struct mailmime * mime)
{
  if ((mime != NULL) && (mime->mm_type == MAILMIME_MESSAGE))
    return mailpgp_is_encrypted(mime->mm_data.mm_message.mm_msg_mime);

  if ((mime == NULL) || (mime->mm_content_type == NULL))
    return 0;

  if (content_type_is_multipart(mime->mm_content_type, "encrypted") &&
      content_param_equal(mime->mm_content_type, "protocol",
      "application/pgp-encrypted"))
    return 1;

  if ((mime->mm_type == MAILMIME_SINGLE) && mime_is_text_like(mime) &&
      mime_body_has_armor(mime, PGP_MESSAGE_ARMOR))
    return 1;

  return 0;
}

int mailpgp_is_key(struct mailmime * mime)
{
  if ((mime != NULL) && (mime->mm_type == MAILMIME_MESSAGE))
    return mailpgp_is_key(mime->mm_data.mm_message.mm_msg_mime);

  if ((mime == NULL) || (mime->mm_content_type == NULL))
    return 0;

  if (content_type_is_application(mime->mm_content_type, "pgp-keys"))
    return 1;

  if ((mime->mm_type == MAILMIME_SINGLE) && mime_is_text_like(mime) &&
      mime_body_has_armor(mime, PGP_PUBLIC_KEY_ARMOR))
    return 1;

  return 0;
}

int mailpgp_is_pgp(struct mailmime * mime)
{
  if (mailpgp_is_signed(mime))
    return 1;
  if (mailpgp_is_encrypted(mime))
    return 1;
  if (mailpgp_is_key(mime))
    return 1;

  return 0;
}

int mailpgp_message_verify(struct mailpgp * pgp, const char * message,
    size_t length, struct mailpgp_result ** result)
{
  struct mailmime * mime;
  int r;

  mime = NULL;
  r = parse_message_mime(message, length, &mime);
  if (r != MAILPGP_NO_ERROR)
    return r;

  r = mailpgp_verify(pgp, message, length, mime, result);
  mailmime_free(mime);

  return r;
}

int mailpgp_verify(struct mailpgp * pgp, const char * message, size_t length,
    struct mailmime * mime, struct mailpgp_result ** result)
{
#ifdef USE_PGP_RNP
  {
    struct mailmime * signed_part;
    struct mailmime * signature_part;
    char * signed_data;
    size_t signed_data_len;
    char * serialized_signed_data;
    char * signature;
    size_t signature_len;
    struct mailpgp_result * verify_result;
    int r;

    if ((pgp == NULL) || (mime == NULL) || (result == NULL))
      return MAILPGP_ERROR_INVAL;
    if (!mailpgp_is_signed(mime))
      return MAILPGP_ERROR_NOT_IMPLEMENTED;

    if ((mime->mm_type == MAILMIME_MESSAGE) &&
        (mime->mm_data.mm_message.mm_msg_mime != NULL))
      mime = mime->mm_data.mm_message.mm_msg_mime;

    if ((mime->mm_type == MAILMIME_SINGLE) && mime_is_text_like(mime) &&
        mime_body_has_armor(mime, PGP_SIGNED_ARMOR)) {
      char * inline_data;
      const char * verify_data;
      size_t inline_data_len;
      size_t verify_data_len;
      int inline_r;

      inline_data = NULL;
      inline_data_len = 0;
      verify_data = NULL;
      verify_data_len = 0;

      if ((message != NULL) &&
          (message_body_bytes(message, length, &verify_data,
          &verify_data_len) == MAILPGP_NO_ERROR)) {
        inline_r = MAILPGP_NO_ERROR;
      }
      else {
        inline_r = mime_body_decode(mime, &inline_data, &inline_data_len);
        if (inline_r != MAILPGP_NO_ERROR)
          return inline_r;
        verify_data = inline_data;
        verify_data_len = inline_data_len;
      }

      inline_r = mailpgp_rnp_verify_inline(pgp, verify_data,
          verify_data_len, result);
      if (inline_data != NULL)
        mmap_string_unref(inline_data);
      return inline_r;
    }

    signed_part = multipart_part(mime, 0);
    signature_part = multipart_part(mime, 1);
    if ((signed_part == NULL) || (signature_part == NULL) ||
        !content_type_is_application(signature_part->mm_content_type,
        "pgp-signature"))
      return MAILPGP_ERROR_NOT_IMPLEMENTED;

    signed_data = (char *) signed_part->mm_mime_start;
    signed_data_len = signed_part->mm_length;
    serialized_signed_data = NULL;
    if ((signed_data == NULL) || (signed_data_len == 0)) {
      r = write_mime_to_mem(signed_part, &serialized_signed_data,
          &signed_data_len);
      if (r != MAILPGP_NO_ERROR)
        return r;
      signed_data = serialized_signed_data;
    }

    signature = NULL;
    signature_len = 0;
    r = mime_body_decode(signature_part, &signature, &signature_len);
    if (r != MAILPGP_NO_ERROR) {
      free(serialized_signed_data);
      return r;
    }

    verify_result = NULL;
    r = mailpgp_rnp_verify_detached(pgp, signed_data, signed_data_len,
        signature, signature_len, &verify_result);
    if ((r == MAILPGP_NO_ERROR) &&
        (mailpgp_result_status(verify_result) != MAILPGP_VERIFY_VALID)) {
      r = mailpgp_rnp_verify_detached_crlf_replace_if_valid(pgp,
          signed_data, signed_data_len, signature, signature_len,
          &verify_result);
      if (r != MAILPGP_NO_ERROR)
        goto free;
    }
    if ((r == MAILPGP_NO_ERROR) &&
        (mailpgp_result_status(verify_result) != MAILPGP_VERIFY_VALID)) {
      char * fallback_data;
      size_t fallback_data_len;

      fallback_data = NULL;
      fallback_data_len = 0;
      if (write_mime_to_mem(signed_part, &fallback_data,
          &fallback_data_len) == MAILPGP_NO_ERROR) {
        r = mailpgp_rnp_verify_detached_replace_if_valid(pgp,
            fallback_data, fallback_data_len, signature, signature_len,
            &verify_result);
        if (r == MAILPGP_NO_ERROR &&
            mailpgp_result_status(verify_result) != MAILPGP_VERIFY_VALID)
          r = mailpgp_rnp_verify_detached_crlf_replace_if_valid(pgp,
              fallback_data, fallback_data_len, signature, signature_len,
              &verify_result);
        free(fallback_data);
        if (r != MAILPGP_NO_ERROR)
          goto free;
      }
    }
    if (r == MAILPGP_NO_ERROR)
      * result = verify_result;
 free:
    if (r != MAILPGP_NO_ERROR)
      mailpgp_result_free(verify_result);
    mmap_string_unref(signature);
    free(serialized_signed_data);

    return r;
  }
#else
  (void) pgp;
  (void) mime;
  (void) result;
  return MAILPGP_ERROR_NOT_IMPLEMENTED;
#endif
}

int mailpgp_message_decrypt(struct mailpgp * pgp, const char * message,
    size_t length, struct mailmime ** result)
{
  struct mailmime * mime;
  int r;

  mime = NULL;
  r = parse_message_mime(message, length, &mime);
  if (r != MAILPGP_NO_ERROR)
    return r;

  r = mailpgp_decrypt(pgp, message, length, mime, result);
  mailmime_free(mime);

  return r;
}

int mailpgp_decrypt(struct mailpgp * pgp, const char * message, size_t length,
    struct mailmime * mime, struct mailmime ** result)
{
  (void) message;
  (void) length;

#ifdef USE_PGP_RNP
  {
    struct mailmime * payload;
    char * encrypted;
    size_t encrypted_len;
    char * decrypted;
    size_t decrypted_len;
    int r;

    if ((pgp == NULL) || (mime == NULL) || (result == NULL))
      return MAILPGP_ERROR_INVAL;
    if (!mailpgp_is_encrypted(mime))
      return MAILPGP_ERROR_NOT_IMPLEMENTED;

    payload = mime;
    if (mime->mm_type == MAILMIME_MESSAGE)
      payload = mime->mm_data.mm_message.mm_msg_mime;

    if ((payload != NULL) && content_type_is_multipart(
        payload->mm_content_type, "encrypted"))
      payload = multipart_part(payload, 1);

    if (payload == NULL)
      return MAILPGP_ERROR_INVAL;

    encrypted = NULL;
    encrypted_len = 0;
    r = mime_body_decode(payload, &encrypted, &encrypted_len);
    if (r != MAILPGP_NO_ERROR)
      return r;

    decrypted = NULL;
    decrypted_len = 0;
    r = mailpgp_rnp_decrypt_data(pgp, encrypted, encrypted_len, &decrypted,
        &decrypted_len);
    mmap_string_unref(encrypted);
    if (r != MAILPGP_NO_ERROR)
      return r;

    r = parse_mime_from_mem(decrypted, decrypted_len, result);
    if (r == MAILPGP_NO_ERROR)
      return MAILPGP_NO_ERROR;

    r = make_single_body("text/plain", NULL, MAILMIME_MECHANISM_8BIT,
        decrypted, decrypted_len, result);
    free(decrypted);

    return r;
  }
#else
  (void) pgp;
  (void) mime;
  (void) result;
  return MAILPGP_ERROR_NOT_IMPLEMENTED;
#endif
}

int mailpgp_sign(struct mailpgp * pgp, struct mailmime * mime,
    const char * signer, struct mailmime ** result)
{
#ifdef USE_PGP_RNP
  char * data;
  size_t data_len;
  char * signature;
  size_t signature_len;
  int r;

  if ((pgp == NULL) || (mime == NULL) || (signer == NULL) ||
      (result == NULL))
    return MAILPGP_ERROR_INVAL;

  data = NULL;
  data_len = 0;
  r = write_mime_sub_to_mem(mime, &data, &data_len);
  if (r != MAILPGP_NO_ERROR)
    return r;

  signature = NULL;
  signature_len = 0;
  r = mailpgp_rnp_sign_detached(pgp, data, data_len, signer, &signature,
      &signature_len);
  free(data);
  if (r != MAILPGP_NO_ERROR)
    return r;

  r = make_multipart_signed(mime, signature, signature_len, result);
  free(signature);

  return r;
#else
  (void) pgp;
  (void) mime;
  (void) signer;
  (void) result;
  return MAILPGP_ERROR_NOT_IMPLEMENTED;
#endif
}

int mailpgp_encrypt(struct mailpgp * pgp, struct mailmime * mime,
    const char ** recipients, unsigned int recipient_count,
    struct mailmime ** result)
{
#ifdef USE_PGP_RNP
  char * data;
  size_t data_len;
  char * encrypted;
  size_t encrypted_len;
  int r;

  if ((pgp == NULL) || (mime == NULL) || (recipients == NULL) ||
      (recipient_count == 0) || (result == NULL))
    return MAILPGP_ERROR_INVAL;

  data = NULL;
  data_len = 0;
  r = write_mime_to_mem(mime, &data, &data_len);
  if (r != MAILPGP_NO_ERROR)
    return r;

  encrypted = NULL;
  encrypted_len = 0;
  r = mailpgp_rnp_encrypt_data(pgp, data, data_len, recipients,
      recipient_count, &encrypted, &encrypted_len);
  free(data);
  if (r != MAILPGP_NO_ERROR)
    return r;

  r = make_multipart_encrypted(encrypted, encrypted_len, result);
  free(encrypted);

  return r;
#else
  (void) pgp;
  (void) mime;
  (void) recipients;
  (void) recipient_count;
  (void) result;
  return MAILPGP_ERROR_NOT_IMPLEMENTED;
#endif
}

static int add_verified_signature_fingerprints(
    struct mailpgp_fingerprint_result * fingerprints, struct mailpgp * pgp,
    const char * message, size_t length, struct mailmime * mime)
{
  struct mailpgp_result * verify_result;
  unsigned int count;
  unsigned int i;
  int r;

  if ((fingerprints == NULL) || (pgp == NULL) || (mime == NULL) ||
      !mailpgp_is_signed(mime))
    return MAILPGP_NO_ERROR;

  verify_result = NULL;
  r = mailpgp_verify(pgp, message, length, mime, &verify_result);
  if (r == MAILPGP_ERROR_NOT_IMPLEMENTED)
    return MAILPGP_NO_ERROR;
  if (r != MAILPGP_NO_ERROR)
    return r;

  if (mailpgp_result_status(verify_result) != MAILPGP_VERIFY_VALID) {
    mailpgp_result_free(verify_result);
    return MAILPGP_NO_ERROR;
  }

  count = mailpgp_result_signer_count(verify_result);
  for (i = 0; i < count; i ++) {
    struct mailpgp_key * key;
    const char * fingerprint;

    key = NULL;
    r = mailpgp_result_get_signer(verify_result, i, &key);
    if (r != MAILPGP_NO_ERROR) {
      mailpgp_result_free(verify_result);
      return r;
    }

    fingerprint = mailpgp_key_fingerprint(key);
    if (fingerprint != NULL) {
      r = mailpgp_fingerprint_result_add(fingerprints, fingerprint, strlen(fingerprint),
          mailpgp_key_key_id(key), MAILPGP_FINGERPRINT_SOURCE_SIGNATURE, 1);
      if (r != MAILPGP_NO_ERROR) {
        mailpgp_key_free(key);
        mailpgp_result_free(verify_result);
        return r;
      }
    }
    mailpgp_key_free(key);
  }

  mailpgp_result_free(verify_result);
  return MAILPGP_NO_ERROR;
}

static int add_signature_packet_fingerprints(
    struct mailpgp_fingerprint_result * fingerprints, struct mailpgp * pgp,
    struct mailmime * mime)
{
  clistiter * cur;

  if ((fingerprints == NULL) || (mime == NULL))
    return MAILPGP_NO_ERROR;

  switch (mime->mm_type) {
  case MAILMIME_SINGLE:
    if (content_type_is_application(mime->mm_content_type,
        "pgp-signature")) {
      char * signature;
      size_t signature_len;
      int r;

      signature = NULL;
      signature_len = 0;
      r = mime_body_decode(mime, &signature, &signature_len);
      if (r != MAILPGP_NO_ERROR)
        return MAILPGP_NO_ERROR;
#ifdef USE_PGP_RNP
      r = mailpgp_rnp_import_signature_fingerprints(pgp, fingerprints,
          signature, signature_len);
      if (r != MAILPGP_NO_ERROR)
        r = MAILPGP_NO_ERROR;
#else
      r = MAILPGP_NO_ERROR;
#endif
      mmap_string_unref(signature);
      return r;
    }
    break;

  case MAILMIME_MULTIPLE:
    for (cur = clist_begin(mime->mm_data.mm_multipart.mm_mp_list);
        cur != NULL; cur = clist_next(cur)) {
      int r;

      r = add_signature_packet_fingerprints(fingerprints, pgp,
          clist_content(cur));
      (void) r;
    }
    break;

  case MAILMIME_MESSAGE:
    if (mime->mm_data.mm_message.mm_msg_mime != NULL)
      return add_signature_packet_fingerprints(fingerprints, pgp,
          mime->mm_data.mm_message.mm_msg_mime);
    break;
  }

  return MAILPGP_NO_ERROR;
}

static int add_encrypted_recipient_fingerprints(
    struct mailpgp_fingerprint_result * fingerprints, struct mailpgp * pgp,
    struct mailmime * mime)
{
  struct mailmime * payload;
  char * encrypted;
  size_t encrypted_len;
  int r;

  if ((fingerprints == NULL) || (mime == NULL) || !mailpgp_is_encrypted(mime))
    return MAILPGP_NO_ERROR;

  payload = mime;
  if (mime->mm_type == MAILMIME_MESSAGE)
    payload = mime->mm_data.mm_message.mm_msg_mime;
  if ((payload != NULL) && content_type_is_multipart(
      payload->mm_content_type, "encrypted"))
    payload = multipart_part(payload, 1);
  if (payload == NULL)
    return MAILPGP_NO_ERROR;

  encrypted = NULL;
  encrypted_len = 0;
  r = mime_body_decode(payload, &encrypted, &encrypted_len);
  if (r != MAILPGP_NO_ERROR)
    return MAILPGP_NO_ERROR;

#ifdef USE_PGP_RNP
  r = mailpgp_rnp_add_encrypted_recipient_fingerprints_from_dump(
      fingerprints, pgp, encrypted, encrypted_len);
  if ((r == MAILPGP_NO_ERROR) &&
      (mailpgp_fingerprint_count(fingerprints) == 0))
    r = mailpgp_rnp_add_encrypted_recipient_fingerprints(fingerprints, pgp,
        encrypted, encrypted_len);
  if (r != MAILPGP_NO_ERROR)
    r = MAILPGP_NO_ERROR;
#else
  (void) pgp;
  r = MAILPGP_NO_ERROR;
#endif

  mmap_string_unref(encrypted);
  return r;
}

int mailpgp_message_extract_fingerprints(struct mailpgp * pgp,
    const char * message, size_t length,
    struct mailpgp_fingerprint_result ** result)
{
  struct mailmime * mime;
  int r;

  mime = NULL;
  r = parse_message_mime(message, length, &mime);
  if (r != MAILPGP_NO_ERROR)
    return r;

  r = mailpgp_extract_fingerprints(pgp, message, length, mime, result);
  mailmime_free(mime);

  return r;
}

int mailpgp_extract_fingerprints(struct mailpgp * pgp, const char * message,
    size_t length, struct mailmime * mime,
    struct mailpgp_fingerprint_result ** result)
{
  struct mailpgp_fingerprint_result * fingerprints;
  int r;

  (void) pgp;
  (void) mime;

  if ((result == NULL) || ((message == NULL) && (mime == NULL)))
    return MAILPGP_ERROR_INVAL;

  fingerprints = fingerprint_result_new();
  if (fingerprints == NULL)
    return MAILPGP_ERROR_MEMORY;

  if (mime != NULL) {
    struct mailpgp_key_list * keys;

    keys = key_list_new();
    if (keys == NULL) {
      mailpgp_fingerprint_result_free(fingerprints);
      return MAILPGP_ERROR_MEMORY;
    }

    r = scan_public_keys_mime(keys, mime);
    if (r == MAILPGP_NO_ERROR)
      r = add_public_key_fingerprints(fingerprints, keys);
    mailpgp_key_list_free(keys);
    if (r != MAILPGP_NO_ERROR) {
      mailpgp_fingerprint_result_free(fingerprints);
      return r;
    }

    r = add_verified_signature_fingerprints(fingerprints, pgp, message,
        length, mime);
    (void) r;

    r = add_signature_packet_fingerprints(fingerprints, pgp, mime);
    (void) r;

    r = add_encrypted_recipient_fingerprints(fingerprints, pgp, mime);
    (void) r;
  }

  if (message != NULL) {
    r = scan_message_header_hints(fingerprints, message, length);
    if (r != MAILPGP_NO_ERROR) {
      mailpgp_fingerprint_result_free(fingerprints);
      return r;
    }
  }

  * result = fingerprints;

  return MAILPGP_NO_ERROR;
}

int mailpgp_message_extract_public_keys(struct mailpgp * pgp,
    const char * message, size_t length, struct mailpgp_key_list ** result)
{
  struct mailmime * mime;
  int r;

  mime = NULL;
  r = parse_message_mime(message, length, &mime);
  if (r != MAILPGP_NO_ERROR)
    return r;

  r = mailpgp_extract_public_keys(pgp, message, length, mime, result);
  mailmime_free(mime);

  return r;
}

int mailpgp_extract_public_keys(struct mailpgp * pgp, const char * message,
    size_t length, struct mailmime * mime, struct mailpgp_key_list ** result)
{
  int r;

  (void) pgp;
  (void) message;
  (void) length;

  if (result == NULL)
    return MAILPGP_ERROR_INVAL;

  * result = key_list_new();
  if (* result == NULL)
    return MAILPGP_ERROR_MEMORY;

  if (mime != NULL) {
    r = scan_public_keys_mime(* result, mime);
    if (r != MAILPGP_NO_ERROR) {
      mailpgp_key_list_free(* result);
      * result = NULL;
      return r;
    }
  }

  return MAILPGP_NO_ERROR;
}

int mailpgp_result_status(struct mailpgp_result * result)
{
  if (result == NULL)
    return MAILPGP_VERIFY_ERROR;
  return result->status;
}

const char * mailpgp_result_error(struct mailpgp_result * result)
{
  if (result == NULL)
    return NULL;
  return result->error;
}

unsigned int mailpgp_result_signer_count(struct mailpgp_result * result)
{
  if ((result == NULL) || (result->signers == NULL))
    return 0;
  return (unsigned int) clist_count(result->signers);
}

int mailpgp_result_signed_by_address(struct mailpgp_result * result,
    const char * email)
{
  clistiter * cur;

  if ((result == NULL) || (email == NULL) || (result->signers == NULL))
    return 0;

  for (cur = clist_begin(result->signers); cur != NULL;
      cur = clist_next(cur)) {
    struct mailpgp_key * key;

    key = clist_content(cur);
    if ((key != NULL) && str_case_equal(key->email, email))
      return 1;
  }

  return 0;
}

int mailpgp_result_get_signer(struct mailpgp_result * result,
    unsigned int index, struct mailpgp_key ** key)
{
  struct mailpgp_key * signer;

  if ((result == NULL) || (key == NULL) || (result->signers == NULL))
    return MAILPGP_ERROR_INVAL;

  signer = clist_nth_data(result->signers, index);
  if (signer == NULL)
    return MAILPGP_ERROR_INVAL;

  * key = key_clone(signer);
  if (* key == NULL)
    return MAILPGP_ERROR_MEMORY;

  return MAILPGP_NO_ERROR;
}

void mailpgp_result_free(struct mailpgp_result * result)
{
  if (result == NULL)
    return;

  free(result->error);
  if (result->signers != NULL) {
    clist_foreach(result->signers, key_free_func, NULL);
    clist_free(result->signers);
  }
  free(result);
}

void mailpgp_mime_free(struct mailmime * mime)
{
  if (mime == NULL)
    return;

  mime_unregister_owned_buffers_recursive(mime);
  mailmime_free(mime);
}

unsigned int mailpgp_fingerprint_count(
    struct mailpgp_fingerprint_result * result)
{
  if ((result == NULL) || (result->fingerprints == NULL))
    return 0;
  return (unsigned int) clist_count(result->fingerprints);
}

const char * mailpgp_fingerprint_value(
    struct mailpgp_fingerprint_result * result, unsigned int index)
{
  struct mailpgp_fingerprint * fingerprint;

  if ((result == NULL) || (result->fingerprints == NULL))
    return NULL;

  fingerprint = clist_nth_data(result->fingerprints, index);
  if (fingerprint == NULL)
    return NULL;

  return fingerprint->value;
}

int mailpgp_fingerprint_source(struct mailpgp_fingerprint_result * result,
    unsigned int index)
{
  struct mailpgp_fingerprint * fingerprint;

  if ((result == NULL) || (result->fingerprints == NULL))
    return MAILPGP_FINGERPRINT_SOURCE_HEADER_HINT;

  fingerprint = clist_nth_data(result->fingerprints, index);
  if (fingerprint == NULL)
    return MAILPGP_FINGERPRINT_SOURCE_HEADER_HINT;

  return fingerprint->source;
}

int mailpgp_fingerprint_is_verified(
    struct mailpgp_fingerprint_result * result, unsigned int index)
{
  struct mailpgp_fingerprint * fingerprint;

  if ((result == NULL) || (result->fingerprints == NULL))
    return 0;

  fingerprint = clist_nth_data(result->fingerprints, index);
  if (fingerprint == NULL)
    return 0;

  return fingerprint->verified;
}

const char * mailpgp_fingerprint_key_id(
    struct mailpgp_fingerprint_result * result, unsigned int index)
{
  struct mailpgp_fingerprint * fingerprint;

  if ((result == NULL) || (result->fingerprints == NULL))
    return NULL;

  fingerprint = clist_nth_data(result->fingerprints, index);
  if (fingerprint == NULL)
    return NULL;

  return fingerprint->key_id;
}

void mailpgp_fingerprint_result_free(
    struct mailpgp_fingerprint_result * result)
{
  if (result == NULL)
    return;

  if (result->fingerprints != NULL) {
    clist_foreach(result->fingerprints, fingerprint_free_func, NULL);
    clist_free(result->fingerprints);
  }
  free(result);
}

unsigned int mailpgp_key_list_count(struct mailpgp_key_list * list)
{
  if ((list == NULL) || (list->keys == NULL))
    return 0;
  return (unsigned int) clist_count(list->keys);
}

int mailpgp_key_list_get(struct mailpgp_key_list * list, unsigned int index,
    struct mailpgp_key ** key)
{
  struct mailpgp_key * item;

  if ((list == NULL) || (key == NULL) || (list->keys == NULL))
    return MAILPGP_ERROR_INVAL;

  item = clist_nth_data(list->keys, index);
  if (item == NULL)
    return MAILPGP_ERROR_INVAL;

  * key = key_clone(item);
  if (* key == NULL)
    return MAILPGP_ERROR_MEMORY;

  return MAILPGP_NO_ERROR;
}

void mailpgp_key_list_free(struct mailpgp_key_list * list)
{
  if (list == NULL)
    return;

  if (list->keys != NULL) {
    clist_foreach(list->keys, key_free_func, NULL);
    clist_free(list->keys);
  }
  free(list);
}

const char * mailpgp_key_email(struct mailpgp_key * key)
{
  if (key == NULL)
    return NULL;
  return key->email;
}

const char * mailpgp_key_name(struct mailpgp_key * key)
{
  if (key == NULL)
    return NULL;
  return key->name;
}

const char * mailpgp_key_user_id(struct mailpgp_key * key)
{
  if (key == NULL)
    return NULL;
  return key->user_id;
}

const char * mailpgp_key_fingerprint(struct mailpgp_key * key)
{
  if (key == NULL)
    return NULL;
  return key->fingerprint;
}

const char * mailpgp_key_key_id(struct mailpgp_key * key)
{
  if (key == NULL)
    return NULL;
  return key->key_id;
}

const char * mailpgp_key_algorithm(struct mailpgp_key * key)
{
  if (key == NULL)
    return NULL;
  return key->algorithm;
}

int mailpgp_key_is_expired(struct mailpgp_key * key)
{
  if (key == NULL)
    return 0;
  return key->expired;
}

int mailpgp_key_is_revoked(struct mailpgp_key * key)
{
  if (key == NULL)
    return 0;
  return key->revoked;
}

int mailpgp_key_export_armored(struct mailpgp_key * key, char ** armored,
    size_t * armored_len)
{
  char * copy;

  if ((key == NULL) || (armored == NULL) || (armored_len == NULL))
    return MAILPGP_ERROR_INVAL;

  if (key->armored == NULL)
    return MAILPGP_ERROR_NOT_IMPLEMENTED;

  copy = mem_dup_to_string(key->armored, key->armored_len);
  if (copy == NULL)
    return MAILPGP_ERROR_MEMORY;

  * armored = copy;
  * armored_len = key->armored_len;

  return MAILPGP_NO_ERROR;
}

void mailpgp_key_free(struct mailpgp_key * key)
{
  mailpgp_key_free_internal(key);
}
