# Low-Level JMAP Compatibility Notes

This document describes the compatibility boundary for the first low-level
JMAP implementation in libEtPan. It is meant to document what callers can rely
on today, what is intentionally not implemented yet, and where provider behavior
may require extra handling.

## Implemented Scope

The low-level `mailjmap_*` API currently targets RFC 8620 core JMAP and the
mail methods from RFC 8621 that are needed for a later high-level mail driver.

Implemented session and transport behavior:

- Bearer-token authentication using `mailjmap_login_oauth2()` or
  `mailjmap_set_oauth2_token()`.
- Direct Session URL configuration with `mailjmap_connect()`.
- `.well-known/jmap` discovery with `mailjmap_discover()`.
- Session object parsing for `apiUrl`, `uploadUrl`, `downloadUrl`,
  `eventSourceUrl`, `sessionState`, capabilities, accounts, and
  primary accounts.
- JSON method-call batching through `mailjmap_request_*` and `mailjmap_call()`.
- Result references through `mailjmap_request_arguments_set_result_reference()`.
- Top-level problem response diagnostics and first method-level `error`
  diagnostics on the session.
- Upload and download helpers for RFC 8620 blob endpoints.

JSON dependency and adapter boundary:

- libjansson is the selected JSON dependency for low-level JSON support.
- Jansson is exposed only through the shared `mailjson_*` adapter in
  `src/data-types`.
- Request, response, session, mail, and blob code use `mailjson_value` and
  `mailjson_*` directly rather than depending on Jansson directly.
- `unittest/jmap/jmap-json-test.c` may include Jansson directly when it needs to verify
  adapter behavior against the backend.

Implemented mail methods:

- `Mailbox/get`, `Mailbox/changes`, `Mailbox/query`, `Mailbox/set`
- `Thread/get`, `Thread/changes`
- `Email/query`, `Email/queryChanges`, `Email/changes`, `Email/get`,
  including typed filter-tree helpers, text-filter helpers, and typed helpers
  for common query pagination, sort comparators, total-count, change-limit,
  `upToId`, and thread-collapse options.
- `Email/set`, `Email/copy`, `Email/import`, `Email/parse`
- `SearchSnippet/get`
- `Identity/get`
- `EmailSubmission/set`

Implemented test and diagnostic tools:

- Fake-transport unit coverage for session, call, mailbox, thread, email, blob,
  request, response, HTTP, and JSON behavior.
- Request serialization golden files under `unittest/jmap/data/request/`.
- A libFuzzer harness for `mailjmap_response_parse()` under `tests/fuzz/`.
- `tests/jmap-sample.c` for manual live-provider smoke testing.

## Unsupported Extensions

The first phase does not implement these JMAP extensions or data families:

- JMAP Calendars
- JMAP Contacts
- JMAP MDNs
- JMAP Sieve
- JMAP Sharing
- JMAP WebSocket push or EventSource push loops
- RFC 9404 Blob Management methods beyond current upload/download
  compatibility

The implementation may preserve or ignore unknown capability and object
properties where that is cheap, but callers should not expect typed accessors
for unimplemented extensions.

The public generic request/response layer can batch raw JSON method calls for
extension experiments. `mailjmap_request_add_call_empty()` and
`mailjmap_request_add_string_argument()` cover simple extension probes while
keeping JSON ownership inside the adapter. Prefer typed helpers for stable RFC
8621 mail workflows.

## Known Limitations

- OAuth token acquisition and refresh are outside the low-level JMAP scope.
  Callers must provide a valid bearer token.
- The first phase is not a `mailsession_driver`, `mailstorage`, or cached
  driver. It is a protocol-layer API intended to support those later.
- Upload and download helpers currently operate on in-memory buffers.
- `Email/query` and `Email/queryChanges` expose common text filters, typed
  filter trees, sort comparators, and common pagination/change options through
  typed helpers.
- `SearchSnippet/get` accepts email ids, and exposes both the common text
  filter shape and the same typed filter-tree helpers used by `Email/query`.
- JMAP ids and state strings are opaque and must not be interpreted as numbers
  or ordered values by callers.
- Mailboxes may represent labels rather than strict folders. Callers should not
  assume one mailbox per email.
- JSON parsing is hidden behind `mailjson_*`. As of this snapshot, that adapter
  boundary is stable and the concrete backend is libjansson.
- Typed method helpers map common method-level JMAP `error` types to specific
  low-level errors when possible, while preserving method name, call id, error
  type, and description in session diagnostics.

## Provider Quirks To Watch

The following areas commonly vary between JMAP providers and should be checked
when adding live-provider coverage:

- Service discovery redirects. Providers may redirect `/.well-known/jmap` to a
  hosted Session URL; callers should use the final discovered Session URL.
- Capability sets. Providers may omit submission support even when mail access
  is available, or they may advertise vendor capabilities the typed API ignores.
- Primary account selection. Some accounts may not have a primary mail account;
  fallback to the first account advertising `urn:ietf:params:jmap:mail` is
  useful for diagnostics but should be surfaced to callers.
- Email body fields. Providers may omit optional body values, truncate body
  values, or return vendor properties. Callers should handle absent optional
  fields.
- Rate limiting and upload limits. HTTP 429, JMAP limit problem responses, and
  HTTP 413 uploads are expected operational failures and should be reported to
  callers with session diagnostics.
- Submission flow. Sending requires `urn:ietf:params:jmap:submission`, an
  identity, successful upload/import, and then `EmailSubmission/set`. Some
  providers may require additional account policy outside JMAP.

## Manual Smoke Test

Use `tests/jmap-sample` with environment variables:

```sh
JMAP_SESSION_URL=https://example.com/jmap/session \
JMAP_EMAIL=user@example.com \
JMAP_ACCESS_TOKEN=token \
./tests/jmap-sample
```

Alternatively omit `JMAP_SESSION_URL` and set `JMAP_EMAIL` to use
`.well-known/jmap` discovery.

The sample reads the Session object, finds a mail account, lists mailboxes,
queries recent email ids, and fetches one email. It sends only when
`JMAP_SEND_TO` is explicitly set:

```sh
JMAP_SESSION_URL=https://example.com/jmap/session \
JMAP_EMAIL=user@example.com \
JMAP_ACCESS_TOKEN=token \
JMAP_SEND_TO=recipient@example.com \
./tests/jmap-sample
```

Do not set `JMAP_SEND_TO` during read-only smoke testing.

For Fastmail OAuth testing, `tests/fastmail-jmap-oauth.py` can obtain a bearer
token using authorization-code flow with PKCE:

```sh
./tests/fastmail-jmap-oauth.py user@example.com \
  --client-id=registered-fastmail-client-id \
  --token-json=/tmp/fastmail-jmap-token.json \
  --env-file=/tmp/fastmail-jmap.env \
  --probe-jmap

. /tmp/fastmail-jmap.env
./tests/jmap-live-smoke-test.sh
```

Fastmail OAuth clients must be registered with Fastmail. For personal one-user
testing, a Fastmail JMAP API token can be used directly as `JMAP_ACCESS_TOKEN`;
the HTTP authentication mechanism is the same `Authorization: Bearer ...`
header.

## Local Validation

The local fake-transport suite is expected to pass without live credentials:

```sh
make -C src/low-level/jmap
make -C src
make -C tests check
```

`make -C tests check` runs the local JMAP unit tests and reports
`jmap-live-smoke-test.sh` as skipped when live credentials are absent.

The source distribution should also include the JMAP implementation, public
headers, tests, fixtures, and fuzz harness:

```sh
make distdir distdir=/tmp/libetpan-root-distcheck
```

Additional validation that remains environment-dependent:

- Run `tests/jmap-live-smoke-test.sh` against at least one real JMAP provider
  with an OAuth2 bearer token.
- Run the response parser fuzz harness under libFuzzer.
- Run sanitizer builds where available.
- Build through Apple/Swift Package, Android NDK, and Windows toolchains.
