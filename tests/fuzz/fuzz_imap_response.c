/*
 * fuzz_imap_response.c — libFuzzer harness for libetpan's IMAP response parser.
 *
 * Drives mailimap_response_parse() on attacker-controlled byte buffers.
 * Surfaces memory-safety issues (use-after-free, heap-buffer-overflow,
 * NULL deref) and resource issues (leaks, unbounded allocations) under
 * AddressSanitizer + LeakSanitizer.
 *
 * Build (assumes libetpan has been built with -fsanitize=address):
 *
 *   clang -g -O1 -fsanitize=address,fuzzer \
 *     -I . -I .include -I src/data-types -I src/low-level/imap -I src/main \
 *     tests/fuzz/fuzz_imap_response.c src/.libs/libetpan.a \
 *     -lpthread -lz \
 *     -o fuzz_imap_response
 *
 * Run:
 *
 *   ./fuzz_imap_response -dict=tests/fuzz/imap.dict tests/fuzz/seeds/
 *
 * See tests/fuzz/README.md for full instructions, dependencies, and
 * notes on how the bugs in EXIM-Security-2026-05-* were discovered and
 * verified via this harness.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Skip the <libetpan/libetpan.h> umbrella header — it expects an
 * installed include tree. Include the specific headers from the
 * source layout directly. */
#include "mmapstring.h"
#include "mailstream.h"
#include "mailimap_types.h"
#include "mailimap_parser.h"
#include "mailimap.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size == 0 || size > 65536) {
        return 0;
    }

    MMAPString *buffer = mmap_string_new("");
    if (buffer == NULL) {
        return 0;
    }
    if (mmap_string_append_len(buffer, (const char *)data, size) == NULL) {
        mmap_string_free(buffer);
        return 0;
    }

    mailimap *session = mailimap_new(0, NULL);
    if (session == NULL) {
        mmap_string_free(buffer);
        return 0;
    }

    struct mailimap_parser_context *parser_ctx =
        mailimap_parser_context_new(session);
    if (parser_ctx == NULL) {
        mailimap_free(session);
        mmap_string_free(buffer);
        return 0;
    }

    size_t indx = 0;
    struct mailimap_response *response = NULL;

    int r = mailimap_response_parse(NULL, buffer, parser_ctx, &indx,
                                    &response, 0, NULL);

    if (r == MAILIMAP_NO_ERROR && response != NULL) {
        mailimap_response_free(response);
    }

    mailimap_parser_context_free(parser_ctx);
    mailimap_free(session);
    mmap_string_free(buffer);

    return 0;
}
