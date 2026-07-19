# ActiveSync WBXML Encoding/Decoding Plan

## Goal

Implement a small internal WBXML encoder/decoder for the ActiveSync low-level
client. The codec should be independent from HTTP and from individual
ActiveSync commands, so it can be tested with byte fixtures before any network
interop work begins.

## Implementation Status

Phase 1 through the first encoder/decoder slice are implemented:

- `mailactivesync_codes.{h,c}` defines the initial mail-oriented ActiveSync
  code pages and tag tokens.
- `mailactivesync_wbxml.{h,c}` provides document/node construction, ownership,
  WBXML encode, and WBXML decode.
- `tests/activesync-wbxml-test.c` validates tag lookup, the public FolderSync
  golden fixture, opaque payload round-trip, and malformed input rejection.
- `src/low-level/activesync/Makefile.am` installs the new headers and links the
  new sources into `libactivesync.la`.

Remaining work from this plan is still relevant: broaden fixtures, add
command-specific builders/parsers, add a debug serializer if useful, and add a
fuzz harness.

The first implementation target is enough WBXML support to encode and decode
mail-oriented ActiveSync commands:

- `FolderSync`
- `Sync`
- `ItemOperations`
- `SendMail`
- `MoveItems`
- `Ping`

The protocol scope is mail-only. Contacts, calendars, tasks, notes, documents,
SMS, and rights-management payloads are out of scope except for safely skipping
unknown WBXML nodes when they appear in server responses.

## Specification Sources

Use public specifications:

- WAP Binary XML Content Format
  - Base WBXML format: version, public ID, charset, string table, tokens,
    code-page switching, inline strings, opaque values, entities, and end tags.
  - https://www.w3.org/1999/06/NOTE-wbxml-19990624/
- `[MS-ASWBXML] ActiveSync WBXML Algorithm Details`
  - ActiveSync-specific WBXML encoding and decoding rules.
  - ActiveSync code page and tag/token mappings.
  - https://learn.microsoft.com/en-us/openspecs/exchange_server_protocols/ms-aswbxml/52e4f9d1-2b43-48a8-82cc-85d674858351
- `[MS-ASCMD] Exchange ActiveSync: Command Reference Protocol`
  - XML element shapes for the command payloads that WBXML carries.
  - https://learn.microsoft.com/en-us/openspecs/exchange_server_protocols/ms-ascmd/1a3490f1-afe1-418a-aa92-6f630036d65a

## Dependency Decision

Do not add a general XML dependency for the first WBXML implementation.

Reasons:

- ActiveSync does not require arbitrary XML document handling in the client
  core. Requests are known command trees, and responses are tokenized
  ActiveSync elements.
- WBXML decoding already yields structured events or nodes. Converting decoded
  WBXML to text XML only to parse it again would add avoidable complexity.
- The repository already has C container/string primitives (`clist`,
  `MMAPString`) that are enough for this codec.
- Avoiding a dependency keeps the low-level protocol usable on the same
  platforms as the rest of libEtPan.

Use an XML library only if a later developer tool needs debugging-only XML
pretty-printing or fixture conversion. Even then, keep it out of the production
codec path. If pretty-printing is needed, write a tiny debug serializer for the
ActiveSync node tree instead of validating against a full XML parser.

## Concrete ActiveSync WBXML Format

ActiveSync uses a small, concrete subset of WBXML.

Default document header for ActiveSync protocol 16.1 command bodies:

```text
03 01 6A 00
```

Meaning:

- `0x03`: WBXML version 1.3.
- `0x01`: unknown public identifier.
- `0x6A`: UTF-8 charset.
- `0x00`: empty string table.

Required global tokens:

```c
#define MAILACTIVESYNC_WBXML_SWITCH_PAGE 0x00
#define MAILACTIVESYNC_WBXML_END         0x01
#define MAILACTIVESYNC_WBXML_ENTITY      0x02
#define MAILACTIVESYNC_WBXML_STR_I       0x03
#define MAILACTIVESYNC_WBXML_LITERAL     0x04
#define MAILACTIVESYNC_WBXML_STR_T       0x83
#define MAILACTIVESYNC_WBXML_OPAQUE      0xC3

#define MAILACTIVESYNC_WBXML_HAS_CONTENT 0x40
#define MAILACTIVESYNC_WBXML_HAS_ATTRS   0x80
#define MAILACTIVESYNC_WBXML_TOKEN_MASK  0x3F
```

Initial implementation policy:

- Emit `STR_I` for strings.
- Decode `STR_I`, `STR_T`, and `OPAQUE`.
- Reject tags with `HAS_ATTRS`, because ActiveSync command payloads for this
  scope do not require attributes.
- Do not emit `ENTITY`, `LITERAL`, extension tokens, or string-table
  references in the encoder.
- Decode unsupported global tokens as parse errors unless a test fixture proves
  they are needed for Exchange/Outlook.com interop.

Code page switching:

```text
00 07
```

selects code page 7, for example. The current code page remains active until
another `SWITCH_PAGE` token is read or written.

Tag encoding:

- Leaf tag: raw token, such as `0x17`.
- Tag with content: token OR `0x40`, such as `0x57`.
- Text content: `0x03`, UTF-8 bytes, `0x00`, then `0x01` to close the parent
  element.
- Child content: child tags, then `0x01` to close the parent element.

## XML Correctness Strategy

The codec should enforce ActiveSync/WBXML structural correctness, not general
XML correctness.

Enforce:

- one root element per document;
- proper element nesting;
- known code page/tag pairs for strict mode;
- safe skipping of unknown tags in permissive mode;
- valid string termination for inline strings and string-table strings;
- valid multibyte integer encodings;
- bounds for nesting depth, string length, string table size, opaque value size,
  and total node count;
- UTF-8 input/output by default.

Do not attempt to support:

- DTDs;
- processing instructions;
- attributes, unless a future ActiveSync version requires them;
- namespaces as text URIs in the runtime tree;
- arbitrary XML entity expansion;
- validation against a schema.

## Proposed Files

```text
src/low-level/activesync/
  mailactivesync_wbxml.h
  mailactivesync_wbxml.c
  mailactivesync_codes.h
  mailactivesync_codes.c
```

Add these to `src/low-level/activesync/Makefile.am`.

Tests:

```text
unittest/activesync/
  Makefile.am
  main.c
  wbxml_encode_test.c
  wbxml_decode_test.c
  data/wbxml/*.wbxml
  data/xml/*.xml
```

Fuzzing:

```text
tests/fuzz/fuzz_activesync_wbxml.c
tests/fuzz/activesync-wbxml.dict
```

## Core Data Model

Use a small ActiveSync node tree:

```c
struct mailactivesync_wbxml_node {
  uint8_t code_page;
  uint8_t token;
  char * name;
  char * text;
  unsigned char * opaque;
  size_t opaque_len;
  clist * children; /* struct mailactivesync_wbxml_node * */
};

struct mailactivesync_wbxml_document {
  uint8_t version;
  uint32_t public_id;
  uint32_t charset;
  struct mailactivesync_wbxml_node * root;
};
```

For command builders, prefer token constants over string names:

```c
mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNC,
    MAILACTIVESYNC_AIRSYNC_SYNC);
```

Names are useful for debugging and fixture output, but encoding should be
driven by code-page/token pairs.

## Public/Internal API Sketch

Keep this API low-level but not necessarily installed as public API until it
settles.

```c
struct mailactivesync_wbxml_document *
mailactivesync_wbxml_document_new(void);

void mailactivesync_wbxml_document_free(
    struct mailactivesync_wbxml_document * document);

struct mailactivesync_wbxml_node *
mailactivesync_wbxml_node_new(uint8_t code_page, uint8_t token);

struct mailactivesync_wbxml_node *
mailactivesync_wbxml_node_new_text(uint8_t code_page, uint8_t token,
    const char * text);

struct mailactivesync_wbxml_node *
mailactivesync_wbxml_node_new_opaque(uint8_t code_page, uint8_t token,
    const unsigned char * data, size_t len);

int mailactivesync_wbxml_node_add_child(
    struct mailactivesync_wbxml_node * parent,
    struct mailactivesync_wbxml_node * child);

int mailactivesync_wbxml_encode(
    struct mailactivesync_wbxml_document * document,
    unsigned char ** result,
    size_t * result_len);

int mailactivesync_wbxml_decode(
    const unsigned char * data,
    size_t data_len,
    struct mailactivesync_wbxml_document ** result);

const char * mailactivesync_wbxml_tag_name(uint8_t code_page,
    uint8_t token);

int mailactivesync_wbxml_tag_lookup(const char * name,
    uint8_t * code_page,
    uint8_t * token);
```

## Token And Code Page Tables

Start with mail-only pages:

- Code page 0: AirSync
- Code page 2: Email
- Code page 5: Move
- Code page 7: FolderHierarchy
- Code page 13: Ping
- Code page 17: AirSyncBase
- Code page 18: Settings, only if needed by interop
- Code page 20: ItemOperations
- Code page 21: ComposeMail

Each table entry should include:

```c
struct mailactivesync_wbxml_token_info {
  uint8_t code_page;
  uint8_t token;
  const char * name;
  unsigned int protocol_versions;
};
```

Do not block initial implementation on version gating. Store version metadata
where useful, but enforce version compatibility in command builders later.

Concrete first-pass token constants:

```c
enum {
  MAILACTIVESYNC_CP_AIRSYNC = 0,
  MAILACTIVESYNC_CP_EMAIL = 2,
  MAILACTIVESYNC_CP_MOVE = 5,
  MAILACTIVESYNC_CP_FOLDERHIERARCHY = 7,
  MAILACTIVESYNC_CP_PING = 13,
  MAILACTIVESYNC_CP_AIRSYNCBASE = 17,
  MAILACTIVESYNC_CP_ITEMOPERATIONS = 20,
  MAILACTIVESYNC_CP_COMPOSEMAIL = 21
};

enum {
  MAILACTIVESYNC_AIRSYNC_SYNC = 0x05,
  MAILACTIVESYNC_AIRSYNC_ADD = 0x07,
  MAILACTIVESYNC_AIRSYNC_CHANGE = 0x08,
  MAILACTIVESYNC_AIRSYNC_DELETE = 0x09,
  MAILACTIVESYNC_AIRSYNC_FETCH = 0x0A,
  MAILACTIVESYNC_AIRSYNC_SYNC_KEY = 0x0B,
  MAILACTIVESYNC_AIRSYNC_CLIENT_ID = 0x0C,
  MAILACTIVESYNC_AIRSYNC_SERVER_ID = 0x0D,
  MAILACTIVESYNC_AIRSYNC_STATUS = 0x0E,
  MAILACTIVESYNC_AIRSYNC_COLLECTION = 0x0F,
  MAILACTIVESYNC_AIRSYNC_CLASS = 0x10,
  MAILACTIVESYNC_AIRSYNC_COLLECTION_ID = 0x12,
  MAILACTIVESYNC_AIRSYNC_GET_CHANGES = 0x13,
  MAILACTIVESYNC_AIRSYNC_MORE_AVAILABLE = 0x14,
  MAILACTIVESYNC_AIRSYNC_WINDOW_SIZE = 0x15,
  MAILACTIVESYNC_AIRSYNC_COMMANDS = 0x16,
  MAILACTIVESYNC_AIRSYNC_OPTIONS = 0x17,
  MAILACTIVESYNC_AIRSYNC_COLLECTIONS = 0x1C,
  MAILACTIVESYNC_AIRSYNC_APPLICATION_DATA = 0x1D,
  MAILACTIVESYNC_AIRSYNC_MIME_SUPPORT = 0x22,
  MAILACTIVESYNC_AIRSYNC_MIME_TRUNCATION = 0x23,
  MAILACTIVESYNC_AIRSYNC_HEARTBEAT_INTERVAL = 0x29
};

enum {
  MAILACTIVESYNC_EMAIL_DATE_RECEIVED = 0x0F,
  MAILACTIVESYNC_EMAIL_DISPLAY_TO = 0x11,
  MAILACTIVESYNC_EMAIL_IMPORTANCE = 0x12,
  MAILACTIVESYNC_EMAIL_MESSAGE_CLASS = 0x13,
  MAILACTIVESYNC_EMAIL_SUBJECT = 0x14,
  MAILACTIVESYNC_EMAIL_READ = 0x15,
  MAILACTIVESYNC_EMAIL_TO = 0x16,
  MAILACTIVESYNC_EMAIL_CC = 0x17,
  MAILACTIVESYNC_EMAIL_FROM = 0x18,
  MAILACTIVESYNC_EMAIL_REPLY_TO = 0x19,
  MAILACTIVESYNC_EMAIL_FLAG = 0x1B
};

enum {
  MAILACTIVESYNC_MOVE_MOVES = 0x05,
  MAILACTIVESYNC_MOVE_MOVE = 0x06,
  MAILACTIVESYNC_MOVE_SRCMSGID = 0x07,
  MAILACTIVESYNC_MOVE_SRCFLDID = 0x08,
  MAILACTIVESYNC_MOVE_DSTFLDID = 0x09,
  MAILACTIVESYNC_MOVE_RESPONSE = 0x0A,
  MAILACTIVESYNC_MOVE_STATUS = 0x0B,
  MAILACTIVESYNC_MOVE_DSTMSGID = 0x0C
};

enum {
  MAILACTIVESYNC_FOLDER_DISPLAY_NAME = 0x07,
  MAILACTIVESYNC_FOLDER_SERVER_ID = 0x08,
  MAILACTIVESYNC_FOLDER_PARENT_ID = 0x09,
  MAILACTIVESYNC_FOLDER_TYPE = 0x0A,
  MAILACTIVESYNC_FOLDER_STATUS = 0x0C,
  MAILACTIVESYNC_FOLDER_CHANGES = 0x0E,
  MAILACTIVESYNC_FOLDER_ADD = 0x0F,
  MAILACTIVESYNC_FOLDER_DELETE = 0x10,
  MAILACTIVESYNC_FOLDER_UPDATE = 0x11,
  MAILACTIVESYNC_FOLDER_SYNC_KEY = 0x12,
  MAILACTIVESYNC_FOLDER_FOLDER_SYNC = 0x16,
  MAILACTIVESYNC_FOLDER_COUNT = 0x17
};

enum {
  MAILACTIVESYNC_PING_PING = 0x05,
  MAILACTIVESYNC_PING_STATUS = 0x07,
  MAILACTIVESYNC_PING_HEARTBEAT_INTERVAL = 0x08,
  MAILACTIVESYNC_PING_FOLDERS = 0x09,
  MAILACTIVESYNC_PING_FOLDER = 0x0A,
  MAILACTIVESYNC_PING_ID = 0x0B,
  MAILACTIVESYNC_PING_CLASS = 0x0C,
  MAILACTIVESYNC_PING_MAX_FOLDERS = 0x0D
};

enum {
  MAILACTIVESYNC_AIRSYNCBASE_BODY_PREFERENCE = 0x05,
  MAILACTIVESYNC_AIRSYNCBASE_TYPE = 0x06,
  MAILACTIVESYNC_AIRSYNCBASE_TRUNCATION_SIZE = 0x07,
  MAILACTIVESYNC_AIRSYNCBASE_ALL_OR_NONE = 0x08,
  MAILACTIVESYNC_AIRSYNCBASE_BODY = 0x0A,
  MAILACTIVESYNC_AIRSYNCBASE_DATA = 0x0B,
  MAILACTIVESYNC_AIRSYNCBASE_ESTIMATED_DATA_SIZE = 0x0C,
  MAILACTIVESYNC_AIRSYNCBASE_TRUNCATED = 0x0D,
  MAILACTIVESYNC_AIRSYNCBASE_NATIVE_BODY_TYPE = 0x16,
  MAILACTIVESYNC_AIRSYNCBASE_CONTENT_TYPE = 0x17,
  MAILACTIVESYNC_AIRSYNCBASE_PREVIEW = 0x18
};

enum {
  MAILACTIVESYNC_ITEMOPERATIONS_ITEM_OPERATIONS = 0x05,
  MAILACTIVESYNC_ITEMOPERATIONS_FETCH = 0x06,
  MAILACTIVESYNC_ITEMOPERATIONS_STORE = 0x07,
  MAILACTIVESYNC_ITEMOPERATIONS_OPTIONS = 0x08,
  MAILACTIVESYNC_ITEMOPERATIONS_RANGE = 0x09,
  MAILACTIVESYNC_ITEMOPERATIONS_TOTAL = 0x0A,
  MAILACTIVESYNC_ITEMOPERATIONS_PROPERTIES = 0x0B,
  MAILACTIVESYNC_ITEMOPERATIONS_DATA = 0x0C,
  MAILACTIVESYNC_ITEMOPERATIONS_STATUS = 0x0D,
  MAILACTIVESYNC_ITEMOPERATIONS_RESPONSE = 0x0E,
  MAILACTIVESYNC_ITEMOPERATIONS_SCHEMA = 0x10
};

enum {
  MAILACTIVESYNC_COMPOSEMAIL_SEND_MAIL = 0x05,
  MAILACTIVESYNC_COMPOSEMAIL_SMART_FORWARD = 0x06,
  MAILACTIVESYNC_COMPOSEMAIL_SMART_REPLY = 0x07,
  MAILACTIVESYNC_COMPOSEMAIL_SAVE_IN_SENT_ITEMS = 0x08,
  MAILACTIVESYNC_COMPOSEMAIL_REPLACE_MIME = 0x09,
  MAILACTIVESYNC_COMPOSEMAIL_SOURCE = 0x0B,
  MAILACTIVESYNC_COMPOSEMAIL_FOLDER_ID = 0x0C,
  MAILACTIVESYNC_COMPOSEMAIL_ITEM_ID = 0x0D,
  MAILACTIVESYNC_COMPOSEMAIL_MIME = 0x10,
  MAILACTIVESYNC_COMPOSEMAIL_CLIENT_ID = 0x11,
  MAILACTIVESYNC_COMPOSEMAIL_STATUS = 0x12,
  MAILACTIVESYNC_COMPOSEMAIL_ACCOUNT_ID = 0x13
};
```

## Encoder Requirements

Implement:

- WBXML header:
  - version;
  - public identifier;
  - charset;
  - empty string table initially.
- code page switching with `SWITCH_PAGE`;
- start tags with content bit when children/text/opaque data exist;
- `END` token for content-bearing elements;
- inline strings with `STR_I`;
- opaque values with `OPAQUE`;
- multibyte integer encoding;
- deterministic output for golden tests.

Initially avoid:

- string table optimization;
- attributes;
- extension tokens;
- entity references.

String table support can be added later if real servers require it, but
ActiveSync command payloads can begin with inline strings.

First golden encoder fixtures:

```text
FolderSync sync-key 0:
03 01 6A 00 00 07 56 52 03 30 00 01 01

FolderSync sync-key 2:
03 01 6A 00 00 07 56 52 03 32 00 01 01
```

These correspond to:

```xml
<FolderSync xmlns="FolderHierarchy">
  <SyncKey>0</SyncKey>
</FolderSync>
```

and the same request with `SyncKey` value `2`.

The byte derivation is:

- `03 01 6A 00`: ActiveSync WBXML header.
- `00 07`: switch to FolderHierarchy code page.
- `56`: `FolderSync` token `0x16` with content bit `0x40`.
- `52`: `SyncKey` token `0x12` with content bit `0x40`.
- `03 30 00`: inline string `"0"`.
- `01`: end `SyncKey`.
- `01`: end `FolderSync`.

## Decoder Requirements

Implement:

- WBXML header parsing;
- string table parsing;
- code page switching;
- start tags with and without content;
- `END` token handling;
- inline strings;
- string table references;
- opaque data;
- safe unknown-token handling in permissive mode;
- strict error reporting for malformed streams.

Return parse errors for:

- truncated multibyte integers;
- unterminated inline strings;
- out-of-range string table references;
- unmatched `END`;
- content after the root document closes;
- exceeding configured resource limits;
- unsupported attributes if encountered.

## Safety Limits

Add codec-level limits with conservative defaults:

```c
#define MAILACTIVESYNC_WBXML_MAX_DEPTH 64
#define MAILACTIVESYNC_WBXML_MAX_STRING_SIZE (1024 * 1024)
#define MAILACTIVESYNC_WBXML_MAX_OPAQUE_SIZE (32 * 1024 * 1024)
#define MAILACTIVESYNC_WBXML_MAX_STRING_TABLE_SIZE (1024 * 1024)
#define MAILACTIVESYNC_WBXML_MAX_NODE_COUNT 100000
```

Command-specific parsers can apply tighter limits later.

## Implementation Phases

### Phase 1: Tables And Skeleton

- Add `mailactivesync_codes.{h,c}` with code-page and token tables.
- Add lookup by code-page/token.
- Add lookup by name for tests/debugging.
- Add `mailactivesync_wbxml.{h,c}` with document/node constructors and free
  helpers.
- Compile through the existing ActiveSync low-level library.

### Phase 2: Multibyte Integers And Header

- Implement WBXML multibyte unsigned integer read/write helpers.
- Encode/decode WBXML document header.
- Add unit tests for boundary values and malformed encodings.

### Phase 3: Encoder

- Encode node trees into WBXML bytes.
- Add golden tests for:
  - empty-ish `FolderSync` request;
  - initial `Sync` request;
  - `SendMail` request with MIME opaque/text payload;
  - `Ping` request.

### Phase 4: Decoder

- Decode WBXML bytes into node trees.
- Add fixture tests for:
  - `FolderSync` response with adds/deletes;
  - initial `Sync` response;
  - incremental `Sync` response;
  - `MoreAvailable`;
  - empty changes.

### Phase 5: XML Debug Serializer

- Add debug-only node tree serializer to readable XML-like text.
- Use it in failing tests and captured-fixture inspection.
- Keep it non-validating and separate from production command parsing.

### Phase 6: Fuzzing

- Add a fuzz harness for `mailactivesync_wbxml_decode()`.
- Seed with valid command/response fixtures.
- Add dictionary entries for common WBXML tokens and ActiveSync tag bytes.

### Phase 7: Command Builders

- Build command-specific helper functions on top of the generic node API:
  - `mailactivesync_wbxml_build_folder_sync()`
  - `mailactivesync_wbxml_build_sync()`
  - `mailactivesync_wbxml_build_send_mail()`
  - `mailactivesync_wbxml_build_ping()`
- These helpers should be the only code constructing command payloads in
  `mailactivesync.c`.

## Acceptance Criteria

- Codec builds as part of `src/low-level/activesync`.
- Encoder has deterministic golden tests.
- Decoder passes malformed-input tests without leaks or invalid reads.
- Fuzzer can run against `mailactivesync_wbxml_decode()`.
- No production dependency on a general XML library.
- ActiveSync command code can encode/decode payloads without touching raw WBXML
  byte handling directly.
