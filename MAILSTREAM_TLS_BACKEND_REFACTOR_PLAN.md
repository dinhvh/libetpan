# Mailstream TLS Backend Refactor Plan

## Goal

Split the provider-specific TLS code currently contained in
`src/data-types/mailstream_ssl.c` into independent OpenSSL and GnuTLS
implementations. `mailstream_ssl.c` will retain the public API, generic
`mailstream_low` integration, backend selection, and dispatch.

The resulting library must be able to compile OpenSSL and GnuTLS into the same
binary. On Apple platforms, CFNetwork remains an independent implementation.
The default preference order is:

1. CFNetwork
2. OpenSSL
3. GnuTLS

Explicit selection through `mailstream_ssl_set_backend()` must override the
default when the requested backend is compiled in.

## Files

### New files

- `src/data-types/mailstream_ssl_backend.h`
  - Private provider interface used only inside libEtPan.
- `src/data-types/mailstream_openssl.c`
  - Complete OpenSSL transport implementation.
- `src/data-types/mailstream_openssl.h`
  - Private OpenSSL factory and lifecycle declarations.
- `src/data-types/mailstream_gnutls.c`
  - Complete GnuTLS transport implementation.
- `src/data-types/mailstream_gnutls.h`
  - Private GnuTLS factory and lifecycle declarations.
- `unittest/mailstream-ssl/backend_selection_test.c`
  - Deterministic selection and dispatch tests using stub backend drivers.

### Existing files to refactor

- `src/data-types/mailstream_ssl.c`
  - Becomes the provider-neutral coordinator and public API implementation.
- `src/data-types/mailstream_ssl.h`
  - Keeps the existing public API and opaque context declaration.
- `src/data-types/mailstream_ssl_private.h`
  - Keeps generic lock/lifecycle declarations only if they remain necessary.
- `src/data-types/mailstream_cfstream.c`
  - Continues to own CFNetwork streams; uses the common availability and
    selection rules.

## Private types

### `struct mailstream_ssl_backend_driver`

Declare this vtable in `mailstream_ssl_backend.h`:

```c
struct mailstream_ssl_backend_driver {
  enum mailstream_ssl_backend backend;
  const char * name;

  int (*global_init)(void);
  void (*global_deinit)(void);
  void (*init_not_required)(void);

  void * (*open)(int fd, int starttls, time_t timeout,
      struct mailstream_ssl_context * context);
  int (*close)(void * backend_data);
  void (*free)(void * backend_data);

  ssize_t (*read)(void * backend_data, void * buffer, size_t count);
  ssize_t (*write)(void * backend_data, const void * buffer, size_t count);
  int (*get_fd)(void * backend_data);

  int (*set_client_certificate_file)(void * backend_context,
      const char * filename);
  int (*set_client_certificate_data)(void * backend_context,
      const unsigned char * der, size_t length);
  int (*set_client_private_key_data)(void * backend_context,
      const unsigned char * der, size_t length);
  int (*set_ca_locations)(void * backend_context,
      const char * ca_file, const char * ca_path);
  int (*set_server_name)(void * backend_context, const char * hostname);

  ssize_t (*copy_peer_certificate)(void * backend_data,
      unsigned char ** der);
  carray * (*copy_peer_certificate_chain)(void * backend_data);
  void * (*get_native_context)(void * backend_context);
};
```

Names and signatures may be adjusted for existing libEtPan ownership
conventions, but the responsibilities should remain separate and explicit.

### `struct mailstream_ssl_context`

Move the provider-specific fields out of the public callback context:

```c
struct mailstream_ssl_context {
  int fd;
  enum mailstream_ssl_backend backend;
  const struct mailstream_ssl_backend_driver * driver;
  void * backend_context;
};
```

### `struct mailstream_ssl_data`

The generic `mailstream_low` state becomes:

```c
struct mailstream_ssl_data {
  int fd;
  time_t timeout;
  struct mailstream_cancel * cancel;
  const struct mailstream_ssl_backend_driver * driver;
  void * backend_data;
};
```

OpenSSL and GnuTLS session fields must not remain in this structure.

## Provider discovery and selection functions

Create these private functions in `mailstream_ssl.c`:

```c
static const struct mailstream_ssl_backend_driver *
mailstream_ssl_backend_driver_for_type(enum mailstream_ssl_backend backend);

static const struct mailstream_ssl_backend_driver *
mailstream_ssl_default_backend_driver(void);

static const struct mailstream_ssl_backend_driver *
mailstream_ssl_selected_backend_driver(void);
```

`mailstream_ssl_default_backend_driver()` applies the preference order. The
CFNetwork path is handled before creating a socket TLS stream. For the socket
provider, it returns OpenSSL when available and GnuTLS otherwise.

Keep these existing public functions, implemented using provider discovery:

```c
int mailstream_ssl_set_backend(enum mailstream_ssl_backend backend);
enum mailstream_ssl_backend mailstream_ssl_get_backend(void);
int mailstream_ssl_backend_is_available(enum mailstream_ssl_backend backend);
```

## Generic dispatch functions in `mailstream_ssl.c`

Keep or create these provider-neutral functions:

```c
static mailstream_low *
mailstream_low_ssl_open_full(int fd, int starttls, time_t timeout,
    void (*callback)(struct mailstream_ssl_context *, void *),
    void * callback_data);

static int mailstream_low_ssl_close(mailstream_low * stream);
static void mailstream_low_ssl_free(mailstream_low * stream);
static ssize_t mailstream_low_ssl_read(mailstream_low * stream,
    void * buffer, size_t count);
static ssize_t mailstream_low_ssl_write(mailstream_low * stream,
    const void * buffer, size_t count);
static int mailstream_low_ssl_get_fd(mailstream_low * stream);
static void mailstream_low_ssl_cancel(mailstream_low * stream);
static struct mailstream_cancel *
mailstream_low_ssl_get_cancel(mailstream_low * stream);
static carray *
mailstream_low_ssl_get_certificate_chain(mailstream_low * stream);
```

These functions must contain no `SSL_*` or `gnutls_*` calls. They dispatch
through `mailstream_ssl_data::driver`.

## OpenSSL implementation

### Private state

Define in `mailstream_openssl.c`:

```c
struct mailstream_openssl_context;
struct mailstream_openssl_data;
```

These own all `SSL_CTX`, `SSL`, `X509`, `EVP_PKEY`, certificate-chain, and
OpenSSL initialization state.

### Functions to create

```c
const struct mailstream_ssl_backend_driver *
mailstream_openssl_backend_driver(void);

static int mailstream_openssl_global_init(void);
static void mailstream_openssl_global_deinit(void);
static void mailstream_openssl_init_not_required_impl(void);

static void * mailstream_openssl_open(int fd, int starttls,
    time_t timeout, struct mailstream_ssl_context * context);
static int mailstream_openssl_close(void * backend_data);
static void mailstream_openssl_free(void * backend_data);
static ssize_t mailstream_openssl_read(void * backend_data,
    void * buffer, size_t count);
static ssize_t mailstream_openssl_write(void * backend_data,
    const void * buffer, size_t count);
static int mailstream_openssl_get_fd(void * backend_data);

static int mailstream_openssl_set_client_certificate_file(
    void * backend_context, const char * filename);
static int mailstream_openssl_set_client_certificate_data(
    void * backend_context, const unsigned char * der, size_t length);
static int mailstream_openssl_set_client_private_key_data(
    void * backend_context, const unsigned char * der, size_t length);
static int mailstream_openssl_set_ca_locations(
    void * backend_context, const char * ca_file, const char * ca_path);
static int mailstream_openssl_set_server_name(
    void * backend_context, const char * hostname);

static ssize_t mailstream_openssl_copy_peer_certificate(
    void * backend_data, unsigned char ** der);
static carray * mailstream_openssl_copy_peer_certificate_chain(
    void * backend_data);
static void * mailstream_openssl_get_native_context(
    void * backend_context);
```

Move the existing OpenSSL client-certificate callback under the name:

```c
static int mailstream_openssl_client_certificate_callback(
    SSL * ssl, X509 ** certificate, EVP_PKEY ** private_key);
```

Compile the real driver only under `HAVE_OPENSSL`. When unavailable,
`mailstream_openssl_backend_driver()` returns `NULL` from a small stub so the
coordinator does not require preprocessor branches.

## GnuTLS implementation

### Private state

Define in `mailstream_gnutls.c`:

```c
struct mailstream_gnutls_context;
struct mailstream_gnutls_data;
```

These own `gnutls_session_t`, credentials, certificates, private keys, and
GnuTLS initialization state.

### Functions to create

```c
const struct mailstream_ssl_backend_driver *
mailstream_gnutls_backend_driver(void);

static int mailstream_gnutls_global_init(void);
static void mailstream_gnutls_global_deinit(void);
static void mailstream_gnutls_init_not_required_impl(void);

static void * mailstream_gnutls_open(int fd, int starttls,
    time_t timeout, struct mailstream_ssl_context * context);
static int mailstream_gnutls_close(void * backend_data);
static void mailstream_gnutls_free(void * backend_data);
static ssize_t mailstream_gnutls_read(void * backend_data,
    void * buffer, size_t count);
static ssize_t mailstream_gnutls_write(void * backend_data,
    const void * buffer, size_t count);
static int mailstream_gnutls_get_fd(void * backend_data);

static int mailstream_gnutls_set_client_certificate_file(
    void * backend_context, const char * filename);
static int mailstream_gnutls_set_client_certificate_data(
    void * backend_context, const unsigned char * der, size_t length);
static int mailstream_gnutls_set_client_private_key_data(
    void * backend_context, const unsigned char * der, size_t length);
static int mailstream_gnutls_set_ca_locations(
    void * backend_context, const char * ca_file, const char * ca_path);
static int mailstream_gnutls_set_server_name(
    void * backend_context, const char * hostname);

static ssize_t mailstream_gnutls_copy_peer_certificate(
    void * backend_data, unsigned char ** der);
static carray * mailstream_gnutls_copy_peer_certificate_chain(
    void * backend_data);
static void * mailstream_gnutls_get_native_context(
    void * backend_context);
```

Rename the existing credential callback to:

```c
static int mailstream_gnutls_client_certificate_callback(
    gnutls_session_t session,
    const gnutls_datum_t * requested_ca_names,
    int requested_ca_count,
    const gnutls_pk_algorithm_t * signature_algorithms,
    int signature_algorithm_count,
    gnutls_retr2_st * result);
```

Compile the real driver only under `HAVE_GNUTLS`. When unavailable,
`mailstream_gnutls_backend_driver()` returns `NULL` from a stub.

## Public compatibility wrappers

The following existing public functions remain in `mailstream_ssl.c` and
dispatch through `mailstream_ssl_context::driver`:

```c
int mailstream_ssl_set_client_certicate(
    struct mailstream_ssl_context * context, char * filename);
int mailstream_ssl_set_client_certificate_data(
    struct mailstream_ssl_context * context,
    unsigned char * der, size_t length);
int mailstream_ssl_set_client_private_key_data(
    struct mailstream_ssl_context * context,
    unsigned char * der, size_t length);
int mailstream_ssl_set_server_certicate(
    struct mailstream_ssl_context * context,
    char * ca_file, char * ca_path);
int mailstream_ssl_set_server_name(
    struct mailstream_ssl_context * context, const char * hostname);
int mailstream_ssl_get_fd(struct mailstream_ssl_context * context);
```

Keep the misspelled `certicate` functions for ABI compatibility. Correctly
spelled aliases can be introduced separately, but removing or renaming the
old symbols is outside this refactor.

Provider-specific compatibility behavior:

```c
void * mailstream_ssl_get_openssl_ssl_ctx(
    struct mailstream_ssl_context * context);
```

This returns the OpenSSL `SSL_CTX *` only when the context uses OpenSSL;
otherwise it returns `NULL`.

Initialization wrappers remain public:

```c
void mailstream_openssl_init_not_required(void);
void mailstream_gnutls_init_not_required(void);
void mailstream_ssl_init_not_required(void);
```

The first two call the corresponding driver hook. The generic function calls
the selected socket provider's hook.

## Callback behavior

Callbacks passed to the existing open functions continue receiving a
`struct mailstream_ssl_context *`. Before invoking the callback:

1. The provider creates its private context.
2. `mailstream_ssl.c` fills the generic context with the provider enum,
   driver, file descriptor, and opaque private pointer.
3. The callback uses existing public setters.
4. Each setter dispatches to the provider-specific implementation.
5. The generic context is destroyed after the provider has copied or retained
   the configured data according to current behavior.

## Error and ownership conventions

- Provider `open` returns `NULL` on failure.
- Setter functions return `0` on success and `-1` when unsupported or failed.
- Provider `read` and `write` preserve existing `ssize_t` behavior.
- `copy_peer_certificate` allocates the DER buffer for the caller exactly as
  the existing API does.
- `copy_peer_certificate_chain` returns a `carray` with the same ownership
  rules as the current `mailstream_low` driver callback.
- `close` releases the TLS session but not the generic allocation.
- `free` releases remaining provider allocation and must tolerate a partially
  initialized object.

## Build-system changes

Add both provider source files to all source manifests. Each file supplies a
stub factory when its dependency is unavailable, allowing unconditional source
registration:

- `src/data-types/Makefile.am`
- `build-mac/libetpan.xcodeproj/project.pbxproj`
- `Package.swift`
- Android build files
- `build-windows/libetpan/libetpan.vcxproj`

Do not expose the private backend headers through the installed public-header
lists.

## Test plan

### Deterministic unit tests

Create stub drivers and test:

- `mailstream_ssl_backend_is_available()` for every compiled combination.
- Default preference: CFNetwork, then OpenSSL, then GnuTLS.
- Explicit backend selection overrides the default.
- Selecting an unavailable backend returns `-1` and preserves the previous
  selection.
- Generic read, write, close, certificate, SNI, and native-context calls reach
  the selected driver exactly once.
- OpenSSL native context access returns `NULL` for a GnuTLS stream.
- Partial provider initialization is cleaned up exactly once.

### Compile matrix

Build these configurations:

1. No TLS provider.
2. `HAVE_OPENSSL` only.
3. `HAVE_GNUTLS` only.
4. `HAVE_OPENSSL` and `HAVE_GNUTLS`.
5. `HAVE_CFNETWORK` only.
6. `HAVE_CFNETWORK` and `HAVE_OPENSSL`.
7. All three providers.

### Platform verification

- macOS framework build and unit tests.
- macOS static-library build.
- iOS device static-library build.
- iOS Simulator static-library build.
- Autotools OpenSSL-only, GnuTLS-only, and dual-provider builds.
- Windows OpenSSL build.

## Migration sequence

1. Add the private driver interface and provider factories returning `NULL`.
2. Add selection tests against stub drivers.
3. Extract OpenSSL code without changing the active-provider behavior.
4. Run OpenSSL and Apple builds.
5. Extract GnuTLS code without changing behavior.
6. Run GnuTLS builds.
7. Convert generic stream operations to driver dispatch.
8. Enable OpenSSL and GnuTLS in the same binary.
9. Enable explicit runtime switching between them.
10. Run the complete compile and platform matrix.

Each extraction should be a separately buildable step. Avoid combining the
physical code move with behavior changes so regressions can be localized.
