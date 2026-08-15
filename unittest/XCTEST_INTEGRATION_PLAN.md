# XCTest Integration Plan

## Goal

Run the tests in `unittest/` both as Unix command-line binaries and as macOS
XCTest tests, while keeping the test logic in one place and reporting each
logical test case independently in Xcode.

## Migration Status

Last updated: 2026-08-15

The XCTest bundle currently runs **452 independently reported tests**, with all
452 passing.

### Completed

- [x] Added the `libetpan-unit-tests` macOS XCTest bundle target.
- [x] Added a shared `libetpan-unit-tests` scheme to the workspace.
- [x] Linked tests against the workspace-built `libetpan.framework` rather
  than a developer-specific DerivedData archive path.
- [x] Added runner-neutral failure callbacks that retain C file, line,
  expression, and message information.
- [x] Added fixture-root injection and uniquely named XCTest resource copies.
- [x] IMAP response-done: 10 XCTest cases.
- [x] IMAP response-data: 76 XCTest cases covering 38 parser cases in plain
  and compressed modes.
- [x] IMAP unsupported responses: 14 XCTest cases covering 7 rejection cases
  in plain and compressed modes.
- [x] IMAP command sender: 32 XCTest cases.
- [x] IMAP command-parameter sender: 15 XCTest cases.
- [x] IMAP idle handling: 6 XCTest cases.
- [x] IMAP modified UTF-7: 6 XCTest cases.
- [x] IMF parser: 8 XCTest cases.
- [x] MIME parser: 12 XCTest cases.
- [x] Data types: 6 XCTest cases.
- [x] Charset detection: 4 XCTest cases.
- [x] MIME builder: 3 XCTest cases.
- [x] MIME parser serialization: 162 XCTest cases, one per corpus fixture.
- [x] Plaintext rendering: 41 XCTest cases, one per message fixture.
- [x] POP3: 1 local-loopback regression XCTest case.
- [x] ActiveSync WBXML: 4 XCTest cases.
- [x] ActiveSync HTTP: 48 XCTest cases.
- [x] ActiveSync sample flows: 4 XCTest cases.
- [x] Preserved and verified the migrated Unix runners.

### Completion Checklist

- [x] Complete the IMAP suite.
- [x] Added macOS CI coverage for both XCTest and all Unix test executables.
- [x] Ran the complete final verification matrix.
- [x] Confirmed obsolete command-line sample targets are isolated from the
  XCTest target; removing those targets can remain a separate cleanup change.

### Current Verification Baseline

```sh
xcodebuild test \
  -workspace build-mac/libetpan.xcworkspace \
  -scheme libetpan-unit-tests \
  -destination 'platform=macOS'
```

Result: 452 tests executed, 0 failures.

The migrated data-types, charset-detection, IMAP UTF-7, IMF, MIME parser, MIME
builder, MIME parser serialization, and plaintext rendering Unix runners have
also been compiled and run successfully. The local-loopback POP3 Unix runner
also passes when allowed to bind its fake server socket. The ActiveSync WBXML,
HTTP, and sample-flow Unix runners pass. The complete Unix matrix was compiled
and run against the Xcode-built framework with no failures. CI stages that
framework in the runtime location expected by the command-line executables.

The default Unix Makefile behavior still targets the autotools static archive.
CI overrides that dependency and library path because the checked-in generated
autotools files do not currently reflect all newer low-level source directories
and headers.

## Original State

- `unittest/Makefile` builds separate Unix test executables.
- Most suites have a `main()` entry point and use C `assert()` for validation.
- Several suites load fixtures through paths relative to their working
  directory.
- `build-mac/libetpan Tests.xcodeproj` originally contained only command-line
  sample targets and no XCTest bundle target.
- The project contains a hard-coded reference to a `libetpan.a` file inside a
  particular DerivedData directory. The XCTest target must not depend on that
  path.

## Recommended Architecture

Use three layers:

1. Shared C test cases containing all test setup, execution, and validation.
2. Thin Unix runners that invoke those cases from `main()`.
3. Thin Objective-C XCTest adapters with one XCTest method per logical C case.

The shared C sources must not import XCTest or depend on Objective-C. Both
runners should compile or link the same C test implementation.

## Failure Reporting

Each meaningful test case should be independently callable. An XCTest method
should invoke one C test case so that Xcode reports failures separately and
continues running unaffected cases.

Do not invoke an entire existing suite from one XCTest method. That would make
the suite appear as one test, and a failed C `assert()` could abort the test
process before the remaining cases run.

Gradually replace test-facing `assert()` calls with a runner-neutral result or
failure callback. Assertions used only for unrecoverable internal invariants
can be evaluated separately.

A possible shared interface is:

```c
struct test_failure {
  const char * file;
  unsigned line;
  const char * expression;
  const char * message;
};

typedef void (*test_failure_callback)(
    const struct test_failure * failure,
    void * context);

int imap_response_done_tagged_ok_test(
    const char * fixture_root,
    test_failure_callback callback,
    void * context);
```

The precise API can be refined during the pilot. It should allow the Unix
runner to print a useful diagnostic and XCTest to record an issue at the
original C source location.

## Implementation Steps

### 1. Create a Small Pilot

Start with IMAP `response_done` or IMAP UTF-7. Prefer `response_done` if fixture
handling should be validated in the pilot; prefer UTF-7 for the smallest
possible proof of concept.

Split the pilot into:

- Shared, independently callable C cases.
- A Unix `main.c` that invokes every case and returns a nonzero status if any
  case fails.
- An Objective-C XCTest adapter with one `-test...` method per C case.

Keep `make -C unittest check` behavior intact.

### 2. Introduce Runner-Neutral Test Support

Add common helpers for:

- Recording a failed expression, message, source file, and line.
- Continuing to later cases after a case fails when it is safe to do so.
- Producing readable Unix diagnostics.
- Returning an explicit pass/fail result.

Avoid compile-time XCTest branches throughout the C test implementation.

### 3. Add the XCTest Target

Add a macOS Unit Testing Bundle target named `libetpan-unit-tests` to the
workspace/project setup.

The target should:

- Depend on the real `libetpan` Xcode target.
- Link the target's built product rather than a hard-coded DerivedData path.
- Compile the shared C test sources and Objective-C adapter sources.
- Use the same relevant header search paths and compiler definitions as the
  library and Unix tests.
- Have a shared scheme suitable for `xcodebuild test`.

### 4. Add XCTest Adapters

Create small `.m` adapter files grouped by suite. Each XCTest method should:

- Resolve the fixture root when needed.
- Invoke exactly one logical shared C test case.
- Translate callback failures into XCTest issues or assertions.
- Preserve the original C file, line, expression, and message where possible.

Use explicit XCTest methods for stable, readable case lists. `XCTContext`
activities may provide extra diagnostic grouping, but they should not replace
separate XCTest methods when independent reporting is required.

### 5. Make Fixtures Runner-Independent

Keep fixture files in their existing source locations.

- Unix runners pass their expected fixture root to shared test code.
- The XCTest target copies required fixtures into its test bundle resources.
- XCTest adapters resolve the bundled resource directory and pass it into the
  shared code.

Shared test code should not depend on Xcode's current working directory.

### 6. Migrate Remaining Suites Incrementally

After the pilot passes under both runners, migrate suites one at a time. The
checkboxes reflect current progress:

1. [x] IMAP and IMAP UTF-7.
2. [x] IMF and MIME.
3. [x] Data types and charset detection.
4. [x] MIME builder.
5. [x] MIME parser serialization.
6. [x] Plaintext rendering.
7. [x] POP3.
8. [x] ActiveSync tests.

For larger `main.c` files, first extract the case logic from orchestration.
Avoid duplicating test bodies in Objective-C.

### 7. Add Continuous Verification

Run both paths in CI:

```sh
make -C unittest check
xcodebuild test \
  -workspace build-mac/libetpan.xcworkspace \
  -scheme libetpan-unit-tests \
  -destination 'platform=macOS'
```

The migration is complete when both commands run the same underlying test
cases, each logical XCTest case is reported independently, and neither runner
depends on a developer-specific path or working directory.

## Acceptance Criteria for the Pilot

- The Unix test executable still builds and runs.
- The XCTest bundle builds and runs from Xcode and `xcodebuild`.
- Every pilot case appears as an individual XCTest test.
- A deliberately failing case reports its C source location and does not stop
  unrelated XCTest cases from running.
- Fixture lookup works regardless of the launch working directory.
- The test target links the built libetpan product without a hard-coded
  DerivedData path.
- No test logic is duplicated between the Unix and XCTest runners.
