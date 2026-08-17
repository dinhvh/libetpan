# S/MIME Runtime Backend Plan

## Goal

Allow one libetpan build to contain both the Apple Security and OpenSSL S/MIME
implementations and select the implementation for each `mailsmime` context at
runtime. This lets the same XCTest bundle exercise both backends without
changing build configurations.

Existing callers of `mailsmime_new()` must remain source- and behavior-compatible.

## Public API

Add a backend enum and an explicit constructor:

```c
enum mailsmime_backend {
  MAILSMIME_BACKEND_DEFAULT,
  MAILSMIME_BACKEND_APPLE,
  MAILSMIME_BACKEND_OPENSSL
};

struct mailsmime * mailsmime_new_with_backend(enum mailsmime_backend backend);
```

Keep the existing constructor as a compatibility wrapper:

```c
struct mailsmime * mailsmime_new(void)
{
  return mailsmime_new_with_backend(MAILSMIME_BACKEND_DEFAULT);
}
```

`MAILSMIME_BACKEND_DEFAULT` should preserve the existing platform choice:
prefer Apple Security when `USE_SMIME_APPLE` is available, otherwise use
OpenSSL. Requesting a backend that was not compiled in should fail cleanly and
return `NULL`.

## Internal Design

Store the selected backend in `struct mailsmime`. Compile both implementations
when both `USE_SMIME_APPLE` and `USE_SMIME_OPENSSL` are defined, but dispatch
operations using the context's runtime backend.

Rename colliding backend-specific fields before enabling both macros. For
example:

```c
struct mailsmime_key_entry {
  /* Shared fields. */

#ifdef USE_SMIME_APPLE
  SecCertificateRef apple_cert;
  SecIdentityRef apple_identity;
#endif

#ifdef USE_SMIME_OPENSSL
  X509 * openssl_cert;
  EVP_PKEY * openssl_key;
#endif
};
```

Apply the same naming rule to certificate entries, result certificates, trust
stores, keychains, and other backend-owned state.

Extract the current preprocessor branches into backend-specific functions, then
make the public functions dispatch on `smime->backend`. The affected areas are:

- context initialization and cleanup;
- trusted and recipient certificate loading;
- private-key loading and passphrase callbacks;
- signing and signature verification;
- encryption and decryption;
- signer certificate metadata;
- certificate PEM export.

Compile-time guards should determine whether a backend exists. They should no
longer choose the operation whenever both backends are compiled.

## Implementation Sequence

1. Rename all Apple and OpenSSL fields so a translation unit builds with both
   feature macros enabled.
2. Add the public backend enum, `mailsmime_new_with_backend()`, and the backend
   member on `struct mailsmime`.
3. Implement default-backend resolution and reject unavailable explicit
   backends.
4. Split initialization and cleanup into Apple and OpenSSL helpers.
5. Extract and dispatch certificate and private-key management.
6. Extract and dispatch signing and verification.
7. Extract and dispatch encryption and decryption.
8. Extract and dispatch certificate inspection and export.
9. Audit all `#ifdef USE_SMIME_*` blocks to ensure defining both macros never
   silently selects the first branch.
10. Update documentation and public-header exports.

Keep each step buildable. Avoid changing cryptographic behavior while moving
code; backend-specific fixes should be separate changes where practical.

## Test Refactoring

Parameterize the shared deterministic tests by `enum mailsmime_backend` and use
`mailsmime_new_with_backend()` for every context created by a test. Fixture
generation remains backend-specific:

- Apple Security uses PKCS#12 private-key fixtures;
- OpenSSL uses PEM private-key fixtures.

Expose XCTest cases for both implementations:

- Apple crypto round trip;
- Apple passphrase callback;
- OpenSSL crypto round trip;
- OpenSSL passphrase callback.

The round-trip test should continue to cover signing, trusted and untrusted
verification, tamper detection, signer metadata, certificate export,
single-recipient encryption, wrong-key rejection, and multi-recipient
encryption.

Build the library and focused XCTest target with both macros:

```text
USE_SMIME_APPLE=1
USE_SMIME_OPENSSL=1
```

Link both Security.framework and the bundled OpenSSL XCFramework. Run the
existing XCTest target from the workspace with a focused `-only-testing`
selection. A separate S/MIME target is only necessary if the workspace target
can no longer build independently of unrelated tests.

## Verification Matrix

| Build | Requested backend | Expected result |
| --- | --- | --- |
| Apple only | default / Apple | Apple tests pass |
| Apple only | OpenSSL | constructor rejects backend |
| OpenSSL only | default / OpenSSL | OpenSSL tests pass |
| OpenSSL only | Apple | constructor rejects backend |
| Both | default | documented default is selected |
| Both | Apple | Apple tests pass |
| Both | OpenSSL | OpenSSL tests pass |

Also verify that existing code using only `mailsmime_new()` builds unchanged.

## Risks and Constraints

- Apple keychain search-list changes are process-global. Keep them serialized
  and scoped to the Apple operation; OpenSSL contexts must not participate.
- Backend-owned objects must be freed only by their matching backend.
- Error codes should remain consistent between implementations where the public
  API describes the same failure.
- Defining both feature macros must not produce duplicate struct members or
  select an implementation through `#ifdef` ordering.
- Tests must use unique certificate serial numbers and isolated keychain state
  so repeated and parallel runs remain deterministic.

## Completion Criteria

- One macOS build contains both implementations.
- Callers can select Apple or OpenSSL independently for each context.
- Existing `mailsmime_new()` callers retain current behavior.
- All four focused XCTest cases pass in one test invocation.
- Single-backend builds continue to compile and pass their applicable tests.
- No test identity remains in the user's login keychain after execution.
