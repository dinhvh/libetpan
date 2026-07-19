# ActiveSync HTTP Command Plan

## Goal

Implement the HTTP layer and command-level ActiveSync primitives on top of the
WBXML codec described in `plans/activesync-wbxml-plan.md`.

This plan starts after the WBXML layer can:

- build command payload trees;
- encode those trees to WBXML bytes;
- decode response WBXML bytes into nodes;
- expose code-page/token names for command parsers and tests.

The HTTP layer should not know how to hand-encode WBXML tokens. It should call
WBXML command builders and response parsers.

## Specification Sources

Use public Microsoft Open Specifications:

- `[MS-ASHTTP] Exchange ActiveSync: HTTP Protocol`
  - HTTP methods, endpoint path, query parameters, headers, protocol versions,
    redirect behavior, status codes, and request/response framing.
  - https://learn.microsoft.com/en-us/openspecs/exchange_server_protocols/ms-ashttp/bc92056f-5c48-4775-9f0d-b16b86998e55
- `[MS-ASCMD] Exchange ActiveSync: Command Reference Protocol`
  - Command XML shapes and command-specific status values.
  - https://learn.microsoft.com/en-us/openspecs/exchange_server_protocols/ms-ascmd/1a3490f1-afe1-418a-aa92-6f630036d65a
- `[MS-ASWBXML] ActiveSync WBXML Algorithm Details`
  - WBXML payload encoding/decoding.
  - https://learn.microsoft.com/en-us/openspecs/exchange_server_protocols/ms-aswbxml/52e4f9d1-2b43-48a8-82cc-85d674858351

## Dependency Decision

Use libcurl as the required HTTP backend for ActiveSync.

The rest of libEtPan can keep curl optional, but the ActiveSync HTTP module
should compile only when `HAVE_CURL` is defined.

Rationale:

- This repository already has optional curl support. `configure.ac` defaults
  `--with-curl` to `auto`, uses `curl-config` when found, and defines
  `HAVE_CURL` after header and link checks pass.
- The feed implementation already uses `#ifdef HAVE_CURL`.
- libcurl simplifies HTTPS, redirects, proxies, request headers, response
  headers, timeouts, and long-poll command behavior.
- Requiring curl for ActiveSync avoids implementing and maintaining a separate
  HTTP/1.1 client, TLS verification behavior, proxy handling, redirect handling,
  and long-poll timeout behavior.
- A narrow transport interface is still useful because fake transports make
  unit tests straightforward.

Configure/build shape:

- Do not add new configure flags for curl; reuse the existing `--with-curl`
  behavior.
- Compile ActiveSync HTTP only when `HAVE_CURL` is defined.
- If ActiveSync is built without curl, command functions that need HTTP should
  return `MAILACTIVESYNC_ERROR_HTTP_UNAVAILABLE`.
- Keep the transport interface even though curl is the only production backend.

## Proposed Files

```text
src/low-level/activesync/
  mailactivesync_http.h
  mailactivesync_http.c
  mailactivesync_command.h
  mailactivesync_command.c
  mailactivesync_parser.h
  mailactivesync_parser.c
```

Optional future files:

```text
src/low-level/activesync/
  mailactivesync_http_curl.c
```

Tests:

```text
unittest/activesync/
  http_request_test.c
  http_response_test.c
  command_options_test.c
  command_folder_sync_test.c
  command_sync_test.c
  command_send_mail_test.c
```

## Transport Model

Represent one HTTP request/response independent from the backend:

```c
struct mailactivesync_http_header {
  char * name;
  char * value;
};

struct mailactivesync_http_request {
  char * method;
  char * url;
  clist * headers; /* struct mailactivesync_http_header * */
  unsigned char * body;
  size_t body_len;
  time_t timeout;
};

struct mailactivesync_http_response {
  int status_code;
  clist * headers; /* struct mailactivesync_http_header * */
  unsigned char * body;
  size_t body_len;
};

struct mailactivesync_http_transport {
  void * context;
  int (*perform)(struct mailactivesync_http_transport * transport,
      struct mailactivesync_http_request * request,
      struct mailactivesync_http_response ** response);
  void (*free)(struct mailactivesync_http_transport * transport);
};
```

The `mailactivesync` session should own a transport instance. Tests can inject
a fake transport that returns canned responses without network access.

## ActiveSync Request Construction

All command requests should flow through one helper:

```c
int mailactivesync_command_post(mailactivesync * session,
    const char * command,
    const char * collection_id,
    const unsigned char * request_body,
    size_t request_body_len,
    struct mailactivesync_http_response ** response);
```

Responsibilities:

- Build URL for `/Microsoft-Server-ActiveSync`.
- Add query parameters:
  - `Cmd`;
  - `User`;
  - `DeviceId`;
  - `DeviceType`;
  - command-specific parameters when required.
- Add headers:
  - `Authorization: Bearer ...` for OAuth2;
  - `Authorization: Basic ...` only for private Basic-auth use;
  - `MS-ASProtocolVersion`;
  - `X-MS-PolicyKey` when present;
  - `Content-Type: application/vnd.ms-sync.wbxml` for WBXML commands;
  - `Accept: application/vnd.ms-sync.wbxml`;
  - `User-Agent`;
  - `Connection: close` for first internal backend implementation.
- For `OPTIONS`, send HTTP `OPTIONS` without WBXML body and parse the protocol
  headers.

## URL And Endpoint Handling

The public `mailactivesync_connect()` currently accepts a server URL. Preserve
that shape, but normalize internally:

- Accept a full endpoint:
  - `https://outlook.office365.com/Microsoft-Server-ActiveSync`
- Accept a host URL:
  - `https://outlook.office365.com`
- Store:
  - scheme;
  - host;
  - port;
  - path, defaulting to `/Microsoft-Server-ActiveSync`.

Do not implement Autodiscover in this phase.

## Authentication

Primary:

- OAuth2 bearer token from `mailactivesync_login_oauth2()`.
- The library consumes an already acquired access token.
- The library does not implement OAuth device code, refresh token, or browser
  flows.
- Outlook.com OAuth scope selection and token acquisition are outside libEtPan.

Secondary:

- Basic auth from `mailactivesync_login()`.
- Mark this as intended for private/on-premises Exchange servers only.
- If Outlook.com or Exchange Online returns HTTP 401 for Basic auth, surface
  `MAILACTIVESYNC_ERROR_UNAUTHORIZED` and let the sample explain that OAuth2 is
  required.

Token redaction:

- Never log full `Authorization` headers.
- If request logging is added, redact bearer and Basic credentials.

## Response Handling

HTTP status classes:

- `2xx`: parse response normally.
- `3xx`: support a small redirect limit for `OPTIONS` and command `POST`.
- `401`: return `MAILACTIVESYNC_ERROR_UNAUTHORIZED`.
- `403`: return auth/policy access error.
- `449` or provisioning-related status: surface a policy/provisioning error
  once such an enum exists.
- `451` or redirect-like ActiveSync responses: follow `[MS-ASHTTP]`.
- `5xx`: return HTTP/server error and preserve status code for diagnostics.

Command status:

- Decode WBXML body first.
- Parse command-specific `Status` elements.
- Map protocol status to ActiveSync low-level error codes.
- Preserve raw numeric command status in result structs where useful.

## Primitive Implementation Order

### Phase 1: HTTP Request/Response Core

- Add request/response structs and free helpers.
- Add header add/find helpers.
- Add URL parser/normalizer.
- Add fake transport support for tests.
- Implement the curl transport backend behind the transport interface.
- Capture HTTP status, response headers, and response body from curl callbacks.
- Add tests for request formatting and response parsing.

### Phase 2: Curl Transport

- Use libcurl for HTTPS, redirects, proxies, TLS verification, and timeouts.
- Set request method and body using curl options.
- Add command headers through a curl header list.
- Use write/header callbacks to collect response data.
- Set normal command timeout separately from long-poll `Ping` timeout.
- Return `MAILACTIVESYNC_ERROR_HTTP_UNAVAILABLE` if built without `HAVE_CURL`.

### Phase 3: OPTIONS

- Implement `mailactivesync_options()`.
- Parse:
  - `MS-ASProtocolVersions`;
  - `MS-ASProtocolCommands`.
- Choose version `16.1` when supported unless the caller has explicitly set a
  version.
- Add fake-transport tests for successful options, missing headers, 401, and
  redirects.

### Phase 3.5: Provisioning Policy Handling

Implement minimal provisioning support only if a server requires it before mail
sync.

- Detect policy-required responses from command responses or HTTP status.
- Add `mailactivesync_provision()` if needed.
- Store the server-returned policy key in the session.
- Send `X-MS-PolicyKey` on later command requests, except where the protocol
  does not require it.
- Do not attempt to enforce device security policies locally in libEtPan.
- Surface remote wipe directives as an explicit error/status and do not perform
  any wipe action.

For Outlook.com consumer mail, start by testing whether `OPTIONS`,
`FolderSync`, and `Sync` work without provisioning. Add this phase only when a
real server response proves it is required.

### Phase 4: FolderSync

Depends on WBXML command builder and parser:

- Build `FolderSync` WBXML with caller-provided sync key.
- POST `Cmd=FolderSync`.
- Decode response WBXML.
- Parse:
  - `Status`;
  - new `SyncKey`;
  - folder adds;
  - folder updates;
  - folder deletes.
- Populate `mailactivesync_folder_sync_result`.
- Handle invalid sync key by returning a distinct error if possible.

### Phase 5: Sync

Depends on WBXML command builder and parser:

- Build `Sync` request from `mailactivesync_sync_request`.
- Support:
  - `Collections`;
  - `Collection`;
  - `SyncKey`;
  - `CollectionId`;
  - `GetChanges`;
  - `WindowSize`;
  - body preference for MIME.
- POST `Cmd=Sync`.
- Decode response WBXML.
- Parse:
  - collection `Status`;
  - new collection `SyncKey`;
  - `MoreAvailable`;
  - added messages;
  - changed messages;
  - deleted server IDs;
  - read state;
  - flag state;
  - subject/from/date/estimated size when present.
- Parse raw MIME when returned.
- Parse AirSyncBase body fields into `mailactivesync_airsyncbase_body` when
  returned.
- Populate `mailactivesync_sync_result`.

### Phase 6: ItemOperations Fetch

- Build `ItemOperations` fetch request with `CollectionId` and `ServerId`.
- Request MIME and AirSyncBase body preference where supported.
- Parse fetched raw MIME and AirSyncBase body fields.
- Populate `mailactivesync_item`.

### Phase 7: SendMail

- Build `SendMail` request for protocol version 16.1:
  - `ClientId`;
  - optional `SaveInSentItems`;
  - `Mime`.
- POST `Cmd=SendMail`.
- Parse command status.
- Return success only after both HTTP and command status are successful.

### Phase 8: MoveItems

- Build `MoveItems` request from a list of move operations.
- Parse per-item response status and destination IDs when returned.
- Populate `mailactivesync_move_items_result`.

### Phase 9: SmartReply And SmartForward

- Build `SmartReply` and `SmartForward` requests with:
  - source `CollectionId`;
  - source `ServerId`;
  - MIME body;
  - optional `SaveInSentItems`.
- Parse status.
- Share helper code with `SendMail`.

### Phase 10: Ping

- Build `Ping` request with heartbeat and watched collection IDs.
- Use longer request timeout than normal commands.
- Parse changed collection IDs.
- Return timeout/no-change status distinctly from transport failure.

## Error Model Updates

Extend `mailactivesync_types.h` over time with errors such as:

```c
MAILACTIVESYNC_ERROR_REDIRECT_LIMIT,
MAILACTIVESYNC_ERROR_INVALID_SYNC_KEY,
MAILACTIVESYNC_ERROR_POLICY_REQUIRED,
MAILACTIVESYNC_ERROR_SERVER_BUSY,
MAILACTIVESYNC_ERROR_UNSUPPORTED_PROTOCOL_VERSION,
MAILACTIVESYNC_ERROR_UNSUPPORTED_HTTP_RESPONSE
```

Keep the current broad errors until each command parser knows enough to map
status codes precisely.

## Test Strategy

No live server should be required for unit tests.

Use fake transport fixtures:

- `OPTIONS` response with protocol versions/commands.
- HTTP 401.
- HTTP 451/redirect.
- `FolderSync` WBXML response.
- `Sync` WBXML response.
- malformed HTTP response.
- truncated WBXML body.
- command status failure with HTTP 200.

Live interop tests should remain manual/sample-driven:

```text
tests/activesync-sample \
  --server https://outlook.office365.com/Microsoft-Server-ActiveSync \
  --login user@outlook.com \
  --oauth-token ACCESS_TOKEN \
  --state-file /tmp/libetpan-activesync-state.txt
```

The test account, bearer token, server URL, state path, optional recipient for
send tests, and any destructive-test flags must all be provided through command
line parameters. The sample must not contain built-in credentials or acquire
tokens itself.

## Logging And Diagnostics

Add optional logging hooks later:

- request method/path/query without bearer token;
- HTTP status;
- ActiveSync command status;
- protocol version selected;
- sync keys only when explicitly enabled, because they are account state.

Do not log:

- OAuth tokens;
- Basic auth headers;
- raw MIME bodies;
- WBXML bodies by default.

## Acceptance Criteria

- `mailactivesync_options()` works against fake transport.
- `mailactivesync_folder_sync()` round-trips through WBXML builder, HTTP fake
  transport, WBXML decoder, and result parser.
- `mailactivesync_sync()` does the same for an Inbox collection.
- `tests/activesync-sample.c` can reach the first implemented command when
  supplied with a real OAuth token.
- Command code does not manually emit or parse raw WBXML bytes; it depends on
  the WBXML plan's codec/builders.
- No hard dependency on libcurl is required.
