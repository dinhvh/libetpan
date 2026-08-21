# Test placement

- Put deterministic unit tests in `unittest/`.
- Put live or integration tests that require external services, credentials, network access, real servers, daemons, certificates, or environment secrets in `tests/`.
- Keep sample programs, manual diagnostic tools, command-line examples, and their shared helpers in `tests/`.
- If a test can run entirely with local fixtures, mocks, or in-memory state, prefer `unittest/`.
- Choose a test's location based on what it exercises, not the location of the implementation file.
- When adding or moving a test, preserve and update the build-system conventions for its destination directory.

# Plan placement

- Put project plans in `plans/`.
- Do not add plan documents at the repository root or alongside implementation and test files.
