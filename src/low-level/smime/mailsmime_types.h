/*
 * libEtPan! -- a mail stuff library
 */

#ifndef MAILSMIME_TYPES_H
#define MAILSMIME_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <libetpan/libetpan-config.h>

struct mailsmime;
struct mailsmime_result;
struct mailsmime_certificate;

typedef const char * (* mailsmime_passphrase_callback)(const char * email,
    void * context);

enum {
  MAILSMIME_NO_ERROR = 0,
  MAILSMIME_ERROR_MEMORY,
  MAILSMIME_ERROR_INVAL,
  MAILSMIME_ERROR_PARSE,
  MAILSMIME_ERROR_CERT,
  MAILSMIME_ERROR_PRIVATE_KEY,
  MAILSMIME_ERROR_PASSPHRASE,
  MAILSMIME_ERROR_VERIFY,
  MAILSMIME_ERROR_DECRYPT,
  MAILSMIME_ERROR_CRYPTO
};

enum {
  MAILSMIME_VERIFY_VALID = 0,
  MAILSMIME_VERIFY_INVALID,
  MAILSMIME_VERIFY_UNTRUSTED,
  MAILSMIME_VERIFY_EXPIRED,
  MAILSMIME_VERIFY_REVOKED,
  MAILSMIME_VERIFY_SIGNER_MISMATCH,
  MAILSMIME_VERIFY_ERROR
};

#ifdef __cplusplus
}
#endif

#endif
