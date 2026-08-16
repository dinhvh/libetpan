# POP3 Socketpair Test Plan

## Goal

Make `testLISTIgnoresZeroMessageNumber` run reliably from both XCTest and the
Unix test executable without requiring permission to bind a TCP loopback
socket.

All implementation changes remain in test code. Production libetpan sources
and public APIs are unchanged.

## Approach

Replace the loopback TCP listener and forked fake server in
`unittest/pop3/main.c` with a local `socketpair()` and a pthread:

- Create a connected pair with `socketpair(AF_UNIX, SOCK_STREAM, 0, ...)`.
- Give one endpoint to `mailstream_socket_open()`.
- Run the existing fake POP3 server protocol on the other endpoint in a
  pthread.
- Join the server thread before completing the test.

This continues to exercise the real mailstream socket transport, POP3 command
handling, LIST parsing, and UIDL parsing, but does not use TCP, allocate a
port, or require `bind()` permission.

## Implementation Steps

1. [x] Add the pthread and Unix-domain socket includes required by the test.
2. [x] Replace `socket()`, `bind()`, `listen()`, `getsockname()`, `fork()`,
   `accept()`, and `waitpid()` with `socketpair()`, `pthread_create()`, and
   `pthread_join()`.
3. [x] Adapt the fake server entry point to return a thread result instead of
   terminating a child process with `_exit()`.
4. [x] Preserve the existing fake POP3 conversation and assertions:
   - Message number `0` is ignored.
   - Exactly one valid message remains.
   - Its message number is `1`.
   - Its size is `10`.
   - Its UIDL is `valid-uid`.
5. [x] Ensure both socket endpoints and the server thread are cleaned up on
   every success and failure path.
6. [x] Report infrastructure failures separately from LIST parsing failures,
   so XCTest does not label a socket or thread setup error as a parser
   regression.
7. [x] Keep the shared `pop3_test_run_case()` API unchanged unless cleanup
   requirements make a small test-only adjustment necessary.

## Verification

1. [x] Compile the standalone POP3 Unix test with warnings treated as errors.
2. [x] Run the Unix POP3 test inside the normal restricted environment and
   confirm it no longer needs elevated loopback-network permission.
3. [x] Run `POP3Tests/testLISTIgnoresZeroMessageNumber` through XCTest.
4. [x] Run the complete XCTest scheme and confirm all 452 tests pass.
5. [x] Run the complete Unix test matrix.
6. [x] Run `git diff --check` and validate the Xcode project and shared scheme.

## Expected Files

- `unittest/pop3/main.c`
- `unittest/pop3/pop3_tests.h`, only if the shared test API needs adjustment
- `unittest/xctest/POP3Tests.m`, only if improved diagnostics require an
  adapter change
- `unittest/pop3/SOCKETPAIR_TEST_PLAN.md`

No production source files should change.
