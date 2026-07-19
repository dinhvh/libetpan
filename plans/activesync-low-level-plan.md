# Low-Level ActiveSync Implementation Plan

## Goal

Add Microsoft Exchange ActiveSync as a low-level protocol client in libEtPan,
similar in spirit to `mailimap_*`, without adding a `mailsession_driver` or
storage integration in the first phase.

The first target is a reliable `mailactivesync_*` API that can talk to
Outlook.com / Exchange Online for mail synchronization, folder synchronization,
message fetch, message send, and change notification.

## Provider Notes

Outlook.com accounts are Exchange-backed and Microsoft documents Exchange-style
sync behavior for Outlook.com accounts. Microsoft also documents that devices
using ActiveSync protocol versions lower than 16.1 cannot connect to Microsoft
services starting March 1, 2026.

Implementation should therefore default to ActiveSync protocol version 16.1 for
Outlook.com / Exchange Online interoperability unless a caller explicitly
chooses another version.

Outlook.com and Exchange Online should be tested with OAuth2 / Modern
Authentication. Username/password Basic authentication for Outlook.com and
Exchange Online is either disabled or being removed across Microsoft mail
protocols, including Exchange ActiveSync. The low-level API can keep a Basic
authentication entry point for private/on-premises Exchange deployments, but the
primary sample and interop path should use bearer tokens.

## Specification Sources

The implementation should be driven from Microsoft's Open Specifications rather
than reverse-engineered traces.

Primary sources:

- `[MS-ASCMD] Exchange ActiveSync: Command Reference Protocol`
  - Defines commands such as `Sync`, `FolderSync`, `SendMail`, `SmartReply`,
    `SmartForward`, `MoveItems`, `ItemOperations`, `Ping`, and their XML
    elements.
  - Published version 28.0 is dated May 20, 2025.
  - https://learn.microsoft.com/en-us/openspecs/exchange_server_protocols/ms-ascmd/1a3490f1-afe1-418a-aa92-6f630036d65a
- `[MS-ASHTTP] Exchange ActiveSync: HTTP Protocol`
  - Defines HTTP `OPTIONS` and `POST` handling, query parameters, request
    headers, response headers, HTTP status handling, protocol version
    negotiation, and redirect behavior.
  - https://learn.microsoft.com/en-us/openspecs/exchange_server_protocols/ms-ashttp/bc92056f-5c48-4775-9f0d-b16b86998e55
- `[MS-ASWBXML] ActiveSync WBXML Algorithm Details`
  - Defines how ActiveSync XML requests and responses are encoded to and
    decoded from WBXML.
  - https://learn.microsoft.com/en-us/openspecs/exchange_server_protocols/ms-aswbxml/52e4f9d1-2b43-48a8-82cc-85d674858351
- ActiveSync code page references
  - Map WBXML code pages to namespaces such as AirSync, Email, Move,
    GetItemEstimate, FolderHierarchy, and others.
  - https://learn.microsoft.com/en-us/openspecs/exchange_server_protocols/ms-aswbxml/2e57ccd4-cf38-45a0-bb11-95304bd1180d

Primitive mapping:

- HTTP transport and `OPTIONS`
  - Use `[MS-ASHTTP]` request/response sections.
  - `OPTIONS` responses expose `MS-ASProtocolVersions` and
    `MS-ASProtocolCommands`.
- WBXML
  - Use `[MS-ASWBXML]` for token encoding, code page switching, string handling,
    opaque values, and XML/WBXML conversion rules.
- `FolderSync`
  - Use `[MS-ASCMD]` folder hierarchy synchronization. Initial sync sends
    `SyncKey` value `0`; the response returns a new sync key and folder
    `ServerId` values.
- `Sync`
  - Use `[MS-ASCMD]` folder item synchronization. Each folder collection needs
    an initial `Sync` with `SyncKey` value `0`; subsequent calls use the
    server-returned sync key.
- `SendMail`
  - Use `[MS-ASCMD]` `SendMail`. For protocol versions 14.0, 14.1, 16.0, and
    16.1, the request body is WBXML containing `SendMail`, `ClientId`,
    `SaveInSentItems`, and `Mime`. Older protocol versions use `message/rfc822`
    directly.
- OAuth2 / authentication behavior
  - Use Microsoft identity platform documentation outside the ActiveSync specs
    to obtain access tokens. The ActiveSync layer should accept an already
    acquired access token and put it in the HTTP `Authorization` header.

## Scope

Implement mail-only ActiveSync protocol support.

In scope:

- HTTP(S) transport for ActiveSync commands.
- WBXML encoding and decoding.
- `OPTIONS` command.
- `FolderSync` command.
- `Sync` command for email collections.
- MIME/body retrieval through `Sync` body preferences and/or `ItemOperations`.
- `SendMail`.
- `SmartReply` and `SmartForward`, if feasible after `SendMail`.
- `MoveItems`.
- `Ping` for change notification.
- Unit tests, parser fixtures, encoder golden tests, and WBXML fuzz coverage.

Out of scope for the first phase:

- libEtPan `mailsession_driver` integration.
- `mailstorage` integration.
- Cached driver implementation.
- Contacts, calendars, tasks, notes, SMS, and document sync.
- Device provisioning policy enforcement beyond carrying a policy key when
  provided.
- Autodiscover, unless manual server URL configuration proves insufficient for
  interop testing.

## Proposed Layout

```text
src/low-level/activesync/
  Makefile.am
  mailactivesync.h
  mailactivesync.c
  mailactivesync_types.h
  mailactivesync_types.c
  mailactivesync_http.h
  mailactivesync_http.c
  mailactivesync_wbxml.h
  mailactivesync_wbxml.c
  mailactivesync_codes.h
  mailactivesync_parser.h
  mailactivesync_parser.c
  mailactivesync_sender.h
  mailactivesync_sender.c
  mailactivesync_oauth2.h
  mailactivesync_oauth2.c
```

Build integration should eventually update:

- `src/low-level/Makefile.am`
- `src/main/libetpan.h`
- `include/Makefile.am`
- `build-spm/include/libetpan/`
- Windows, macOS, Android, and Swift package build metadata

For the planning phase, only the low-level source layout is required.

## Public API Sketch

```c
typedef struct mailactivesync mailactivesync;

mailactivesync * mailactivesync_new(int cached,
    const char * cache_directory);

void mailactivesync_free(mailactivesync * session);

int mailactivesync_connect(mailactivesync * session,
    const char * server_url);

int mailactivesync_set_device(mailactivesync * session,
    const char * device_id,
    const char * device_type);

int mailactivesync_set_protocol_version(mailactivesync * session,
    const char * version);

int mailactivesync_set_policy_key(mailactivesync * session,
    const char * policy_key);

int mailactivesync_login(mailactivesync * session,
    const char * user,
    const char * password);

int mailactivesync_login_oauth2(mailactivesync * session,
    const char * user,
    const char * access_token);

int mailactivesync_options(mailactivesync * session,
    struct mailactivesync_options ** result);

int mailactivesync_folder_sync(mailactivesync * session,
    const char * sync_key,
    struct mailactivesync_folder_sync_result ** result);

int mailactivesync_sync(mailactivesync * session,
    struct mailactivesync_sync_request * request,
    struct mailactivesync_sync_result ** result);

int mailactivesync_item_operations_fetch(mailactivesync * session,
    const char * collection_id,
    const char * server_id,
    struct mailactivesync_item ** result);

int mailactivesync_send_mail(mailactivesync * session,
    const char * mime_message,
    size_t mime_message_len,
    int save_in_sent);

int mailactivesync_smart_reply(mailactivesync * session,
    const char * collection_id,
    const char * server_id,
    const char * mime_message,
    size_t mime_message_len,
    int save_in_sent);

int mailactivesync_smart_forward(mailactivesync * session,
    const char * collection_id,
    const char * server_id,
    const char * mime_message,
    size_t mime_message_len,
    int save_in_sent);

int mailactivesync_move_items(mailactivesync * session,
    clist * moves,
    struct mailactivesync_move_items_result ** result);

int mailactivesync_ping(mailactivesync * session,
    struct mailactivesync_ping_request * request,
    struct mailactivesync_ping_result ** result);
```

## Outlook.com Test Setup

For Outlook.com, plan to test with OAuth2 rather than a raw login/password.
Microsoft has been removing Basic authentication support for Outlook.com and
Exchange Online mail protocols. A username/password test path can remain useful
for private Exchange servers where Basic authentication is explicitly enabled,
but it should not be the default Outlook.com sample.

Recommended Outlook.com test flow:

1. Register a test application in Microsoft Entra ID.
2. Configure it as a public/native client with a redirect URI suitable for a
   local development flow.
3. Request delegated mail scopes that allow mail access and send. Exact scopes
   should be verified against Microsoft identity documentation during
   implementation.
4. Obtain an OAuth2 access token outside libEtPan.
5. Pass the token to `mailactivesync_login_oauth2()`.
6. Connect to `https://outlook.office365.com/Microsoft-Server-ActiveSync`.
7. Run `OPTIONS`, select protocol version 16.1, run `FolderSync` with key `0`,
   find the Inbox `ServerId`, then run `Sync` with collection sync key `0`.

Add a low-level example program for this flow:

```text
tests/activesync-sample.c
```

The sample should accept:

```text
activesync-sample \
  --server https://outlook.office365.com/Microsoft-Server-ActiveSync \
  --login user@outlook.com \
  --oauth-token ACCESS_TOKEN \
  --state-file /tmp/libetpan-activesync-state.txt
```

Sample behavior:

- Read the state file if it exists.
- If no folder sync key exists, use `FolderSync` key `0`.
- Persist the newest folder sync key after every successful `FolderSync`.
- Store folder `ServerId` values by display name, including Inbox.
- If no Inbox collection sync key exists, use `Sync` key `0`.
- Persist the newest Inbox collection sync key after every successful `Sync`.
- Print added, changed, and deleted message `ServerId` values.
- Print basic message metadata when available: subject, from, date, read state,
  flag state, and estimated size.
- Treat HTTP 401 with OAuth as an auth/token failure.
- Treat HTTP 401 with Basic auth as expected for Outlook.com and explain that
  an OAuth token is required.
- If the server reports an invalid sync key, discard only the affected key and
  retry from `0`.

A simple text state file is enough for the sample. It should be easy to inspect
and edit by hand:

```text
protocol_version=16.1
folder_sync_key=123456789
folder.Inbox=5
folder.Sent=6
sync.Inbox=987654321
```

The example should not acquire OAuth tokens itself. Token acquisition belongs
outside libEtPan; the sample should only consume an already acquired bearer
token and exercise the low-level ActiveSync API.

Minimal sample shape:

```c
mailactivesync * as;
struct mailactivesync_options * options;
struct mailactivesync_folder_sync_result * folders;
struct mailactivesync_sync_request * sync_req;
struct mailactivesync_sync_result * sync_res;
const char * inbox_id;
int r;

as = mailactivesync_new(0, NULL);
mailactivesync_set_device(as, "libetpan-test-device-001", "libetpan");
mailactivesync_set_protocol_version(as, "16.1");

r = mailactivesync_connect(as,
    "https://outlook.office365.com/Microsoft-Server-ActiveSync");

r = mailactivesync_login_oauth2(as,
    "user@outlook.com",
    access_token);

r = mailactivesync_options(as, &options);

r = mailactivesync_folder_sync(as, "0", &folders);
inbox_id = find_folder_server_id(folders, "Inbox");

sync_req = mailactivesync_sync_request_new(inbox_id, "0");
mailactivesync_sync_request_set_get_changes(sync_req, 1);
mailactivesync_sync_request_set_window_size(sync_req, 25);
mailactivesync_sync_request_set_mime_body_preference(sync_req, 200 * 1024);

r = mailactivesync_sync(as, sync_req, &sync_res);

/* Caller persists folders->sync_key and sync_res->sync_key. */
```

Optional private Exchange Basic-auth sample:

```c
as = mailactivesync_new(0, NULL);
mailactivesync_set_device(as, "libetpan-test-device-001", "libetpan");
mailactivesync_set_protocol_version(as, "16.1");

r = mailactivesync_connect(as,
    "https://exchange.example.com/Microsoft-Server-ActiveSync");

r = mailactivesync_login(as, "user@example.com", password);
```

If Outlook.com returns HTTP 401 for the Basic-auth sample, that is expected.
The test harness should report that Basic authentication is unavailable and ask
for an OAuth2 token instead of treating it as a protocol implementation failure.

## Core Types

```c
struct mailactivesync_options {
  clist * protocol_versions; /* char * */
  clist * commands;          /* char * */
};

struct mailactivesync_folder {
  char * server_id;
  char * parent_id;
  char * display_name;
  int type;
};

struct mailactivesync_folder_sync_result {
  char * sync_key;
  int status;
  clist * added;   /* struct mailactivesync_folder * */
  clist * updated; /* struct mailactivesync_folder * */
  clist * deleted; /* char * server_id */
};

struct mailactivesync_body_preference {
  int type;
  uint32_t truncation_size;
  int all_or_none;
};

struct mailactivesync_message {
  char * server_id;
  char * subject;
  char * from;
  char * to;
  char * cc;
  char * reply_to;
  char * date_received;
  char * message_class;
  uint32_t estimated_size;
  int read;
  int flagged;
  char * mime;
  size_t mime_len;
};

struct mailactivesync_sync_request {
  char * collection_id;
  char * sync_key;
  int get_changes;
  uint32_t window_size;
  struct mailactivesync_body_preference * body_preference;
  clist * client_commands;
};

struct mailactivesync_sync_result {
  char * sync_key;
  int status;
  int more_available;
  clist * added;   /* struct mailactivesync_message * */
  clist * changed; /* struct mailactivesync_message * */
  clist * deleted; /* char * server_id */
};
```

## Implementation Phases

### Phase 1: Protocol Skeleton

- Add `mailactivesync` session allocation and cleanup.
- Store server URL, user, auth state, device ID/type, protocol version, and
  policy key.
- Add ActiveSync-specific error codes and map transport/status failures to
  libEtPan-style integer results.
- Add no-network unit tests for object allocation, cleanup, and error paths.

### Phase 2: HTTP Transport

- Implement ActiveSync HTTP request construction.
- Support `OPTIONS` and `POST` to `/Microsoft-Server-ActiveSync`.
- Add headers for protocol version, device ID, device type, policy key,
  authorization, and content type.
- Support Basic authentication only if still useful for private Exchange
  servers; prioritize OAuth2 bearer token support for Outlook.com / Exchange
  Online.
- Parse HTTP status, ActiveSync command status, and throttling/auth failures
  separately.

### Phase 3: WBXML Codec

- Implement WBXML token reader/writer.
- Add ActiveSync code page tables for the mail-only subset:
  - AirSync
  - FolderHierarchy
  - Email
  - AirSyncBase
  - ItemOperations
  - ComposeMail
  - Move
  - Ping
- Preserve unknown tags enough to skip safely.
- Add parser tests and fuzz harnesses before enabling network interop.

### Phase 4: OPTIONS

- Send `OPTIONS` to discover supported protocol versions and commands.
- Select version 16.1 by default when available.
- Return supported versions and commands to callers without baking provider
  assumptions into the API.

### Phase 5: FolderSync

- Implement initial `FolderSync` with sync key `0`.
- Parse new folder sync key.
- Parse add/update/delete folder changes.
- Represent folder IDs and parent IDs as opaque strings.
- Add fixtures for initial hierarchy, updates, deletes, and status failures.

### Phase 6: Sync

- Implement initial collection sync using sync key `0`.
- Implement incremental sync with server-returned sync keys.
- Support `GetChanges`, `WindowSize`, `MoreAvailable`, and email body
  preferences.
- Parse server additions, changes, deletes, and soft deletes.
- Keep `ServerId` as the stable low-level message identifier.
- Add tests for empty response reuse behavior, invalid sync keys, windowed
  result paging, and changed read/flag state.

### Phase 7: Body And MIME Retrieval

- Request MIME where supported.
- Fall back to AirSyncBase body fields when MIME is not returned.
- Add `ItemOperations` fetch for full message retrieval by collection ID and
  server ID.
- Preserve raw MIME bytes so higher layers can reuse existing MIME parsers.

### Phase 8: Sending And Message Operations

- Implement `SendMail` with raw RFC822/MIME request body.
- Implement `SmartReply` and `SmartForward` after `SendMail` is stable.
- Implement `MoveItems` for moving messages between folders.
- Defer append-to-folder semantics unless a real ActiveSync command maps
  cleanly enough for callers.

### Phase 9: Ping

- Implement `Ping` for watched folders.
- Return changed collection IDs and status.
- Keep timeout handling explicit so callers control long-poll behavior.
- Do not hide `Ping` behind a generic idle API in the low-level layer.

### Phase 10: Interop And Hardening

- Test against Outlook.com / Exchange Online using OAuth2 and protocol 16.1.
- Capture sanitized WBXML fixtures from real responses.
- Add regression tests for all captured server quirks.
- Add limits for nesting, string sizes, opaque blobs, and collection counts.
- Validate memory ownership under success and every failure path.

## Non-Goals For This Plan

- No `activesyncdriver.c`.
- No `activesyncstorage.c`.
- No generic `mailmessage` conversion.
- No cache-driver behavior.
- No UI or account setup flow.
- No automatic OAuth device-code flow inside libEtPan.

## Later Driver-Layer Follow-Up

Once the low-level API stabilizes, a separate plan can map ActiveSync into
libEtPan's driver/storage interface:

- `src/driver/implementation/activesync/activesyncdriver.c`
- `activesyncdriver_message.c`
- `activesyncdriver_cached.c`
- `activesyncstorage.c`
- folder IDs mapped to mailbox names
- `ServerId` mapped to UID strings
- ActiveSync read/flag/delete state mapped to `mail_flags`

That layer should be kept separate from this first low-level protocol effort.
