/*
 * libEtPan! -- a mail stuff library
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libetpan/libetpan.h>
#include <libetpan/mailpgp.h>

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

static struct mailmime * parse_mime_file(const char * filename,
    char ** backing_data)
{
  char * data;
  size_t len;
  struct mailmime * mime;
  size_t index;
  int r;

  if (backing_data == NULL)
    return NULL;

  * backing_data = NULL;
  if (read_file(filename, &data, &len) < 0)
    return NULL;

  mime = NULL;
  index = 0;
  r = mailmime_parse(data, len, &index, &mime);
  if (r != MAILIMF_NO_ERROR) {
    free(data);
    return NULL;
  }

  * backing_data = data;
  return mime;
}

static int write_mime_file(const char * filename, struct mailmime * mime)
{
  FILE * f;
  int col;
  int r;

  f = fopen(filename, "wb");
  if (f == NULL)
    return -1;
  col = 0;
  r = mailmime_write(f, &col, mime);
  fclose(f);

  return r == MAILIMF_NO_ERROR ? 0 : -1;
}

static int write_mime_entity_file(const char * filename, struct mailmime * mime)
{
  struct mailmime * saved_parent;
  int saved_parent_type;
  int r;

  if (mime == NULL)
    return -1;

  saved_parent = mime->mm_parent;
  saved_parent_type = mime->mm_parent_type;
  if (mime->mm_parent == NULL) {
    mime->mm_parent = mime;
    mime->mm_parent_type = MAILMIME_MULTIPLE;
  }

  r = write_mime_file(filename, mime);

  mime->mm_parent = saved_parent;
  mime->mm_parent_type = saved_parent_type;

  return r;
}

static int mmap_to_alloc(MMAPString * mmapstr, char ** result,
    size_t * result_len)
{
  char * str;

  str = malloc(mmapstr->len + 1);
  if (str == NULL)
    return -1;

  memcpy(str, mmapstr->str, mmapstr->len);
  str[mmapstr->len] = '\0';
  * result = str;
  * result_len = mmapstr->len;
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
    return -1;

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

static int write_bytes_file(const char * filename, const char * data,
    size_t data_len)
{
  FILE * f;
  int ok;

  f = fopen(filename, "wb");
  if (f == NULL)
    return -1;
  ok = fwrite(data, 1, data_len, f) == data_len;
  fclose(f);
  return ok ? 0 : -1;
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

static int decode_mime_body(struct mailmime * mime, char ** result,
    size_t * result_len)
{
  struct mailmime_single_fields fields;
  int encoding;
  size_t index;
  int r;

  if ((mime == NULL) || (result == NULL) || (result_len == NULL) ||
      (mime->mm_body == NULL) ||
      (mime->mm_body->dt_type != MAILMIME_DATA_TEXT))
    return -1;

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
  return r == MAILIMF_NO_ERROR ? 0 : -1;
}

static int verify_file(const char * public_key_file, const char * signed_file)
{
  struct mailpgp * pgp;
  struct mailpgp_result * result;
  char * signed_data;
  size_t signed_len;
  int r;
  int status;

  pgp = mailpgp_new();
  result = NULL;
  signed_data = NULL;
  signed_len = 0;
  if (pgp == NULL)
    return 2;

  r = mailpgp_add_public_key_file(pgp, public_key_file);
  if (r == MAILPGP_NO_ERROR &&
      read_file(signed_file, &signed_data, &signed_len) < 0)
    r = MAILPGP_ERROR_PARSE;
  if (r == MAILPGP_NO_ERROR)
    r = mailpgp_message_verify(pgp, signed_data, signed_len, &result);

  status = result != NULL ? mailpgp_result_status(result) :
      MAILPGP_VERIFY_ERROR;
  printf("verify r=%d status=%d signers=%u error=%s\n", r, status,
      result != NULL ? mailpgp_result_signer_count(result) : 0,
      result != NULL && mailpgp_result_error(result) != NULL ?
      mailpgp_result_error(result) : "");

  free(signed_data);
  mailpgp_result_free(result);
  mailpgp_free(pgp);
  return (r == MAILPGP_NO_ERROR) && (status == MAILPGP_VERIFY_VALID) ? 0 : 1;
}

static int decrypt_file(const char * secret_key_file,
    const char * encrypted_file, const char * out_file)
{
  struct mailpgp * pgp;
  struct mailmime * decrypted;
  char * encrypted_data;
  size_t encrypted_len;
  int r;

  pgp = mailpgp_new();
  decrypted = NULL;
  encrypted_data = NULL;
  encrypted_len = 0;
  if (pgp == NULL)
    return 2;

  r = mailpgp_add_secret_key_file(pgp, secret_key_file, "");
  if (r == MAILPGP_NO_ERROR &&
      read_file(encrypted_file, &encrypted_data, &encrypted_len) < 0)
    r = MAILPGP_ERROR_PARSE;
  if (r == MAILPGP_NO_ERROR)
    r = mailpgp_message_decrypt(pgp, encrypted_data, encrypted_len,
        &decrypted);
  if ((r == MAILPGP_NO_ERROR) && (decrypted != NULL))
    r = write_mime_file(out_file, decrypted) == 0 ? MAILPGP_NO_ERROR :
        MAILPGP_ERROR_PARSE;

  printf("decrypt r=%d\n", r);

  free(encrypted_data);
  mailpgp_mime_free(decrypted);
  mailpgp_free(pgp);
  return r == MAILPGP_NO_ERROR ? 0 : 1;
}

static int sign_file(const char * secret_key_file,
    const char * in_file, const char * out_file)
{
  struct mailpgp * pgp;
  struct mailmime * mime;
  struct mailmime * signed_mime;
  char * mime_data;
  int r;

  pgp = mailpgp_new();
  mime_data = NULL;
  mime = parse_mime_file(in_file, &mime_data);
  signed_mime = NULL;
  if ((pgp == NULL) || (mime == NULL))
    return 2;

  r = mailpgp_add_secret_key_file(pgp, secret_key_file, "");
  if (r == MAILPGP_NO_ERROR)
    r = mailpgp_sign(pgp, mime, "alice@example.test", &signed_mime);
  if ((r == MAILPGP_NO_ERROR) && (signed_mime != NULL))
    r = write_mime_entity_file(out_file, signed_mime) == 0 ? MAILPGP_NO_ERROR :
        MAILPGP_ERROR_PARSE;

  printf("sign r=%d\n", r);

  mailpgp_mime_free(signed_mime);
  mailmime_free(mime);
  free(mime_data);
  mailpgp_free(pgp);
  return r == MAILPGP_NO_ERROR ? 0 : 1;
}

static int encrypt_file(const char * public_key_file,
    const char * in_file, const char * out_file)
{
  struct mailpgp * pgp;
  struct mailmime * mime;
  struct mailmime * encrypted_mime;
  const char * recipients[1];
  char * mime_data;
  int r;

  pgp = mailpgp_new();
  mime_data = NULL;
  mime = parse_mime_file(in_file, &mime_data);
  encrypted_mime = NULL;
  if ((pgp == NULL) || (mime == NULL))
    return 2;

  recipients[0] = "alice@example.test";
  r = mailpgp_add_public_key_file(pgp, public_key_file);
  if (r == MAILPGP_NO_ERROR)
    r = mailpgp_encrypt(pgp, mime, recipients, 1, &encrypted_mime);
  if ((r == MAILPGP_NO_ERROR) && (encrypted_mime != NULL))
    r = write_mime_entity_file(out_file, encrypted_mime) == 0 ? MAILPGP_NO_ERROR :
        MAILPGP_ERROR_PARSE;

  printf("encrypt r=%d\n", r);

  mailpgp_mime_free(encrypted_mime);
  mailmime_free(mime);
  free(mime_data);
  mailpgp_free(pgp);
  return r == MAILPGP_NO_ERROR ? 0 : 1;
}

static int extract_signed_file(const char * signed_file,
    const char * data_file, const char * signature_file)
{
  struct mailmime * mime;
  struct mailmime * signed_part;
  struct mailmime * signature_part;
  char * mime_data;
  char * signed_data;
  char * signature;
  size_t signed_data_len;
  size_t signature_len;
  int r;

  mime_data = NULL;
  signed_data = NULL;
  signature = NULL;
  signed_data_len = 0;
  signature_len = 0;
  mime = parse_mime_file(signed_file, &mime_data);
  if (mime == NULL)
    return 2;

  signed_part = multipart_part(mime, 0);
  signature_part = multipart_part(mime, 1);
  if ((signed_part == NULL) || (signature_part == NULL)) {
    fprintf(stderr, "extract-signed: missing signed part or signature part "
        "mime_type=%d signed=%p signature=%p\n",
        mime != NULL ? mime->mm_type : -1, (void *) signed_part,
        (void *) signature_part);
    r = 2;
    goto free;
  }

  if ((signed_part->mm_mime_start != NULL) && (signed_part->mm_length != 0)) {
    signed_data = malloc(signed_part->mm_length + 1);
    if (signed_data == NULL) {
      r = 1;
      goto free;
    }
    memcpy(signed_data, signed_part->mm_mime_start, signed_part->mm_length);
    signed_data[signed_part->mm_length] = '\0';
    signed_data_len = signed_part->mm_length;
  }
  else if (write_mime_sub_to_mem(signed_part, &signed_data,
      &signed_data_len) < 0) {
    r = 1;
    goto free;
  }

  if (write_bytes_file(data_file, signed_data, signed_data_len) < 0) {
    r = 1;
    goto free;
  }

  if (decode_mime_body(signature_part, &signature, &signature_len) < 0) {
    r = 1;
    goto free;
  }
  r = write_bytes_file(signature_file, signature, signature_len) == 0 ? 0 : 1;

 free:
  free(signed_data);
  if (signature != NULL)
    mmap_string_unref(signature);
  mailmime_free(mime);
  free(mime_data);
  return r;
}

int main(int argc, char ** argv)
{
  if ((argc == 4) && (strcmp(argv[1], "verify") == 0))
    return verify_file(argv[2], argv[3]);
  if ((argc == 5) && (strcmp(argv[1], "decrypt") == 0))
    return decrypt_file(argv[2], argv[3], argv[4]);
  if ((argc == 5) && (strcmp(argv[1], "sign") == 0))
    return sign_file(argv[2], argv[3], argv[4]);
  if ((argc == 5) && (strcmp(argv[1], "encrypt") == 0))
    return encrypt_file(argv[2], argv[3], argv[4]);
  if ((argc == 5) && (strcmp(argv[1], "extract-signed") == 0))
    return extract_signed_file(argv[2], argv[3], argv[4]);

  fprintf(stderr, "usage: %s verify public.asc signed.eml\n", argv[0]);
  fprintf(stderr, "       %s decrypt secret.asc encrypted.eml out.eml\n",
      argv[0]);
  fprintf(stderr, "       %s sign secret.asc body.eml out.eml\n", argv[0]);
  fprintf(stderr, "       %s encrypt public.asc body.eml out.eml\n", argv[0]);
  fprintf(stderr, "       %s extract-signed signed.eml data.out sig.asc\n",
      argv[0]);
  return 2;
}
