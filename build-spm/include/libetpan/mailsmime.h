/*
 * libEtPan! -- a mail stuff library
 */

#ifndef MAILSMIME_H
#define MAILSMIME_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <libetpan/mailsmime_types.h>
#include <libetpan/mailimf_types.h>
#include <libetpan/mailmime.h>

enum mailsmime_backend {
  MAILSMIME_BACKEND_DEFAULT,
  MAILSMIME_BACKEND_APPLE,
  MAILSMIME_BACKEND_OPENSSL
};

LIBETPAN_EXPORT
struct mailsmime * mailsmime_new(void);

LIBETPAN_EXPORT
struct mailsmime * mailsmime_new_with_backend(enum mailsmime_backend backend);

LIBETPAN_EXPORT
void mailsmime_free(struct mailsmime * smime);

LIBETPAN_EXPORT
int mailsmime_add_trusted_cert_file(struct mailsmime * smime,
    const char * filename);

LIBETPAN_EXPORT
int mailsmime_add_cert_file(struct mailsmime * smime,
    const char * email,
    const char * filename);

LIBETPAN_EXPORT
int mailsmime_set_private_key_file(struct mailsmime * smime,
    const char * email,
    const char * cert_filename,
    const char * key_filename,
    const char * passphrase);

LIBETPAN_EXPORT
int mailsmime_set_passphrase_callback(struct mailsmime * smime,
    mailsmime_passphrase_callback callback,
    void * context);

LIBETPAN_EXPORT
int mailsmime_is_signed(struct mailmime * mime);

LIBETPAN_EXPORT
int mailsmime_is_encrypted(struct mailmime * mime);

LIBETPAN_EXPORT
int mailsmime_is_smime(struct mailmime * mime);

LIBETPAN_EXPORT
int mailsmime_sign(struct mailsmime * smime,
    struct mailmime * mime,
    const char * signer_email,
    struct mailmime ** result);

LIBETPAN_EXPORT
int mailsmime_verify(struct mailsmime * smime,
    struct mailmime * mime,
    struct mailsmime_result ** result);

LIBETPAN_EXPORT
int mailsmime_encrypt(struct mailsmime * smime,
    struct mailmime * mime,
    const char ** recipient_emails,
    unsigned int recipient_count,
    struct mailmime ** result);

LIBETPAN_EXPORT
int mailsmime_decrypt(struct mailsmime * smime,
    struct mailmime * mime,
    struct mailmime ** result);

/*
 * Frees MIME entities returned by mailsmime_sign(), mailsmime_encrypt(),
 * and mailsmime_decrypt(). These MIME trees can reference internal backing
 * buffers that are not released by mailmime_free().
 */
LIBETPAN_EXPORT
void mailsmime_mime_free(struct mailmime * mime);

LIBETPAN_EXPORT
int mailsmime_result_status(struct mailsmime_result * result);

LIBETPAN_EXPORT
const char * mailsmime_result_error(struct mailsmime_result * result);

LIBETPAN_EXPORT
unsigned int mailsmime_result_signer_count(struct mailsmime_result * result);

LIBETPAN_EXPORT
int mailsmime_result_signed_by_address(struct mailsmime_result * result,
    const char * email);

LIBETPAN_EXPORT
int mailsmime_result_signed_by_from(struct mailsmime_result * result,
    struct mailimf_fields * fields);

/*
 * Returns a MIME entity owned by result. Do not free it directly; it is released
 * by mailsmime_result_free().
 */
LIBETPAN_EXPORT
struct mailmime * mailsmime_result_get_signed_mime(
    struct mailsmime_result * result);

LIBETPAN_EXPORT
int mailsmime_result_get_signer(struct mailsmime_result * result,
    unsigned int index,
    struct mailsmime_certificate ** cert);

LIBETPAN_EXPORT
void mailsmime_result_free(struct mailsmime_result * result);

LIBETPAN_EXPORT
const char * mailsmime_certificate_email(struct mailsmime_certificate * cert);

LIBETPAN_EXPORT
const char * mailsmime_certificate_name(struct mailsmime_certificate * cert);

LIBETPAN_EXPORT
const char * mailsmime_certificate_subject(struct mailsmime_certificate * cert);

LIBETPAN_EXPORT
const char * mailsmime_certificate_issuer(struct mailsmime_certificate * cert);

LIBETPAN_EXPORT
const char * mailsmime_certificate_not_before(
    struct mailsmime_certificate * cert);

LIBETPAN_EXPORT
const char * mailsmime_certificate_not_after(
    struct mailsmime_certificate * cert);

LIBETPAN_EXPORT
const char * mailsmime_certificate_fingerprint_sha256(
    struct mailsmime_certificate * cert);

LIBETPAN_EXPORT
int mailsmime_certificate_export_pem(struct mailsmime_certificate * cert,
    char ** pem,
    size_t * pem_len);

LIBETPAN_EXPORT
void mailsmime_certificate_free(struct mailsmime_certificate * cert);

#ifdef __cplusplus
}
#endif

#endif
