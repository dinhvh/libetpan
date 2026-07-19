# RFC 2231 MIME Parameter Support Plan

## Goal

Add RFC 2231 support for MIME parameters while preserving libetpan's public ABI:

- Decode extended single parameters such as `filename*=utf-8''%E2%82%AC.txt`.
- Join continued parameters such as `filename*0="long"; filename*1=".txt"`.
- Join and decode extended continued parameters such as `filename*0*=utf-8''%E2%82%AC; filename*1*=%20rates.txt`.
- Apply the decoded result to existing lookup/helper surfaces like `mailmime_content_param_get()`, `mailmime_single_fields_init()`, and Content-Disposition filename extraction.

Keep `struct mailmime_parameter` unchanged unless a later requirement explicitly needs access to raw RFC 2231 metadata.

## Current Code Shape

Relevant parser and helper points:

- `src/low-level/mime/mailmime.c`
  - `mailmime_content_parse()` parses Content-Type parameter lists into `clist *` of `struct mailmime_parameter`.
  - `mailmime_parameter_parse()` parses `attribute "=" value`.
  - `mailmime_value_parse()` handles token or quoted-string only.
- `src/low-level/mime/mailmime_disposition.c`
  - `mailmime_disposition_parse()` parses `Content-Disposition`.
  - `mailmime_filename_parm_parse()` currently recognizes only bare `filename=...`.
  - RFC 2231 names like `filename*0*=` fall through to generic `MAILMIME_DISPOSITION_PARM_PARAMETER`.
- `src/low-level/mime/mailmime_types_helper.c`
  - `mailmime_content_single_fields_init()` exposes `boundary`, `charset`, and `name`.
  - `mailmime_disposition_single_fields_init()` exposes `filename`.
- `src/data-types/charconv.c`
  - Existing charset conversion should be reused for extended values that specify a charset.
- `unittest/mime/parser_test.c`
  - Best first home for focused parser tests.
- `unittest/mime-parser`
  - Best home for end-to-end parsed message/serialization fixture coverage.

## Implementation Strategy

1. Add an internal RFC 2231 normalizer for parameter lists.

   Create private helpers in `src/low-level/mime/mailmime.c` or a new private C file included in `src/low-level/mime/Makefile.am`.

   Recommended API shape:

   ```c
   static int mailmime_parameters_rfc2231_normalize(clist * parameters);
   ```

   The helper should scan a parsed list and replace RFC 2231 fragments with one decoded ordinary `struct mailmime_parameter` per base name.

2. Parse RFC 2231 parameter names without changing the raw parser.

   Treat parsed `pa_name` values as raw attributes and classify them during normalization:

   - `name` -> regular parameter
   - `name*` -> single extended parameter
   - `name*0`, `name*1`, ... -> continuations
   - `name*0*`, `name*1*`, ... -> encoded continuations

   Use strict decimal indexes. Reject malformed suffixes from the RFC 2231 path and leave them as normal parameters.

3. Decode extended values.

   Implement percent decoding for RFC 2231 `ext-value` data:

   ```text
   charset'language'value
   ```

   Behavior:

   - Percent-decode only valid `%HH` triples.
   - Preserve invalid percent sequences literally rather than failing the whole header.
   - Ignore language for now, but parse past it.
   - Convert from declared charset to UTF-8 using existing libetpan charset conversion helpers.
   - If charset is missing, unknown, or conversion fails, fall back to percent-decoded bytes.

4. Join continuations deterministically.

   For a base name:

   - Require segment `0` for continuation assembly.
   - Append segments in numeric order.
   - Stop at the first missing segment to match deployed client behavior.
   - If segment `0` is extended, parse `charset'language'` from segment `0` only.
   - Percent-decode each segment marked with trailing `*`.
   - Apply charset conversion after all bytes are assembled.

5. Resolve duplicate and mixed parameters conservatively.

   Recommended precedence after normalization:

   - A valid RFC 2231 extended/continued value wins over a regular parameter with the same base name.
   - If no valid RFC 2231 value can be assembled, keep the regular parameter.
   - Preserve unrelated extension parameters.
   - Remove consumed fragment parameters from the public parameter list so callers see `filename` or `name`, not `filename*0*`.

6. Normalize Content-Type parameters immediately after parsing.

   Call `mailmime_parameters_rfc2231_normalize(parameters_list)` in `mailmime_content_parse()` after the parameter loop and before `mailmime_content_new()`.

   This makes `mailmime_content_param_get(content, "name")` and `mailmime_single_fields_init()` work without extra public API.

7. Normalize Content-Disposition with minimal parser churn.

   Preferred approach:

   - Let bare `filename=` continue using the existing `MAILMIME_DISPOSITION_PARM_FILENAME` path.
   - Let RFC 2231 filename continuations parse as generic parameters.
   - Add a private post-parse normalizer in `mailmime_disposition_parse()` that scans `dsp_parms`, assembles RFC 2231 fragments for base names, and inserts/replaces typed disposition parms for:
     - `filename`
     - `creation-date`
     - `modification-date`
     - `read-date`
     - optionally `size`, but only if the decoded value parses cleanly as a number

   At minimum, implement `filename` because it is the user-visible case most clients need.

8. Add writer support as a second phase unless parsing is the only requirement.

   `src/low-level/mime/mailmime_write_generic.c` currently writes parameters in their existing form. For generating RFC 2231:

   - Keep ASCII token/quoted-string behavior unchanged.
   - For non-ASCII or very long parameter values, emit RFC 2231 extended form.
   - Prefer single `name*=utf-8''...` when it fits within folding constraints.
   - Use `name*0*`, `name*1*`, ... for long encoded values.

   If only inbound support is desired, explicitly defer writer changes.

## Edge Cases To Cover

- Single extended: `filename*=utf-8''%E2%82%AC%20rates.txt` -> `€ rates.txt`.
- Continuation plain: `filename*0="quarterly "; filename*1="report.pdf"` -> `quarterly report.pdf`.
- Continuation extended: `filename*0*=utf-8''%E2%82%AC%20; filename*1*=rates.pdf` -> `€ rates.pdf`.
- Mixed encoded/plain continuation segments.
- Out-of-order segments.
- Missing segment `0`.
- Missing middle segment.
- Duplicate regular and extended forms.
- Upper/lowercase parameter names.
- Invalid `%` escape.
- Unknown charset fallback.
- Quoted semicolons in segments.
- Existing non-RFC-2231 parsing regressions for `boundary`, `charset`, `name`, and bare `filename`.

## Test Plan

1. Add focused unit tests in `unittest/mime/parser_test.c`.

   Suggested test functions:

   - `check_rfc2231_content_type_parameters()`
   - `check_rfc2231_content_disposition_filename()`
   - `check_rfc2231_invalid_and_duplicate_parameters()`

   Assert both the normalized `clist` contents and helper accessors such as `mailmime_content_param_get()`.

2. Add an end-to-end fixture under `unittest/mime-parser/data/input/messages/`.

   Include one multipart message with:

   - Content-Type `name*=` parameter.
   - Content-Disposition `filename*0*=` / `filename*1*=` parameters.

   Update the expected serializer output so the attachment filename is decoded.

3. Run targeted tests.

   ```sh
   cd unittest/mime && make && ./mime
   cd ../mime-parser && make && ./mime-parser
   ```

4. Run broader tests if build setup allows.

   ```sh
   make check
   ```

## Risks

- ABI risk if `struct mailmime_parameter` changes; avoid that.
- Ownership risk when replacing list entries; use existing `mailmime_parameter_free()` and disposition parm free helpers consistently.
- Charset conversion behavior may vary by build options; tests should include UTF-8 and one fallback case that does not depend on optional iconv/ICU behavior.
- Writer support can alter generated MIME output and should be landed separately if downstream compatibility is sensitive.

## Milestones

1. Parser-only Content-Type normalization with unit tests.
2. Content-Disposition filename normalization with unit tests.
3. MIME parser fixture update for decoded attachment filename.
4. Optional writer support for emitting RFC 2231 parameters.
5. Regression sweep and documentation/NEWS entry.
