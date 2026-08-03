# Low-Level JMAP Implementation Plan

## Goal

Add JMAP as a low-level protocol client in libEtPan, similar in placement to
`mailimap_*` and `mailactivesync_*`, without adding a `mailsession_driver`,
`mailstorage`, or cached driver in the first phase.

The first target is a reliable `mailjmap_*` API that can discover a JMAP
session, authenticate with bearer tokens, batch JSON method calls, parse JMAP
responses, manage binary upload/download endpoints, and expose the core mail
objects needed by a later high-level driver.

## Specification Sources

Primary sources:

- RFC 8620, "The JSON Meta Application Protocol (JMAP)"
  - Defines service discovery, Session objects, capabilities, method-call
    envelopes, result references, state handling, push, errors, and upload /
    download endpoints.
  - Updated by RFC 9404 and RFC 9670; the low-level implementation should stay
    compatible with RFC 8620 core behavior and only add extension methods when
    explicitly implemented.
  - https://www.rfc-editor.org/info/rfc8620/
- RFC 8621, "The JSON Meta Application Protocol (JMAP) for Mail"
  - Defines `Mailbox`, `Thread`, `Email`, `SearchSnippet`, `Identity`,
    `EmailSubmission`, and `VacationResponse` data models and methods.
  - https://www.rfc-editor.org/info/rfc8621/
- RFC 9404, "JMAP Blob Management Extension"
  - Optional extension family that is not part of this implementation plan
    beyond the current RFC 8620 upload/download endpoint compatibility.
  - https://www.rfc-editor.org/info/rfc9404/
- IANA JMAP registries
  - Track capability URIs, error codes, and data type registrations.
  - https://www.iana.org/assignments/jmap/jmap.xhtml

## Scope

In scope:

- Low-level JMAP session object and lifecycle.
- HTTPS transport abstraction, with a libcurl default implementation.
- `.well-known/jmap` discovery and direct session URL configuration.
- Bearer-token authentication.
- JMAP Session object parsing.
- JSON request envelope creation: `using`, `methodCalls`, call ids, and method
  arguments.
- JSON response envelope parsing: `methodResponses`, `sessionState`,
  per-method errors, top-level problem responses, and result references.
- Core mail methods:
  - `Mailbox/get`, `Mailbox/changes`, `Mailbox/query`, `Mailbox/set`
  - `Thread/get`, `Thread/changes`
  - `Email/get`, `Email/changes`, `Email/query`, `Email/queryChanges`,
    `Email/set`, `Email/copy`, `Email/import`, `Email/parse`
  - `SearchSnippet/get`
  - `Identity/get`
  - `EmailSubmission/set`
- Upload and download helpers for RFC 8620 blob endpoints.
- Unit tests with JSON fixtures, transport fakes, golden request tests, parser
  tests, and fuzz coverage for response parsing.

Out of scope for the first phase:

- libEtPan `mailsession_driver` integration.
- `mailstorage` integration.
- Cached/offline driver behavior.
- OAuth token acquisition or refresh-token flows.
- WebSocket push and EventSource push loops.
- JMAP Calendars, Contacts, MDNs, Sieve, Sharing, and RFC 9404 Blob
  Management methods beyond current upload/download compatibility.
- Provider-specific account setup beyond discovery URL and manual session URL.

## Dependency Strategy

JMAP requires JSON support. Jansson is the selected JSON dependency for the
low-level JMAP JSON layer. It is wired through configure, package metadata, and
the current low-level build as the JMAP JSON provider. The implementation needs
parsing, object/array construction, string escaping, integer handling,
booleans, nulls, duplicate-key rejection, and predictable serialization for
golden tests.

Expose Jansson only through a narrow `mailjmap_json_*` adapter in
`mailjmap_json.h` / `mailjmap_json.c`. JMAP request builders, response parsers,
and typed method helpers must use adapter-owned types such as
`mailjmap_json_value` instead of including Jansson headers directly. This keeps
the public and internal JMAP API insulated from the concrete JSON library and
leaves room to swap the backend later if packaging requirements change.

The adapter boundary is part of the implementation contract: no JMAP file other
than `mailjmap_json.c` should include Jansson headers or call Jansson APIs
directly unless a private adapter implementation header is introduced later.

Do not hand-roll JSON parsing with string searches. JMAP response parsing is
security-sensitive and will receive untrusted server input.

## Proposed Layout

```text
src/low-level/jmap/
  Makefile.am
  mailjmap.h
  mailjmap.c
  mailjmap_types.h
  mailjmap_types.c
  mailjmap_http.h
  mailjmap_http.c
  mailjmap_json.h
  mailjmap_json.c
  mailjmap_request.h
  mailjmap_request.c
  mailjmap_response.h
  mailjmap_response.c
  mailjmap_mail.h
  mailjmap_mail.c
  mailjmap_blob.h
  mailjmap_blob.c
```

Suggested header roles:

- `mailjmap.h`: public session lifecycle and high-level command entry points.
- `mailjmap_types.h`: public structs, enums, list ownership rules, and free
  functions.
- `mailjmap_http.h`: transport interface and libcurl-backed default transport.
- `mailjmap_json.h`: narrow adapter around the selected JSON library.
- `mailjmap_request.h`: internal builders for method-call envelopes.
- `mailjmap_response.h`: internal parsers for envelopes and errors.
- `mailjmap_mail.h`: public or semi-public mail method helpers.
- `mailjmap_blob.h`: upload/download helpers.

Build integration should eventually update:

- `src/low-level/Makefile.am`
- `src/main/libetpan.h`
- `include/Makefile.am`
- `build-spm/include/libetpan/`
- `Package.swift`
- Windows, macOS, Android, and generated public-header build metadata
- `configure.ac` for Jansson and curl feature checks

## Public API Sketch

```c
typedef struct mailjmap mailjmap;

mailjmap * mailjmap_new(int cached, const char * cache_directory);
void mailjmap_free(mailjmap * session);

int mailjmap_set_http_transport(mailjmap * session,
    struct mailjmap_http_transport * transport);

int mailjmap_connect(mailjmap * session, const char * session_url);
int mailjmap_discover(mailjmap * session, const char * domain_or_email);

int mailjmap_login_oauth2(mailjmap * session,
    const char * user, const char * access_token);
int mailjmap_set_oauth2_token(mailjmap * session,
    const char * access_token);

int mailjmap_get_session(mailjmap * session,
    struct mailjmap_session ** result);

int mailjmap_call(mailjmap * session,
    struct mailjmap_request * request,
    struct mailjmap_response ** result);

int mailjmap_mailbox_get(mailjmap * session,
    const char * account_id,
    clist * ids,
    clist * properties,
    struct mailjmap_mailbox_get_result ** result);

int mailjmap_mailbox_changes(mailjmap * session,
    const char * account_id,
    const char * since_state,
    struct mailjmap_changes_result ** result);

int mailjmap_email_query(mailjmap * session,
    struct mailjmap_email_query_request * request,
    struct mailjmap_email_query_result ** result);

int mailjmap_email_get(mailjmap * session,
    struct mailjmap_email_get_request * request,
    struct mailjmap_email_get_result ** result);

int mailjmap_email_set(mailjmap * session,
    struct mailjmap_email_set_request * request,
    struct mailjmap_set_result ** result);

int mailjmap_email_import(mailjmap * session,
    struct mailjmap_email_import_request * request,
    struct mailjmap_set_result ** result);

int mailjmap_email_submission_set(mailjmap * session,
    struct mailjmap_email_submission_set_request * request,
    struct mailjmap_set_result ** result);

int mailjmap_upload(mailjmap * session,
    const char * account_id,
    const char * content_type,
    const char * data,
    size_t data_len,
    struct mailjmap_blob_upload ** result);

int mailjmap_download(mailjmap * session,
    const char * account_id,
    const char * blob_id,
    const char * name,
    const char * accept,
    char ** data,
    size_t * data_len);
```

The API should also expose constructors/free functions for each request and
result type. For complex requests, prefer typed structs over forcing callers to
assemble JSON by hand.

## Data Model

Core session state:

- Session URL, API URL, upload URL, download URL, event source URL.
- Bearer token and login identifier.
- `sessionState`.
- Top-level capabilities and account capabilities.
- Primary account ids by capability.
- Request counter for unique method call ids.
- Last HTTP status, last problem body, and last per-method JMAP error.

Mail structs:

- `mailjmap_mailbox`
  - id, name, parent id, role, sort order, subscription flag, rights, total
    counts, unread counts, myRights.
- `mailjmap_thread`
  - id, email id list.
- `mailjmap_email`
  - id, blob id, thread id, mailbox ids, keywords, size, receivedAt,
    messageId, inReplyTo, references, sender/from/to/cc/bcc/replyTo,
    subject, sentAt, bodyStructure, bodyValues, textBody, htmlBody,
    attachments, preview.
- `mailjmap_email_body_part`
  - part id, blob id, size, headers, name, type, charset, disposition,
    cid, language, location, subparts.
- `mailjmap_identity`
  - id, name, email, replyTo, bcc, textSignature, htmlSignature.
- `mailjmap_email_submission`
  - id, identityId, emailId, envelope, sendAt, undoStatus, deliveryStatus,
    dsnBlobIds.

Represent JSON maps that naturally behave as maps, such as `mailboxIds` and
`keywords`, with `chash` or a thin typed map wrapper rather than parallel
lists.

## HTTP And Discovery

Implement the transport in the same spirit as ActiveSync:

- `mailjmap_http_request`
  - method, URL, headers, body pointer/length, content type, accept type.
- `mailjmap_http_response`
  - status code, headers, body pointer/length, final URL.
- `mailjmap_http_transport`
  - function pointers for perform/free and a context pointer.

Required behavior:

- `mailjmap_discover()` resolves `.well-known/jmap` from a domain or email
  domain and follows HTTPS redirects according to RFC 8620 service discovery.
- `mailjmap_connect()` accepts an already-known session URL.
- `mailjmap_get_session()` performs GET on the session URL with
  `Authorization: Bearer`.
- API calls POST JSON to `apiUrl` with `Content-Type: application/json` and
  `Accept: application/json`.
- Upload POSTs raw bytes to the expanded `uploadUrl`.
- Download GETs the expanded `downloadUrl`.
- Store useful diagnostics for 401/403, 404 discovery failures, 413 upload
  failures, 429 rate limits, and server problem responses.

URL-template expansion for upload/download must implement the RFC 8620
placeholders needed by the session object, including `accountId`, `blobId`,
`type`, and `name`.

## JSON Request And Response Layer

Implement a small internal representation:

```c
struct mailjmap_method_call {
  char * name;
  void * args;
  char * call_id;
  int (* serialize)(void * args, mailjmap_json_value ** result);
};

struct mailjmap_method_response {
  char * name;
  mailjmap_json_value * args;
  char * call_id;
};
```

Required support:

- Build `using` from requested capabilities, always including
  `urn:ietf:params:jmap:core` and adding mail/submission when needed.
- Preserve method response order.
- Parse JMAP method-level `error` responses without discarding successful
  sibling method responses in the same envelope.
- Support result references with `#ids` and paths for common batched flows.
- Validate required fields and gracefully ignore unknown fields.
- Distinguish malformed JSON, valid JSON with protocol errors, HTTP failures,
  and unsupported capabilities in the error model.

## First Implementation Milestones

1. Skeleton and build flags
   - Add `src/low-level/jmap`.
   - Add public and private headers with empty session lifecycle.
   - Integrate autotools and public headers behind JSON/curl feature checks.
   - Add `mailjmap.h` include to `src/main/libetpan.h` only once the public API
     is stable enough to compile across supported platforms.

2. JSON adapter
   - Wire the selected Jansson dependency.
   - Expose Jansson through the existing `mailjmap_json_*` adapter rather than
     directly from request, response, or typed JMAP method code.
   - Implement the `mailjmap_json_*` adapter as the only JMAP-facing JSON API.
   - Keep Jansson includes and ownership rules private to `mailjmap_json.c`
     unless a private implementation header becomes necessary.
   - Add helpers for object lookup, array iteration, string duplication,
     integer parsing with bounds checks, booleans, nulls, and serialization.
   - Add unit tests for invalid JSON, missing fields, duplicate fields where
     relevant, UTF-8 strings, large integers, and object maps.

3. HTTP transport
   - Add fake transport for tests.
   - Add libcurl-backed transport.
   - Support GET, POST, raw upload, download, headers, redirects, timeout
     settings, response capture, and deterministic cleanup.

4. Session discovery and Session parsing
   - Implement `mailjmap_discover()`, `mailjmap_connect()`,
     `mailjmap_login_oauth2()`, and `mailjmap_get_session()`.
   - Parse capabilities, accounts, primaryAccounts, apiUrl, uploadUrl,
     downloadUrl, eventSourceUrl, and sessionState.
   - Add fixtures for Fastmail-style and minimal RFC-style session objects.

5. Request envelope and generic call API
   - Implement `mailjmap_request_new/free/add_call`.
   - Implement `mailjmap_call()` and response/free handling.
   - Add golden tests for single and batched method calls.

6. Mailbox and Thread read methods
   - Implement `Mailbox/get`, `Mailbox/changes`, `Mailbox/query`.
   - Implement `Thread/get` and `Thread/changes`.
   - Validate state and pagination behavior enough for a later driver to sync
     folder lists and thread references.

7. Email read/search methods
   - Implement `Email/query`, `Email/queryChanges`, and `Email/get`.
   - Parse envelopes, body structures, body values, headers, preview, keywords,
     and mailbox membership.
   - Add fixture coverage for multipart messages, missing optional fields,
     large bodies, and unknown vendor properties.

8. Blob upload/download and import
   - Implement `mailjmap_upload()` and `mailjmap_download()`.
   - Implement `Email/import` for raw RFC 5322 messages.
   - Reuse existing MIME test fixtures for import/download round-trip tests.

9. Mutation and send path
   - Implement `Mailbox/set`, `Email/set`, `Email/copy`.
   - Implement `Identity/get`.
   - Implement `EmailSubmission/set` for sending existing/imported emails.
   - Add tests for create/update/destroy result maps and per-id failures.

10. Hardening and compatibility
    - Add response-parser fuzz target.
    - Add request serialization golden files.
    - Run under ASAN/UBSAN where available.
    - Test against at least one live JMAP provider manually with OAuth2 tokens.
    - Document known unsupported extensions and provider quirks.

## Error Model

Add JMAP-specific errors while mapping obvious transport failures to existing
driver-style conventions where possible:

- `MAILJMAP_ERROR_BAD_STATE`
- `MAILJMAP_ERROR_AUTHENTICATION`
- `MAILJMAP_ERROR_DISCOVERY`
- `MAILJMAP_ERROR_HTTP`
- `MAILJMAP_ERROR_JSON_PARSE`
- `MAILJMAP_ERROR_PROTOCOL`
- `MAILJMAP_ERROR_CAPABILITY`
- `MAILJMAP_ERROR_METHOD`
- `MAILJMAP_ERROR_LIMIT`
- `MAILJMAP_ERROR_STREAM`

Keep enough structured detail on the session for callers to inspect:

- HTTP status code.
- Top-level JMAP/problem error type.
- Last failed method name and call id.
- Last JMAP method error type and description.

## Testing Plan

Unit and fixture tests:

- `tests/jmap-json-test.c`
- `tests/jmap-http-test.c`
- `tests/jmap-session-test.c`
- `tests/jmap-request-test.c`
- `tests/jmap-mailbox-test.c`
- `tests/jmap-email-test.c`
- `tests/jmap-blob-test.c`

Fixture directories:

```text
tests/jmap/data/session/
tests/jmap/data/request/
tests/jmap/data/response/
tests/jmap/data/mailbox/
tests/jmap/data/email/
tests/jmap/data/blob/
```

Fuzzing:

- Add `tests/fuzz/fuzz_jmap_response.c`.
- Feed arbitrary JSON bytes into the response parser.
- Seed with real Session, method response, method error, and problem response
  fixtures.

Interop smoke test:

- Add `tests/jmap-sample.c` using environment variables:
  - `JMAP_SESSION_URL`
  - `JMAP_EMAIL`
  - `JMAP_ACCESS_TOKEN`
- Steps:
  - GET Session.
  - Read primary mail account.
  - Fetch mailboxes.
  - Query recent emails.
  - Fetch one email preview/body structure.
  - Optionally send only when an explicit `JMAP_SEND_TO` is provided.

## Ownership And Memory Rules

- Follow existing libEtPan style: explicit `*_new`, `*_free`, and typed list
  free helpers.
- Returned strings and structs are owned by the caller unless documented
  otherwise.
- Session-owned diagnostic strings remain valid until the next operation or
  `mailjmap_free()`.
- Request structs own their nested fields after successful `*_add_*` calls.
- Parser functions must either fully transfer ownership on success or release
  all partial allocations on failure.

## Compatibility Notes

- JMAP ids are opaque strings. Do not convert them to integers.
- JMAP states are opaque strings. Do not compare them semantically.
- Email keyword names are protocol strings, not IMAP flag enums, though later
  driver code can map `$seen`, `$draft`, `$flagged`, `$answered`, and
  `$deleted`.
- Mailboxes may be labels rather than strict folders. Avoid assuming one
  mailbox per email.
- Method batching is core to JMAP; design all method helpers so they can later
  be composed into multi-call requests.
- Unknown properties and capabilities should be preserved where cheap or
  ignored safely, not treated as fatal.

## Open Questions

- Should low-level JMAP be mandatory when curl and JSON are available, or gated
  by `--with-jmap` / `--without-jmap`?
- Should the first public API expose only typed helpers, or also expose a raw
  JSON method-call escape hatch for unimplemented JMAP extensions?
- Should upload/download helpers stream to callbacks/files, or is in-memory
  storage acceptable for phase one?
- Which provider should be the first interop target?

## Suggested Phase Boundary

Phase one is complete when:

- A caller can discover or configure a JMAP Session URL.
- The caller can authenticate with an existing bearer token.
- The caller can fetch and parse the Session object.
- The caller can list mailboxes, query messages, fetch message metadata/body
  structure/body values, upload/import an RFC 5322 message, and submit it.
- The implementation has fake-transport unit coverage, parser fixtures, golden
  request tests, and a fuzz target.

High-level `mailsession_driver`, `mailstorage`, cache/offline behavior, and the
optional extension families listed above are intentionally outside this plan.
