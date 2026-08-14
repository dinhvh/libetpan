/*
 * libEtPan! -- a mail stuff library
 */

#ifndef MAILPGP_TYPES_H
#define MAILPGP_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <libetpan/libetpan-config.h>

struct mailpgp;
struct mailpgp_result;
struct mailpgp_key;
struct mailpgp_key_list;
struct mailpgp_fingerprint_result;

typedef const char * (* mailpgp_passphrase_callback)(const char * identifier,
    void * context);

enum {
  MAILPGP_NO_ERROR = 0,
  MAILPGP_ERROR_MEMORY,
  MAILPGP_ERROR_INVAL,
  MAILPGP_ERROR_PARSE,
  MAILPGP_ERROR_KEY,
  MAILPGP_ERROR_SECRET_KEY,
  MAILPGP_ERROR_PASSPHRASE,
  MAILPGP_ERROR_VERIFY,
  MAILPGP_ERROR_DECRYPT,
  MAILPGP_ERROR_CRYPTO,
  MAILPGP_ERROR_NOT_IMPLEMENTED
};

enum {
  MAILPGP_VERIFY_VALID = 0,
  MAILPGP_VERIFY_INVALID,
  MAILPGP_VERIFY_NO_PUBLIC_KEY,
  MAILPGP_VERIFY_UNTRUSTED,
  MAILPGP_VERIFY_EXPIRED,
  MAILPGP_VERIFY_REVOKED,
  MAILPGP_VERIFY_SIGNER_MISMATCH,
  MAILPGP_VERIFY_ERROR
};

enum {
  MAILPGP_FINGERPRINT_SOURCE_PUBLIC_KEY = 0,
  MAILPGP_FINGERPRINT_SOURCE_SIGNATURE,
  MAILPGP_FINGERPRINT_SOURCE_ENCRYPTED_TO,
  MAILPGP_FINGERPRINT_SOURCE_HEADER_HINT
};

#ifdef __cplusplus
}
#endif

#endif
