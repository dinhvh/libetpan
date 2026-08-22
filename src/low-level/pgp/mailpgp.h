/*
 * libEtPan! -- a mail stuff library
 */

#ifndef MAILPGP_H
#define MAILPGP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <libetpan/mailpgp_types.h>
#include <libetpan/mailmime.h>

LIBETPAN_EXPORT
struct mailpgp * mailpgp_new(void);

LIBETPAN_EXPORT
void mailpgp_free(struct mailpgp * pgp);

LIBETPAN_EXPORT
int mailpgp_set_passphrase_callback(struct mailpgp * pgp,
    mailpgp_passphrase_callback callback,
    void * context);

LIBETPAN_EXPORT
int mailpgp_add_public_key_file(struct mailpgp * pgp,
    const char * filename);

LIBETPAN_EXPORT
int mailpgp_add_secret_key_file(struct mailpgp * pgp,
    const char * filename,
    const char * passphrase);

LIBETPAN_EXPORT
int mailpgp_add_public_key(struct mailpgp * pgp,
    const char * data,
    size_t length);

LIBETPAN_EXPORT
int mailpgp_add_secret_key(struct mailpgp * pgp,
    const char * data,
    size_t length,
    const char * passphrase);

LIBETPAN_EXPORT
int mailpgp_is_signed(struct mailmime * mime);

LIBETPAN_EXPORT
int mailpgp_is_encrypted(struct mailmime * mime);

LIBETPAN_EXPORT
int mailpgp_is_key(struct mailmime * mime);

LIBETPAN_EXPORT
int mailpgp_is_pgp(struct mailmime * mime);

LIBETPAN_EXPORT
int mailpgp_message_verify(struct mailpgp * pgp,
    const char * message,
    size_t length,
    struct mailpgp_result ** result);

LIBETPAN_EXPORT
int mailpgp_verify(struct mailpgp * pgp,
    const char * message,
    size_t length,
    struct mailmime * mime,
    struct mailpgp_result ** result);

LIBETPAN_EXPORT
int mailpgp_message_decrypt(struct mailpgp * pgp,
    const char * message,
    size_t length,
    struct mailmime ** result);

LIBETPAN_EXPORT
int mailpgp_decrypt(struct mailpgp * pgp,
    const char * message,
    size_t length,
    struct mailmime * mime,
    struct mailmime ** result);

LIBETPAN_EXPORT
int mailpgp_sign(struct mailpgp * pgp,
    struct mailmime * mime,
    const char * signer,
    struct mailmime ** result);

LIBETPAN_EXPORT
int mailpgp_encrypt(struct mailpgp * pgp,
    struct mailmime * mime,
    const char ** recipients,
    unsigned int recipient_count,
    struct mailmime ** result);

LIBETPAN_EXPORT
int mailpgp_message_extract_fingerprints(struct mailpgp * pgp,
    const char * message,
    size_t length,
    struct mailpgp_fingerprint_result ** result);

LIBETPAN_EXPORT
int mailpgp_extract_fingerprints(struct mailpgp * pgp,
    const char * message,
    size_t length,
    struct mailmime * mime,
    struct mailpgp_fingerprint_result ** result);

LIBETPAN_EXPORT
int mailpgp_message_extract_public_keys(struct mailpgp * pgp,
    const char * message,
    size_t length,
    struct mailpgp_key_list ** result);

LIBETPAN_EXPORT
int mailpgp_extract_public_keys(struct mailpgp * pgp,
    const char * message,
    size_t length,
    struct mailmime * mime,
    struct mailpgp_key_list ** result);

LIBETPAN_EXPORT
int mailpgp_result_status(struct mailpgp_result * result);

LIBETPAN_EXPORT
const char * mailpgp_result_error(struct mailpgp_result * result);

LIBETPAN_EXPORT
unsigned int mailpgp_result_signer_count(struct mailpgp_result * result);

LIBETPAN_EXPORT
int mailpgp_result_signed_by_address(struct mailpgp_result * result,
    const char * email);

LIBETPAN_EXPORT
int mailpgp_result_get_signer(struct mailpgp_result * result,
    unsigned int index,
    struct mailpgp_key ** key);

LIBETPAN_EXPORT
void mailpgp_result_free(struct mailpgp_result * result);

LIBETPAN_EXPORT
unsigned int mailpgp_fingerprint_count(
    struct mailpgp_fingerprint_result * result);

LIBETPAN_EXPORT
const char * mailpgp_fingerprint_value(
    struct mailpgp_fingerprint_result * result,
    unsigned int index);

LIBETPAN_EXPORT
int mailpgp_fingerprint_source(
    struct mailpgp_fingerprint_result * result,
    unsigned int index);

LIBETPAN_EXPORT
int mailpgp_fingerprint_is_verified(
    struct mailpgp_fingerprint_result * result,
    unsigned int index);

LIBETPAN_EXPORT
const char * mailpgp_fingerprint_key_id(
    struct mailpgp_fingerprint_result * result,
    unsigned int index);

LIBETPAN_EXPORT
void mailpgp_fingerprint_result_free(
    struct mailpgp_fingerprint_result * result);

LIBETPAN_EXPORT
unsigned int mailpgp_key_list_count(struct mailpgp_key_list * list);

LIBETPAN_EXPORT
int mailpgp_key_list_get(struct mailpgp_key_list * list,
    unsigned int index,
    struct mailpgp_key ** key);

LIBETPAN_EXPORT
void mailpgp_key_list_free(struct mailpgp_key_list * list);

LIBETPAN_EXPORT
const char * mailpgp_key_email(struct mailpgp_key * key);

LIBETPAN_EXPORT
const char * mailpgp_key_name(struct mailpgp_key * key);

LIBETPAN_EXPORT
const char * mailpgp_key_user_id(struct mailpgp_key * key);

LIBETPAN_EXPORT
const char * mailpgp_key_fingerprint(struct mailpgp_key * key);

LIBETPAN_EXPORT
const char * mailpgp_key_key_id(struct mailpgp_key * key);

LIBETPAN_EXPORT
const char * mailpgp_key_algorithm(struct mailpgp_key * key);

LIBETPAN_EXPORT
int mailpgp_key_is_expired(struct mailpgp_key * key);

LIBETPAN_EXPORT
int mailpgp_key_is_revoked(struct mailpgp_key * key);

LIBETPAN_EXPORT
int mailpgp_key_export_armored(struct mailpgp_key * key,
    char ** armored,
    size_t * armored_len);

LIBETPAN_EXPORT
void mailpgp_key_free(struct mailpgp_key * key);

/*
 * Frees MIME entities returned by mailpgp_sign(), mailpgp_encrypt(),
 * mailpgp_decrypt(), and mailpgp_message_decrypt(). These MIME trees can
 * reference internal backing buffers that are not released by mailmime_free().
 */
LIBETPAN_EXPORT
void mailpgp_mime_free(struct mailmime * mime);

#ifdef __cplusplus
}
#endif

#endif
