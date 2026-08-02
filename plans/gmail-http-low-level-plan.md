# Low-Level Gmail HTTP API Implementation Plan

## Goal

Add Gmail's REST API as a low-level protocol client in libEtPan, similar in
placement to `mailimap_*`, `mailactivesync_*`, `mailjmap_*`, and
`mailgraphmail_*` planning work. The first phase should provide a reusable C
API for Gmail HTTP requests, JSON payloads, message/label/history resources,
and Gmail-specific error handling without adding a `mailsession_driver`,
`mailstorage`, or cached driver integration.

The target API should let callers bring their own Google OAuth access token.
OAuth token acquisition, refresh-token handling, browser/device flows, service
account delegation, and credential storage remain the caller's responsibility.

## Provider Notes

Gmail's REST API is an HTTPS JSON API rooted at:

```text
https://gmail.googleapis.com/gmail/v1
```

Most methods address a user path segment:

```text
/users/{userId}/...
```

The special user id `me` means the authenticated user and should be the default.
Message ids, thread ids, label ids, history ids, and page tokens are opaque
strings. The implementation should preserve them exactly and URL-escape them
only when placing them in paths or query strings.

Gmail message content can be returned as structured JSON payload parts or as
base64url-encoded RFC 822 data depending on `messages.get` format. Keep MIME
parsing/building outside the first low-level HTTP layer; expose the raw Gmail
resource fields and let higher layers adapt them to libEtPan's existing MIME
tools.

## Specification Sources

Primary Google documentation:

- Gmail API REST reference
  - Lists users, drafts, history, labels, messages, attachments, settings, and
    threads resources.
  - https://developers.google.com/workspace/gmail/api/reference/rest
- `users.messages.list`
  - `GET /gmail/v1/users/{userId}/messages`
  - Supports `maxResults`, `pageToken`, `q`, `labelIds[]`, and
    `includeSpamTrash`.
  - https://developers.google.com/workspace/gmail/api/reference/rest/v1/users.messages/list
- `users.messages.get`
  - `GET /gmail/v1/users/{userId}/messages/{id}`
  - Supports `format` and `metadataHeaders[]`.
  - https://developers.google.com/workspace/gmail/api/reference/rest/v1/users.messages/get
- `users.messages.modify`
  - `POST /gmail/v1/users/{userId}/messages/{id}/modify`
  - Adds/removes label ids.
  - https://developers.google.com/workspace/gmail/api/reference/rest/v1/users.messages/modify
- `users.messages.send`
  - `POST /gmail/v1/users/{userId}/messages/send`
  - Also supports upload URI variants.
  - https://developers.google.com/workspace/gmail/api/reference/rest/v1/users.messages/send
- `users.labels.list` and `users.labels.create`
  - Label discovery and label management.
  - https://developers.google.com/workspace/gmail/api/reference/rest/v1/users.labels/list
  - https://developers.google.com/workspace/gmail/api/reference/rest/v1/users.labels/create
- `users.history.list`
  - Incremental mailbox changes from a saved `historyId`.
  - https://developers.google.com/workspace/gmail/api/reference/rest/v1/users.history/list

## Scope

In scope for the low-level phase:

- Internal HTTPS transport abstraction for Gmail REST calls.
- Default `NSURLSession` transport on macOS and iOS.
- Default libcurl-backed transport behind existing `HAVE_CURL` on other
  platforms.
- Internal fake transport for tests.
- Bearer-token authentication from a caller-provided access token.
- Configurable user id, defaulting to `me`.
- URL path escaping and query construction.
- JSON request construction and response parsing.
- Gmail error parsing from Google JSON error responses.
- Message listing, paging, fetching, label mutation, trash, untrash, and delete.
- Label listing, fetching, creating, updating/patching, and deleting.
- Attachment fetching.
- Thread listing/fetching and thread label mutation.
- History listing for incremental synchronization.
- Message send/import/insert helpers that accept already-built RFC 822 data.
- Unit tests for request construction, pagination, JSON parsing, error mapping,
  and fake transport behavior.

Out of scope for the low-level phase:

- libEtPan `mailsession_driver` integration.
- `mailstorage` integration.
- Generic cache integration.
- OAuth token acquisition or refresh.
- Google Pub/Sub push notification setup beyond possibly exposing raw
  `watch`/`stop` methods later.
- Gmail settings, filters, forwarding addresses, delegates, CSE, and S/MIME
  management.
- MIME parsing or message composition beyond accepting/returning raw payloads.
- Live integration tests against real Gmail accounts.

## Dependency Strategy

The low-level Gmail API needs JSON parsing and serialization. libEtPan does not
currently appear to expose a general-purpose JSON dependency in the core C
library, so choose the JSON strategy before implementation:

- Preferred: add a small, maintained C JSON dependency through `configure.ac`,
  platform builds, package metadata, and test builds.
- Alternative: vendor a compact parser/writer under `src/data-types` or
  `src/low-level/gmail` if adding a system dependency is unacceptable.

Do not parse Gmail JSON with ad hoc string searches. Gmail responses are nested,
untrusted input and include arrays, nullable fields, encoded MIME bodies, and
provider error objects.

The HTTP transport should be an internal implementation detail. The public
Gmail API should not expose service-root or transport customization in the first
phase. Use Gmail's default service root internally:

```text
https://gmail.googleapis.com/gmail/v1
```

Transport default selection:

- macOS/iOS: use an Objective-C `NSURLSession` backend by default.
- Other platforms: use the libcurl backend by default when `HAVE_CURL` is
  available.
- If no default backend is available, return a transport-unavailable error.
- Tests may use internal-only fake transport hooks.

The non-Apple transport can reuse the existing libcurl discovery in
`configure.ac` and the transport shape used by
`src/low-level/activesync/mailactivesync_http.*`. Consider extracting a generic
low-level HTTP helper only if Gmail, ActiveSync, Graph Mail, and JMAP all
converge on the same needs; otherwise keep the first Gmail implementation local
and boring.

## Proposed Layout

```text
src/low-level/gmail/
  Makefile.am
  mailgmail.h
  mailgmail.c
  mailgmail_types.h
  mailgmail_types.c
  mailgmail_http.h
  mailgmail_http.c
  mailgmail_http_curl.c
  mailgmail_http_nsurlsession.m
  mailgmail_json.h
  mailgmail_json.c
  mailgmail_url.h
  mailgmail_url.c
  mailgmail_messages.h
  mailgmail_messages.c
  mailgmail_labels.h
  mailgmail_labels.c
  mailgmail_threads.h
  mailgmail_threads.c
  mailgmail_history.h
  mailgmail_history.c
```

Suggested header roles:

- `mailgmail.h`: public session lifecycle and top-level includes.
- `mailgmail_types.h`: public structs, enums, ownership rules, and free
  functions.
- `mailgmail_http.h`: internal request/response structs, transport interface,
  and default transport selection.
- `mailgmail_http_curl.c`: internal libcurl transport for non-Apple platforms
  when curl is available.
- `mailgmail_http_nsurlsession.m`: internal Objective-C `NSURLSession`
  transport for macOS/iOS.
- `mailgmail_json.h`: narrow adapter around the selected JSON parser/writer.
- `mailgmail_url.h`: path/query escaping and endpoint construction.
- `mailgmail_messages.h`: message, attachment, send/import/insert APIs.
- `mailgmail_labels.h`: label APIs.
- `mailgmail_threads.h`: thread APIs.
- `mailgmail_history.h`: history/sync APIs.

Build integration should eventually update:

- `src/low-level/Makefile.am`
- `configure.ac`
- `src/main/libetpan.h`
- `include/Makefile.am`, exporting only public Gmail headers and not internal
  HTTP transport headers.
- `build-spm/include/libetpan/`
- `Package.swift`
- Windows, macOS, iOS, Android, and generated public-header build metadata.
  Apple builds need Objective-C compilation and Foundation linkage for the
  `NSURLSession` backend.

## Public API Sketch

```c
typedef struct mailgmail mailgmail;

mailgmail * mailgmail_new(void);
void mailgmail_free(mailgmail * session);

int mailgmail_set_user(mailgmail * session,
    const char * user_id);

int mailgmail_set_oauth2_token(mailgmail * session,
    const char * access_token);

int mailgmail_set_user_agent(mailgmail * session,
    const char * user_agent);

int mailgmail_set_timeout(mailgmail * session, time_t timeout);

int mailgmail_get_profile(mailgmail * session,
    struct mailgmail_profile ** result);

int mailgmail_list_labels(mailgmail * session,
    struct mailgmail_label_list ** result);

int mailgmail_get_label(mailgmail * session,
    const char * label_id,
    struct mailgmail_label ** result);

int mailgmail_create_label(mailgmail * session,
    struct mailgmail_label_create_request * request,
    struct mailgmail_label ** result);

int mailgmail_list_messages(mailgmail * session,
    struct mailgmail_message_list_request * request,
    struct mailgmail_message_list ** result);

int mailgmail_get_message(mailgmail * session,
    const char * message_id,
    struct mailgmail_message_get_request * request,
    struct mailgmail_message ** result);

int mailgmail_get_attachment(mailgmail * session,
    const char * message_id,
    const char * attachment_id,
    struct mailgmail_attachment ** result);

int mailgmail_modify_message(mailgmail * session,
    const char * message_id,
    struct mailgmail_modify_request * request,
    struct mailgmail_message ** result);

int mailgmail_batch_modify_messages(mailgmail * session,
    struct mailgmail_batch_modify_request * request);

int mailgmail_trash_message(mailgmail * session,
    const char * message_id,
    struct mailgmail_message ** result);

int mailgmail_untrash_message(mailgmail * session,
    const char * message_id,
    struct mailgmail_message ** result);

int mailgmail_delete_message(mailgmail * session,
    const char * message_id);

int mailgmail_send_message(mailgmail * session,
    const char * rfc822_data,
    size_t rfc822_data_len,
    struct mailgmail_message ** result);

int mailgmail_list_history(mailgmail * session,
    struct mailgmail_history_list_request * request,
    struct mailgmail_history_list ** result);
```

Every public result/request type should have explicit constructors and free
functions. Complex lists should use existing libEtPan list conventions where
reasonable, but the API should document whether strings and child structs are
owned by the returned object.

## Data Model

Core session state:

- Internal service root, fixed to `https://gmail.googleapis.com/gmail/v1` in
  the public API.
- User id, defaulting to `me`.
- Bearer token.
- User agent.
- Timeout.
- Internal default HTTP transport.

Core result structs:

- Profile:
  - email address
  - messages total
  - threads total
  - history id
- Label:
  - id
  - name
  - type
  - list/message visibility
  - optional total/unread counts when returned by `labels.get`
- Message summary:
  - id
  - thread id
- Message:
  - id
  - thread id
  - label ids
  - snippet
  - history id
  - internal date
  - size estimate
  - raw base64url data when requested
  - payload tree when returned
- Payload part:
  - part id
  - mime type
  - filename
  - headers
  - body size
  - body data or attachment id
  - child parts
- History:
  - id
  - messages added/deleted
  - labels added/removed

## HTTP Layer

Create a Gmail-local internal transport interface similar to ActiveSync:

```c
struct mailgmail_http_header {
  char * name;
  char * value;
};

struct mailgmail_http_request {
  char * method;
  char * url;
  clist * headers;
  unsigned char * body;
  size_t body_len;
  time_t timeout;
};

struct mailgmail_http_response {
  int status_code;
  clist * headers;
  unsigned char * body;
  size_t body_len;
};

struct mailgmail_http_transport {
  void * context;
  int (*perform)(struct mailgmail_http_transport * transport,
      struct mailgmail_http_request * request,
      struct mailgmail_http_response ** response);
  void (*free)(struct mailgmail_http_transport * transport);
};
```

The Gmail command layer should add common headers:

- `Authorization: Bearer <access token>`
- `Accept: application/json`
- `Content-Type: application/json` for JSON bodies
- `User-Agent` when configured

Debug logging:

- Use `LIBETPAN_GMAIL_HTTP_DEBUG=1`.
- Redact `Authorization`.
- For non-2xx responses, log status and a short body preview.
- Avoid dumping full message bodies by default.

### Apple Transport

On macOS and iOS, the default transport should be implemented with
`NSURLSession` in an Objective-C source file while exposing only internal C
entry points to the Gmail module.

Implementation requirements:

- Convert `mailgmail_http_request` to `NSMutableURLRequest`.
- Copy method, URL, timeout, headers, and optional body exactly.
- Use `NSURLSession`/`NSURLSessionDataTask` and bridge completion back to the
  synchronous low-level C call shape.
- Convert `NSHTTPURLResponse` status and headers into
  `mailgmail_http_response`.
- Copy `NSData` response bytes into the C response body.
- Use platform TLS and trust behavior.
- Do not expose an `NSURLSession` constructor in public libEtPan headers.

The Objective-C backend should be selected by build configuration for Apple
targets. If a project embeds libEtPan in an app that needs custom network
policy, that can be revisited later; the first API should stay smaller.

### Curl Transport

On non-Apple platforms, use a libcurl backend by default when `HAVE_CURL` is
defined. Keep its constructor internal to the Gmail module unless a later
requirement proves public transport customization is needed.

## Error Handling

Map failures in layers:

- Local bad state, invalid arguments, allocation failure.
- Transport unavailable, `NSURLSession` failure, curl failure, TLS failure,
  timeout.
- HTTP status:
  - `400`: invalid request
  - `401`: authentication failure
  - `403`: permission/rate/quota failure
  - `404`: not found
  - `409`: conflict
  - `429`: rate limited
  - `5xx`: server failure
- Gmail JSON error body:
  - top-level status code
  - message
  - reason/domain metadata where present

Expose the original HTTP status and parsed Gmail error details on the session or
through a `mailgmail_error` result helper so callers can decide whether to
refresh tokens, retry later, or surface a provider message.

## URL and Encoding Rules

- Centralize endpoint construction in `mailgmail_url.*`.
- Path-escape user ids, message ids, label ids, thread ids, and attachment ids.
- Query-escape `q`, `pageToken`, `labelIds[]`, `metadataHeaders[]`, and other
  parameters.
- Preserve server-provided `nextPageToken` and `historyId` exactly.
- Encode outgoing raw RFC 822 data as base64url for JSON `raw` message payloads.
- Decode base64url for attachment `data` and raw message output only when the
  API explicitly promises decoded data; otherwise expose encoded data and let
  callers choose.

## Milestones

### 1. Foundation

- Add `src/low-level/gmail` skeleton and build wiring.
- Add session lifecycle, user/token setters, and timeout/user-agent setters.
- Keep Gmail service root internal and fixed to the default Gmail API root.
- Add internal HTTP request/response structs and fake transport support.
- Add `NSURLSession` transport for macOS/iOS.
- Add libcurl transport behind `HAVE_CURL` for non-Apple platforms.
- Add default transport selection.
- Add Gmail debug logging with token redaction.
- Add URL and query builder tests.

### 2. JSON and Error Layer

- Select and integrate JSON parser/writer.
- Add JSON adapter helpers for strings, arrays, integers, booleans, nulls, and
  object lookup.
- Add Gmail error parser.
- Add HTTP status/error mapping tests.

### 3. Read-Only Mailbox APIs

- Implement `mailgmail_get_profile`.
- Implement `mailgmail_list_labels` and `mailgmail_get_label`.
- Implement `mailgmail_list_messages`.
- Implement `mailgmail_get_message`.
- Implement `mailgmail_get_attachment`.
- Test pagination, label filters, metadata headers, raw format, full format,
  and malformed responses.

### 4. Mutation APIs

- Implement message modify and batch modify.
- Implement trash, untrash, delete, and batch delete.
- Implement label create, patch/update, and delete.
- Test request JSON bodies and expected status handling.

### 5. Send and Import APIs

- Implement message send with JSON `raw`.
- Implement insert/import with JSON `raw`.
- Decide whether upload URI support belongs in this phase or a follow-up.
- Test base64url encoding, empty/invalid message handling, and error responses.

### 6. Threads and History

- Implement thread list/get/modify/trash/untrash/delete.
- Implement history list.
- Preserve `nextPageToken` and `historyId` exactly.
- Test incremental sync fixture parsing.

### 7. Packaging and Platform Follow-Through

- Update aggregate public headers.
- Update Swift package and generated header snapshots if required.
- Update Android build file if the low-level Gmail module should be available
  there.
- Add documentation snippets for the low-level API.

## Testing Strategy

Unit tests should use a fake HTTP transport that records the outgoing request
and returns fixture responses. Cover:

- Required authorization and accept headers.
- Token redaction in debug output if practical.
- Path and query escaping.
- `messages.list` pagination.
- `messages.get` formats: `minimal`, `metadata`, `full`, and `raw`.
- Payload tree parsing with nested MIME parts.
- Attachment parsing.
- Label list and create/update request bodies.
- Message modify and batch modify request bodies.
- History parsing for messages and label changes.
- Gmail JSON error responses.
- Malformed JSON, missing required fields, empty bodies, and non-JSON error
  bodies.
- Transport-unavailable behavior when neither the Apple backend nor curl backend
  is compiled.

Live Gmail tests should be manual or opt-in only. Do not make the normal test
suite depend on network access or real account credentials.

## Suggested First Implementation Slice

Start with a narrow but complete read-only vertical slice:

- `mailgmail_new/free`
- token/user setters
- internal fake HTTP transport
- `NSURLSession` transport on macOS/iOS
- libcurl transport on other platforms when available
- URL/query builder
- JSON adapter and Gmail error parser
- `mailgmail_get_profile`
- `mailgmail_list_labels`
- `mailgmail_list_messages`
- `mailgmail_get_message`
- fixture-based unit tests

This proves authentication headers, default transport selection, URL building,
JSON parsing, ownership rules, pagination, and error handling before mutation
and MIME upload paths expand the surface area.
