# Low-Level S/MIME

The low-level S/MIME API works with `struct mailmime *` values directly. The
caller configures certificates and keys on a `struct mailsmime *` context, then
passes MIME entities in and gets MIME entities back.

MIME entities returned by `mailsmime_sign()`, `mailsmime_encrypt()`, and
`mailsmime_decrypt()` can reference backing buffers owned by the S/MIME layer.
Free those results with `mailsmime_mime_free()`, not `mailmime_free()`. The input
`struct mailmime *` remains owned by the caller.

## Backend Selection

`mailsmime_new()` keeps the platform's default behavior. When a build contains
both S/MIME implementations, a caller can select one for each context:

```c
struct mailsmime * apple_smime;
struct mailsmime * openssl_smime;

apple_smime = mailsmime_new_with_backend(MAILSMIME_BACKEND_APPLE);
openssl_smime = mailsmime_new_with_backend(MAILSMIME_BACKEND_OPENSSL);
```

The available values are `MAILSMIME_BACKEND_DEFAULT`,
`MAILSMIME_BACKEND_APPLE`, and `MAILSMIME_BACKEND_OPENSSL`. The explicit
constructor returns `NULL` when the requested backend was not compiled into
libetpan. On macOS, the Xcode framework builds both implementations and the
default remains Apple Security.

## Signing

```c
struct mailsmime * smime;
struct mailmime * signed_mime;
int r;

smime = mailsmime_new();
if (smime == NULL)
  return;

r = mailsmime_set_private_key_file(smime,
    "alice@example.com",
    "alice-cert.pem",
    "alice-key.pem",
    "secret");
if (r == MAILSMIME_NO_ERROR) {
  r = mailsmime_sign(smime, original_mime, "alice@example.com",
      &signed_mime);
  if (r == MAILSMIME_NO_ERROR) {
    /* signed_mime is a multipart/signed MIME entity. */
    mailsmime_mime_free(signed_mime);
  }
}

mailsmime_free(smime);
```

## Verifying

```c
struct mailsmime * smime;
struct mailsmime_result * result;
struct mailsmime_certificate * cert;
int r;

smime = mailsmime_new();
if (smime == NULL)
  return;

mailsmime_add_trusted_cert_file(smime, "ca-cert.pem");

r = mailsmime_verify(smime, signed_mime, &result);
if (r == MAILSMIME_NO_ERROR) {
  if ((mailsmime_result_status(result) == MAILSMIME_VERIFY_VALID) &&
      mailsmime_result_signed_by_address(result, "alice@example.com")) {
    r = mailsmime_result_get_signer(result, 0, &cert);
    if (r == MAILSMIME_NO_ERROR) {
      const char * email = mailsmime_certificate_email(cert);
      const char * name = mailsmime_certificate_name(cert);
      const char * issuer = mailsmime_certificate_issuer(cert);
      const char * fingerprint =
          mailsmime_certificate_fingerprint_sha256(cert);

      (void) email;
      (void) name;
      (void) issuer;
      (void) fingerprint;
      mailsmime_certificate_free(cert);
    }
  }
  /* The signed MIME returned by mailsmime_result_get_signed_mime() is borrowed
     from result and is freed by mailsmime_result_free(). */
  mailsmime_result_free(result);
}

mailsmime_free(smime);
```

If the message headers were parsed with `mailimf`, callers can compare the
verified signer against the `From` field:

```c
if (mailsmime_result_signed_by_from(result, imf_fields)) {
  /* The signer certificate contains one of the From addresses. */
}
```

## Encrypting

```c
struct mailsmime * smime;
struct mailmime * encrypted_mime;
const char * recipients[] = { "bob@example.com" };
int r;

smime = mailsmime_new();
if (smime == NULL)
  return;

r = mailsmime_add_cert_file(smime, "bob@example.com", "bob-cert.pem");
if (r == MAILSMIME_NO_ERROR) {
  r = mailsmime_encrypt(smime, original_mime, recipients, 1,
      &encrypted_mime);
  if (r == MAILSMIME_NO_ERROR) {
    /* encrypted_mime is an application/pkcs7-mime entity. */
    mailsmime_mime_free(encrypted_mime);
  }
}

mailsmime_free(smime);
```

## Decrypting

```c
struct mailsmime * smime;
struct mailmime * decrypted_mime;
int r;

smime = mailsmime_new();
if (smime == NULL)
  return;

r = mailsmime_set_private_key_file(smime,
    "bob@example.com",
    "bob-cert.pem",
    "bob-key.pem",
    NULL);
if (r == MAILSMIME_NO_ERROR) {
  r = mailsmime_decrypt(smime, encrypted_mime, &decrypted_mime);
  if (r == MAILSMIME_NO_ERROR)
    mailsmime_mime_free(decrypted_mime);
}

mailsmime_free(smime);
```

For encrypted private keys, set a callback before loading the key:

```c
static const char * passphrase_for_email(const char * email, void * context)
{
  (void) email;
  return context;
}

mailsmime_set_passphrase_callback(smime, passphrase_for_email, "secret");
```
