/*
 * libEtPan! -- a mail stuff library
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libetpan/libetpan.h>
#include <libetpan/mailpgp.h>
#include <libetpan/mailmime_write_mem.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef USE_PGP_RNP
#include <rnp/rnp_err.h>
#include <rnp/rnp.h>
#define RNP_SUCCESS 0
#endif

static int check(int condition, const char * message)
{
  if (!condition) {
    fprintf(stderr, "%s\n", message);
    return 0;
  }
  return 1;
}

static int read_file(const char * filename, char ** result, size_t * result_len)
{
  FILE * f;
  long size;
  char * data;

  f = fopen(filename, "rb");
  if (f == NULL)
    return -1;

  if (fseek(f, 0, SEEK_END) < 0)
    goto err;
  size = ftell(f);
  if (size < 0)
    goto err;
  if (fseek(f, 0, SEEK_SET) < 0)
    goto err;

  data = malloc((size_t) size + 1);
  if (data == NULL)
    goto err;
  if (fread(data, 1, (size_t) size, f) != (size_t) size) {
    free(data);
    goto err;
  }
  fclose(f);

  data[size] = '\0';
  * result = data;
  * result_len = (size_t) size;
  return 0;

 err:
  fclose(f);
  return -1;
}

static char * fixture_path(const char * filename)
{
  const char * srcdir;
  static const char fixture_dir[] = "/fixtures/openpgp/";
  char * path;
  size_t srcdir_len;
  size_t filename_len;

  srcdir = getenv("srcdir");
  if (srcdir == NULL)
    srcdir = ".";

  srcdir_len = strlen(srcdir);
  filename_len = strlen(filename);
  path = malloc(srcdir_len + sizeof(fixture_dir) - 1 + filename_len + 1);
  if (path == NULL)
    return NULL;

  memcpy(path, srcdir, srcdir_len);
  memcpy(path + srcdir_len, fixture_dir, sizeof(fixture_dir) - 1);
  memcpy(path + srcdir_len + sizeof(fixture_dir) - 1, filename,
      filename_len);
  path[srcdir_len + sizeof(fixture_dir) - 1 + filename_len] = '\0';
  return path;
}

static struct mailmime * parse_message(const char * message)
{
  struct mailmime * mime;
  size_t index;
  int r;

  mime = NULL;
  index = 0;
  r = mailmime_parse(message, strlen(message), &index, &mime);
  if (r != MAILIMF_NO_ERROR)
    return NULL;

  return mime;
}

static int check_signed_mime_detection(void)
{
  static const char message[] =
      "From: Alice <alice@example.com>\r\n"
      "Content-Type: multipart/signed; protocol=\"application/pgp-signature\"; boundary=\"b\"\r\n"
      "\r\n"
      "--b\r\n"
      "Content-Type: text/plain\r\n"
      "\r\n"
      "hello\r\n"
      "--b\r\n"
      "Content-Type: application/pgp-signature\r\n"
      "\r\n"
      "signature\r\n"
      "--b--\r\n";
  struct mailmime * mime;
  int ok;

  mime = parse_message(message);
  if (mime == NULL)
    return check(0, "could not parse PGP signed message");

  ok = check(mailpgp_is_pgp(mime), "signed message not detected as PGP") &&
      check(mailpgp_is_signed(mime), "signed message not detected as signed") &&
      check(!mailpgp_is_encrypted(mime),
          "signed message detected as encrypted") &&
      check(!mailpgp_is_key(mime), "signed message detected as key");

  mailmime_free(mime);
  return ok;
}

static int check_encrypted_mime_detection(void)
{
  static const char message[] =
      "From: Alice <alice@example.com>\r\n"
      "Content-Type: multipart/encrypted; protocol=\"application/pgp-encrypted\"; boundary=\"b\"\r\n"
      "\r\n"
      "--b\r\n"
      "Content-Type: application/pgp-encrypted\r\n"
      "\r\n"
      "Version: 1\r\n"
      "--b\r\n"
      "Content-Type: application/octet-stream\r\n"
      "\r\n"
      "encrypted\r\n"
      "--b--\r\n";
  struct mailmime * mime;
  int ok;

  mime = parse_message(message);
  if (mime == NULL)
    return check(0, "could not parse PGP encrypted message");

  ok = check(mailpgp_is_pgp(mime), "encrypted message not detected as PGP") &&
      check(!mailpgp_is_signed(mime),
          "encrypted message detected as signed") &&
      check(mailpgp_is_encrypted(mime),
          "encrypted message not detected as encrypted") &&
      check(!mailpgp_is_key(mime), "encrypted message detected as key");

  mailmime_free(mime);
  return ok;
}

static int check_inline_signed_detection(void)
{
  static const char message[] =
      "From: Alice <alice@example.com>\r\n"
      "Content-Type: text/plain\r\n"
      "\r\n"
      "-----BEGIN PGP SIGNED MESSAGE-----\r\n"
      "Hash: SHA256\r\n"
      "\r\n"
      "hello\r\n";
  struct mailmime * mime;
  int ok;

  mime = parse_message(message);
  if (mime == NULL)
    return check(0, "could not parse inline signed message");

  ok = check(mailpgp_is_pgp(mime),
          "inline signed message not detected as PGP") &&
      check(mailpgp_is_signed(mime),
          "inline signed message not detected as signed");

  mailmime_free(mime);
  return ok;
}

static int check_header_fingerprint_extraction(void)
{
  static const char message[] =
      "From: Alice <alice@example.com>\r\n"
      "X-PGP-Fingerprint: 0123 4567 89AB CDEF 0123 4567 89AB CDEF 0123 4567\r\n"
      "Content-Type: text/plain\r\n"
      "\r\n"
      "hello\r\n";
  struct mailpgp * pgp;
  struct mailpgp_fingerprint_result * result;
  int r;
  int ok;

  pgp = mailpgp_new();
  if (pgp == NULL)
    return check(0, "could not create PGP context");

  result = NULL;
  r = mailpgp_message_extract_fingerprints(pgp, message, strlen(message),
      &result);

  ok = check(r == MAILPGP_NO_ERROR, "fingerprint extraction failed") &&
      check(mailpgp_fingerprint_count(result) == 1,
          "unexpected fingerprint count") &&
      check(strcmp(mailpgp_fingerprint_value(result, 0),
          "0123456789ABCDEF0123456789ABCDEF01234567") == 0,
          "fingerprint was not normalized") &&
      check(mailpgp_fingerprint_source(result, 0) ==
          MAILPGP_FINGERPRINT_SOURCE_HEADER_HINT,
          "fingerprint source was not header hint") &&
      check(!mailpgp_fingerprint_is_verified(result, 0),
          "header hint fingerprint marked verified");

  mailpgp_fingerprint_result_free(result);
  mailpgp_free(pgp);

  return ok;
}

static int check_public_key_extraction(void)
{
  static const char key_block[] =
      "-----BEGIN PGP PUBLIC KEY BLOCK-----\r\n"
      "\r\n"
      "xsBNBGExampleBCAD\r\n"
      "-----END PGP PUBLIC KEY BLOCK-----\r\n";
  static const char message_prefix[] =
      "From: Alice <alice@example.com>\r\n"
      "Content-Type: application/pgp-keys\r\n"
      "\r\n";
  char * message;
  struct mailpgp * pgp;
  struct mailpgp_key_list * keys;
  struct mailpgp_key * key;
  char * armored;
  size_t armored_len;
  int r;
  int ok;

  message = malloc(strlen(message_prefix) + strlen(key_block) + 1);
  if (message == NULL)
    return check(0, "could not allocate public key message");
  strcpy(message, message_prefix);
  strcat(message, key_block);

  pgp = mailpgp_new();
  if (pgp == NULL) {
    free(message);
    return check(0, "could not create PGP context");
  }

  keys = NULL;
  key = NULL;
  armored = NULL;
  armored_len = 0;

  r = mailpgp_message_extract_public_keys(pgp, message, strlen(message),
      &keys);
  ok = check(r == MAILPGP_NO_ERROR, "public key extraction failed") &&
      check(mailpgp_key_list_count(keys) == 1, "unexpected public key count");

  if (ok) {
    r = mailpgp_key_list_get(keys, 0, &key);
    ok = check(r == MAILPGP_NO_ERROR, "could not get public key");
  }

  if (ok) {
    r = mailpgp_key_export_armored(key, &armored, &armored_len);
    ok = check(r == MAILPGP_NO_ERROR, "could not export armored key") &&
        check(armored_len == strlen(key_block),
            "unexpected armored key length") &&
        check(strcmp(armored, key_block) == 0, "unexpected armored key data");
  }

  free(armored);
  mailpgp_key_free(key);
  mailpgp_key_list_free(keys);
  mailpgp_free(pgp);
  free(message);

  return ok;
}

#ifdef USE_PGP_RNP
static int generate_public_key_for_user_id(const char * user_id,
    char ** armored, size_t * armored_len)
{
  rnp_ffi_t ffi;
  rnp_key_handle_t key;
  rnp_output_t output;
  uint8_t * buf;
  size_t buf_len;
  int ok;

  ffi = NULL;
  key = NULL;
  output = NULL;
  buf = NULL;
  buf_len = 0;
  ok = 0;

  if (rnp_ffi_create(&ffi, "GPG", "GPG") != RNP_SUCCESS)
    goto free;

  if (rnp_generate_key_25519(ffi, user_id, NULL, &key) != RNP_SUCCESS)
    goto free;

  if (rnp_output_to_memory(&output, 0) != RNP_SUCCESS)
    goto free;

  if (rnp_key_export(key, output,
      RNP_KEY_EXPORT_ARMORED | RNP_KEY_EXPORT_PUBLIC |
      RNP_KEY_EXPORT_SUBKEYS) != RNP_SUCCESS)
    goto free;

  if (rnp_output_memory_get_buf(output, &buf, &buf_len, false) !=
      RNP_SUCCESS)
    goto free;

  * armored = malloc(buf_len + 1);
  if (* armored == NULL)
    goto free;
  memcpy(* armored, buf, buf_len);
  (* armored)[buf_len] = '\0';
  * armored_len = buf_len;

  ok = 1;

 free:
  rnp_output_destroy(output);
  rnp_key_handle_destroy(key);
  rnp_ffi_destroy(ffi);

  return ok ? MAILPGP_NO_ERROR : MAILPGP_ERROR_CRYPTO;
}

static int generate_public_key(char ** armored, size_t * armored_len)
{
  return generate_public_key_for_user_id("Alice <alice@example.com>",
      armored, armored_len);
}

static int export_key_material(rnp_key_handle_t key, uint32_t flags,
    char ** armored, size_t * armored_len)
{
  rnp_output_t output;
  uint8_t * buf;
  size_t buf_len;
  int ok;

  output = NULL;
  buf = NULL;
  buf_len = 0;
  ok = 0;

  if (rnp_output_to_memory(&output, 0) != RNP_SUCCESS)
    goto free;
  if (rnp_key_export(key, output, flags) != RNP_SUCCESS)
    goto free;
  if (rnp_output_memory_get_buf(output, &buf, &buf_len, false) !=
      RNP_SUCCESS)
    goto free;

  * armored = malloc(buf_len + 1);
  if (* armored == NULL)
    goto free;
  memcpy(* armored, buf, buf_len);
  (* armored)[buf_len] = '\0';
  * armored_len = buf_len;
  ok = 1;

 free:
  rnp_output_destroy(output);
  return ok ? MAILPGP_NO_ERROR : MAILPGP_ERROR_CRYPTO;
}

static int generate_key_pair_for_user_id(const char * user_id,
    char ** public_key, size_t * public_key_len,
    char ** secret_key, size_t * secret_key_len)
{
  rnp_ffi_t ffi;
  rnp_key_handle_t key;
  int r;

  ffi = NULL;
  key = NULL;
  r = MAILPGP_ERROR_CRYPTO;

  if (rnp_ffi_create(&ffi, "GPG", "GPG") != RNP_SUCCESS)
    goto free;
  if (rnp_generate_key_25519(ffi, user_id, NULL, &key) != RNP_SUCCESS)
    goto free;

  r = export_key_material(key,
      RNP_KEY_EXPORT_ARMORED | RNP_KEY_EXPORT_PUBLIC |
      RNP_KEY_EXPORT_SUBKEYS, public_key, public_key_len);
  if (r != MAILPGP_NO_ERROR)
    goto free;

  r = export_key_material(key,
      RNP_KEY_EXPORT_ARMORED | RNP_KEY_EXPORT_SECRET |
      RNP_KEY_EXPORT_SUBKEYS, secret_key, secret_key_len);

 free:
  rnp_key_handle_destroy(key);
  rnp_ffi_destroy(ffi);
  return r;
}

static int generate_key_pair(char ** public_key, size_t * public_key_len,
    char ** secret_key, size_t * secret_key_len)
{
  return generate_key_pair_for_user_id("Alice <alice@example.com>",
      public_key, public_key_len, secret_key, secret_key_len);
}

static int rnp_import_test_key(rnp_ffi_t ffi, const char * key_data,
    size_t key_data_len, uint32_t flags)
{
  rnp_input_t input;
  rnp_result_t rr;

  input = NULL;
  rr = rnp_input_from_memory(&input, (const uint8_t *) key_data,
      key_data_len, false);
  if (rr != RNP_SUCCESS)
    return MAILPGP_ERROR_CRYPTO;

  rr = rnp_import_keys(ffi, input, flags | RNP_LOAD_SAVE_PERMISSIVE, NULL);
  rnp_input_destroy(input);

  return (rr == RNP_SUCCESS) ? MAILPGP_NO_ERROR : MAILPGP_ERROR_CRYPTO;
}

static int rnp_output_copy(rnp_output_t output, char ** result,
    size_t * result_len)
{
  uint8_t * buf;
  size_t buf_len;

  buf = NULL;
  buf_len = 0;
  if (rnp_output_memory_get_buf(output, &buf, &buf_len, false) !=
      RNP_SUCCESS)
    return MAILPGP_ERROR_CRYPTO;

  * result = malloc(buf_len + 1);
  if (* result == NULL)
    return MAILPGP_ERROR_MEMORY;
  memcpy(* result, buf, buf_len);
  (* result)[buf_len] = '\0';
  * result_len = buf_len;

  return MAILPGP_NO_ERROR;
}

static int sign_cleartext_message(const char * secret_key,
    size_t secret_key_len, const char * signer, const char * data,
    size_t data_len, char ** signed_data, size_t * signed_data_len)
{
  rnp_ffi_t ffi;
  rnp_key_handle_t key;
  rnp_input_t input;
  rnp_output_t output;
  rnp_op_sign_t op;
  int r;

  ffi = NULL;
  key = NULL;
  input = NULL;
  output = NULL;
  op = NULL;
  r = MAILPGP_ERROR_CRYPTO;

  if (rnp_ffi_create(&ffi, "GPG", "GPG") != RNP_SUCCESS)
    goto free;
  r = rnp_import_test_key(ffi, secret_key, secret_key_len,
      RNP_LOAD_SAVE_PUBLIC_KEYS | RNP_LOAD_SAVE_SECRET_KEYS);
  if (r != MAILPGP_NO_ERROR)
    goto free;

  if (rnp_locate_key(ffi, "userid", signer, &key) != RNP_SUCCESS)
    goto free;
  if (rnp_input_from_memory(&input, (const uint8_t *) data, data_len,
      false) != RNP_SUCCESS)
    goto free;
  if (rnp_output_to_memory(&output, 0) != RNP_SUCCESS)
    goto free;
  if (rnp_op_sign_cleartext_create(&op, ffi, input, output) != RNP_SUCCESS)
    goto free;
  if (rnp_op_sign_add_signature(op, key, NULL) != RNP_SUCCESS)
    goto free;
  rnp_op_sign_set_hash(op, "SHA256");
  if (rnp_op_sign_execute(op) != RNP_SUCCESS)
    goto free;

  r = rnp_output_copy(output, signed_data, signed_data_len);

 free:
  rnp_op_sign_destroy(op);
  rnp_output_destroy(output);
  rnp_input_destroy(input);
  rnp_key_handle_destroy(key);
  rnp_ffi_destroy(ffi);

  return r;
}

static int mime_to_string(struct mailmime * mime, char ** result,
    size_t * result_len)
{
  MMAPString * mmapstr;
  int col;
  int r;

  mmapstr = mmap_string_new("");
  if (mmapstr == NULL)
    return MAILPGP_ERROR_MEMORY;

  col = 0;
  r = mailmime_write_mem(mmapstr, &col, mime);
  if (r != MAILIMF_NO_ERROR) {
    mmap_string_free(mmapstr);
    return MAILPGP_ERROR_PARSE;
  }

  * result = malloc(mmapstr->len + 1);
  if (* result == NULL) {
    mmap_string_free(mmapstr);
    return MAILPGP_ERROR_MEMORY;
  }
  memcpy(* result, mmapstr->str, mmapstr->len);
  (* result)[mmapstr->len] = '\0';
  * result_len = mmapstr->len;
  mmap_string_free(mmapstr);

  return MAILPGP_NO_ERROR;
}

static int mime_sub_to_string(struct mailmime * mime, char ** result,
    size_t * result_len)
{
  struct mailmime * saved_parent;
  int saved_parent_type;
  int r;

  saved_parent = mime->mm_parent;
  saved_parent_type = mime->mm_parent_type;
  if (mime->mm_parent == NULL) {
    mime->mm_parent = mime;
    mime->mm_parent_type = MAILMIME_MULTIPLE;
  }

  r = mime_to_string(mime, result, result_len);

  mime->mm_parent = saved_parent;
  mime->mm_parent_type = saved_parent_type;

  return r;
}

static char * tamper_signed_text(const char * text, size_t text_len)
{
  char * result;
  char * body;

  result = malloc(text_len + 1);
  if (result == NULL)
    return NULL;

  memcpy(result, text, text_len);
  result[text_len] = '\0';

  body = strstr(result, "hello pgp");
  if (body != NULL)
    body[0] = 'j';

  return result;
}

static int check_rnp_public_key_metadata(void)
{
  static const char message_prefix[] =
      "From: Alice <alice@example.com>\r\n"
      "Content-Type: application/pgp-keys\r\n"
      "\r\n";
  char * armored;
  size_t armored_len;
  char * message;
  struct mailpgp * pgp;
  struct mailpgp_key_list * keys;
  struct mailpgp_key * key;
  struct mailpgp_fingerprint_result * fingerprints;
  int r;
  int ok;

  armored = NULL;
  armored_len = 0;
  message = NULL;
  pgp = NULL;
  keys = NULL;
  key = NULL;
  fingerprints = NULL;

  r = generate_public_key(&armored, &armored_len);
  if (r != MAILPGP_NO_ERROR)
    return check(0, "could not generate RNP public key");

  message = malloc(strlen(message_prefix) + armored_len + 1);
  if (message == NULL) {
    free(armored);
    return check(0, "could not allocate RNP public key message");
  }
  strcpy(message, message_prefix);
  memcpy(message + strlen(message_prefix), armored, armored_len);
  message[strlen(message_prefix) + armored_len] = '\0';

  pgp = mailpgp_new();
  if (pgp == NULL) {
    free(message);
    free(armored);
    return check(0, "could not create PGP context");
  }

  r = mailpgp_message_extract_public_keys(pgp, message,
      strlen(message), &keys);
  ok = check(r == MAILPGP_NO_ERROR, "RNP public key extraction failed") &&
      check(mailpgp_key_list_count(keys) == 1,
          "unexpected RNP public key count");

  if (ok) {
    r = mailpgp_key_list_get(keys, 0, &key);
    ok = check(r == MAILPGP_NO_ERROR, "could not get RNP public key") &&
        check(mailpgp_key_fingerprint(key) != NULL,
            "RNP key fingerprint missing") &&
        check(strlen(mailpgp_key_fingerprint(key)) == 40,
            "unexpected RNP fingerprint length") &&
        check(mailpgp_key_key_id(key) != NULL, "RNP key id missing") &&
        check(strcmp(mailpgp_key_email(key), "alice@example.com") == 0,
            "RNP key email missing") &&
        check(mailpgp_key_user_id(key) != NULL, "RNP key user id missing") &&
        check(mailpgp_key_algorithm(key) != NULL,
            "RNP key algorithm missing");
  }

  r = mailpgp_message_extract_fingerprints(pgp, message,
      strlen(message), &fingerprints);
  ok = check(r == MAILPGP_NO_ERROR, "RNP fingerprint extraction failed") &&
      check(mailpgp_fingerprint_count(fingerprints) == 1,
          "unexpected RNP fingerprint count") &&
      check(mailpgp_fingerprint_source(fingerprints, 0) ==
          MAILPGP_FINGERPRINT_SOURCE_PUBLIC_KEY,
          "unexpected RNP fingerprint source") &&
      check(mailpgp_fingerprint_is_verified(fingerprints, 0),
          "RNP public key fingerprint should be verified");

  mailpgp_fingerprint_result_free(fingerprints);
  mailpgp_key_free(key);
  mailpgp_key_list_free(keys);
  mailpgp_free(pgp);
  free(message);
  free(armored);

  return ok;
}

static int check_rnp_crypto_roundtrip(void)
{
  static const char message[] =
      "Content-Type: text/plain\r\n"
      "\r\n"
      "hello pgp\r\n";
  const char * recipients[2];
  char * public_key;
  size_t public_key_len;
  char * secret_key;
  size_t secret_key_len;
  char * bob_public_key;
  size_t bob_public_key_len;
  char * bob_secret_key;
  size_t bob_secret_key_len;
  struct mailpgp * pgp;
  struct mailpgp * bob_pgp;
  struct mailmime * mime;
  struct mailmime * signed_mime;
  struct mailmime * encrypted_mime;
  struct mailmime * multi_encrypted_mime;
  struct mailmime * decrypted_mime;
  struct mailmime * bob_decrypted_mime;
  struct mailmime * wrong_decrypted_mime;
  struct mailpgp_result * verify_result;
  struct mailpgp_fingerprint_result * fingerprints;
  struct mailpgp * empty_pgp;
  char * signed_text;
  char * tampered_text;
  char * clear_signed_text;
  char * clear_signed_message;
  size_t signed_text_len;
  size_t clear_signed_text_len;
  char * decrypted_text;
  size_t decrypted_text_len;
  unsigned int fingerprint_index;
  int found_signature_fingerprint;
  int found_encrypted_recipient;
  int r;
  int ok;

  public_key = NULL;
  public_key_len = 0;
  secret_key = NULL;
  secret_key_len = 0;
  bob_public_key = NULL;
  bob_public_key_len = 0;
  bob_secret_key = NULL;
  bob_secret_key_len = 0;
  pgp = NULL;
  bob_pgp = NULL;
  signed_mime = NULL;
  encrypted_mime = NULL;
  multi_encrypted_mime = NULL;
  decrypted_mime = NULL;
  bob_decrypted_mime = NULL;
  wrong_decrypted_mime = NULL;
  verify_result = NULL;
  fingerprints = NULL;
  empty_pgp = NULL;
  signed_text = NULL;
  tampered_text = NULL;
  clear_signed_text = NULL;
  clear_signed_message = NULL;
  signed_text_len = 0;
  clear_signed_text_len = 0;
  decrypted_text = NULL;
  decrypted_text_len = 0;
  found_signature_fingerprint = 0;
  found_encrypted_recipient = 0;

  r = generate_key_pair(&public_key, &public_key_len, &secret_key,
      &secret_key_len);
  if (r != MAILPGP_NO_ERROR)
    return check(0, "could not generate RNP key pair");
  r = generate_key_pair_for_user_id("Bob <bob@example.com>",
      &bob_public_key, &bob_public_key_len, &bob_secret_key,
      &bob_secret_key_len);
  if (r != MAILPGP_NO_ERROR) {
    free(public_key);
    free(secret_key);
    return check(0, "could not generate second RNP key pair");
  }

  mime = parse_message(message);
  if (mime == NULL) {
    free(public_key);
    free(secret_key);
    free(bob_public_key);
    free(bob_secret_key);
    return check(0, "could not parse crypto test message");
  }

  pgp = mailpgp_new();
  if (pgp == NULL) {
    mailmime_free(mime);
    free(public_key);
    free(secret_key);
    free(bob_public_key);
    free(bob_secret_key);
    return check(0, "could not create PGP context");
  }
  bob_pgp = mailpgp_new();
  if (bob_pgp == NULL) {
    mailmime_free(mime);
    mailpgp_free(pgp);
    free(public_key);
    free(secret_key);
    free(bob_public_key);
    free(bob_secret_key);
    return check(0, "could not create second PGP context");
  }

  ok = 1;
  r = mailpgp_add_public_key(pgp, public_key, public_key_len);
  ok = check(r == MAILPGP_NO_ERROR, "could not import public key") && ok;
  r = mailpgp_add_public_key(pgp, bob_public_key, bob_public_key_len);
  ok = check(r == MAILPGP_NO_ERROR, "could not import Bob public key") && ok;
  r = mailpgp_add_secret_key(pgp, secret_key, secret_key_len, NULL);
  ok = check(r == MAILPGP_NO_ERROR, "could not import secret key") && ok;
  r = mailpgp_add_public_key(bob_pgp, bob_public_key, bob_public_key_len);
  ok = check(r == MAILPGP_NO_ERROR,
      "could not import Bob public key into Bob context") && ok;
  r = mailpgp_add_secret_key(bob_pgp, bob_secret_key, bob_secret_key_len,
      NULL);
  ok = check(r == MAILPGP_NO_ERROR, "could not import Bob secret key") && ok;

  r = mailpgp_sign(pgp, mime, "Alice <alice@example.com>", &signed_mime);
  ok = check(r == MAILPGP_NO_ERROR, "RNP sign failed") && ok;
  ok = check(mailpgp_is_signed(signed_mime),
      "signed output was not PGP/MIME signed") && ok;

  r = mailpgp_verify(pgp, NULL, 0, signed_mime, &verify_result);
  ok = check(r == MAILPGP_NO_ERROR, "RNP verify failed") && ok;
  ok = check(mailpgp_result_status(verify_result) == MAILPGP_VERIFY_VALID,
      "RNP signature was not valid") && ok;
  ok = check(mailpgp_result_signed_by_address(verify_result,
      "alice@example.com"), "RNP signer identity missing") && ok;

  r = mime_sub_to_string(signed_mime, &signed_text, &signed_text_len);
  ok = check(r == MAILPGP_NO_ERROR, "could not serialize signed MIME") && ok;
  mailpgp_result_free(verify_result);
  verify_result = NULL;
  r = mailpgp_message_verify(pgp, signed_text, signed_text_len,
      &verify_result);
  ok = check(r == MAILPGP_NO_ERROR, "RNP serialized verify failed") && ok;
  ok = check(mailpgp_result_status(verify_result) == MAILPGP_VERIFY_VALID,
      "serialized RNP signature was not valid") && ok;

  tampered_text = tamper_signed_text(signed_text, signed_text_len);
  ok = check(tampered_text != NULL, "could not tamper signed text") && ok;
  mailpgp_result_free(verify_result);
  verify_result = NULL;
  r = mailpgp_message_verify(pgp, tampered_text, signed_text_len,
      &verify_result);
  ok = check(r == MAILPGP_NO_ERROR, "RNP tampered verify failed") && ok;
  ok = check(mailpgp_result_status(verify_result) == MAILPGP_VERIFY_INVALID,
      "tampered RNP signature was not invalid") && ok;

  empty_pgp = mailpgp_new();
  ok = check(empty_pgp != NULL, "could not create empty PGP context") && ok;
  mailpgp_result_free(verify_result);
  verify_result = NULL;
  r = mailpgp_message_verify(empty_pgp, signed_text, signed_text_len,
      &verify_result);
  ok = check(r == MAILPGP_NO_ERROR, "RNP unknown signer verify failed") && ok;
  ok = check(mailpgp_result_status(verify_result) ==
      MAILPGP_VERIFY_NO_PUBLIC_KEY,
      "unknown RNP signer was not reported as no public key") && ok;

  r = sign_cleartext_message(secret_key, secret_key_len,
      "Alice <alice@example.com>", "hello cleartext\r\n",
      strlen("hello cleartext\r\n"), &clear_signed_text,
      &clear_signed_text_len);
  ok = check(r == MAILPGP_NO_ERROR, "RNP cleartext sign failed") && ok;
  if (r == MAILPGP_NO_ERROR) {
    static const char clear_prefix[] =
        "From: Alice <alice@example.com>\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n";
    size_t clear_prefix_len;

    clear_prefix_len = strlen(clear_prefix);
    clear_signed_message = malloc(clear_prefix_len + clear_signed_text_len +
        1);
    ok = check(clear_signed_message != NULL,
        "could not allocate clear-signed message") && ok;
    if (clear_signed_message != NULL) {
      memcpy(clear_signed_message, clear_prefix, clear_prefix_len);
      memcpy(clear_signed_message + clear_prefix_len, clear_signed_text,
          clear_signed_text_len);
      clear_signed_message[clear_prefix_len + clear_signed_text_len] = '\0';

      mailpgp_result_free(verify_result);
      verify_result = NULL;
      r = mailpgp_message_verify(pgp, clear_signed_message,
          clear_prefix_len + clear_signed_text_len, &verify_result);
      ok = check(r == MAILPGP_NO_ERROR,
          "RNP inline clear-signed verify failed") && ok;
      ok = check(mailpgp_result_status(verify_result) ==
          MAILPGP_VERIFY_VALID,
          "RNP inline clear-signed signature was not valid") && ok;
      ok = check(mailpgp_result_signed_by_address(verify_result,
          "alice@example.com"),
          "RNP inline clear-signed signer identity missing") && ok;
    }
  }

  r = mailpgp_message_extract_fingerprints(pgp, signed_text,
      signed_text_len, &fingerprints);
  ok = check(r == MAILPGP_NO_ERROR,
      "RNP signed message fingerprint extraction failed") && ok;
  if (r == MAILPGP_NO_ERROR) {
    for (fingerprint_index = 0;
        fingerprint_index < mailpgp_fingerprint_count(fingerprints);
        fingerprint_index ++) {
      if ((mailpgp_fingerprint_source(fingerprints, fingerprint_index) ==
          MAILPGP_FINGERPRINT_SOURCE_SIGNATURE) &&
          mailpgp_fingerprint_is_verified(fingerprints, fingerprint_index)) {
        found_signature_fingerprint = 1;
        break;
      }
    }
  }
  ok = check(found_signature_fingerprint,
      "verified RNP signature fingerprint missing") && ok;
  mailpgp_fingerprint_result_free(fingerprints);
  fingerprints = NULL;
  found_signature_fingerprint = 0;

  r = mailpgp_extract_fingerprints(pgp, NULL, 0, signed_mime,
      &fingerprints);
  ok = check(r == MAILPGP_NO_ERROR,
      "RNP parsed signed MIME fingerprint extraction failed") && ok;
  if (r == MAILPGP_NO_ERROR) {
    for (fingerprint_index = 0;
        fingerprint_index < mailpgp_fingerprint_count(fingerprints);
        fingerprint_index ++) {
      if ((mailpgp_fingerprint_source(fingerprints, fingerprint_index) ==
          MAILPGP_FINGERPRINT_SOURCE_SIGNATURE) &&
          mailpgp_fingerprint_is_verified(fingerprints, fingerprint_index)) {
        found_signature_fingerprint = 1;
        break;
      }
    }
  }
  ok = check(found_signature_fingerprint,
      "parsed MIME verified RNP signature fingerprint missing") && ok;

  recipients[0] = "Alice <alice@example.com>";
  r = mailpgp_encrypt(pgp, mime, recipients, 1, &encrypted_mime);
  ok = check(r == MAILPGP_NO_ERROR, "RNP encrypt failed") && ok;
  ok = check(mailpgp_is_encrypted(encrypted_mime),
      "encrypted output was not PGP/MIME encrypted") && ok;

  mailpgp_fingerprint_result_free(fingerprints);
  fingerprints = NULL;
  r = mailpgp_extract_fingerprints(pgp, NULL, 0, encrypted_mime,
      &fingerprints);
  ok = check(r == MAILPGP_NO_ERROR,
      "RNP encrypted recipient extraction failed") && ok;
  if (r == MAILPGP_NO_ERROR) {
    for (fingerprint_index = 0;
        fingerprint_index < mailpgp_fingerprint_count(fingerprints);
        fingerprint_index ++) {
      if ((mailpgp_fingerprint_source(fingerprints, fingerprint_index) ==
          MAILPGP_FINGERPRINT_SOURCE_ENCRYPTED_TO) &&
          (mailpgp_fingerprint_key_id(fingerprints,
              fingerprint_index) != NULL)) {
        found_encrypted_recipient = 1;
        break;
      }
    }
  }
  ok = check(found_encrypted_recipient,
      "RNP encrypted recipient key id missing") && ok;

  mailpgp_fingerprint_result_free(fingerprints);
  fingerprints = NULL;
  found_encrypted_recipient = 0;
  r = mailpgp_extract_fingerprints(empty_pgp, NULL, 0, encrypted_mime,
      &fingerprints);
  ok = check(r == MAILPGP_NO_ERROR,
      "RNP encrypted recipient extraction without secret key failed") && ok;
  if (r == MAILPGP_NO_ERROR) {
    for (fingerprint_index = 0;
        fingerprint_index < mailpgp_fingerprint_count(fingerprints);
        fingerprint_index ++) {
      if ((mailpgp_fingerprint_source(fingerprints, fingerprint_index) ==
          MAILPGP_FINGERPRINT_SOURCE_ENCRYPTED_TO) &&
          (mailpgp_fingerprint_key_id(fingerprints,
              fingerprint_index) != NULL)) {
        found_encrypted_recipient = 1;
        break;
      }
    }
  }
  ok = check(found_encrypted_recipient,
      "RNP encrypted recipient key id missing without secret key") && ok;

  r = mailpgp_decrypt(empty_pgp, NULL, 0, encrypted_mime,
      &wrong_decrypted_mime);
  ok = check(r == MAILPGP_ERROR_DECRYPT,
      "RNP wrong-key decrypt should fail") && ok;
  ok = check(wrong_decrypted_mime == NULL,
      "RNP wrong-key decrypt returned MIME") && ok;

  r = mailpgp_decrypt(pgp, NULL, 0, encrypted_mime, &decrypted_mime);
  ok = check(r == MAILPGP_NO_ERROR, "RNP decrypt failed") && ok;

  if (ok) {
    r = mime_to_string(decrypted_mime, &decrypted_text,
        &decrypted_text_len);
    ok = check(r == MAILPGP_NO_ERROR, "could not serialize decrypted MIME") &&
        check(strstr(decrypted_text, "hello pgp") != NULL,
            "decrypted MIME did not contain original body");
  }

  recipients[0] = "Alice <alice@example.com>";
  recipients[1] = "Bob <bob@example.com>";
  r = mailpgp_encrypt(pgp, mime, recipients, 2, &multi_encrypted_mime);
  ok = check(r == MAILPGP_NO_ERROR, "RNP multi-recipient encrypt failed") &&
      ok;
  ok = check(mailpgp_is_encrypted(multi_encrypted_mime),
      "multi-recipient output was not PGP/MIME encrypted") && ok;

  r = mailpgp_decrypt(bob_pgp, NULL, 0, multi_encrypted_mime,
      &bob_decrypted_mime);
  ok = check(r == MAILPGP_NO_ERROR,
      "RNP multi-recipient decrypt with Bob failed") && ok;
  if (ok) {
    free(decrypted_text);
    decrypted_text = NULL;
    r = mime_to_string(bob_decrypted_mime, &decrypted_text,
        &decrypted_text_len);
    ok = check(r == MAILPGP_NO_ERROR,
        "could not serialize Bob decrypted MIME") &&
        check(strstr(decrypted_text, "hello pgp") != NULL,
            "Bob decrypted MIME did not contain original body");
  }

  free(decrypted_text);
  free(clear_signed_message);
  free(clear_signed_text);
  free(tampered_text);
  free(signed_text);
  mailpgp_fingerprint_result_free(fingerprints);
  mailpgp_result_free(verify_result);
  mailpgp_mime_free(decrypted_mime);
  mailpgp_mime_free(bob_decrypted_mime);
  mailpgp_mime_free(wrong_decrypted_mime);
  mailpgp_mime_free(encrypted_mime);
  mailpgp_mime_free(multi_encrypted_mime);
  mailpgp_mime_free(signed_mime);
  mailmime_free(mime);
  mailpgp_free(empty_pgp);
  mailpgp_free(bob_pgp);
  mailpgp_free(pgp);
  free(bob_secret_key);
  free(bob_public_key);
  free(secret_key);
  free(public_key);

  return ok;
}

static int check_static_hidden_recipient_fixture(void)
{
  char * secret_path;
  char * message_path;
  char * expected_body_path;
  char * message;
  size_t message_len;
  char * expected_body;
  size_t expected_body_len;
  struct mailpgp * pgp;
  struct mailpgp * empty_pgp;
  struct mailmime * decrypted_mime;
  struct mailpgp_fingerprint_result * fingerprints;
  struct mailpgp_fingerprint_result * empty_fingerprints;
  char * decrypted_text;
  size_t decrypted_text_len;
  unsigned int i;
  int found_hidden_key_id;
  int r;
  int ok;

  secret_path = fixture_path("hidden-recipient-secret.asc");
  message_path = fixture_path("hidden-recipient-message.eml");
  expected_body_path = fixture_path("hidden-recipient-body.txt");
  message = NULL;
  message_len = 0;
  expected_body = NULL;
  expected_body_len = 0;
  pgp = NULL;
  empty_pgp = NULL;
  decrypted_mime = NULL;
  fingerprints = NULL;
  empty_fingerprints = NULL;
  decrypted_text = NULL;
  decrypted_text_len = 0;
  found_hidden_key_id = 0;

  ok = check(secret_path != NULL && message_path != NULL &&
      expected_body_path != NULL, "could not allocate hidden fixture paths");
  if (!ok)
    goto free;

  ok = check(read_file(message_path, &message, &message_len) == 0,
      "could not read hidden recipient message fixture") && ok;
  ok = check(read_file(expected_body_path, &expected_body,
      &expected_body_len) == 0,
      "could not read hidden recipient body fixture") && ok;
  if (!ok)
    goto free;
  while ((expected_body_len > 0) &&
      ((expected_body[expected_body_len - 1] == '\r') ||
      (expected_body[expected_body_len - 1] == '\n'))) {
    expected_body[expected_body_len - 1] = '\0';
    expected_body_len --;
  }

  pgp = mailpgp_new();
  ok = check(pgp != NULL, "could not create hidden fixture PGP context") && ok;
  if (!ok)
    goto free;

  empty_pgp = mailpgp_new();
  ok = check(empty_pgp != NULL,
      "could not create empty hidden fixture PGP context") && ok;
  if (!ok)
    goto free;

  r = mailpgp_add_secret_key_file(pgp, secret_path, "");
  ok = check(r == MAILPGP_NO_ERROR,
      "could not import hidden recipient secret key fixture") && ok;

  r = mailpgp_message_decrypt(pgp, message, message_len, &decrypted_mime);
  ok = check(r == MAILPGP_NO_ERROR,
      "hidden recipient fixture decrypt failed") && ok;
  ok = check(decrypted_mime != NULL,
      "hidden recipient fixture decrypt returned no MIME") && ok;

  if (ok) {
    r = mime_to_string(decrypted_mime, &decrypted_text,
        &decrypted_text_len);
    ok = check(r == MAILPGP_NO_ERROR,
        "could not serialize hidden recipient decrypted MIME") &&
        check(strstr(decrypted_text, expected_body) != NULL,
            "hidden recipient decrypted MIME did not contain fixture body");
  }

  r = mailpgp_message_extract_fingerprints(pgp, message, message_len,
      &fingerprints);
  ok = check(r == MAILPGP_NO_ERROR,
      "hidden recipient fixture fingerprint extraction failed") && ok;
  if (r == MAILPGP_NO_ERROR) {
    for (i = 0; i < mailpgp_fingerprint_count(fingerprints); i ++) {
      const char * key_id;

      key_id = mailpgp_fingerprint_key_id(fingerprints, i);
      if ((mailpgp_fingerprint_source(fingerprints, i) ==
          MAILPGP_FINGERPRINT_SOURCE_ENCRYPTED_TO) &&
          (key_id != NULL) && (strcmp(key_id, "0000000000000000") == 0)) {
        found_hidden_key_id = 1;
        break;
      }
    }
  }
  ok = check(!found_hidden_key_id,
      "hidden recipient all-zero key id should not be reported") && ok;
  found_hidden_key_id = 0;

  r = mailpgp_message_extract_fingerprints(empty_pgp, message, message_len,
      &empty_fingerprints);
  ok = check(r == MAILPGP_NO_ERROR,
      "hidden recipient extraction without local keys failed") && ok;
  if (r == MAILPGP_NO_ERROR) {
    for (i = 0; i < mailpgp_fingerprint_count(empty_fingerprints); i ++) {
      const char * key_id;

      key_id = mailpgp_fingerprint_key_id(empty_fingerprints, i);
      if ((mailpgp_fingerprint_source(empty_fingerprints, i) ==
          MAILPGP_FINGERPRINT_SOURCE_ENCRYPTED_TO) &&
          (key_id != NULL) && (strcmp(key_id, "0000000000000000") == 0)) {
        found_hidden_key_id = 1;
        break;
      }
    }
  }
  ok = check(!found_hidden_key_id,
      "hidden recipient all-zero key id should not be reported without keys") &&
      ok;

 free:
  free(decrypted_text);
  mailpgp_fingerprint_result_free(empty_fingerprints);
  mailpgp_fingerprint_result_free(fingerprints);
  mailpgp_mime_free(decrypted_mime);
  mailpgp_free(empty_pgp);
  mailpgp_free(pgp);
  free(expected_body);
  free(message);
  free(expected_body_path);
  free(message_path);
  free(secret_path);
  return ok;
}
#endif

static int check_crypto_not_implemented(void)
{
  static const char message[] =
      "From: Alice <alice@example.com>\r\n"
      "Content-Type: text/plain\r\n"
      "\r\n"
      "hello\r\n";
  struct mailpgp * pgp;
  struct mailpgp_result * result;
  struct mailmime * output;
  int ok;

  pgp = mailpgp_new();
  if (pgp == NULL)
    return check(0, "could not create PGP context");

  result = NULL;
  output = NULL;

  ok = check(mailpgp_message_verify(pgp, message, strlen(message), &result) ==
          MAILPGP_ERROR_NOT_IMPLEMENTED,
          "verify should report not implemented") &&
      check(mailpgp_message_decrypt(pgp, message, strlen(message), &output) ==
          MAILPGP_ERROR_NOT_IMPLEMENTED,
          "decrypt should report not implemented");

  mailpgp_result_free(result);
  if (output != NULL)
    mailmime_free(output);
  mailpgp_free(pgp);

  return ok;
}

int main(void)
{
  int ok;

  ok = 1;
  ok = check_signed_mime_detection() && ok;
  ok = check_encrypted_mime_detection() && ok;
  ok = check_inline_signed_detection() && ok;
  ok = check_header_fingerprint_extraction() && ok;
  ok = check_public_key_extraction() && ok;
#ifdef USE_PGP_RNP
  ok = check_rnp_public_key_metadata() && ok;
  ok = check_rnp_crypto_roundtrip() && ok;
  ok = check_static_hidden_recipient_fixture() && ok;
#endif
  ok = check_crypto_not_implemented() && ok;

  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
