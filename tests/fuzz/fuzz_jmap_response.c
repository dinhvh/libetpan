/*
 * fuzz_jmap_response.c -- libFuzzer harness for libetpan's JMAP response
 * parser.
 *
 * Drives mailjmap_response_parse() on attacker-controlled byte buffers.
 * Surfaces memory-safety and resource defects in the JMAP response envelope
 * parser and JSON adapter under AddressSanitizer + LeakSanitizer.
 *
 * Build (assumes libetpan has been built with -fsanitize=address):
 *
 *   clang -g -O1 -fsanitize=address,fuzzer \
 *     -I . -I include -I src/low-level/jmap -I src/main \
 *     tests/fuzz/fuzz_jmap_response.c src/.libs/libetpan.a \
 *     -ljansson -lcurl -lexpat -lpthread -lz \
 *     -o fuzz_jmap_response
 *
 * Run:
 *
 *   mkdir -p jmap-corpus
 *   ./fuzz_jmap_response -dict=tests/fuzz/jmap.dict \
 *     jmap-corpus/ tests/fuzz/jmap-seeds/
 */

#include <stddef.h>
#include <stdint.h>

#include "mailjmap_response.h"

int LLVMFuzzerTestOneInput(const uint8_t * data, size_t size)
{
  struct mailjmap_response * response;
  int r;

  if ((data == NULL) || (size == 0) || (size > 1048576))
    return 0;

  response = NULL;
  r = mailjmap_response_parse((const char *) data, size, &response);
  if ((r == MAILJMAP_NO_ERROR) && (response != NULL))
    mailjmap_response_free(response);

  return 0;
}
