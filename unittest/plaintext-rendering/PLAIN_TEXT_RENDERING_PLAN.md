# Plan: Plain Text Rendering Tests

## Goal

Make `plaintext_rendering_test` match the summary rendering behavior from the reference tests: parse each message fixture, render the message through the same HTML-first rendering strategy, flatten the generated HTML to plain text, normalize date lines, and compare against the copied expected `.txt` output.

## Reference Strategy

The source behavior is not a direct MIME-to-text walk. The rendering pipeline is:

1. Parse the message into a MIME/message tree.
2. Prepare header and part data for deterministic unit-test output.
3. Render the full message as HTML using the default message HTML renderer.
4. Convert `text/plain` MIME parts into escaped HTML with line breaks and quote block handling before adding them to the HTML render.
5. Keep `text/html` parts as cleaned HTML.
6. Choose `text/html` over `text/plain` inside `multipart/alternative` when both exist.
7. Render multipart containers, embedded messages, and attachment placeholders using default templates.
8. Flatten the final HTML document to plain text.
9. Normalize lines beginning with `Date:` by replacing `" at "` with `" "`.
10. Compare the normalized generated text with the normalized expected fixture text.

## Implementation Steps

1. Keep the fixture loop in `unittest/plaintext-rendering/main.c`.
   - Print `plaintext-rendering: running <path>` before each fixture.
   - Map `data/input/foo.eml` to `data/output/foo.txt`.
   - If an expected output file is absent, print that it is a known missing output and skip it.

2. Parse each fixture with `mailmime_parse()`.
   - Preserve the original message bytes because single-part bodies need decoded content.
   - Reuse helper patterns from the parser JSON test where useful for content type, charset, disposition, and body decoding.

3. Build a local HTML renderer that mirrors the reference renderer.
   - Run a first pass over the MIME tree to detect mixed text/attachment ordering.
   - Run a second pass to produce HTML.
   - Track:
     - whether a text part has rendered
     - whether an attachment was seen before text
     - related attachments
     - attachment placeholders
     - selected alternative part

4. Implement part selection like the reference renderer.
   - A text part is renderable when it is inline and has MIME type `text/plain` or `text/html`.
   - Attachment parts are excluded from text rendering when they are explicit attachments or named non-inline parts after the first rendered body text.
   - For `multipart/alternative`, choose the last part containing `text/html`; otherwise choose the last part containing `text/plain`; otherwise choose the first child.
   - For `multipart/related`, render the first child as the root body and treat remaining children as related attachments.
   - For `multipart/mixed` and signed-like multiparts, render children in order.
   - For `message/rfc822`, render the nested body wrapped with the embedded-message header template.

5. Decode part data.
   - Decode transfer encodings using libetpan APIs (`base64`, quoted-printable, and raw encodings).
   - Resolve part charset from MIME parameters.
   - Convert decoded bytes to UTF-8 using existing charset conversion facilities in the project.
   - Use detected charset fallback behavior when the MIME charset is missing or unusable.

6. Render `text/plain` parts as HTML before flattening.
   - HTML-escape the decoded UTF-8 text.
   - Replace line breaks with `<br/>`.
   - Convert contiguous quoted lines beginning with `>` into nested `<blockquote type="cite">` blocks using the same recursive line-based behavior as the reference.

7. Render `text/html` parts.
   - Decode bytes to UTF-8 with HTML-aware charset detection.
   - Clean malformed HTML before inserting it into the final message HTML.
   - Do not flatten individual HTML parts early; flatten only the final rendered message.

8. Render default HTML templates.
   - Main message template: header block plus body block.
   - Header template includes From, To/Cc/Bcc, Subject, and Date labels.
   - Attachment template emits filename/size placeholders in the same textual shape after flattening.
   - Attachment separator renders as `<hr/>`.
   - Embedded message template mirrors main message wrapping.

9. Flatten final HTML to plain text by porting `String::flattenHTML()` exactly.
   - Use the implementation in `/Users/dvh/Sources/mailcore2/src/core/basetypes/MCString.cpp` as the source of truth.
   - Port `String::flattenHTML()`, `flattenHTMLAndShowBlockquote()`, and `flattenHTMLAndShowBlockquoteAndLink()` behavior into test-local C helpers.
   - Use libxml2 `htmlSAXParseDoc()` with the same parser state fields:
     - `level`
     - `enabled`
     - `disabledLevel`
     - `result`
     - `logEnabled`
     - `hasQuote`
     - `quoteLevel`
     - `hasText`
     - `lastCharIsWhitespace`
     - `showBlockQuote`
     - `showLink`
     - `hasReturnToLine`
     - link stack
     - paragraph spacing stack
   - Port the same helper behavior:
     - `isWhitespace()`
     - `charactersParsed()`
     - `appendQuote()`
     - `cleanTerminalSpace()`
     - `isPreviousLineBlankLine()`
     - `returnToLine()`
     - `returnToLineAtBeginningOfBlock()`
     - `blockElements()`
     - `dictionaryFromAttributes()`
     - `elementStarted()`
     - `elementEnded()`
     - `commentParsed()`
   - Preserve the same block element set from `MCString.cpp`: `address`, `div`, `p`, `h1` through `h6`, `pre`, `ul`, `ol`, `li`, `dl`, `dt`, `dd`, `form`, `col`, `colgroup`, `th`, `tbody`, `thead`, `tfoot`, `table`, `tr`, and `td`.
   - Preserve anchor behavior: push `href` and starting output offset on `<a>`, and on close append ` (href)` only when link text was emitted and the result does not already end with that URL.
   - Preserve blockquote behavior: increment quote depth on `<blockquote>`, prefix emitted quote lines with `> `, and honor `showBlockQuote=true`.
   - Preserve whitespace behavior exactly: collapse input whitespace through the same `stripWhitespace()`-style logic, track terminal whitespace, and trim terminal trailing space after parsing.
   - Feed `cleanedHTMLString()` output into `htmlSAXParseDoc()` just like `String::flattenHTML()` does. If the full HTML cleaner is not yet ported, first implement the same call boundary with a test-local cleaner shim, then replace the shim with the exact cleaner behavior needed by failing fixtures.

10. Normalize and compare.
    - Apply the date-line tweak to both generated and expected text.
    - Compare normalized UTF-8 strings exactly.
    - On mismatch, print:
      - input fixture path
      - expected fixture path
      - first differing byte
      - generated and expected lengths
    - Write generated output to `/tmp/plaintext-rendering-generated.txt` to make local diffs easy.

## Verification

1. Build the focused test:

   ```sh
   make -C unittest plaintext-rendering/plaintext_rendering_test
   ```

2. Run the focused test:

   ```sh
   cd unittest/plaintext-rendering
   ./plaintext_rendering_test
   ```

3. Run the full suite:

   ```sh
   make -C unittest check
   ```

4. Clean generated test binaries:

   ```sh
   make -C unittest clean
   ```

## Acceptance Criteria

- Every fixture with an expected `.txt` output is parsed and rendered.
- Generated normalized plain text matches expected normalized plain text.
- Missing expected outputs are reported as known missing outputs and skipped.
- Test output shows the fixture currently running.
- `make -C unittest check` passes.
