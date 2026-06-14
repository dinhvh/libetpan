#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "md5.h"
#include "hmac-md5.h"

static unsigned char hex_value(char ch)
{
  if (ch >= '0' && ch <= '9')
    return (unsigned char) (ch - '0');
  if (ch >= 'a' && ch <= 'f')
    return (unsigned char) (ch - 'a' + 10);
  if (ch >= 'A' && ch <= 'F')
    return (unsigned char) (ch - 'A' + 10);
  assert(0);
  return 0;
}

static void decode_hex(const char * hex, unsigned char * bytes, size_t len)
{
  size_t i;

  assert(strlen(hex) == len * 2);
  for (i = 0; i < len; i++)
    bytes[i] = (unsigned char) ((hex_value(hex[i * 2]) << 4) |
        hex_value(hex[i * 2 + 1]));
}

static void print_digest(const unsigned char * digest)
{
  size_t i;

  for (i = 0; i < 16; i++)
    fprintf(stderr, "%02x", digest[i]);
}

static void assert_digest_eq(const char * label, const unsigned char * actual,
    const char * expected_hex)
{
  unsigned char expected[16];

  decode_hex(expected_hex, expected, sizeof(expected));
  if (memcmp(actual, expected, sizeof(expected)) != 0) {
    fprintf(stderr, "%s: expected %s, got ", label, expected_hex);
    print_digest(actual);
    fputc('\n', stderr);
    assert(0);
  }
}

static void digest_md5(const unsigned char * input, unsigned int len,
    unsigned char digest[16])
{
  MD5_CTX context;

  MD5Init(&context);
  MD5Update(&context, input, len);
  MD5Final(digest, &context);
}

static void check_md5_vectors(void)
{
  static const struct {
    const char * input;
    const char * digest;
  } cases[] = {
    { "", "d41d8cd98f00b204e9800998ecf8427e" },
    { "a", "0cc175b9c0f1b6a831c399e269772661" },
    { "abc", "900150983cd24fb0d6963f7d28e17f72" },
    { "message digest", "f96b697d7cb7938d525a2f31aaf161d0" },
    { "abcdefghijklmnopqrstuvwxyz", "c3fcd3d76192e4007dfb496cca67e13b" },
    { "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
        "d174ab98d277d9f5a5611c2c9f419d9f" },
    { "123456789012345678901234567890123456789012345678901234567890"
        "12345678901234567890", "57edf4a22be3c955ac49da2e2107b67a" }
  };
  unsigned char digest[16];
  size_t i;

  for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    digest_md5((const unsigned char *) cases[i].input,
        (unsigned int) strlen(cases[i].input), digest);
    assert_digest_eq(cases[i].input, digest, cases[i].digest);
  }
}

static void check_md5_chunked_updates(void)
{
  const char input[] =
      "The quick brown fox jumps over the lazy dog"
      "The quick brown fox jumps over the lazy dog"
      "The quick brown fox jumps over the lazy dog";
  unsigned char single_digest[16];
  unsigned char chunked_digest[16];
  MD5_CTX context;

  digest_md5((const unsigned char *) input, (unsigned int) strlen(input),
      single_digest);

  MD5Init(&context);
  MD5Update(&context, (const unsigned char *) input, 1);
  MD5Update(&context, (const unsigned char *) input + 1, 62);
  MD5Update(&context, (const unsigned char *) input + 63,
      (unsigned int) strlen(input) - 63);
  MD5Final(chunked_digest, &context);

  assert(memcmp(single_digest, chunked_digest, sizeof(single_digest)) == 0);
}

static void fill_bytes(unsigned char * bytes, size_t len, unsigned char value)
{
  size_t i;

  for (i = 0; i < len; i++)
    bytes[i] = value;
}

static void check_hmac_md5_vectors(void)
{
  unsigned char digest[HMAC_MD5_SIZE];
  unsigned char key[80];
  unsigned char text[50];

  fill_bytes(key, 16, 0x0b);
  hmac_md5((const unsigned char *) "Hi There", 8, key, 16, digest);
  assert_digest_eq("hmac-md5 rfc2104 case 1", digest,
      "9294727a3638bb1c13f48ef8158bfc9d");

  hmac_md5((const unsigned char *) "what do ya want for nothing?", 28,
      (const unsigned char *) "Jefe", 4, digest);
  assert_digest_eq("hmac-md5 rfc2104 case 2", digest,
      "750c783e6ab0b503eaa86e310a5db738");

  fill_bytes(key, 16, 0xaa);
  fill_bytes(text, 50, 0xdd);
  hmac_md5(text, sizeof(text), key, 16, digest);
  assert_digest_eq("hmac-md5 rfc2104 case 3", digest,
      "56be34521d144c88dbb8c733f0e8b3f6");

  fill_bytes(key, sizeof(key), 0xaa);
  hmac_md5((const unsigned char *) "Test Using Larger Than Block-Size Key - "
      "Hash Key First", 54, key, sizeof(key), digest);
  assert_digest_eq("hmac-md5 rfc2104 case 6", digest,
      "6b1ab7fe4bd7bf8f0b62e6ce61b9d0cd");
}

static void check_hmac_md5_streaming(void)
{
  const unsigned char key[] = "Jefe";
  const unsigned char text[] = "what do ya want for nothing?";
  unsigned char one_shot_digest[HMAC_MD5_SIZE];
  unsigned char streaming_digest[HMAC_MD5_SIZE];
  unsigned char precalc_digest[HMAC_MD5_SIZE];
  HMAC_MD5_CTX hmac;
  HMAC_MD5_STATE state;

  hmac_md5(text, (int) strlen((const char *) text), key,
      (int) strlen((const char *) key), one_shot_digest);

  hmac_md5_init(&hmac, key, (int) strlen((const char *) key));
  hmac_md5_update(&hmac, text, 7);
  hmac_md5_update(&hmac, text + 7,
      (int) strlen((const char *) text) - 7);
  hmac_md5_final(streaming_digest, &hmac);
  assert(memcmp(one_shot_digest, streaming_digest,
        sizeof(one_shot_digest)) == 0);

  hmac_md5_precalc(&state, key, (int) strlen((const char *) key));
  hmac_md5_import(&hmac, &state);
  hmac_md5_update(&hmac, text, (int) strlen((const char *) text));
  hmac_md5_final(precalc_digest, &hmac);
  assert(memcmp(one_shot_digest, precalc_digest,
        sizeof(one_shot_digest)) == 0);
}

int main(void)
{
  assert(sizeof(UINT4) == 4);

  check_md5_vectors();
  check_md5_chunked_updates();
  check_hmac_md5_vectors();
  check_hmac_md5_streaming();

  puts("data_types_test: ok");
  return 0;
}
