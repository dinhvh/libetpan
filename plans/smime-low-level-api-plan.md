# Low-Level S/MIME API Plan

This plan defines a dead-simple S/MIME API on top of `mailmime` and `mailimf`.

The public model should stay:

```c
struct mailsmime * smime;
struct mailmime * input;
struct mailmime * output;
```

No public CMS objects. No public OpenSSL types. No caller-managed BIOs.

## Goals

- Make S/MIME usable as "MIME in, MIME out".
- Keep certificate and private-key setup obvious.
- Let email apps display useful signer information.
- Let email apps extract signer certificates.
- Keep the low-level API independent from message stores and sessions.

## Module Layout

Add a new low-level module:

```text
src/low-level/smime/
  mailsmime.h
  mailsmime_types.h
  mailsmime.c
```

Public include:

```c
#include <libetpan/mailsmime.h>
```

## Core Context API

```c
struct mailsmime;

struct mailsmime * mailsmime_new(void);
void mailsmime_free(struct mailsmime * smime);
```

The context owns:

- trusted certificates used for verification
- recipient certificates used for encryption
- local certificates/private keys used for signing and decryption
- passphrases or passphrase callbacks

## Certificate And Key Setup

Start with file-based setup because it is easy to understand and test:

```c
int mailsmime_add_trusted_cert_file(struct mailsmime * smime,
    const char * filename);

int mailsmime_add_cert_file(struct mailsmime * smime,
    const char * email,
    const char * filename);

int mailsmime_set_private_key_file(struct mailsmime * smime,
    const char * email,
    const char * cert_filename,
    const char * key_filename,
    const char * passphrase);

typedef const char * (* mailsmime_passphrase_callback)(const char * email,
    void * context);

int mailsmime_set_passphrase_callback(struct mailsmime * smime,
    mailsmime_passphrase_callback callback,
    void * context);
```

Meaning:

- `mailsmime_add_trusted_cert_file()` adds a trust anchor or trusted certificate
  used when verifying signatures.
- `mailsmime_add_cert_file()` adds a recipient certificate used when encrypting.
- `mailsmime_set_private_key_file()` adds a local identity used when signing and
  decrypting.
- `email` is the caller's lookup key. The implementation should still inspect
  the certificate identity during verification.
- The fixed `passphrase` argument is the simple path. The callback covers UI
  prompts and key stores without making OpenSSL types public.

## MIME Detection Helpers

```c
int mailsmime_is_signed(struct mailmime * mime);
int mailsmime_is_encrypted(struct mailmime * mime);
int mailsmime_is_smime(struct mailmime * mime);
```

These helpers inspect MIME shape only.

Signed forms:

- `multipart/signed`
- `application/pkcs7-signature`
- `application/x-pkcs7-signature`
- `application/pkcs7-mime; smime-type=signed-data`
- `application/x-pkcs7-mime; smime-type=signed-data`

Encrypted forms:

- `application/pkcs7-mime; smime-type=enveloped-data`
- `application/x-pkcs7-mime; smime-type=enveloped-data`
- `application/pkcs7-mime` or `application/x-pkcs7-mime` without `signed-data`,
  treated as probably encrypted for compatibility with older messages

## Signing

```c
int mailsmime_sign(struct mailsmime * smime,
    struct mailmime * mime,
    const char * signer_email,
    struct mailmime ** result);
```

Behavior:

- Finds the local certificate/private-key pair for `signer_email`.
- Serializes the input MIME entity into canonical bytes.
- Creates a detached CMS signature.
- Returns a new `multipart/signed` MIME entity.

Default output shape:

- outer `Content-Type: multipart/signed`
- `protocol="application/pkcs7-signature"`
- `micalg=...`
- first part: original MIME entity
- second part:
  - `Content-Type: application/pkcs7-signature; name="smime.p7s"`
  - `Content-Transfer-Encoding: base64`
  - `Content-Disposition: attachment; filename="smime.p7s"`

Opaque signed messages can be added later if needed, but the simple default
should be clear-signed `multipart/signed`.

## Verification

```c
struct mailsmime_result;

int mailsmime_verify(struct mailsmime * smime,
    struct mailmime * mime,
    struct mailsmime_result ** result);
```

Verification returns structured information instead of only yes/no.

```c
enum {
  MAILSMIME_VERIFY_VALID = 0,
  MAILSMIME_VERIFY_INVALID,
  MAILSMIME_VERIFY_UNTRUSTED,
  MAILSMIME_VERIFY_EXPIRED,
  MAILSMIME_VERIFY_REVOKED,
  MAILSMIME_VERIFY_SIGNER_MISMATCH,
  MAILSMIME_VERIFY_ERROR
};

int mailsmime_result_status(struct mailsmime_result * result);
const char * mailsmime_result_error(struct mailsmime_result * result);

unsigned int mailsmime_result_signer_count(struct mailsmime_result * result);

int mailsmime_result_signed_by_address(struct mailsmime_result * result,
    const char * email);

int mailsmime_result_signed_by_from(struct mailsmime_result * result,
    struct mailimf_fields * fields);

struct mailmime * mailsmime_result_get_signed_mime(
    struct mailsmime_result * result);

void mailsmime_result_free(struct mailsmime_result * result);
```

`mailsmime_result_signed_by_address()` answers the UI question email apps
actually care about: whether the verified signer certificate contains the
address being displayed as the sender.

`mailsmime_result_signed_by_from()` is the convenience form for callers that
already parsed message headers with `mailimf`; it returns whether any mailbox in
the parsed `From` field matches a signer certificate address.

`mailsmime_result_get_signed_mime()` returns the verified inner MIME entity for
opaque signed messages. For clear-signed `multipart/signed`, it may return the
first signed part. The returned pointer is owned by the result object.

## Signer Identity

The signer is the identity from the certificate that created the S/MIME
signature. A cryptographically valid signature does not automatically mean the
message `From` address signed the message.

Signer email extraction order:

1. `subjectAltName:rfc822Name`
2. legacy subject `emailAddress`
3. no email identity

Signer display name extraction:

1. subject `CN`
2. email address
3. empty string

The API should distinguish:

- certificate email address
- certificate display name
- whether the certificate email matches the message `From` address

Email apps usually show:

- signing status
- signer email address
- signer display name
- whether the signer matches the message sender
- trust status
- issuer
- certificate validity period
- fingerprint
- certificate details in a "show certificate" view

## Certificate Wrapper

Expose signer certificates through a small wrapper:

```c
struct mailsmime_certificate;

int mailsmime_result_get_signer(struct mailsmime_result * result,
    unsigned int index,
    struct mailsmime_certificate ** cert);

const char * mailsmime_certificate_email(struct mailsmime_certificate * cert);
const char * mailsmime_certificate_name(struct mailsmime_certificate * cert);
const char * mailsmime_certificate_subject(struct mailsmime_certificate * cert);
const char * mailsmime_certificate_issuer(struct mailsmime_certificate * cert);
const char * mailsmime_certificate_not_before(
    struct mailsmime_certificate * cert);
const char * mailsmime_certificate_not_after(
    struct mailsmime_certificate * cert);
const char * mailsmime_certificate_fingerprint_sha256(
    struct mailsmime_certificate * cert);

int mailsmime_certificate_export_pem(struct mailsmime_certificate * cert,
    char ** pem,
    size_t * pem_len);

void mailsmime_certificate_free(struct mailsmime_certificate * cert);
```

This gives clients enough for:

- "Signed by Alice <alice@example.com>"
- "Certificate issued by Example CA"
- certificate detail dialogs
- contact certificate import
- trust-on-first-use experiments
- debugging verification failures

The first implementation should support multiple signers in the result object,
even if common email messages have only one signer.

## Encryption

```c
int mailsmime_encrypt(struct mailsmime * smime,
    struct mailmime * mime,
    const char ** recipient_emails,
    unsigned int recipient_count,
    struct mailmime ** result);
```

Behavior:

- Finds one certificate for each recipient email.
- Serializes the input MIME entity.
- CMS-encrypts the serialized MIME entity.
- Returns a new encrypted MIME entity.

Default output shape:

- `Content-Type: application/pkcs7-mime; smime-type=enveloped-data; name="smime.p7m"`
- `Content-Transfer-Encoding: base64`
- `Content-Disposition: attachment; filename="smime.p7m"`

## Decryption

```c
int mailsmime_decrypt(struct mailsmime * smime,
    struct mailmime * mime,
    struct mailmime ** result);
```

Behavior:

- Detects the encrypted CMS payload.
- Tries configured local private keys.
- Decrypts the payload.
- Parses the decrypted bytes back into a `struct mailmime *`.
- Returns the decrypted inner MIME entity.

The caller owns the returned MIME entity and frees it with `mailmime_free()`.

## Canonical Serialization

Add an internal helper:

```c
int mailsmime_canonicalize_mime(struct mailmime * mime,
    char ** result,
    size_t * result_len);
```

Rules:

- Serialize with `mailmime_write_mem()`.
- Preserve CRLF line endings.
- Do not mutate the signed part after canonicalization.
- Verify over the exact bytes represented by the signed MIME part.

This matters because S/MIME signs bytes, not an abstract MIME tree.

For newly-created signatures, serializing with `mailmime_write_mem()` is
feasible. For verifying existing `multipart/signed` messages, prefer the
original parsed bytes for the signed part when `mm_mime_start` and `mm_length`
are available. Re-serializing a parsed MIME tree can change folding, boundaries,
or transfer encoding details and break otherwise-valid signatures.

Opaque signed and encrypted payloads can be parsed from decoded body bytes
because the CMS object carries the protected content.

## Ownership Rules

- Functions returning `struct mailmime ** result` transfer ownership to the
  caller. The caller frees with `mailmime_free()`.
- `mailsmime_verify()` transfers ownership of `struct mailsmime_result *` to the
  caller. The caller frees with `mailsmime_result_free()`.
- Certificate objects returned by `mailsmime_result_get_signer()` are independent
  wrappers owned by the caller and freed with `mailsmime_certificate_free()`.
- Strings returned by certificate accessors are owned by the certificate object.
- `mailsmime_certificate_export_pem()` allocates `pem`; the caller frees it with
  `free()`.

## Build Feasibility Notes

The module fits the current autotools layout:

- Add `src/low-level/smime/Makefile.am`.
- Add `smime` to `SUBDIRS` in `src/low-level/Makefile.am`.
- Add `smime/libsmime.la` to `liblow_level_la_LIBADD`.
- Install public headers through the new smime module's
  `etpaninclude_HEADERS`.
- Reuse existing `SSLLIBS` linkage from the top-level libetpan link.

The configure check should be strengthened from "OpenSSL exists" to "OpenSSL
has the CMS APIs this module uses", for example by checking CMS headers and a
representative CMS function. If CMS is unavailable, the module should either not
build or return `MAILSMIME_ERROR_CRYPTO` from crypto operations while keeping
detection helpers available.

## Error Codes

Keep operation errors compact:

```c
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
```

Verification-specific status lives in `mailsmime_result_status()`.

## Typical Usage

Signing:

```c
struct mailsmime * smime;
struct mailmime * signed_mime;

smime = mailsmime_new();

mailsmime_set_private_key_file(smime,
    "alice@example.com",
    "alice-cert.pem",
    "alice-key.pem",
    "secret");

r = mailsmime_sign(smime, original_mime, "alice@example.com", &signed_mime);

mailsmime_free(smime);
```

Verifying:

```c
struct mailsmime * smime;
struct mailsmime_result * result;
struct mailsmime_certificate * cert;

smime = mailsmime_new();
mailsmime_add_trusted_cert_file(smime, "ca.pem");

r = mailsmime_verify(smime, signed_mime, &result);
if (r == MAILSMIME_NO_ERROR &&
    mailsmime_result_status(result) == MAILSMIME_VERIFY_VALID &&
    mailsmime_result_signed_by_address(result, "alice@example.com")) {
  mailsmime_result_get_signer(result, 0, &cert);
  /* show mailsmime_certificate_name(), email(), issuer(), fingerprint(), ... */
  mailsmime_certificate_free(cert);
}

mailsmime_result_free(result);
mailsmime_free(smime);
```

Encrypting:

```c
struct mailsmime * smime;
struct mailmime * encrypted_mime;
const char * recipients[] = { "bob@example.com" };

smime = mailsmime_new();
mailsmime_add_cert_file(smime, "bob@example.com", "bob-cert.pem");

r = mailsmime_encrypt(smime, original_mime, recipients, 1, &encrypted_mime);

mailsmime_free(smime);
```

Decrypting:

```c
struct mailsmime * smime;
struct mailmime * decrypted_mime;

smime = mailsmime_new();

mailsmime_set_private_key_file(smime,
    "bob@example.com",
    "bob-cert.pem",
    "bob-key.pem",
    "secret");

r = mailsmime_decrypt(smime, encrypted_mime, &decrypted_mime);

mailsmime_free(smime);
```

## Testing Plan

Add focused low-level tests with fixtures for:

- detecting clear-signed MIME
- detecting opaque signed MIME
- detecting encrypted MIME
- signing a simple text MIME
- verifying a valid signature
- rejecting a tampered signed body
- extracting signer email/name/certificate/validity dates
- checking signer matches `From`
- checking signer does not match `From`
- exporting signer certificate as PEM
- encrypting for one recipient
- encrypting for multiple recipients
- decrypting with the matching private key
- failing decrypt with the wrong key
- failing verify with an untrusted signer

Existing S/MIME parser fixtures under `unittest/mime-parser/data/input/mbox/jwz`
are relevant for detection tests and malformed/legacy MIME shape tests.
Generated certificates and messages cover crypto round-trip tests.

The Linux/OpenSSL slice is implemented in `tests/smime-low-level-test.c` and
`tests/smime-openssl-fixture-test.sh`, including:

- generated fake CA/cert/key material
- encrypted private-key loading through the passphrase callback
- OpenSSL-created signed messages verified by libetpan
- libetpan-created signed messages verified by OpenSSL
- OpenSSL-created encrypted messages decrypted by libetpan
- libetpan-created encrypted messages decrypted by OpenSSL
- valid, untrusted, tampered, wrong-signer, wrong-key, and multi-recipient
  checks

## First Implementation Slice

Build in this order:

1. Public headers and opaque context/result/certificate types.
2. MIME detection helpers.
3. Certificate and key loading.
4. `mailsmime_sign()`.
5. `mailsmime_verify()` with signer extraction.
6. `mailsmime_certificate_export_pem()`.
7. `mailsmime_encrypt()`.
8. `mailsmime_decrypt()`.
9. Unit tests and short API examples.

## Current Linux/OpenSSL Status

Implemented:

- public low-level API in `src/low-level/smime/mailsmime.h`
- opaque context/result/certificate types
- MIME shape detection helpers
- file-based certificate/key loading
- passphrase string and callback support, including encrypted-key coverage
- detached `multipart/signed` signing
- signature verification with signer extraction
- signer email/name/subject/issuer/fingerprint/validity metadata
- signer address matching against parsed IMF `From` fields
- signer certificate PEM export
- envelope encryption and decryption
- canonical verification input from preserved parser bytes when available
- OpenSSL CMS configure checks
- focused Linux/OpenSSL unit and CLI interop tests
- short API examples in `doc/SMIME.md`

Deferred or intentionally not part of the low-level Linux slice:

- Apple validity-date extraction parity
