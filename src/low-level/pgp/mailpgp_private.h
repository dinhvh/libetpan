/*
 * libEtPan! -- a mail stuff library
 */

#ifndef MAILPGP_PRIVATE_H
#define MAILPGP_PRIVATE_H

#include "mailpgp.h"

#include <libetpan/clist.h>

#ifdef USE_PGP_RNP
#include <rnp/rnp_err.h>
#include <rnp/rnp.h>
#define RNP_SUCCESS 0
#endif

struct mailpgp_mime_owner {
  struct mailmime * mime;
  clist * buffers;
};

struct mailpgp_key_data {
  char * data;
  size_t length;
  char * filename;
  char * passphrase;
};

struct mailpgp {
  clist * public_keys;
  clist * secret_keys;
  mailpgp_passphrase_callback passphrase_callback;
  void * passphrase_context;
#ifdef USE_PGP_RNP
  rnp_ffi_t ffi;
#endif
};

struct mailpgp_key {
  char * email;
  char * name;
  char * user_id;
  char * fingerprint;
  char * key_id;
  char * algorithm;
  int expired;
  int revoked;
  char * armored;
  size_t armored_len;
};

struct mailpgp_result {
  int status;
  char * error;
  clist * signers;
};

struct mailpgp_fingerprint {
  char * value;
  char * key_id;
  int source;
  int verified;
};

struct mailpgp_fingerprint_result {
  clist * fingerprints;
};

struct mailpgp_key_list {
  clist * keys;
};

void mailpgp_key_free_internal(struct mailpgp_key * key);
struct mailpgp_result * mailpgp_result_new(int status);
int mailpgp_fingerprint_result_add(struct mailpgp_fingerprint_result * result,
    const char * text, size_t length, const char * key_id, int source,
    int verified);
int mailpgp_fingerprint_result_add_key_id(
    struct mailpgp_fingerprint_result * result, const char * key_id,
    int source);

#ifdef USE_PGP_RNP
int mailpgp_key_fill_from_rnp_handle(struct mailpgp_key * key,
    rnp_key_handle_t handle);
int mailpgp_rnp_enrich_key(struct mailpgp_key * key,
    const char * data, size_t length);
int mailpgp_rnp_import_signature_fingerprints(struct mailpgp * pgp,
    struct mailpgp_fingerprint_result * fingerprints,
    const char * signature, size_t signature_len);
int mailpgp_rnp_add_encrypted_recipient_fingerprints_from_dump(
    struct mailpgp_fingerprint_result * fingerprints, struct mailpgp * pgp,
    const char * encrypted, size_t encrypted_len);
int mailpgp_rnp_add_encrypted_recipient_fingerprints(
    struct mailpgp_fingerprint_result * fingerprints, struct mailpgp * pgp,
    const char * encrypted, size_t encrypted_len);
int mailpgp_rnp_init(struct mailpgp * pgp);
void mailpgp_rnp_done(struct mailpgp * pgp);
int mailpgp_rnp_import_public_key_file(struct mailpgp * pgp,
    const char * filename);
int mailpgp_rnp_import_secret_key_file(struct mailpgp * pgp,
    const char * filename);
int mailpgp_rnp_import_public_key(struct mailpgp * pgp,
    const char * data, size_t length);
int mailpgp_rnp_import_secret_key(struct mailpgp * pgp,
    const char * data, size_t length);
int mailpgp_rnp_sign_detached(struct mailpgp * pgp, const char * data,
    size_t data_len, const char * signer, char ** signature,
    size_t * signature_len);
int mailpgp_rnp_encrypt_data(struct mailpgp * pgp, const char * data,
    size_t data_len, const char ** recipients, unsigned int recipient_count,
    char ** encrypted, size_t * encrypted_len);
int mailpgp_rnp_decrypt_data(struct mailpgp * pgp, const char * data,
    size_t data_len, char ** decrypted, size_t * decrypted_len);
int mailpgp_rnp_verify_detached(struct mailpgp * pgp, const char * data,
    size_t data_len, const char * signature, size_t signature_len,
    struct mailpgp_result ** result);
int mailpgp_rnp_verify_inline(struct mailpgp * pgp, const char * data,
    size_t data_len, struct mailpgp_result ** result);
int mailpgp_rnp_verify_detached_replace_if_valid(struct mailpgp * pgp,
    const char * data, size_t data_len, const char * signature,
    size_t signature_len, struct mailpgp_result ** result);
int mailpgp_rnp_verify_detached_crlf_replace_if_valid(struct mailpgp * pgp,
    const char * data, size_t data_len, const char * signature,
    size_t signature_len, struct mailpgp_result ** result);
#endif

#endif
