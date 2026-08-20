# VoIP Compatibility Simplification Plan

## Goal

Remove deprecated VoIP behavior from libEtPan's internal connection flow without
removing or changing any existing public symbol, function signature, or global
variable.

The existing `voip_enabled` parameters already have no effect in
`mailstream_low_cfstream_open_voip_timeout()`. The refactor will make that fact
explicit: non-VoIP functions become the canonical implementations and existing
VoIP APIs become compatibility wrappers.

## Compatibility requirements

- Preserve all existing exported VoIP functions.
- Preserve every existing VoIP function signature.
- Preserve `mailstream_cfstream_voip_enabled` as an exported global variable.
- Continue accepting both zero and nonzero `voip_enabled` values.
- Do not set the deprecated `kCFStreamNetworkServiceTypeVoIP` property.
- Do not introduce new public VoIP APIs.
- Preserve connection timeouts, TLS configuration, callback behavior, stream
  ownership, descriptor cleanup, and protocol error mappings.

`mailstream_cfstream_voip_enabled` and all `voip_enabled` parameters will remain
available for source and binary compatibility, but their values will not alter
runtime behavior.

## Desired call structure

Canonical internal paths:

```text
protocol API
  -> mailstream_socket_connect_timeout()
     -> mailstream_cfstream_open_timeout()       [CFNetwork]
     -> mail_tcp_connect_timeout()               [socket]

protocol TLS API
  -> mailstream_ssl_connect_timeout()
     -> mailstream_cfstream_open_timeout()       [CFNetwork]
     -> mail_tcp_connect_timeout() + TLS backend [OpenSSL/GnuTLS]
```

Compatibility entry points:

```text
*_voip(..., voip_enabled)
  -> ignore voip_enabled
  -> corresponding canonical non-VoIP function
```

## Step 1: Make CFStream non-VoIP functions canonical

Refactor `src/data-types/mailstream_cfstream.c` so these functions contain or
lead directly to the real implementations:

```c
mailstream * mailstream_cfstream_open(const char * hostname, int16_t port);

mailstream * mailstream_cfstream_open_timeout(const char * hostname,
    int16_t port, time_t timeout);

mailstream_low * mailstream_low_cfstream_open(const char * hostname,
    int16_t port);

mailstream_low * mailstream_low_cfstream_open_timeout(const char * hostname,
    int16_t port, time_t timeout);
```

Reverse the current dependency direction. Non-VoIP functions must no longer
forward to a function whose name contains `voip`.

The low-level canonical function will own:

- CFHost/CFStream creation;
- timeout setup;
- stream scheduling and opening;
- `mailstream_low` allocation;
- cleanup on failure.

## Step 2: Retain CFStream VoIP APIs as wrappers

Keep these exported functions in `mailstream_cfstream.c` and their declarations
in `mailstream_cfstream.h`:

```c
mailstream * mailstream_cfstream_open_voip(const char * hostname,
    int16_t port, int voip_enabled);

mailstream * mailstream_cfstream_open_voip_timeout(const char * hostname,
    int16_t port, int voip_enabled, time_t timeout);

mailstream_low * mailstream_low_cfstream_open_voip(const char * hostname,
    int16_t port, int voip_enabled);

mailstream_low * mailstream_low_cfstream_open_voip_timeout(
    const char * hostname, int16_t port, int voip_enabled, time_t timeout);
```

Implement them as thin compatibility wrappers:

```c
(void) voip_enabled;
return corresponding_non_voip_function(...);
```

Add a concise comment stating that the argument is retained for API
compatibility because the platform VoIP stream service type is deprecated.

## Step 3: Preserve the global flag as an ABI shim

Keep this definition and declaration unchanged:

```c
int mailstream_cfstream_voip_enabled = 0;
```

Stop reading its value inside libEtPan. It remains writable and readable by
applications, but no internal connection decision depends on it.

Do not remove, rename, make `static`, or change the type of the variable.

## Step 4: Simplify the common socket helper

Keep this as the only canonical public connection helper in
`mailstream_socket.c`:

```c
mailstream * mailstream_socket_connect_timeout(const char * server,
    uint16_t port, time_t timeout,
    enum mailstream_socket_connect_error * error);
```

It will call `mailstream_cfstream_open_timeout()` for CFNetwork and use the TCP
socket path otherwise.

Remove the newly introduced `mailstream_socket_connect_voip_timeout()` API if it
has not shipped in a release. It is not needed to preserve any pre-existing API
and would expand the deprecated surface.

If it must be retained for compatibility with an intermediate release, keep it
as a wrapper that ignores `voip_enabled` and calls
`mailstream_socket_connect_timeout()`.

Update both header copies:

- `src/data-types/mailstream_socket.h`
- `build-spm/include/libetpan/mailstream_socket.h`

## Step 5: Simplify the common SSL helper

Keep this as the canonical public TLS connection helper in
`mailstream_ssl.c`:

```c
mailstream * mailstream_ssl_connect_timeout(const char * server,
    uint16_t port, time_t timeout,
    void (* callback)(struct mailstream_ssl_context *, void *),
    void * callback_data, enum mailstream_ssl_connect_error * error);
```

For CFNetwork, call `mailstream_cfstream_open_timeout()` and configure TLS on
the resulting stream. For OpenSSL and GnuTLS, retain the existing TCP and TLS
backend flow.

Remove the newly introduced `mailstream_ssl_connect_voip_timeout()` API if it
has not shipped in a release. If compatibility requires retaining it, make it a
wrapper that ignores `voip_enabled` and calls
`mailstream_ssl_connect_timeout()`.

Update both header copies:

- `src/data-types/mailstream_ssl.h`
- `build-spm/include/libetpan/mailstream_ssl.h`

## Step 6: Convert IMAP VoIP functions into compatibility wrappers

Preserve these existing public IMAP APIs:

```c
int mailimap_socket_connect_voip(mailimap * session,
    const char * server, uint16_t port, int voip_enabled);

int mailimap_ssl_connect_voip(mailimap * session,
    const char * server, uint16_t port, int voip_enabled);

int mailimap_ssl_connect_voip_with_callback(mailimap * session,
    const char * server, uint16_t port, int voip_enabled,
    void (* callback)(struct mailstream_ssl_context *, void *), void * data);
```

Make each function ignore `voip_enabled` and forward to the corresponding
non-VoIP implementation or common helper.

The ordinary IMAP functions must no longer read
`mailstream_cfstream_voip_enabled`:

```c
mailimap_socket_connect(...)
  -> mailstream_socket_connect_timeout(...)

mailimap_ssl_connect(...)
  -> mailstream_ssl_connect_timeout(...)

mailimap_ssl_connect_with_callback(...)
  -> mailstream_ssl_connect_timeout(...)
```

Keep declarations in the source and SwiftPM header trees unchanged.

## Step 7: Remove internal global-flag usage

In `src/driver/interface/mailstorage_tools.c`, replace:

```c
mailstream_cfstream_open_voip(servername, port,
    mailstream_cfstream_voip_enabled)
```

with:

```c
mailstream_cfstream_open(servername, port)
```

Search the complete source tree for remaining reads of:

```text
mailstream_cfstream_voip_enabled
```

Only its public declaration and definition should remain.

## Step 8: Header and build-tree consistency

Keep the checked-in SwiftPM header copies synchronized with their source
headers:

- `mailstream_cfstream.h`
- `mailstream_socket.h`
- `mailstream_ssl.h`
- `mailimap_socket.h`
- `mailimap_ssl.h`

No Xcode source membership changes should be necessary because this refactor
does not add translation units.

## Step 9: Deterministic tests

Add deterministic tests under `unittest/` covering:

1. A nonzero `voip_enabled` value and zero value take the same wrapper path.
2. Existing VoIP functions remain callable and return the canonical function's
   result.
3. `mailstream_cfstream_voip_enabled` remains readable and writable without
   changing backend selection.
4. The socket helper preserves connection-refused versus allocation errors.
5. The SSL helper preserves connection-refused versus TLS errors.
6. IMAP VoIP wrappers preserve callback forwarding.

Use stubs or local fixtures; these tests must not require a network service.

## Step 10: Verification

Run the following checks:

1. Search for obsolete internal routing:

   ```sh
   rg 'mailstream_cfstream_voip_enabled|connect_voip|open_voip' src
   ```

   Results should be limited to compatibility declarations, definitions, and
   wrappers.

2. Confirm no reference to `kCFStreamNetworkServiceTypeVoIP` remains.

3. Run `git diff --check`.

4. Build the macOS framework target.

5. Build the iOS framework/static target.

6. Compile configurations with `HAVE_CFNETWORK=0` to verify that the ignored
   compatibility parameters do not cause warnings or conditional-build errors.

7. Run deterministic unit tests.

8. Inspect exported symbols and confirm that all pre-existing VoIP symbols and
   `mailstream_cfstream_voip_enabled` remain present.

## Expected result

- No internal behavior depends on VoIP mode.
- Existing applications continue to compile and link unchanged.
- Existing binaries can still resolve all VoIP-related symbols.
- The canonical connection paths contain no VoIP parameters.
- Deprecated naming exists only at the public compatibility boundary.
- No new deprecated API surface is introduced.
