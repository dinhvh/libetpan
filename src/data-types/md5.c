/*
 * libEtPan! -- a mail stuff library
 *
 * Copyright (C) 2001, 2005 - DINH Viet Hoa
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the libEtPan! project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include <string.h>
#include <sys/types.h>
#ifdef WIN32
#	include <win_etpan.h>
#else
#	include <sys/param.h>
#	include <sys/socket.h>
#	include <netinet/in.h>
#endif

#include "md5global.h"
#include "md5.h"
#include "hmac-md5.h"

static const unsigned char md5_padding[64] = {
  0x80
};

static const unsigned char md5_shift[64] = {
  7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
  5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
  4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
  6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21
};

static const UINT4 md5_constant[64] = {
  0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
  0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
  0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
  0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
  0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
  0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
  0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
  0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
  0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
  0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
  0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
  0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
  0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
  0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
  0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
  0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
};

static void md5_transform(UINT4 state[4], const unsigned char block[64]);
static void md5_store_le32(unsigned char * output, const UINT4 * input,
    unsigned int len);
static void md5_load_le32(UINT4 * output, const unsigned char * input,
    unsigned int len);
static void md5_cleanse(void * data, size_t len);

static UINT4 md5_rotate_left(UINT4 value, unsigned int count)
{
  return (value << count) | (value >> (32 - count));
}

void MD5Init(MD5_CTX * context)
{
  context->count[0] = 0;
  context->count[1] = 0;
  context->state[0] = 0x67452301;
  context->state[1] = 0xefcdab89;
  context->state[2] = 0x98badcfe;
  context->state[3] = 0x10325476;
}

void MD5Update(MD5_CTX * context, const unsigned char * input,
    unsigned int inputLen)
{
  unsigned int index;
  unsigned int partLen;
  unsigned int i;
  UINT4 inputBits;

  if (inputLen == 0)
    return;

  index = (unsigned int) ((context->count[0] >> 3) & 0x3f);
  inputBits = (UINT4) inputLen << 3;
  context->count[0] += inputBits;
  if (context->count[0] < inputBits)
    context->count[1]++;
  context->count[1] += (UINT4) inputLen >> 29;

  partLen = 64 - index;
  if (inputLen >= partLen) {
    memcpy(&context->buffer[index], input, partLen);
    md5_transform(context->state, context->buffer);

    for (i = partLen; i + 63 < inputLen; i += 64)
      md5_transform(context->state, &input[i]);

    index = 0;
  }
  else {
    i = 0;
  }

  memcpy(&context->buffer[index], &input[i], inputLen - i);
}

void MD5Final(unsigned char digest[16], MD5_CTX * context)
{
  unsigned char bits[8];
  unsigned int index;
  unsigned int padLen;

  md5_store_le32(bits, context->count, 8);

  index = (unsigned int) ((context->count[0] >> 3) & 0x3f);
  padLen = (index < 56) ? (56 - index) : (120 - index);
  MD5Update(context, md5_padding, padLen);
  MD5Update(context, bits, 8);

  md5_store_le32(digest, context->state, 16);
  md5_cleanse(context, sizeof(*context));
}

static void md5_transform(UINT4 state[4], const unsigned char block[64])
{
  UINT4 a;
  UINT4 b;
  UINT4 c;
  UINT4 d;
  UINT4 f;
  UINT4 g;
  UINT4 temp;
  UINT4 word[16];
  unsigned int i;

  md5_load_le32(word, block, 64);

  a = state[0];
  b = state[1];
  c = state[2];
  d = state[3];

  for (i = 0; i < 64; i++) {
    if (i < 16) {
      f = (b & c) | (~b & d);
      g = i;
    }
    else if (i < 32) {
      f = (d & b) | (~d & c);
      g = (5 * i + 1) & 0x0f;
    }
    else if (i < 48) {
      f = b ^ c ^ d;
      g = (3 * i + 5) & 0x0f;
    }
    else {
      f = c ^ (b | ~d);
      g = (7 * i) & 0x0f;
    }

    temp = d;
    d = c;
    c = b;
    b += md5_rotate_left(a + f + md5_constant[i] + word[g], md5_shift[i]);
    a = temp;
  }

  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
  md5_cleanse(word, sizeof(word));
}

static void md5_store_le32(unsigned char * output, const UINT4 * input,
    unsigned int len)
{
  unsigned int i;
  unsigned int j;

  for (i = 0, j = 0; j < len; i++, j += 4) {
    output[j] = (unsigned char) (input[i] & 0xff);
    output[j + 1] = (unsigned char) ((input[i] >> 8) & 0xff);
    output[j + 2] = (unsigned char) ((input[i] >> 16) & 0xff);
    output[j + 3] = (unsigned char) ((input[i] >> 24) & 0xff);
  }
}

static void md5_load_le32(UINT4 * output, const unsigned char * input,
    unsigned int len)
{
  unsigned int i;
  unsigned int j;

  for (i = 0, j = 0; j < len; i++, j += 4) {
    output[i] = ((UINT4) input[j]) |
        ((UINT4) input[j + 1] << 8) |
        ((UINT4) input[j + 2] << 16) |
        ((UINT4) input[j + 3] << 24);
  }
}

static void md5_cleanse(void * data, size_t len)
{
  volatile unsigned char * p = data;

  while (len-- > 0)
    *p++ = 0;
}

static unsigned int md5_nonnegative_len(int len)
{
  return len > 0 ? (unsigned int) len : 0;
}

void hmac_md5_init(HMAC_MD5_CTX * hmac, const unsigned char * key,
    int key_len)
{
  unsigned char inner_pad[64];
  unsigned char outer_pad[64];
  unsigned char key_digest[16];
  unsigned int key_size;
  unsigned int i;

  key_size = md5_nonnegative_len(key_len);
  if (key_size > sizeof(inner_pad)) {
    MD5_CTX key_context;

    MD5Init(&key_context);
    MD5Update(&key_context, key, key_size);
    MD5Final(key_digest, &key_context);
    key = key_digest;
    key_size = sizeof(key_digest);
  }

  memset(inner_pad, 0, sizeof(inner_pad));
  memset(outer_pad, 0, sizeof(outer_pad));
  if (key_size > 0) {
    memcpy(inner_pad, key, key_size);
    memcpy(outer_pad, key, key_size);
  }

  for (i = 0; i < sizeof(inner_pad); i++) {
    inner_pad[i] ^= 0x36;
    outer_pad[i] ^= 0x5c;
  }

  MD5Init(&hmac->ictx);
  MD5Update(&hmac->ictx, inner_pad, sizeof(inner_pad));
  MD5Init(&hmac->octx);
  MD5Update(&hmac->octx, outer_pad, sizeof(outer_pad));

  md5_cleanse(inner_pad, sizeof(inner_pad));
  md5_cleanse(outer_pad, sizeof(outer_pad));
  md5_cleanse(key_digest, sizeof(key_digest));
}

void hmac_md5_precalc(HMAC_MD5_STATE * state, const unsigned char * key,
    int key_len)
{
  HMAC_MD5_CTX hmac;
  unsigned int i;

  hmac_md5_init(&hmac, key, key_len);
  for (i = 0; i < 4; i++) {
    state->istate[i] = htonl(hmac.ictx.state[i]);
    state->ostate[i] = htonl(hmac.octx.state[i]);
  }
  md5_cleanse(&hmac, sizeof(hmac));
}

void hmac_md5_import(HMAC_MD5_CTX * hmac, HMAC_MD5_STATE * state)
{
  unsigned int i;

  memset(hmac, 0, sizeof(*hmac));
  for (i = 0; i < 4; i++) {
    hmac->ictx.state[i] = ntohl(state->istate[i]);
    hmac->octx.state[i] = ntohl(state->ostate[i]);
  }
  hmac->ictx.count[0] = 64U << 3;
  hmac->octx.count[0] = 64U << 3;
}

void hmac_md5_final(unsigned char digest[HMAC_MD5_SIZE], HMAC_MD5_CTX * hmac)
{
  MD5Final(digest, &hmac->ictx);
  MD5Update(&hmac->octx, digest, HMAC_MD5_SIZE);
  MD5Final(digest, &hmac->octx);
  md5_cleanse(hmac, sizeof(*hmac));
}

void hmac_md5(const unsigned char * text, int text_len,
    const unsigned char * key, int key_len, unsigned char digest[16])
{
  HMAC_MD5_CTX hmac;

  hmac_md5_init(&hmac, key, key_len);
  MD5Update(&hmac.ictx, text, md5_nonnegative_len(text_len));
  hmac_md5_final(digest, &hmac);
}
