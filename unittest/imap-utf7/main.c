#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "charconv.h"

static void check_decode(const char * input, const char * expected)
{
  char * decoded;

  decoded = charconv_decode_mutf7(input);
  assert(decoded != NULL);
  if (strcmp(decoded, expected) != 0) {
    fprintf(stderr, "imap-utf7: decode expected %s, got %s\n",
        expected, decoded);
    abort();
  }
  free(decoded);
}

static void check_encode(const char * input, const char * expected)
{
  char * encoded;

  encoded = charconv_encode_mutf7(input);
  assert(encoded != NULL);
  if (strcmp(encoded, expected) != 0) {
    fprintf(stderr, "imap-utf7: encode expected %s, got %s\n",
        expected, encoded);
    abort();
  }
  free(encoded);
}

static void check_round_trip(const char * input, const char * encoded)
{
  check_encode(input, encoded);
  check_decode(encoded, input);
}

int main(void)
{
  const char * input = "~peter/mail/&U,BTFw-/&ZeVnLIqe-";
  const char * expected[] = {
    "~peter",
    "mail",
    "\xe5\x8f\xb0\xe5\x8c\x97",
    "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e"
  };
  char * copy = strdup(input);
  char * token;
  char * saveptr = NULL;
  unsigned int index = 0;

  assert(copy != NULL);
  token = strtok_r(copy, "/", &saveptr);
  while (token != NULL) {
    assert(index < sizeof(expected) / sizeof(expected[0]));
    check_decode(token, expected[index]);
    index++;
    token = strtok_r(NULL, "/", &saveptr);
  }
  assert(index == sizeof(expected) / sizeof(expected[0]));
  free(copy);

  check_round_trip("&", "&-");
  check_round_trip("~peter/mail/\xe5\x8f\xb0\xe5\x8c\x97/\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e",
      "~peter/mail/&U,BTFw-/&ZeVnLIqe-");

  /* RFC 2152 examples adapted to IMAP modified UTF-7's shift character. */
  check_round_trip("A\xe2\x89\xa2\xce\x91.", "A&ImIDkQ-.");
  check_round_trip("Hi Mom -\xe2\x98\xba-!", "Hi Mom -&Jjo--!");
  check_round_trip("\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e", "&ZeVnLIqe-");
  check_round_trip("Item 3 is \xc2\xa3" "1.", "Item 3 is &AKM-1.");

  puts("imap_utf7_test: ok");
  return 0;
}
