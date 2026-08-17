/*
 * libEtPan! -- a mail stuff library
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libetpan/libetpan.h>
#include "../../src/driver/implementation/data-message/data_message_driver.h"

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

static int read_file(const char * filename, char ** result, size_t * result_len)
{
  FILE * f;
  long size;
  char * data;
  char * parent_filename;

  f = fopen(filename, "rb");
  if (f == NULL) {
    parent_filename = malloc(strlen(filename) + 4);
    if (parent_filename == NULL)
      return -1;
    strcpy(parent_filename, "../");
    strcat(parent_filename, filename);
    f = fopen(parent_filename, "rb");
    free(parent_filename);
    if (f == NULL)
      return -1;
  }

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

static struct mailmime * parse_message_body(const char * filename)
{
  char * data;
  size_t len;
  mailmessage * msg;
  struct mailmime * message_mime;
  struct mailmime * mime;
  int r;

  if (read_file(filename, &data, &len) < 0)
    return NULL;

  msg = data_message_init(data, len);
  if (msg == NULL) {
    free(data);
    return NULL;
  }

  r = mailmessage_get_bodystructure(msg, &message_mime);
  if ((r != MAIL_NO_ERROR) || (message_mime == NULL) ||
      (message_mime->mm_type != MAILMIME_MESSAGE)) {
    mailmessage_free(msg);
    free(data);
    return NULL;
  }

  mime = message_mime->mm_data.mm_message.mm_msg_mime;
  message_mime->mm_data.mm_message.mm_msg_mime = NULL;
  mailmessage_free(msg);

  return mime;
}

int smime_test_signed_fixture(const char * filename)
{
  struct mailmime * mime;
  int ok;

  mime = parse_message_body(filename);
  if (mime == NULL)
    return check(0, "could not parse signed S/MIME fixture");

  ok = check(mailsmime_is_smime(mime), "signed fixture not detected as S/MIME") &&
      check(mailsmime_is_signed(mime), "signed fixture not detected as signed") &&
      check(!mailsmime_is_encrypted(mime),
          "signed fixture detected as encrypted");

  mailmime_free(mime);
  return ok;
}

int smime_test_encrypted_fixture(const char * filename)
{
  struct mailmime * mime;
  int ok;

  mime = parse_message_body(filename);
  if (mime == NULL)
    return check(0, "could not parse encrypted S/MIME fixture");

  ok = check(mailsmime_is_smime(mime),
          "encrypted fixture not detected as S/MIME") &&
      check(!mailsmime_is_signed(mime),
          "encrypted fixture detected as signed") &&
      check(mailsmime_is_encrypted(mime),
          "encrypted fixture not detected as encrypted");

  mailmime_free(mime);
  return ok;
}

#if defined(USE_SMIME_OPENSSL) || defined(USE_SMIME_APPLE)
static const char * fixture_key(enum mailsmime_backend backend,
    const char * openssl_key, const char * apple_identity)
{
  if (backend == MAILSMIME_BACKEND_APPLE)
    return apple_identity;
  return openssl_key;
}

struct passphrase_test_context {
  const char * email;
  const char * passphrase;
  int called;
};

static const char * test_passphrase_callback(const char * email, void * context)
{
  struct passphrase_test_context * pass_context;

  pass_context = context;
  if ((pass_context == NULL) || (email == NULL) ||
      (strcmp(email, pass_context->email) != 0))
    return NULL;

  pass_context->called ++;
  return pass_context->passphrase;
}

static struct mailmime * make_text_part(void)
{
  static const char body[] = "S/MIME low-level round trip\n";
  struct mailmime * mime;
  struct mailmime_data * data;
  int r;

  r = mailmime_new_with_content("text/plain", NULL, &mime);
  if (r != MAILIMF_NO_ERROR)
    return NULL;

  data = mailmime_data_new_data(MAILMIME_MECHANISM_8BIT, 0, body,
      sizeof(body) - 1);
  if (data == NULL) {
    mailmime_free(mime);
    return NULL;
  }

  mime->mm_data.mm_single = data;
  mime->mm_body = data;
  return mime;
}

static int contains_bytes(const char * data, size_t data_len,
    const char * needle)
{
  size_t needle_len;
  size_t i;

  if ((data == NULL) || (needle == NULL))
    return 0;

  needle_len = strlen(needle);
  if (needle_len > data_len)
    return 0;

  for (i = 0; i <= data_len - needle_len; i ++) {
    if (memcmp(data + i, needle, needle_len) == 0)
      return 1;
  }

  return 0;
}

static int write_mime_to_mem(struct mailmime * mime, char ** result,
    size_t * result_len)
{
  MMAPString * mmapstr;
  int col;
  int r;

  if ((mime == NULL) || (result == NULL) || (result_len == NULL))
    return -1;

  mmapstr = mmap_string_new("");
  if (mmapstr == NULL)
    return -1;

  col = 0;
  r = mailmime_write_mem(mmapstr, &col, mime);
  if (r != MAILIMF_NO_ERROR) {
    mmap_string_free(mmapstr);
    return -1;
  }

  * result = malloc(mmapstr->len + 1);
  if (* result == NULL) {
    mmap_string_free(mmapstr);
    return -1;
  }
  memcpy(* result, mmapstr->str, mmapstr->len);
  (* result)[mmapstr->len] = '\0';
  * result_len = mmapstr->len;
  mmap_string_free(mmapstr);
  return 0;
}

static int check_text_part(struct mailmime * mime)
{
  char * data;
  size_t data_len;
  int ok;

  if (!check(mime != NULL, "decrypted MIME is NULL"))
    return 0;

  data = NULL;
  data_len = 0;
  if (!check(write_mime_to_mem(mime, &data, &data_len) == 0,
      "could not serialize decrypted MIME"))
    return 0;

  ok = check(contains_bytes(data, data_len, "S/MIME low-level round trip"),
      "decrypted MIME body does not match original");
  free(data);
  return ok;
}

static int check_signed_by_from(struct mailsmime_result * result,
    const char * matching_email,
    const char * wrong_email)
{
  char buffer[256];
  struct mailimf_fields * fields;
  size_t index;
  int r;
  int ok;

  snprintf(buffer, sizeof(buffer), "From: Alice <%s>\r\n", matching_email);
  index = 0;
  fields = NULL;
  r = mailimf_fields_parse(buffer, strlen(buffer), &index, &fields);
  ok = check(r == MAILIMF_NO_ERROR && fields != NULL,
      "could not parse matching From field");
  if (ok) {
    ok = check(mailsmime_result_signed_by_from(result, fields),
        "valid signature did not match From field") && ok;
  }
  mailimf_fields_free(fields);

  snprintf(buffer, sizeof(buffer), "From: Mallory <%s>\r\n", wrong_email);
  index = 0;
  fields = NULL;
  r = mailimf_fields_parse(buffer, strlen(buffer), &index, &fields);
  ok = check(r == MAILIMF_NO_ERROR && fields != NULL,
      "could not parse mismatching From field") && ok;
  if (r == MAILIMF_NO_ERROR && fields != NULL) {
    ok = check(!mailsmime_result_signed_by_from(result, fields),
        "valid signature matched wrong From field") && ok;
  }
  mailimf_fields_free(fields);

  return ok;
}

static struct mailmime * multipart_part(struct mailmime * mime, unsigned int index)
{
  clistiter * cur;
  unsigned int i;

  if ((mime == NULL) || (mime->mm_type != MAILMIME_MULTIPLE))
    return NULL;

  cur = clist_begin(mime->mm_data.mm_multipart.mm_mp_list);
  for (i = 0; (i < index) && (cur != NULL); i ++)
    cur = clist_next(cur);
  if (cur == NULL)
    return NULL;

  return clist_content(cur);
}

static int tamper_text_part(struct mailmime * mime)
{
  struct mailmime * first_part;
  struct mailmime_data * data;
  char * text;

  first_part = multipart_part(mime, 0);
  if ((first_part != NULL) && (first_part->mm_type == MAILMIME_MESSAGE))
    first_part = first_part->mm_data.mm_message.mm_msg_mime;
  if ((first_part == NULL) || (first_part->mm_type != MAILMIME_SINGLE))
    return 0;

  data = first_part->mm_data.mm_single;
  if ((data == NULL) || (data->dt_type != MAILMIME_DATA_TEXT) ||
      (data->dt_data.dt_text.dt_data == NULL) ||
      (data->dt_data.dt_text.dt_length == 0))
    return 0;

  text = (char *) data->dt_data.dt_text.dt_data;
  text[0] = text[0] == 'X' ? 'Y' : 'X';
  return 1;
}

int smime_test_passphrase_callback_with_backend(enum mailsmime_backend backend)
{
  static const char email[] = "carol@example.test";
  static const char passphrase[] = "libetpan-smime-passphrase";
  static const char cert_filename[] =
      "unittest/smime/fixtures/carol-cert.pem";
  const char * key_filename;
  struct passphrase_test_context pass_context;
  struct mailsmime * smime;
  struct mailmime * original;
  struct mailmime * signed_mime;
  struct mailsmime_result * verify_result;
  int r;
  int ok;

  key_filename = fixture_key(backend,
      "unittest/smime/fixtures/carol-key-encrypted.pem",
      "unittest/smime/fixtures/carol-identity-encrypted.p12");

  smime = mailsmime_new_with_backend(backend);
  original = make_text_part();
  signed_mime = NULL;
  verify_result = NULL;
  pass_context.email = email;
  pass_context.passphrase = passphrase;
  pass_context.called = 0;

  ok = check(smime != NULL, "could not create callback S/MIME context") &&
      check(original != NULL, "could not create callback text MIME part");
  if (!ok)
    goto cleanup;

  r = mailsmime_set_passphrase_callback(smime, test_passphrase_callback,
      &pass_context);
  ok = check(r == MAILSMIME_NO_ERROR, "could not set passphrase callback") &&
      ok;
  r = mailsmime_add_trusted_cert_file(smime, cert_filename);
  ok = check(r == MAILSMIME_NO_ERROR,
      "could not trust callback certificate") && ok;
  r = mailsmime_set_private_key_file(smime, email, cert_filename, key_filename,
      NULL);
  ok = check(r == MAILSMIME_NO_ERROR,
      "could not load encrypted private key through callback") && ok;
  ok = check(pass_context.called > 0,
      "passphrase callback was not invoked") && ok;
  if (!ok)
    goto cleanup;

  r = mailsmime_sign(smime, original, email, &signed_mime);
  ok = check(r == MAILSMIME_NO_ERROR,
      "could not sign with callback-loaded private key") && ok;
  r = mailsmime_verify(smime, signed_mime, &verify_result);
  ok = check(r == MAILSMIME_NO_ERROR,
      "could not verify callback-signed MIME") && ok;
  if ((r == MAILSMIME_NO_ERROR) && (verify_result != NULL)) {
    ok = check(mailsmime_result_status(verify_result) ==
        MAILSMIME_VERIFY_VALID,
        "callback-signed MIME did not verify as valid") && ok;
    ok = check(mailsmime_result_signed_by_address(verify_result, email),
        "callback-signed MIME signer email did not match") && ok;
  }

 cleanup:
  mailsmime_result_free(verify_result);
  mailsmime_mime_free(signed_mime);
  mailmime_free(original);
  mailsmime_free(smime);

  return ok;
}

int smime_test_crypto_round_trip_with_backend(enum mailsmime_backend backend)
{
  static const char email[] = "alice@example.test";
  static const char wrong_email[] = "bob@example.test";
  static const char apple_identity_passphrase[] =
      "libetpan-smime-fixture";
  static const char cert_filename[] =
      "unittest/smime/fixtures/alice-cert.pem";
  static const char wrong_cert_filename[] =
      "unittest/smime/fixtures/bob-cert.pem";
  const char * key_filename;
  const char * wrong_key_filename;
  struct mailsmime * smime;
  struct mailsmime * untrusted_smime;
  struct mailsmime * wrong_smime;
  struct mailmime * original;
  struct mailmime * signed_mime;
  struct mailmime * tampered_mime;
  struct mailmime * encrypted_mime;
  struct mailmime * multi_encrypted_mime;
  struct mailmime * wrong_decrypted_mime;
  struct mailmime * decrypted_mime;
  struct mailsmime_result * verify_result;
  struct mailsmime_result * untrusted_result;
  struct mailsmime_result * tampered_result;
  struct mailsmime_certificate * signer;
  const char * recipients[1];
  const char * multi_recipients[2];
  char * pem;
  size_t pem_len;
  int r;
  int ok;
  const char * key_passphrase;

  key_filename = fixture_key(backend,
      "unittest/smime/fixtures/alice-key.pem",
      "unittest/smime/fixtures/alice-identity.p12");
  wrong_key_filename = fixture_key(backend,
      "unittest/smime/fixtures/bob-key.pem",
      "unittest/smime/fixtures/bob-identity.p12");
  key_passphrase = backend == MAILSMIME_BACKEND_APPLE ?
      apple_identity_passphrase : NULL;

  smime = mailsmime_new_with_backend(backend);
  untrusted_smime = mailsmime_new_with_backend(backend);
  wrong_smime = mailsmime_new_with_backend(backend);
  original = make_text_part();
  signed_mime = NULL;
  tampered_mime = NULL;
  encrypted_mime = NULL;
  multi_encrypted_mime = NULL;
  wrong_decrypted_mime = NULL;
  decrypted_mime = NULL;
  verify_result = NULL;
  untrusted_result = NULL;
  tampered_result = NULL;
  signer = NULL;
  pem = NULL;

  ok = check(smime != NULL, "could not create S/MIME context") &&
      check(untrusted_smime != NULL,
          "could not create untrusted S/MIME context") &&
      check(wrong_smime != NULL, "could not create wrong-key S/MIME context") &&
      check(original != NULL, "could not create text MIME part");
  if (!ok)
    goto cleanup;

  r = mailsmime_add_trusted_cert_file(smime, cert_filename);
  ok = check(r == MAILSMIME_NO_ERROR, "could not trust test certificate") && ok;
  r = mailsmime_add_cert_file(smime, email, cert_filename);
  ok = check(r == MAILSMIME_NO_ERROR, "could not add recipient cert") && ok;
  r = mailsmime_set_private_key_file(smime, email, cert_filename, key_filename,
      key_passphrase);
  ok = check(r == MAILSMIME_NO_ERROR, "could not add private key") && ok;
  r = mailsmime_add_cert_file(smime, wrong_email, wrong_cert_filename);
  ok = check(r == MAILSMIME_NO_ERROR, "could not add second recipient cert") &&
      ok;
  r = mailsmime_set_private_key_file(wrong_smime, wrong_email,
      wrong_cert_filename, wrong_key_filename, key_passphrase);
  ok = check(r == MAILSMIME_NO_ERROR, "could not add wrong private key") && ok;
  if (!ok)
    goto cleanup;

  r = mailsmime_sign(smime, original, email, &signed_mime);
  ok = check(r == MAILSMIME_NO_ERROR, "mailsmime_sign failed") && ok;
  ok = check(mailsmime_is_signed(signed_mime),
      "generated signed MIME was not detected as signed") && ok;

  r = mailsmime_verify(smime, signed_mime, &verify_result);
  ok = check(r == MAILSMIME_NO_ERROR, "mailsmime_verify failed") && ok;
  if ((r == MAILSMIME_NO_ERROR) && (verify_result != NULL)) {
    if (mailsmime_result_status(verify_result) != MAILSMIME_VERIFY_VALID)
      fprintf(stderr, "valid verify status=%d error=%s\n",
          mailsmime_result_status(verify_result),
          mailsmime_result_error(verify_result) != NULL ?
          mailsmime_result_error(verify_result) : "");
    ok = check(mailsmime_result_status(verify_result) ==
        MAILSMIME_VERIFY_VALID, "signed MIME did not verify as valid") && ok;
    ok = check(mailsmime_result_signer_count(verify_result) == 1,
        "valid signature did not expose one signer") && ok;
    ok = check(mailsmime_result_signed_by_address(verify_result, email),
        "valid signature signer email did not match") && ok;
    ok = check(!mailsmime_result_signed_by_address(verify_result, wrong_email),
        "valid signature matched the wrong signer email") && ok;
    ok = check_signed_by_from(verify_result, email, wrong_email) && ok;

    r = mailsmime_result_get_signer(verify_result, 0, &signer);
    ok = check(r == MAILSMIME_NO_ERROR, "could not extract signer cert") && ok;
    if ((r == MAILSMIME_NO_ERROR) && (signer != NULL)) {
      ok = check(strcmp(mailsmime_certificate_email(signer), email) == 0,
          "signer cert email did not match") && ok;
      ok = check(mailsmime_certificate_not_before(signer) != NULL,
          "signer cert notBefore was not exposed") && ok;
      ok = check(mailsmime_certificate_not_after(signer) != NULL,
          "signer cert notAfter was not exposed") && ok;
      r = mailsmime_certificate_export_pem(signer, &pem, &pem_len);
      ok = check(r == MAILSMIME_NO_ERROR && pem != NULL && pem_len > 0,
          "could not export signer cert PEM") && ok;
    }
  }

  r = mailsmime_verify(untrusted_smime, signed_mime, &untrusted_result);
  ok = check(r == MAILSMIME_NO_ERROR,
      "untrusted mailsmime_verify failed") && ok;
  if ((r == MAILSMIME_NO_ERROR) && (untrusted_result != NULL)) {
    ok = check(mailsmime_result_status(untrusted_result) ==
        MAILSMIME_VERIFY_UNTRUSTED,
        "untrusted signature did not report untrusted status") && ok;
  }

  r = mailsmime_sign(smime, original, email, &tampered_mime);
  ok = check(r == MAILSMIME_NO_ERROR, "could not create tamper test MIME") &&
      ok;
  ok = check(tamper_text_part(tampered_mime),
      "could not tamper signed MIME body") && ok;
  r = mailsmime_verify(smime, tampered_mime, &tampered_result);
  ok = check(r == MAILSMIME_NO_ERROR,
      "tampered mailsmime_verify returned transport error") && ok;
  if ((r == MAILSMIME_NO_ERROR) && (tampered_result != NULL)) {
    ok = check(mailsmime_result_status(tampered_result) ==
        MAILSMIME_VERIFY_INVALID,
        "tampered signature did not report invalid status") && ok;
  }

  recipients[0] = email;
  r = mailsmime_encrypt(smime, original, recipients, 1, &encrypted_mime);
  ok = check(r == MAILSMIME_NO_ERROR, "mailsmime_encrypt failed") && ok;
  ok = check(mailsmime_is_encrypted(encrypted_mime),
      "generated encrypted MIME was not detected as encrypted") && ok;

  r = mailsmime_decrypt(wrong_smime, encrypted_mime, &wrong_decrypted_mime);
  ok = check(r == MAILSMIME_ERROR_DECRYPT,
      "wrong private key unexpectedly decrypted MIME") && ok;

  r = mailsmime_decrypt(smime, encrypted_mime, &decrypted_mime);
  ok = check(r == MAILSMIME_NO_ERROR, "mailsmime_decrypt failed") && ok;
  ok = check_text_part(decrypted_mime) && ok;

  multi_recipients[0] = email;
  multi_recipients[1] = wrong_email;
  r = mailsmime_encrypt(smime, original, multi_recipients, 2,
      &multi_encrypted_mime);
  ok = check(r == MAILSMIME_NO_ERROR, "multi-recipient encrypt failed") && ok;
  ok = check(mailsmime_is_encrypted(multi_encrypted_mime),
      "multi-recipient MIME was not detected as encrypted") && ok;

 cleanup:
  free(pem);
  mailsmime_certificate_free(signer);
  mailsmime_result_free(tampered_result);
  mailsmime_result_free(untrusted_result);
  mailsmime_result_free(verify_result);
  mailsmime_mime_free(decrypted_mime);
  mailsmime_mime_free(wrong_decrypted_mime);
  mailsmime_mime_free(multi_encrypted_mime);
  mailsmime_mime_free(encrypted_mime);
  mailsmime_mime_free(tampered_mime);
  mailsmime_mime_free(signed_mime);
  mailmime_free(original);
  mailsmime_free(wrong_smime);
  mailsmime_free(untrusted_smime);
  mailsmime_free(smime);

  return ok;
}

int smime_test_crypto_round_trip(void)
{
  return smime_test_crypto_round_trip_with_backend(MAILSMIME_BACKEND_DEFAULT);
}

int smime_test_passphrase_callback(void)
{
  return smime_test_passphrase_callback_with_backend(
      MAILSMIME_BACKEND_DEFAULT);
}
#endif

#ifndef SMIME_LOW_LEVEL_NO_MAIN
int main(void)
{
  int ok;

  ok = 1;
  ok = smime_test_signed_fixture(
      "unittest/mime-parser/data/input/mbox/jwz/118") && ok;
  ok = smime_test_signed_fixture(
      "unittest/mime-parser/data/input/mbox/jwz/128") && ok;
  ok = smime_test_encrypted_fixture(
      "unittest/mime-parser/data/input/mbox/jwz/105") && ok;
  ok = smime_test_encrypted_fixture(
      "unittest/mime-parser/data/input/mbox/jwz/121") && ok;
#ifdef USE_SMIME_OPENSSL
  ok = smime_test_crypto_round_trip_with_backend(
      MAILSMIME_BACKEND_OPENSSL) && ok;
  ok = smime_test_passphrase_callback_with_backend(
      MAILSMIME_BACKEND_OPENSSL) && ok;
#endif
#ifdef USE_SMIME_APPLE
  ok = smime_test_crypto_round_trip_with_backend(
      MAILSMIME_BACKEND_APPLE) && ok;
  ok = smime_test_passphrase_callback_with_backend(
      MAILSMIME_BACKEND_APPLE) && ok;
#endif

  if (!ok)
    return EXIT_FAILURE;

  puts("smime-low-level-test: ok");
  return EXIT_SUCCESS;
}
#endif
