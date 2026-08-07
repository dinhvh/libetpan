---
name: libetpan-test-layout
description: Use when adding, moving, reviewing, or modifying tests in the libetpan repository. Enforce the project convention that isolated unit tests belong in unittest/ and live integration or service-backed tests belong in tests/.
---

# libetpan Test Layout

Keep test placement explicit:

- Put deterministic unit tests in `unittest/`.
- Put live tests, integration tests, and tests that require external services, credentials, network access, or real server behavior in `tests/`.
- Keep sample programs, manual diagnostic tools, command-line examples, and their shared helper sources in `tests/`.
- Do not treat `tests/Makefile.am` membership in `noinst_PROGRAMS` alone as a reason to keep a file in `tests/`; deterministic automated test programs listed in `TESTS` may still belong in `unittest/`.
- When adding a new test, choose the directory based on what the test exercises, not on the implementation file it happens to touch.
- When reviewing or moving tests, flag misplaced coverage and preserve any existing build-system conventions for the destination directory.
- If a test can run entirely against local fixtures, mocks, or in-memory state, prefer `unittest/`.
- If a test depends on a real provider, account, endpoint, daemon, certificate, or environment secret, use `tests/`.
