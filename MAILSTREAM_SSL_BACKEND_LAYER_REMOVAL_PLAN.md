# Mailstream SSL Backend-Layer Removal Plan

## Goal

Remove `struct mailstream_ssl_backend_driver` and its vtable dispatch layer.
Keep one `mailstream_low_driver` for OpenSSL and one for GnuTLS, use direct
provider functions from `mailstream_ssl.c`, and extract genuinely shared socket,
timeout, cancellation, and stream-allocation code into common helpers.

The resulting structure will be:

```text
mailstream_ssl.c
  public API
  backend selection
  direct OpenSSL/GnuTLS dispatch

mailstream_ssl_internal.c
  shared fd, timeout, readiness, cancellation, and wrapping helpers

mailstream_openssl.c
  OpenSSL state and operations
  one OpenSSL mailstream_low_driver

mailstream_gnutls.c
  GnuTLS state and operations
  one GnuTLS mailstream_low_driver
```

CFNetwork remains a separate Apple stream implementation and is not represented
by either socket TLS low driver.

## Compatibility requirements

- Preserve every existing public function and signature in
  `mailstream_ssl.h`.
- Preserve the values of `enum mailstream_ssl_backend`.
- Preserve backend preference: CFNetwork, then OpenSSL, then GnuTLS.
- Preserve explicit selection through `mailstream_ssl_set_backend()`.
- Preserve callback timing and callback context behavior.
- Preserve SNI, client-certificate, CA, and private-key configuration.
- Preserve certificate retrieval and native OpenSSL context access.
- Preserve STARTTLS ownership: replacing a low stream must not close the shared
  descriptor prematurely.
- Preserve timeout, cancellation, descriptor cleanup, and error mappings.
- Continue supporting OpenSSL-only, GnuTLS-only, and combined builds.
- Do not expose provider-private helpers as public API.

## Files

### Add

- `src/data-types/mailstream_ssl_internal.c`
- `src/data-types/mailstream_ssl_internal.h`
- `unittest/mailstream-ssl/provider_dispatch_test.c`
- `unittest/mailstream-ssl/common_transport_test.c`

### Refactor

- `src/data-types/mailstream_ssl.c`
- `src/data-types/mailstream_openssl.c`
- `src/data-types/mailstream_openssl.h`
- `src/data-types/mailstream_gnutls.c`
- `src/data-types/mailstream_gnutls.h`
- `src/data-types/Makefile.am`
- `Package.swift`
- `build-android/jni/Android.mk`
- `build-windows/libetpan/libetpan.vcxproj`
- `build-mac/libetpan.xcodeproj/project.pbxproj`
- `tests/Makefile.am`

### Remove

- `src/data-types/mailstream_ssl_backend.h`

Remove its build-system and Xcode project references as part of the same change.

## Step 1: Inventory current backend-driver responsibilities

Before editing, map each `mailstream_ssl_backend_driver` field to its direct
replacement:

| Existing field | Direct replacement |
|---|---|
| `backend` | compile-time provider case in `mailstream_ssl.c` |
| `name` | remove unless diagnostics use it |
| `low_driver` | `mailstream_openssl_low_driver()` or `mailstream_gnutls_low_driver()` |
| `open_low` | provider `*_open_low()` function |
| `copy_certificate` | provider `*_copy_certificate()` function |
| certificate setters | direct provider context functions |
| `set_server_name` | direct provider context function |
| `get_native_context` | OpenSSL-only direct function |
| `get_context_fd` | direct provider context function |
| initialization hooks | direct provider lifecycle functions |

Verify that no caller outside `src/data-types/` includes
`mailstream_ssl_backend.h`.

## Step 2: Define the common private context

Keep the callback context provider-neutral in `mailstream_ssl.c` or a private
header:

```c
struct mailstream_ssl_context {
  enum mailstream_ssl_backend backend;
  void * provider_context;
};
```

Remove the `driver` pointer.

Create a provider-neutral callback envelope:

```c
struct mailstream_ssl_callback_data {
  enum mailstream_ssl_backend backend;
  const char * server_name;
  void (* callback)(struct mailstream_ssl_context *, void *);
  void * callback_data;
};
```

The coordinator creates the public context from the backend enum and opaque
provider context. Provider files must not define a conflicting public callback
context layout.

## Step 3: Define direct OpenSSL private functions

Declare these in `mailstream_openssl.h`, guarded by `HAVE_OPENSSL`:

```c
mailstream_low_driver * mailstream_openssl_low_driver(void);

mailstream_low * mailstream_openssl_open_low(int fd, int starttls,
    time_t timeout,
    void (* callback)(void * provider_context, void * data), void * data);

ssize_t mailstream_openssl_copy_certificate(mailstream * stream,
    unsigned char ** der);

int mailstream_openssl_set_client_certificate_file(void * context,
    char * filename);
int mailstream_openssl_set_client_certificate_data(void * context,
    unsigned char * der, size_t length);
int mailstream_openssl_set_client_private_key_data(void * context,
    unsigned char * der, size_t length);
int mailstream_openssl_set_server_certificate(void * context,
    char * ca_file, char * ca_path);
int mailstream_openssl_set_server_name(void * context,
    const char * hostname);
void * mailstream_openssl_get_native_context(void * context);
int mailstream_openssl_get_context_fd(void * context);

void mailstream_openssl_init_not_required_impl(void);
void mailstream_openssl_init_lock(void);
void mailstream_openssl_uninit_lock(void);
```

Use exact provider types internally, but expose `void *` only at the private
coordinator boundary where necessary.

Rename or wrap existing provider functions directly. Remove all casts that were
needed only to initialize `mailstream_ssl_backend_driver`.

## Step 4: Define direct GnuTLS private functions

Declare corresponding functions in `mailstream_gnutls.h`, guarded by
`HAVE_GNUTLS`:

```c
mailstream_low_driver * mailstream_gnutls_low_driver(void);

mailstream_low * mailstream_gnutls_open_low(int fd, int starttls,
    time_t timeout,
    void (* callback)(void * provider_context, void * data), void * data);

ssize_t mailstream_gnutls_copy_certificate(mailstream * stream,
    unsigned char ** der);

int mailstream_gnutls_set_client_certificate_file(void * context,
    char * filename);
int mailstream_gnutls_set_client_certificate_data(void * context,
    unsigned char * der, size_t length);
int mailstream_gnutls_set_client_private_key_data(void * context,
    unsigned char * der, size_t length);
int mailstream_gnutls_set_server_certificate(void * context,
    char * ca_file, char * ca_path);
int mailstream_gnutls_set_server_name(void * context,
    const char * hostname);
int mailstream_gnutls_get_context_fd(void * context);

void mailstream_gnutls_init_not_required_impl(void);
void mailstream_gnutls_init_lock(void);
void mailstream_gnutls_uninit_lock(void);
```

GnuTLS has no OpenSSL native context function. The public OpenSSL accessor must
return `NULL` for a GnuTLS context in `mailstream_ssl.c`.

## Step 5: Create shared transport helpers

Create `mailstream_ssl_internal.h` as a private header. Start with helpers whose
behavior is demonstrably identical in both provider files:

```c
int mailstream_ssl_internal_prepare_fd(int fd);
void mailstream_ssl_internal_close_fd(int fd);

int mailstream_ssl_internal_wait_read(int fd,
    struct mailstream_cancel * cancel, time_t timeout);
int mailstream_ssl_internal_wait_write(int fd,
    struct mailstream_cancel * cancel, time_t timeout);

struct mailstream_cancel * mailstream_ssl_internal_cancel_new(void);
void mailstream_ssl_internal_cancel_free(struct mailstream_cancel * cancel);
void mailstream_ssl_internal_cancel(struct mailstream_cancel * cancel);

mailstream * mailstream_ssl_internal_wrap_low(mailstream_low * low);
```

The header must remain private to `src/data-types/`: do not copy it into
`include/libetpan`, install it, expose it through SwiftPM public headers, or use
`LIBETPAN_EXPORT` on these functions. Where a helper is used by only one
translation unit, prefer a `static` function instead of adding it to the
internal header.

Adjust signatures to existing ownership conventions. In particular:

- distinguish timeout, cancellation, readiness, and system error if providers
  need different retry behavior;
- do not hide OpenSSL `SSL_get_error()` decisions in common code;
- do not hide GnuTLS retry/error classification in common code;
- do not close an fd owned by another low stream during STARTTLS replacement;
- keep Windows event handling inside the common implementation where possible.

Only extract code after confirming byte-for-byte or behavior-level equivalence.
Provider-specific handshake and record semantics must stay in provider files.

## Step 6: Simplify each provider low driver

Keep exactly one static `mailstream_low_driver` per provider:

```c
static mailstream_low_driver mailstream_openssl_driver = { ... };
static mailstream_low_driver mailstream_gnutls_driver = { ... };
```

Expose accessors rather than writable global driver pointers:

```c
mailstream_low_driver * mailstream_openssl_low_driver(void)
{
  return &mailstream_openssl_driver;
}
```

Do the same for GnuTLS.

Rename provider-local generic names so ownership is clear:

```c
mailstream_openssl_low_read
mailstream_openssl_low_write
mailstream_openssl_low_close
mailstream_openssl_low_free
mailstream_openssl_low_get_fd
mailstream_openssl_low_cancel

mailstream_gnutls_low_read
mailstream_gnutls_low_write
mailstream_gnutls_low_close
mailstream_gnutls_low_free
mailstream_gnutls_low_get_fd
mailstream_gnutls_low_cancel
```

Remove convenience functions duplicated inside providers when only the
coordinator used them. The provider boundary should expose the smallest direct
API needed by `mailstream_ssl.c`.

## Step 7: Replace vtable selection with enum selection

Remove these functions from `mailstream_ssl.c`:

```c
mailstream_ssl_backend_driver_for_type()
mailstream_ssl_default_socket_backend_driver()
mailstream_ssl_selected_socket_backend_driver()
mailstream_ssl_driver_for_stream()
```

Replace them with enum helpers:

```c
static enum mailstream_ssl_backend mailstream_ssl_default_socket_backend(void);
static enum mailstream_ssl_backend mailstream_ssl_selected_socket_backend(void);
static enum mailstream_ssl_backend mailstream_ssl_backend_for_stream(
    mailstream * stream);
```

`mailstream_ssl_backend_for_stream()` identifies the provider by comparing the
low driver's address with:

```c
mailstream_openssl_low_driver()
mailstream_gnutls_low_driver()
```

Use compile-time guarded switches for direct calls.

## Step 8: Implement direct open dispatch

Refactor `mailstream_low_ssl_open_full()`:

```c
switch (mailstream_ssl_selected_socket_backend()) {
#ifdef HAVE_OPENSSL
case MAILSTREAM_SSL_BACKEND_OPENSSL:
  return mailstream_openssl_open_low(fd, starttls, timeout,
      callback_bridge, &callback_data);
#endif

#ifdef HAVE_GNUTLS
case MAILSTREAM_SSL_BACKEND_GNUTLS:
  return mailstream_gnutls_open_low(fd, starttls, timeout,
      callback_bridge, &callback_data);
#endif

default:
  return NULL;
}
```

Use provider-specific callback bridge functions if that avoids unsafe
function-pointer casts.

The callback bridge must:

1. create the public `mailstream_ssl_context`;
2. record the backend enum;
3. apply the requested server name before the user callback when required;
4. invoke the user callback;
5. preserve the provider context lifetime rules.

## Step 9: Implement direct public-context dispatch

Replace each backend vtable call with a switch on `context->backend`.

Functions to migrate:

```c
mailstream_ssl_set_client_certicate()
mailstream_ssl_set_client_certificate_data()
mailstream_ssl_set_client_private_key_data()
mailstream_ssl_set_server_certicate()
mailstream_ssl_set_server_name()
mailstream_ssl_get_openssl_ssl_ctx()
mailstream_ssl_get_fd()
```

Return the same unsupported-provider values as today:

- setters: `-1`;
- native OpenSSL context: `NULL`;
- fd getter: `-1`.

Do not duplicate provider certificate parsing or TLS operations in the
coordinator.

## Step 10: Implement direct stream dispatch

Refactor `mailstream_ssl_get_certificate()`:

```c
switch (mailstream_ssl_backend_for_stream(stream)) {
#ifdef HAVE_OPENSSL
case MAILSTREAM_SSL_BACKEND_OPENSSL:
  return mailstream_openssl_copy_certificate(stream, cert_der);
#endif
#ifdef HAVE_GNUTLS
case MAILSTREAM_SSL_BACKEND_GNUTLS:
  return mailstream_gnutls_copy_certificate(stream, cert_der);
#endif
default:
  return -1;
}
```

Certificate-chain access through the low driver remains provider-owned unless a
common representation can be extracted without changing ownership.

## Step 11: Implement direct lifecycle dispatch

Refactor:

```c
mailstream_openssl_init_not_required()
mailstream_gnutls_init_not_required()
mailstream_ssl_init_not_required()
mailstream_ssl_init_lock()
mailstream_ssl_uninit_lock()
```

Provider-specific public compatibility functions call their provider directly.
Generic lifecycle functions switch on the selected socket backend.

Avoid initializing both providers unless existing semantics require it.

## Step 12: Remove the backend layer

After all callers use direct functions:

1. delete `mailstream_ssl_backend.h`;
2. remove all `struct mailstream_ssl_backend_driver` declarations;
3. remove both provider backend-driver objects and factory functions;
4. remove all `.low_driver` field mutation;
5. remove obsolete casts and adapter functions;
6. verify this search returns no results:

   ```sh
   rg 'mailstream_ssl_backend_driver|backend_driver\(' src
   ```

## Step 13: Update build systems

Add `mailstream_ssl_internal.c` and `.h` and remove
`mailstream_ssl_backend.h` from:

- Autotools source lists;
- SwiftPM sources;
- Android makefiles;
- Windows project files;
- every Xcode library target;
- Xcode data-types file groups.

Run `plutil -lint` on modified Xcode project files.

## Step 14: Tests

Add deterministic tests under `unittest/mailstream-ssl/`.

### Provider dispatch test

Test compile-time combinations and explicit selection:

- OpenSSL only;
- GnuTLS only;
- both providers;
- invalid or unavailable selection;
- default OpenSSL-before-GnuTLS order;
- CFNetwork remains preferred at the connection layer.

Verify direct context dispatch reaches the expected provider stub without a
network connection.

### Common transport test

Use local socket pairs, cancellation objects, or stubs to test:

- read readiness;
- write readiness;
- timeout;
- cancellation;
- fd cleanup on allocation failure where practical;
- no double close during STARTTLS-style ownership transfer.

These tests belong in `unittest/` and must not use external services.

### Existing compatibility tests

Keep and rerun:

- backend selection test;
- VoIP compatibility test;
- certificate-related deterministic tests, if present.

## Step 15: Verification matrix

Run:

1. `git diff --check`.
2. Search for the removed abstraction.
3. Compile `mailstream_ssl.c` with neither socket provider enabled.
4. Compile OpenSSL-only.
5. Compile GnuTLS-only.
6. Compile with both OpenSSL and GnuTLS enabled.
7. Build the macOS framework.
8. Build the iOS static target.
9. Build or syntax-check Android sources.
10. Validate the Windows project includes the common source.
11. Run deterministic unit tests.
12. Inspect framework exports and confirm no new provider-private symbols are
    exported.
13. Confirm all existing public `mailstream_ssl_*` symbols remain exported.

## Expected result

- `mailstream_ssl_backend_driver` no longer exists.
- OpenSSL owns one `mailstream_low_driver`.
- GnuTLS owns one `mailstream_low_driver`.
- `mailstream_ssl.c` performs small, explicit enum-based dispatch.
- Shared transport mechanics exist once in `mailstream_ssl_internal.c`.
- Provider files contain only provider state, TLS operations, and thin low-driver
  integration.
- Function-pointer casts used by the removed vtable are gone.
- Public API and runtime backend-selection behavior remain compatible.
