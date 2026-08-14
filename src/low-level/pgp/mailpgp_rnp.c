/*
 * libEtPan! -- a mail stuff library
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "mailpgp_private.h"

#ifdef USE_PGP_RNP

#include <stdbool.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static int rnp_to_mailpgp_error(rnp_result_t result)
{
  if (result == RNP_SUCCESS)
    return MAILPGP_NO_ERROR;

  return MAILPGP_ERROR_CRYPTO;
}

static bool rnp_pass_provider(rnp_ffi_t ffi, void * app_ctx,
    rnp_key_handle_t key, const char * pgp_context, char buf[],
    size_t buf_len)
{
  struct mailpgp * pgp;
  char * keyid;
  const char * identifier;
  const char * passphrase;

  (void) ffi;

  pgp = app_ctx;
  if ((pgp == NULL) || (pgp->passphrase_callback == NULL) ||
      (buf == NULL) || (buf_len == 0))
    return false;

  keyid = NULL;
  identifier = pgp_context;
  if ((key != NULL) && (rnp_key_get_keyid(key, &keyid) == RNP_SUCCESS) &&
      (keyid != NULL))
    identifier = keyid;

  passphrase = pgp->passphrase_callback(identifier, pgp->passphrase_context);
  if (keyid != NULL)
    rnp_buffer_destroy(keyid);
  if (passphrase == NULL)
    return false;

  if (strlen(passphrase) >= buf_len)
    return false;

  strcpy(buf, passphrase);
  return true;
}

static int import_key_memory_rnp(struct mailpgp * pgp, const char * data,
    size_t length, uint32_t flags, char ** import_results)
{
  rnp_input_t input;
  rnp_result_t rr;

  if ((pgp == NULL) || (pgp->ffi == NULL) ||
      ((data == NULL) && (length != 0)))
    return MAILPGP_ERROR_INVAL;

  input = NULL;
  rr = rnp_input_from_memory(&input, (const uint8_t *) data, length, false);
  if (rr != RNP_SUCCESS)
    return rnp_to_mailpgp_error(rr);

  rr = rnp_import_keys(pgp->ffi, input,
      flags | RNP_LOAD_SAVE_PERMISSIVE, import_results);
  rnp_input_destroy(input);
  if ((rr != RNP_SUCCESS) && (rr != RNP_ERROR_EOF))
    return rnp_to_mailpgp_error(rr);

  return MAILPGP_NO_ERROR;
}

static int import_key_file_rnp(struct mailpgp * pgp, const char * filename,
    uint32_t flags)
{
  rnp_input_t input;
  rnp_result_t rr;

  if ((pgp == NULL) || (pgp->ffi == NULL) || (filename == NULL))
    return MAILPGP_ERROR_INVAL;

  input = NULL;
  rr = rnp_input_from_path(&input, filename);
  if (rr != RNP_SUCCESS)
    return MAILPGP_ERROR_KEY;

  rr = rnp_import_keys(pgp->ffi, input,
      flags | RNP_LOAD_SAVE_PERMISSIVE, NULL);
  rnp_input_destroy(input);
  if ((rr != RNP_SUCCESS) && (rr != RNP_ERROR_EOF))
    return rnp_to_mailpgp_error(rr);

  return MAILPGP_NO_ERROR;
}

int mailpgp_rnp_init(struct mailpgp * pgp)
{
  if (pgp == NULL)
    return MAILPGP_ERROR_INVAL;

  if (rnp_ffi_create(&pgp->ffi, "GPG", "GPG") != RNP_SUCCESS)
    return MAILPGP_ERROR_CRYPTO;
  rnp_ffi_set_pass_provider(pgp->ffi, rnp_pass_provider, pgp);

  return MAILPGP_NO_ERROR;
}

void mailpgp_rnp_done(struct mailpgp * pgp)
{
  if ((pgp != NULL) && (pgp->ffi != NULL)) {
    rnp_ffi_destroy(pgp->ffi);
    pgp->ffi = NULL;
  }
}

int mailpgp_rnp_import_public_key_file(struct mailpgp * pgp,
    const char * filename)
{
  return import_key_file_rnp(pgp, filename, RNP_LOAD_SAVE_PUBLIC_KEYS);
}

int mailpgp_rnp_import_secret_key_file(struct mailpgp * pgp,
    const char * filename)
{
  return import_key_file_rnp(pgp, filename,
      RNP_LOAD_SAVE_PUBLIC_KEYS | RNP_LOAD_SAVE_SECRET_KEYS);
}

int mailpgp_rnp_import_public_key(struct mailpgp * pgp,
    const char * data, size_t length)
{
  return import_key_memory_rnp(pgp, data, length,
      RNP_LOAD_SAVE_PUBLIC_KEYS, NULL);
}

int mailpgp_rnp_import_secret_key(struct mailpgp * pgp,
    const char * data, size_t length)
{
  return import_key_memory_rnp(pgp, data, length,
      RNP_LOAD_SAVE_PUBLIC_KEYS | RNP_LOAD_SAVE_SECRET_KEYS, NULL);
}

static int add_imported_signature_fingerprints(
    struct mailpgp_fingerprint_result * result, const char * import_results)
{
  const char * cur;

  if ((result == NULL) || (import_results == NULL))
    return MAILPGP_NO_ERROR;

  cur = import_results;
  while ((cur = strstr(cur, "\"signer fingerprint\"")) != NULL) {
    const char * colon;
    const char * begin;
    const char * end;
    int r;

    colon = strchr(cur, ':');
    if (colon == NULL)
      break;
    begin = strchr(colon, '"');
    if (begin == NULL)
      break;
    begin ++;
    end = strchr(begin, '"');
    if (end == NULL)
      break;

    r = mailpgp_fingerprint_result_add(result, begin,
        (size_t) (end - begin), NULL,
        MAILPGP_FINGERPRINT_SOURCE_SIGNATURE, 0);
    if (r != MAILPGP_NO_ERROR)
      return r;

    cur = end + 1;
  }

  return MAILPGP_NO_ERROR;
}

int mailpgp_rnp_enrich_key(struct mailpgp_key * key,
    const char * data, size_t length)
{
  const char * fingerprint;
  size_t fingerprint_len;
  rnp_ffi_t ffi;
  rnp_key_handle_t handle;
  struct mailpgp temp_pgp;
  char * import_results;
  int r;

  if ((key == NULL) || (data == NULL))
    return MAILPGP_ERROR_INVAL;

  ffi = NULL;
  import_results = NULL;
  r = MAILPGP_NO_ERROR;
  if (rnp_ffi_create(&ffi, "GPG", "GPG") != RNP_SUCCESS)
    return MAILPGP_NO_ERROR;

  memset(&temp_pgp, 0, sizeof(temp_pgp));
  temp_pgp.ffi = ffi;
  if (import_key_memory_rnp(&temp_pgp, data, length,
      RNP_LOAD_SAVE_PUBLIC_KEYS, &import_results) != MAILPGP_NO_ERROR)
    goto free_ffi;
  if (import_results == NULL)
    goto free_ffi;

  fingerprint = strstr(import_results, "\"fingerprint\"");
  if (fingerprint == NULL)
    goto free_ffi;
  fingerprint = strchr(fingerprint, ':');
  if (fingerprint == NULL)
    goto free_ffi;
  fingerprint = strchr(fingerprint, '"');
  if (fingerprint == NULL)
    goto free_ffi;
  fingerprint ++;
  fingerprint_len = strcspn(fingerprint, "\"");
  if (fingerprint_len == 0)
    goto free_ffi;

  key->fingerprint = malloc(fingerprint_len + 1);
  if (key->fingerprint == NULL) {
    r = MAILPGP_ERROR_MEMORY;
    goto free_ffi;
  }
  memcpy(key->fingerprint, fingerprint, fingerprint_len);
  key->fingerprint[fingerprint_len] = '\0';

  handle = NULL;
  if (rnp_locate_key(ffi, "fingerprint", key->fingerprint, &handle) ==
      RNP_SUCCESS) {
    if (handle != NULL) {
      r = mailpgp_key_fill_from_rnp_handle(key, handle);
      rnp_key_handle_destroy(handle);
    }
  }

 free_ffi:
  if (import_results != NULL)
    rnp_buffer_destroy(import_results);
  rnp_ffi_destroy(ffi);
  return r;
}

int mailpgp_rnp_import_signature_fingerprints(struct mailpgp * pgp,
    struct mailpgp_fingerprint_result * fingerprints,
    const char * signature, size_t signature_len)
{
  rnp_input_t input;
  char * import_results;
  rnp_result_t rr;
  int r;

  if ((pgp == NULL) || (pgp->ffi == NULL) || (fingerprints == NULL) ||
      (signature == NULL))
    return MAILPGP_ERROR_INVAL;

  input = NULL;
  import_results = NULL;
  rr = rnp_input_from_memory(&input, (const uint8_t *) signature,
      signature_len, false);
  if (rr != RNP_SUCCESS)
    return rnp_to_mailpgp_error(rr);

  rr = rnp_import_signatures(pgp->ffi, input, 0, &import_results);
  rnp_input_destroy(input);
  if (rr != RNP_SUCCESS)
    return rnp_to_mailpgp_error(rr);

  r = add_imported_signature_fingerprints(fingerprints, import_results);
  rnp_buffer_destroy(import_results);

  return r;
}

static int add_recipient_key_id(
    struct mailpgp_fingerprint_result * fingerprints,
    struct mailpgp * pgp, const char * key_id)
{
  rnp_key_handle_t key;
  char * fingerprint;
  int r;

  if ((fingerprints == NULL) || (key_id == NULL))
    return MAILPGP_ERROR_INVAL;

  key = NULL;
  fingerprint = NULL;
  if ((pgp != NULL) && (pgp->ffi != NULL) &&
      (rnp_locate_key(pgp->ffi, "keyid", key_id, &key) == RNP_SUCCESS) &&
      (key != NULL) &&
      (rnp_key_get_fprint(key, &fingerprint) == RNP_SUCCESS) &&
      (fingerprint != NULL)) {
    r = mailpgp_fingerprint_result_add(fingerprints, fingerprint,
        strlen(fingerprint), key_id, MAILPGP_FINGERPRINT_SOURCE_ENCRYPTED_TO,
        0);
    rnp_buffer_destroy(fingerprint);
    rnp_key_handle_destroy(key);
    return r;
  }

  if (fingerprint != NULL)
    rnp_buffer_destroy(fingerprint);
  rnp_key_handle_destroy(key);

  return mailpgp_fingerprint_result_add_key_id(fingerprints, key_id,
      MAILPGP_FINGERPRINT_SOURCE_ENCRYPTED_TO);
}

static int pgp_key_id_is_hidden(const char * key_id)
{
  size_t i;

  if (key_id == NULL)
    return 0;

  for (i = 0; key_id[i] != '\0'; i ++) {
    if (key_id[i] != '0')
      return 0;
  }

  return i > 0;
}

static int add_recipient_key_ids_from_json(
    struct mailpgp_fingerprint_result * fingerprints, struct mailpgp * pgp,
    const char * json)
{
  const char * cur;
  int r;

  if ((fingerprints == NULL) || (json == NULL))
    return MAILPGP_ERROR_INVAL;

  cur = json;
  while ((cur = strstr(cur, "\"keyid\"")) != NULL) {
    const char * colon;
    const char * begin;
    const char * end;

    colon = strchr(cur, ':');
    if (colon == NULL)
      break;
    begin = strchr(colon, '"');
    if (begin == NULL)
      break;
    begin ++;
    end = strchr(begin, '"');
    if (end == NULL)
      break;

    if ((end - begin == 16) || (end - begin == 40)) {
      char * key_id;

      key_id = malloc((size_t) (end - begin) + 1);
      if (key_id == NULL)
        return MAILPGP_ERROR_MEMORY;
      memcpy(key_id, begin, (size_t) (end - begin));
      key_id[end - begin] = '\0';
      if (pgp_key_id_is_hidden(key_id))
        r = MAILPGP_NO_ERROR;
      else
        r = add_recipient_key_id(fingerprints, pgp, key_id);
      free(key_id);
      if (r != MAILPGP_NO_ERROR)
        return r;
    }

    cur = end + 1;
  }

  return MAILPGP_NO_ERROR;
}

int mailpgp_rnp_add_encrypted_recipient_fingerprints_from_dump(
    struct mailpgp_fingerprint_result * fingerprints, struct mailpgp * pgp,
    const char * encrypted, size_t encrypted_len)
{
  rnp_input_t input;
  char * json;
  rnp_result_t rr;
  int r;

  if ((fingerprints == NULL) || (encrypted == NULL))
    return MAILPGP_ERROR_INVAL;

  input = NULL;
  json = NULL;
  rr = rnp_input_from_memory(&input, (const uint8_t *) encrypted,
      encrypted_len, false);
  if (rr != RNP_SUCCESS)
    return rnp_to_mailpgp_error(rr);

  rr = rnp_dump_packets_to_json(input, 0, &json);
  if ((rr == RNP_SUCCESS) && (json != NULL))
    r = add_recipient_key_ids_from_json(fingerprints, pgp, json);
  else
    r = MAILPGP_NO_ERROR;

  if (json != NULL)
    rnp_buffer_destroy(json);
  rnp_input_destroy(input);

  return r;
}

int mailpgp_rnp_add_encrypted_recipient_fingerprints(
    struct mailpgp_fingerprint_result * fingerprints, struct mailpgp * pgp,
    const char * encrypted, size_t encrypted_len)
{
  rnp_input_t input;
  rnp_output_t output;
  rnp_op_verify_t op;
  size_t count;
  size_t i;
  rnp_result_t rr;
  int r;
#ifdef RNP_VERIFY_ALLOW_HIDDEN_RECIPIENT
  uint32_t flags;
#endif

  if ((fingerprints == NULL) || (pgp == NULL) || (pgp->ffi == NULL) ||
      (encrypted == NULL))
    return MAILPGP_ERROR_INVAL;

  input = NULL;
  output = NULL;
  op = NULL;
  rr = rnp_input_from_memory(&input, (const uint8_t *) encrypted,
      encrypted_len, false);
  if (rr != RNP_SUCCESS)
    return rnp_to_mailpgp_error(rr);

  if (rnp_output_to_memory(&output, 0) != RNP_SUCCESS) {
    rnp_input_destroy(input);
    return MAILPGP_ERROR_CRYPTO;
  }
  if (rnp_op_verify_create(&op, pgp->ffi, input, output) != RNP_SUCCESS) {
    rnp_output_destroy(output);
    rnp_input_destroy(input);
    return MAILPGP_ERROR_CRYPTO;
  }

#ifdef RNP_VERIFY_ALLOW_HIDDEN_RECIPIENT
  flags = RNP_VERIFY_ALLOW_HIDDEN_RECIPIENT;
  (void) rnp_op_verify_set_flags(op, flags);
#endif

  rr = rnp_op_verify_execute(op);
  if (rr != RNP_SUCCESS) {
    r = MAILPGP_NO_ERROR;
    goto free;
  }

  count = 0;
  if (rnp_op_verify_get_recipient_count(op, &count) != RNP_SUCCESS) {
    r = MAILPGP_NO_ERROR;
    goto free;
  }

  r = MAILPGP_NO_ERROR;
  for (i = 0; i < count; i ++) {
    rnp_recipient_handle_t recipient;
    char * key_id;

    recipient = NULL;
    key_id = NULL;
    if ((rnp_op_verify_get_recipient_at(op, i, &recipient) == RNP_SUCCESS) &&
        (recipient != NULL) &&
        (rnp_recipient_get_keyid(recipient, &key_id) == RNP_SUCCESS) &&
        (key_id != NULL)) {
      if (pgp_key_id_is_hidden(key_id))
        r = MAILPGP_NO_ERROR;
      else
        r = add_recipient_key_id(fingerprints, pgp, key_id);
      rnp_buffer_destroy(key_id);
      if (r != MAILPGP_NO_ERROR)
        goto free;
    }
    else if (key_id != NULL) {
      rnp_buffer_destroy(key_id);
    }
  }

 free:
  rnp_op_verify_destroy(op);
  rnp_output_destroy(output);
  rnp_input_destroy(input);

  return r;
}

static int rnp_output_to_alloc(rnp_output_t output, char ** result,
    size_t * result_len)
{
  uint8_t * buf;
  size_t buf_len;

  if ((output == NULL) || (result == NULL) || (result_len == NULL))
    return MAILPGP_ERROR_INVAL;

  buf = NULL;
  buf_len = 0;
  if (rnp_output_memory_get_buf(output, &buf, &buf_len, false) !=
      RNP_SUCCESS)
    return MAILPGP_ERROR_CRYPTO;

  * result = malloc(buf_len + 1);
  if (* result == NULL)
    return MAILPGP_ERROR_MEMORY;
  if (buf_len != 0)
    memcpy(* result, buf, buf_len);
  (* result)[buf_len] = '\0';
  * result_len = buf_len;

  return MAILPGP_NO_ERROR;
}

static int rnp_locate_key_any(rnp_ffi_t ffi, const char * identifier,
    rnp_key_handle_t * key)
{
  static const char * types[] = { "userid", "fingerprint", "keyid" };
  unsigned int i;

  if ((ffi == NULL) || (identifier == NULL) || (key == NULL))
    return MAILPGP_ERROR_INVAL;

  * key = NULL;
  for (i = 0; i < sizeof(types) / sizeof(types[0]); i ++) {
    rnp_result_t rr;

    rr = rnp_locate_key(ffi, types[i], identifier, key);
    if (rr != RNP_SUCCESS)
      return rnp_to_mailpgp_error(rr);
    if (* key != NULL)
      return MAILPGP_NO_ERROR;
  }

  return MAILPGP_ERROR_KEY;
}

static int ascii_case_equal_len(const char * a, size_t a_len,
    const char * b, size_t b_len)
{
  size_t i;

  if ((a == NULL) || (b == NULL) || (a_len != b_len))
    return 0;

  for (i = 0; i < a_len; i ++) {
    if (tolower((unsigned char) a[i]) !=
        tolower((unsigned char) b[i]))
      return 0;
  }

  return 1;
}

static int uid_matches_identifier(const char * uid, const char * identifier)
{
  const char * begin;
  const char * end;

  if ((uid == NULL) || (identifier == NULL))
    return 0;

  if (ascii_case_equal_len(uid, strlen(uid), identifier, strlen(identifier)))
    return 1;

  begin = strchr(uid, '<');
  end = strchr(uid, '>');
  if ((begin == NULL) || (end == NULL) || (end <= begin + 1))
    return 0;

  begin ++;
  return ascii_case_equal_len(begin, (size_t) (end - begin), identifier,
      strlen(identifier));
}

static int rnp_key_matches_user_id(rnp_key_handle_t key,
    const char * identifier)
{
  size_t count;
  size_t i;

  if ((key == NULL) || (identifier == NULL))
    return 0;

  count = 0;
  if (rnp_key_get_uid_count(key, &count) != RNP_SUCCESS)
    return 0;

  for (i = 0; i < count; i ++) {
    char * uid;
    int matches;

    uid = NULL;
    matches = 0;
    if (rnp_key_get_uid_at(key, i, &uid) == RNP_SUCCESS) {
      matches = uid_matches_identifier(uid, identifier);
      if (uid != NULL)
        rnp_buffer_destroy(uid);
    }
    if (matches)
      return 1;
  }

  return 0;
}

static int rnp_locate_key_by_user_id_scan(rnp_ffi_t ffi,
    const char * identifier, rnp_key_handle_t * key)
{
  rnp_identifier_iterator_t it;
  const char * fingerprint;
  int r;

  if ((ffi == NULL) || (identifier == NULL) || (key == NULL))
    return MAILPGP_ERROR_INVAL;

  * key = NULL;
  it = NULL;
  if (rnp_identifier_iterator_create(ffi, &it, "fingerprint") != RNP_SUCCESS)
    return MAILPGP_ERROR_KEY;

  r = MAILPGP_ERROR_KEY;
  while (rnp_identifier_iterator_next(it, &fingerprint) == RNP_SUCCESS) {
    rnp_key_handle_t candidate;

    if (fingerprint == NULL)
      break;

    candidate = NULL;
    if (rnp_locate_key(ffi, "fingerprint", fingerprint, &candidate) !=
        RNP_SUCCESS)
      continue;
    if (candidate == NULL)
      continue;

    if (rnp_key_matches_user_id(candidate, identifier)) {
      * key = candidate;
      r = MAILPGP_NO_ERROR;
      break;
    }

    rnp_key_handle_destroy(candidate);
  }

  rnp_identifier_iterator_destroy(it);
  return r;
}

static int rnp_locate_key_for_usage(rnp_ffi_t ffi, const char * identifier,
    const char * usage, rnp_key_handle_t * key)
{
  rnp_key_handle_t located;
  rnp_key_handle_t default_key;
  rnp_result_t rr;
  bool allowed;
  int r;

  if ((ffi == NULL) || (identifier == NULL) || (usage == NULL) ||
      (key == NULL))
    return MAILPGP_ERROR_INVAL;

  * key = NULL;
  located = NULL;
  default_key = NULL;

  r = rnp_locate_key_any(ffi, identifier, &located);
  if (r != MAILPGP_NO_ERROR)
    r = rnp_locate_key_by_user_id_scan(ffi, identifier, &located);
  if (r != MAILPGP_NO_ERROR)
    return r;

  rr = rnp_key_get_default_key(located, usage, 0, &default_key);
  if ((rr == RNP_SUCCESS) && (default_key != NULL)) {
    rnp_key_handle_destroy(located);
    * key = default_key;
    return MAILPGP_NO_ERROR;
  }

  allowed = false;
  rr = rnp_key_allows_usage(located, usage, &allowed);
  if ((rr == RNP_SUCCESS) && allowed) {
    * key = located;
    return MAILPGP_NO_ERROR;
  }

  rnp_key_handle_destroy(located);
  return MAILPGP_ERROR_KEY;
}

int mailpgp_rnp_sign_detached(struct mailpgp * pgp, const char * data,
    size_t data_len, const char * signer, char ** signature,
    size_t * signature_len)
{
  rnp_input_t input;
  rnp_output_t output;
  rnp_op_sign_t op;
  rnp_key_handle_t key;
  rnp_result_t rr;
  int r;

  if ((pgp == NULL) || (pgp->ffi == NULL) || (data == NULL) ||
      (signer == NULL) || (signature == NULL) || (signature_len == NULL))
    return MAILPGP_ERROR_INVAL;

  input = NULL;
  output = NULL;
  op = NULL;
  key = NULL;
  r = MAILPGP_ERROR_CRYPTO;

  rr = rnp_input_from_memory(&input, (const uint8_t *) data, data_len, false);
  if (rr != RNP_SUCCESS)
    return rnp_to_mailpgp_error(rr);

  if (rnp_output_to_memory(&output, 0) != RNP_SUCCESS)
    goto free;

  r = rnp_locate_key_for_usage(pgp->ffi, signer, "sign", &key);
  if (r != MAILPGP_NO_ERROR)
    goto free;

  if (rnp_op_sign_detached_create(&op, pgp->ffi, input, output) !=
      RNP_SUCCESS)
    goto free;
  if (rnp_op_sign_add_signature(op, key, NULL) != RNP_SUCCESS) {
    r = MAILPGP_ERROR_SECRET_KEY;
    goto free;
  }
  rnp_op_sign_set_armor(op, true);
  rnp_op_sign_set_hash(op, "SHA256");

  if (rnp_op_sign_execute(op) != RNP_SUCCESS) {
    r = MAILPGP_ERROR_CRYPTO;
    goto free;
  }

  r = rnp_output_to_alloc(output, signature, signature_len);

 free:
  rnp_op_sign_destroy(op);
  rnp_key_handle_destroy(key);
  rnp_output_destroy(output);
  rnp_input_destroy(input);

  return r;
}

int mailpgp_rnp_encrypt_data(struct mailpgp * pgp, const char * data,
    size_t data_len, const char ** recipients, unsigned int recipient_count,
    char ** encrypted, size_t * encrypted_len)
{
  rnp_input_t input;
  rnp_output_t output;
  rnp_op_encrypt_t op;
  rnp_result_t rr;
  unsigned int i;
  int r;

  if ((pgp == NULL) || (pgp->ffi == NULL) || (data == NULL) ||
      (recipients == NULL) || (recipient_count == 0) ||
      (encrypted == NULL) || (encrypted_len == NULL))
    return MAILPGP_ERROR_INVAL;

  input = NULL;
  output = NULL;
  op = NULL;
  r = MAILPGP_ERROR_CRYPTO;

  rr = rnp_input_from_memory(&input, (const uint8_t *) data, data_len, false);
  if (rr != RNP_SUCCESS)
    return rnp_to_mailpgp_error(rr);

  if (rnp_output_to_memory(&output, 0) != RNP_SUCCESS)
    goto free;
  if (rnp_op_encrypt_create(&op, pgp->ffi, input, output) != RNP_SUCCESS)
    goto free;

  for (i = 0; i < recipient_count; i ++) {
    rnp_key_handle_t key;

    key = NULL;
    r = rnp_locate_key_for_usage(pgp->ffi, recipients[i], "encrypt", &key);
    if (r != MAILPGP_NO_ERROR)
      goto free;
    if (rnp_op_encrypt_add_recipient(op, key) != RNP_SUCCESS) {
      rnp_key_handle_destroy(key);
      r = MAILPGP_ERROR_KEY;
      goto free;
    }
    rnp_key_handle_destroy(key);
  }

  rnp_op_encrypt_set_armor(op, true);
  rnp_op_encrypt_set_cipher(op, "AES256");
  rnp_op_encrypt_set_aead(op, "None");
  rnp_op_encrypt_set_compression(op, "ZIP", 6);
  rnp_op_encrypt_set_file_name(op, "message.mime");

  if (rnp_op_encrypt_execute(op) != RNP_SUCCESS) {
    r = MAILPGP_ERROR_CRYPTO;
    goto free;
  }

  r = rnp_output_to_alloc(output, encrypted, encrypted_len);

 free:
  rnp_op_encrypt_destroy(op);
  rnp_output_destroy(output);
  rnp_input_destroy(input);

  return r;
}

int mailpgp_rnp_decrypt_data(struct mailpgp * pgp, const char * data,
    size_t data_len, char ** decrypted, size_t * decrypted_len)
{
  rnp_input_t input;
  rnp_output_t output;
  rnp_op_verify_t op;
  rnp_result_t rr;
  int r;
#if defined(RNP_VERIFY_ALLOW_HIDDEN_RECIPIENT) || \
    defined(RNP_VERIFY_IGNORE_SIGS_ON_DECRYPT)
  uint32_t flags;
#endif

  if ((pgp == NULL) || (pgp->ffi == NULL) || (data == NULL) ||
      (decrypted == NULL) || (decrypted_len == NULL))
    return MAILPGP_ERROR_INVAL;

  input = NULL;
  output = NULL;
  op = NULL;
  rr = rnp_input_from_memory(&input, (const uint8_t *) data, data_len, false);
  if (rr != RNP_SUCCESS)
    return rnp_to_mailpgp_error(rr);

  if (rnp_output_to_memory(&output, 0) != RNP_SUCCESS) {
    rnp_input_destroy(input);
    return MAILPGP_ERROR_CRYPTO;
  }

  rr = rnp_op_verify_create(&op, pgp->ffi, input, output);
  if (rr != RNP_SUCCESS) {
    rnp_output_destroy(output);
    rnp_input_destroy(input);
    return MAILPGP_ERROR_CRYPTO;
  }

#if defined(RNP_VERIFY_ALLOW_HIDDEN_RECIPIENT) || \
    defined(RNP_VERIFY_IGNORE_SIGS_ON_DECRYPT)
  flags = 0;
#ifdef RNP_VERIFY_ALLOW_HIDDEN_RECIPIENT
  flags |= RNP_VERIFY_ALLOW_HIDDEN_RECIPIENT;
#endif
#ifdef RNP_VERIFY_IGNORE_SIGS_ON_DECRYPT
  flags |= RNP_VERIFY_IGNORE_SIGS_ON_DECRYPT;
#endif
  (void) rnp_op_verify_set_flags(op, flags);
#endif

  rr = rnp_op_verify_execute(op);
  if (rr == RNP_SUCCESS)
    r = rnp_output_to_alloc(output, decrypted, decrypted_len);
  else
    r = MAILPGP_ERROR_DECRYPT;

  rnp_op_verify_destroy(op);
  rnp_output_destroy(output);
  rnp_input_destroy(input);

  return r;
}

static int rnp_status_to_verify_status(rnp_result_t status)
{
  switch (status) {
  case RNP_SUCCESS:
    return MAILPGP_VERIFY_VALID;
  case RNP_ERROR_SIGNATURE_INVALID:
  case RNP_ERROR_SIGNATURE_UNKNOWN:
    return MAILPGP_VERIFY_INVALID;
  case RNP_ERROR_KEY_NOT_FOUND:
  case RNP_ERROR_SIG_NO_SIGNER_ID:
  case RNP_ERROR_SIG_NO_SIGNER_KEY:
    return MAILPGP_VERIFY_NO_PUBLIC_KEY;
  case RNP_ERROR_SIGNATURE_EXPIRED:
    return MAILPGP_VERIFY_EXPIRED;
  default:
    return MAILPGP_VERIFY_ERROR;
  }
}

static int rnp_result_add_signatures(struct mailpgp_result * result,
    rnp_op_verify_t op)
{
  size_t count;
  size_t i;
  int worst_status;

  if ((result == NULL) || (op == NULL))
    return MAILPGP_ERROR_INVAL;

  count = 0;
  if (rnp_op_verify_get_signature_count(op, &count) != RNP_SUCCESS)
    return MAILPGP_ERROR_VERIFY;
  if (count == 0) {
    result->status = MAILPGP_VERIFY_INVALID;
    return MAILPGP_NO_ERROR;
  }

  worst_status = MAILPGP_VERIFY_VALID;
  for (i = 0; i < count; i ++) {
    rnp_op_verify_signature_t sig;
    rnp_result_t sig_status;
    int verify_status;

    sig = NULL;
    if (rnp_op_verify_get_signature_at(op, i, &sig) != RNP_SUCCESS)
      return MAILPGP_ERROR_VERIFY;

    sig_status = rnp_op_verify_signature_get_status(sig);
    verify_status = rnp_status_to_verify_status(sig_status);
    if ((worst_status == MAILPGP_VERIFY_VALID) ||
        (verify_status != MAILPGP_VERIFY_VALID))
      worst_status = verify_status;

    if (sig_status == RNP_SUCCESS) {
      rnp_key_handle_t key;

      key = NULL;
      if ((rnp_op_verify_signature_get_key(sig, &key) == RNP_SUCCESS) &&
          (key != NULL)) {
        struct mailpgp_key * signer;
        int r;

        signer = calloc(1, sizeof(* signer));
        if (signer == NULL) {
          rnp_key_handle_destroy(key);
          return MAILPGP_ERROR_MEMORY;
        }

        r = mailpgp_key_fill_from_rnp_handle(signer, key);
        rnp_key_handle_destroy(key);
        if (r != MAILPGP_NO_ERROR) {
          mailpgp_key_free_internal(signer);
          return r;
        }
        if (clist_append(result->signers, signer) < 0) {
          mailpgp_key_free_internal(signer);
          return MAILPGP_ERROR_MEMORY;
        }
      }
    }
  }

  result->status = worst_status;
  return MAILPGP_NO_ERROR;
}

int mailpgp_rnp_verify_detached(struct mailpgp * pgp, const char * data,
    size_t data_len, const char * signature, size_t signature_len,
    struct mailpgp_result ** result)
{
  rnp_input_t input;
  rnp_input_t sig_input;
  rnp_op_verify_t op;
  struct mailpgp_result * verify_result;
  rnp_result_t rr;
  int r;

  if ((pgp == NULL) || (pgp->ffi == NULL) || (data == NULL) ||
      (signature == NULL) || (result == NULL))
    return MAILPGP_ERROR_INVAL;

  input = NULL;
  sig_input = NULL;
  op = NULL;
  verify_result = mailpgp_result_new(MAILPGP_VERIFY_ERROR);
  if (verify_result == NULL)
    return MAILPGP_ERROR_MEMORY;

  rr = rnp_input_from_memory(&input, (const uint8_t *) data, data_len, false);
  if (rr != RNP_SUCCESS) {
    r = rnp_to_mailpgp_error(rr);
    goto free_result;
  }

  rr = rnp_input_from_memory(&sig_input, (const uint8_t *) signature,
      signature_len, false);
  if (rr != RNP_SUCCESS) {
    r = rnp_to_mailpgp_error(rr);
    goto free_input;
  }

  rr = rnp_op_verify_detached_create(&op, pgp->ffi, input, sig_input);
  if (rr != RNP_SUCCESS) {
    r = rnp_to_mailpgp_error(rr);
    goto free_sig_input;
  }

  rr = rnp_op_verify_execute(op);
  (void) rr;
  r = rnp_result_add_signatures(verify_result, op);
  if (r != MAILPGP_NO_ERROR)
    goto free_op;

  * result = verify_result;
  r = MAILPGP_NO_ERROR;

 free_op:
  rnp_op_verify_destroy(op);
 free_sig_input:
  rnp_input_destroy(sig_input);
 free_input:
  rnp_input_destroy(input);
  if (r == MAILPGP_NO_ERROR)
    return r;
 free_result:
  mailpgp_result_free(verify_result);
  return r;
}

int mailpgp_rnp_verify_inline(struct mailpgp * pgp, const char * data,
    size_t data_len, struct mailpgp_result ** result)
{
  rnp_input_t input;
  rnp_output_t output;
  rnp_op_verify_t op;
  struct mailpgp_result * verify_result;
  rnp_result_t rr;
  int r;

  if ((pgp == NULL) || (pgp->ffi == NULL) || (data == NULL) ||
      (result == NULL))
    return MAILPGP_ERROR_INVAL;

  input = NULL;
  output = NULL;
  op = NULL;
  verify_result = mailpgp_result_new(MAILPGP_VERIFY_ERROR);
  if (verify_result == NULL)
    return MAILPGP_ERROR_MEMORY;

  rr = rnp_input_from_memory(&input, (const uint8_t *) data, data_len, false);
  if (rr != RNP_SUCCESS) {
    r = rnp_to_mailpgp_error(rr);
    goto free_result;
  }

  rr = rnp_output_to_memory(&output, 0);
  if (rr != RNP_SUCCESS) {
    r = MAILPGP_ERROR_CRYPTO;
    goto free_input;
  }

  rr = rnp_op_verify_create(&op, pgp->ffi, input, output);
  if (rr != RNP_SUCCESS) {
    r = rnp_to_mailpgp_error(rr);
    goto free_output;
  }

  rr = rnp_op_verify_execute(op);
  (void) rr;
  r = rnp_result_add_signatures(verify_result, op);
  if (r != MAILPGP_NO_ERROR)
    goto free_op;

  * result = verify_result;
  r = MAILPGP_NO_ERROR;

 free_op:
  rnp_op_verify_destroy(op);
 free_output:
  rnp_output_destroy(output);
 free_input:
  rnp_input_destroy(input);
  if (r == MAILPGP_NO_ERROR)
    return r;
 free_result:
  mailpgp_result_free(verify_result);
  return r;
}

int mailpgp_rnp_verify_detached_replace_if_valid(struct mailpgp * pgp,
    const char * data, size_t data_len, const char * signature,
    size_t signature_len, struct mailpgp_result ** result)
{
  struct mailpgp_result * candidate;
  int r;

  if ((result == NULL) || (* result == NULL))
    return MAILPGP_ERROR_INVAL;

  candidate = NULL;
  r = mailpgp_rnp_verify_detached(pgp, data, data_len, signature,
      signature_len, &candidate);
  if (r != MAILPGP_NO_ERROR) {
    mailpgp_result_free(candidate);
    return MAILPGP_NO_ERROR;
  }

  if (mailpgp_result_status(candidate) == MAILPGP_VERIFY_VALID) {
    mailpgp_result_free(* result);
    * result = candidate;
  }
  else {
    mailpgp_result_free(candidate);
  }

  return MAILPGP_NO_ERROR;
}

int mailpgp_rnp_verify_detached_crlf_replace_if_valid(struct mailpgp * pgp,
    const char * data, size_t data_len, const char * signature,
    size_t signature_len, struct mailpgp_result ** result)
{
  char * crlf_data;
  int r;

  if ((data == NULL) || (result == NULL) || (* result == NULL))
    return MAILPGP_ERROR_INVAL;

  crlf_data = malloc(data_len + 3);
  if (crlf_data == NULL)
    return MAILPGP_ERROR_MEMORY;

  if (data_len != 0)
    memcpy(crlf_data, data, data_len);
  crlf_data[data_len] = '\r';
  crlf_data[data_len + 1] = '\n';
  crlf_data[data_len + 2] = '\0';

  r = mailpgp_rnp_verify_detached_replace_if_valid(pgp, crlf_data,
      data_len + 2, signature, signature_len, result);
  free(crlf_data);

  return r;
}

#endif
