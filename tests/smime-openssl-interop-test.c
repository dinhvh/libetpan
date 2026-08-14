/*
 * libEtPan! -- a mail stuff library
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libetpan/libetpan.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static struct mailmime * parse_mime_file(const char * filename)
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
  if ((r != MAIL_NO_ERROR) || (message_mime == NULL)) {
    mailmessage_free(msg);
    free(data);
    return NULL;
  }

  if ((message_mime->mm_type == MAILMIME_MESSAGE) &&
      (message_mime->mm_data.mm_message.mm_msg_mime != NULL)) {
    mime = message_mime->mm_data.mm_message.mm_msg_mime;
    message_mime->mm_data.mm_message.mm_msg_mime = NULL;
  }
  else {
    mime = message_mime;
    msg->msg_mime = NULL;
  }

  mailmessage_free(msg);
  return mime;
}

static int write_mime_file(const char * filename, struct mailmime * mime)
{
  FILE * f;
  struct mailmime fake_parent;
  struct mailmime * saved_parent;
  int saved_parent_type;
  int col;
  int r;

  f = fopen(filename, "wb");
  if (f == NULL)
    return -1;
  saved_parent = mime->mm_parent;
  saved_parent_type = mime->mm_parent_type;
  if ((mime->mm_type != MAILMIME_MESSAGE) && (mime->mm_parent == NULL)) {
    memset(&fake_parent, 0, sizeof(fake_parent));
    mime->mm_parent = &fake_parent;
    mime->mm_parent_type = MAILMIME_MESSAGE;
  }
  col = 0;
  r = mailmime_write(f, &col, mime);
  mime->mm_parent = saved_parent;
  mime->mm_parent_type = saved_parent_type;
  fclose(f);
  return r == MAILIMF_NO_ERROR ? 0 : -1;
}

static int verify_file(const char * ca_file, const char * signed_file)
{
  struct mailsmime * smime;
  struct mailmime * mime;
  struct mailsmime_result * result;
  int r;
  int status;

  smime = mailsmime_new();
  mime = parse_mime_file(signed_file);
  result = NULL;
  if ((smime == NULL) || (mime == NULL))
    return 2;

  r = mailsmime_add_trusted_cert_file(smime, ca_file);
  if (r == MAILSMIME_NO_ERROR)
    r = mailsmime_verify(smime, mime, &result);
  status = result != NULL ? mailsmime_result_status(result) :
      MAILSMIME_VERIFY_ERROR;

  printf("verify r=%d status=%d signers=%u error=%s\n", r, status,
      result != NULL ? mailsmime_result_signer_count(result) : 0,
      result != NULL && mailsmime_result_error(result) != NULL ?
      mailsmime_result_error(result) : "");

  mailsmime_result_free(result);
  mailmime_free(mime);
  mailsmime_free(smime);
  return (r == MAILSMIME_NO_ERROR) && (status == MAILSMIME_VERIFY_VALID) ?
      0 : 1;
}

static int decrypt_file(const char * cert_file, const char * key_file,
    const char * encrypted_file, const char * out_file)
{
  struct mailsmime * smime;
  struct mailmime * mime;
  struct mailmime * decrypted;
  int r;

  smime = mailsmime_new();
  mime = parse_mime_file(encrypted_file);
  decrypted = NULL;
  if ((smime == NULL) || (mime == NULL))
    return 2;

  r = mailsmime_set_private_key_file(smime, "alice@example.test", cert_file,
      key_file, NULL);
  if (r == MAILSMIME_NO_ERROR)
    r = mailsmime_decrypt(smime, mime, &decrypted);
  if ((r == MAILSMIME_NO_ERROR) && (decrypted != NULL))
    r = write_mime_file(out_file, decrypted) == 0 ? MAILSMIME_NO_ERROR :
        MAILSMIME_ERROR_PARSE;

  printf("decrypt r=%d\n", r);

  mailsmime_mime_free(decrypted);
  mailmime_free(mime);
  mailsmime_free(smime);
  return r == MAILSMIME_NO_ERROR ? 0 : 1;
}

static int sign_file(const char * cert_file, const char * key_file,
    const char * in_file, const char * out_file)
{
  struct mailsmime * smime;
  struct mailmime * mime;
  struct mailmime * signed_mime;
  int r;

  smime = mailsmime_new();
  mime = parse_mime_file(in_file);
  signed_mime = NULL;
  if ((smime == NULL) || (mime == NULL))
    return 2;

  r = mailsmime_set_private_key_file(smime, "alice@example.test", cert_file,
      key_file, NULL);
  if (r == MAILSMIME_NO_ERROR)
    r = mailsmime_sign(smime, mime, "alice@example.test", &signed_mime);
  if ((r == MAILSMIME_NO_ERROR) && (signed_mime != NULL))
    r = write_mime_file(out_file, signed_mime) == 0 ? MAILSMIME_NO_ERROR :
        MAILSMIME_ERROR_PARSE;

  printf("sign r=%d\n", r);

  mailsmime_mime_free(signed_mime);
  mailmime_free(mime);
  mailsmime_free(smime);
  return r == MAILSMIME_NO_ERROR ? 0 : 1;
}

static int encrypt_file(const char * cert_file, const char * in_file,
    const char * out_file)
{
  struct mailsmime * smime;
  struct mailmime * mime;
  struct mailmime * encrypted_mime;
  const char * recipients[1];
  int r;

  smime = mailsmime_new();
  mime = parse_mime_file(in_file);
  encrypted_mime = NULL;
  if ((smime == NULL) || (mime == NULL))
    return 2;

  recipients[0] = "alice@example.test";
  r = mailsmime_add_cert_file(smime, recipients[0], cert_file);
  if (r == MAILSMIME_NO_ERROR)
    r = mailsmime_encrypt(smime, mime, recipients, 1, &encrypted_mime);
  if ((r == MAILSMIME_NO_ERROR) && (encrypted_mime != NULL))
    r = write_mime_file(out_file, encrypted_mime) == 0 ? MAILSMIME_NO_ERROR :
        MAILSMIME_ERROR_PARSE;

  printf("encrypt r=%d\n", r);

  mailsmime_mime_free(encrypted_mime);
  mailmime_free(mime);
  mailsmime_free(smime);
  return r == MAILSMIME_NO_ERROR ? 0 : 1;
}

static int rewrite_file(const char * in_file, const char * out_file)
{
  struct mailmime * mime;
  int r;

  mime = parse_mime_file(in_file);
  if (mime == NULL)
    return 2;

  r = write_mime_file(out_file, mime);
  mailmime_free(mime);
  return r == 0 ? 0 : 1;
}

static int rewrite_mem_file(const char * in_file, const char * out_file)
{
  struct mailmime * mime;
  MMAPString * mmapstr;
  FILE * f;
  int col;
  int r;

  mime = parse_mime_file(in_file);
  if (mime == NULL)
    return 2;

  mmapstr = mmap_string_new("");
  if (mmapstr == NULL) {
    mailmime_free(mime);
    return 2;
  }

  col = 0;
  r = mailmime_write_mem(mmapstr, &col, mime);
  if (r == MAILIMF_NO_ERROR) {
    f = fopen(out_file, "wb");
    if (f == NULL)
      r = MAILIMF_ERROR_FILE;
    else {
      if (fwrite(mmapstr->str, 1, mmapstr->len, f) != mmapstr->len)
        r = MAILIMF_ERROR_FILE;
      fclose(f);
    }
  }

  mmap_string_free(mmapstr);
  mailmime_free(mime);
  return r == MAILIMF_NO_ERROR ? 0 : 1;
}

int main(int argc, char ** argv)
{
  if ((argc == 4) && (strcmp(argv[1], "verify") == 0))
    return verify_file(argv[2], argv[3]);
  if ((argc == 6) && (strcmp(argv[1], "decrypt") == 0))
    return decrypt_file(argv[2], argv[3], argv[4], argv[5]);
  if ((argc == 6) && (strcmp(argv[1], "sign") == 0))
    return sign_file(argv[2], argv[3], argv[4], argv[5]);
  if ((argc == 5) && (strcmp(argv[1], "encrypt") == 0))
    return encrypt_file(argv[2], argv[3], argv[4]);
  if ((argc == 4) && (strcmp(argv[1], "rewrite") == 0))
    return rewrite_file(argv[2], argv[3]);
  if ((argc == 4) && (strcmp(argv[1], "rewrite-mem") == 0))
    return rewrite_mem_file(argv[2], argv[3]);

  fprintf(stderr, "usage: %s verify ca.pem signed.eml\n", argv[0]);
  fprintf(stderr, "       %s decrypt cert.pem key.pem encrypted.eml out.eml\n",
      argv[0]);
  fprintf(stderr, "       %s sign cert.pem key.pem body.eml out.eml\n",
      argv[0]);
  fprintf(stderr, "       %s encrypt cert.pem body.eml out.eml\n", argv[0]);
  fprintf(stderr, "       %s rewrite in.eml out.eml\n", argv[0]);
  fprintf(stderr, "       %s rewrite-mem in.eml out.eml\n", argv[0]);
  return 2;
}
