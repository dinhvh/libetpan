# Plan: MIME Parser JSON Serialization Tests

## Goal

Make `mime_parser_serialization_test` parse each fixture under `data/input`, serialize the resulting libetpan `mailmime` tree to JSON, and compare it against the matching file under `data/output`.

## Steps

1. Inspect all expected JSON files and catalog the schema used for headers, addresses, multipart nodes, attachments, and nested message parts.
2. Add a test-local JSON writer with deterministic key ordering and string escaping.
3. Parse each input fixture with `mailmime_parse()`.
4. Serialize message headers from `mailimf_fields`, including dates, message IDs, subjects, address fields, and extra headers.
5. Serialize address objects and address arrays in the same shape as the fixtures.
6. Serialize MIME parts recursively:
   - `MAILMIME_SINGLE` as attachment-like JSON.
   - `MAILMIME_MULTIPLE` as multipart JSON with child parts.
   - `MAILMIME_MESSAGE` as a message-part JSON node.
7. Match expected `uniqueID` conventions from fixture output.
8. Extract MIME metadata from content type, disposition, content ID, charset, and filename parameters.
9. Compare generated JSON to expected output and print the input path, expected path, and first byte difference on failure.
10. Run `make -C unittest check` and keep the parser test wired into the full suite.

## Acceptance

- Every copied parser input fixture is parsed.
- Generated JSON matches every corresponding expected fixture.
- `make -C unittest check` passes.
