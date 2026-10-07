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

/*
 * $Id: charconv.c,v 1.25 2011/03/29 14:39:55 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "charconv.h"

#ifdef HAVE_ICONV
#include <iconv.h>
#endif
#ifdef HAVE_ICU
#include <unicode/ucnv.h>
#include <unicode/udata.h>
#include "charconv-icu/aliases.h"
#include "charconv-icu/data.h"
#endif
#ifdef HAVE_COREFOUNDATION_CHARCONV
#include <CoreFoundation/CoreFoundation.h>
#ifndef HAVE_ICONV
#include "charconv-icu/apple-legacy.h"
#endif
#endif
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>

#include "mmapstring.h"

int (*extended_charconv)(const char * tocode, const char * fromcode, const char * str, size_t length,
    char * result, size_t* result_len) = NULL;

static int mutf7_base64_value(char ch)
{
  if (ch >= 'A' && ch <= 'Z')
    return ch - 'A';
  if (ch >= 'a' && ch <= 'z')
    return ch - 'a' + 26;
  if (ch >= '0' && ch <= '9')
    return ch - '0' + 52;
  if (ch == '+')
    return 62;
  if (ch == ',')
    return 63;
  return -1;
}

static int mutf7_is_direct(unsigned int cp)
{
  return (cp >= 0x20) && (cp <= 0x7e) && (cp != '&');
}

static int mutf7_append_utf8(char * output, size_t * output_len,
    unsigned int cp)
{
  if (cp <= 0x7f) {
    output[(*output_len)++] = (char) cp;
  }
  else if (cp <= 0x7ff) {
    output[(*output_len)++] = (char) (0xc0 | (cp >> 6));
    output[(*output_len)++] = (char) (0x80 | (cp & 0x3f));
  }
  else if (cp <= 0xffff) {
    if ((cp >= 0xd800) && (cp <= 0xdfff))
      return -1;
    output[(*output_len)++] = (char) (0xe0 | (cp >> 12));
    output[(*output_len)++] = (char) (0x80 | ((cp >> 6) & 0x3f));
    output[(*output_len)++] = (char) (0x80 | (cp & 0x3f));
  }
  else if (cp <= 0x10ffff) {
    output[(*output_len)++] = (char) (0xf0 | (cp >> 18));
    output[(*output_len)++] = (char) (0x80 | ((cp >> 12) & 0x3f));
    output[(*output_len)++] = (char) (0x80 | ((cp >> 6) & 0x3f));
    output[(*output_len)++] = (char) (0x80 | (cp & 0x3f));
  }
  else {
    return -1;
  }

  return 0;
}

static int mutf7_decode_utf16be(const unsigned char * bytes,
    size_t byte_count, char * output, size_t * output_len)
{
  size_t i;

  if ((byte_count % 2) != 0)
    return -1;

  for (i = 0 ; i < byte_count ; i += 2) {
    unsigned int cp;

    cp = ((unsigned int) bytes[i] << 8) | bytes[i + 1];
    if ((cp >= 0xd800) && (cp <= 0xdbff)) {
      unsigned int low;

      if (i + 3 >= byte_count)
        return -1;
      low = ((unsigned int) bytes[i + 2] << 8) | bytes[i + 3];
      if ((low < 0xdc00) || (low > 0xdfff))
        return -1;
      cp = 0x10000 + (((cp - 0xd800) << 10) | (low - 0xdc00));
      i += 2;
    }
    else if ((cp >= 0xdc00) && (cp <= 0xdfff)) {
      return -1;
    }

    if (mutf7_append_utf8(output, output_len, cp) < 0)
      return -1;
  }

  return 0;
}

static int mutf7_decode(const char * str, size_t input_len,
    char ** result, size_t * result_len)
{
  char * output;
  size_t output_len;
  size_t i;
  int res = MAIL_CHARCONV_ERROR_CONV;

  if (input_len > (((size_t) -1) - 1) / 4)
    return MAIL_CHARCONV_ERROR_MEMORY;
  output = malloc(input_len * 4 + 1);
  if (output == NULL)
    return MAIL_CHARCONV_ERROR_MEMORY;

  output_len = 0;
  i = 0;

  while (i < input_len) {
    if (str[i] != '&') {
      output[output_len++] = str[i++];
      continue;
    }

    i++;
    if (i < input_len && str[i] == '-') {
      output[output_len++] = '&';
      i++;
      continue;
    }

    {
      unsigned int bits;
      unsigned int bit_count;
      unsigned char * bytes;
      size_t bytes_size;
      size_t byte_count;
      int has_base64;

      bits = 0;
      bit_count = 0;
      bytes_size = input_len;
      bytes = malloc(bytes_size + 1);
      if (bytes == NULL) {
        res = MAIL_CHARCONV_ERROR_MEMORY;
        goto err;
      }
      byte_count = 0;
      has_base64 = 0;

      while (i < input_len) {
        int value;

        value = mutf7_base64_value(str[i]);
        if (value < 0)
          break;
        has_base64 = 1;
        i++;
        bits = (bits << 6) | (unsigned int) value;
        bit_count += 6;
        while (bit_count >= 8) {
          bit_count -= 8;
          bytes[byte_count++] =
            (unsigned char) ((bits >> bit_count) & 0xff);
        }
        if (bit_count != 0)
          bits &= (1U << bit_count) - 1;
        else
          bits = 0;
      }

      if (!has_base64) {
        free(bytes);
        goto err;
      }

      if (bit_count != 0) {
        unsigned int mask;

        mask = (1U << bit_count) - 1;
        if ((bits & mask) != 0) {
          free(bytes);
          goto err;
        }
      }

      if (mutf7_decode_utf16be(bytes, byte_count, output, &output_len) < 0) {
        free(bytes);
        goto err;
      }
      free(bytes);

      if (i < input_len && str[i] == '-')
        i++;
    }
  }

  output[output_len] = '\0';
  *result = output;
  *result_len = output_len;
  return MAIL_CHARCONV_NO_ERROR;

 err:
  free(output);
  return res;
}

LIBETPAN_EXPORT
char * charconv_decode_mutf7(const char * str)
{
  char * result = NULL;
  size_t length;
  if (str != NULL)
    mutf7_decode(str, strlen(str), &result, &length);
  return result;
}

static int mutf7_output_reserve(char ** output, size_t * output_size,
    size_t output_len, size_t needed)
{
  char * new_output;
  size_t new_size;

  if (output_len == (size_t) -1 ||
      needed > ((size_t) -1) - output_len - 1)
    return -1;
  if (output_len + needed + 1 <= *output_size)
    return 0;

  new_size = *output_size;
  while (output_len + needed + 1 > new_size) {
    if (new_size > ((size_t) -1) / 2) {
      new_size = output_len + needed + 1;
      break;
    }
    new_size *= 2;
  }

  new_output = realloc(*output, new_size);
  if (new_output == NULL)
    return -1;

  *output = new_output;
  *output_size = new_size;
  return 0;
}

static int mutf7_output_char(char ** output, size_t * output_size,
    size_t * output_len, char ch)
{
  if (mutf7_output_reserve(output, output_size, *output_len, 1) < 0)
    return -1;

  (*output)[(*output_len)++] = ch;
  return 0;
}

static int mutf7_decode_utf8_char(const char * str, size_t input_len,
    size_t * index, unsigned int * cp)
{
  const unsigned char * s;
  unsigned char ch;

  s = (const unsigned char *) str;
  ch = s[*index];

  if (ch <= 0x7f) {
    *cp = ch;
    (*index)++;
    return 0;
  }

  if ((ch >= 0xc2) && (ch <= 0xdf)) {
    if (*index + 1 >= input_len)
      return -1;
    if ((s[*index + 1] & 0xc0) != 0x80)
      return -1;
    *cp = ((unsigned int) (ch & 0x1f) << 6) |
      (unsigned int) (s[*index + 1] & 0x3f);
    *index += 2;
    return 0;
  }

  if ((ch >= 0xe0) && (ch <= 0xef)) {
    if (*index + 2 >= input_len)
      return -1;
    if (((s[*index + 1] & 0xc0) != 0x80) ||
        ((s[*index + 2] & 0xc0) != 0x80))
      return -1;
    if ((ch == 0xe0) && (s[*index + 1] < 0xa0))
      return -1;
    if ((ch == 0xed) && (s[*index + 1] >= 0xa0))
      return -1;
    *cp = ((unsigned int) (ch & 0x0f) << 12) |
      ((unsigned int) (s[*index + 1] & 0x3f) << 6) |
      (unsigned int) (s[*index + 2] & 0x3f);
    *index += 3;
    return 0;
  }

  if ((ch >= 0xf0) && (ch <= 0xf4)) {
    if (*index + 3 >= input_len)
      return -1;
    if (((s[*index + 1] & 0xc0) != 0x80) ||
        ((s[*index + 2] & 0xc0) != 0x80) ||
        ((s[*index + 3] & 0xc0) != 0x80))
      return -1;
    if ((ch == 0xf0) && (s[*index + 1] < 0x90))
      return -1;
    if ((ch == 0xf4) && (s[*index + 1] >= 0x90))
      return -1;
    *cp = ((unsigned int) (ch & 0x07) << 18) |
      ((unsigned int) (s[*index + 1] & 0x3f) << 12) |
      ((unsigned int) (s[*index + 2] & 0x3f) << 6) |
      (unsigned int) (s[*index + 3] & 0x3f);
    *index += 4;
    return 0;
  }

  return -1;
}

static int mutf7_encode_shift_byte(char ** output, size_t * output_size,
    size_t * output_len, unsigned int * bits, unsigned int * bit_count,
    unsigned char byte)
{
  static const char alphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+,";

  *bits = (*bits << 8) | byte;
  *bit_count += 8;
  while (*bit_count >= 6) {
    *bit_count -= 6;
    if (mutf7_output_char(output, output_size, output_len,
          alphabet[(*bits >> *bit_count) & 0x3f]) < 0)
      return -1;
  }

  if (*bit_count != 0)
    *bits &= (1U << *bit_count) - 1;
  else
    *bits = 0;

  return 0;
}

static int mutf7_encode_shift_codepoint(char ** output,
    size_t * output_size, size_t * output_len, unsigned int * bits,
    unsigned int * bit_count, unsigned int cp)
{
  if (cp <= 0xffff) {
    if (mutf7_encode_shift_byte(output, output_size, output_len,
          bits, bit_count, (unsigned char) (cp >> 8)) < 0)
      return -1;
    return mutf7_encode_shift_byte(output, output_size, output_len,
        bits, bit_count, (unsigned char) (cp & 0xff));
  }
  else {
    unsigned int value;
    unsigned int high;
    unsigned int low;

    value = cp - 0x10000;
    high = 0xd800 | (value >> 10);
    low = 0xdc00 | (value & 0x3ff);

    if (mutf7_encode_shift_codepoint(output, output_size, output_len,
          bits, bit_count, high) < 0)
      return -1;
    return mutf7_encode_shift_codepoint(output, output_size, output_len,
        bits, bit_count, low);
  }
}

static int mutf7_flush_shift(char ** output, size_t * output_size,
    size_t * output_len, unsigned int * bits, unsigned int * bit_count)
{
  static const char alphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+,";

  if (*bit_count != 0) {
    if (mutf7_output_char(output, output_size, output_len,
          alphabet[(*bits << (6 - *bit_count)) & 0x3f]) < 0)
      return -1;
  }

  *bits = 0;
  *bit_count = 0;
  return mutf7_output_char(output, output_size, output_len, '-');
}

static int mutf7_encode(const char * str, size_t input_len,
    char ** result, size_t * result_len)
{
  char * output;
  size_t output_size;
  size_t output_len;
  size_t i;
  int in_shift;
  int res = MAIL_CHARCONV_ERROR_MEMORY;
  unsigned int bits;
  unsigned int bit_count;

  if (input_len > (((size_t) -1) - 16) / 2)
    return MAIL_CHARCONV_ERROR_MEMORY;
  output_size = input_len * 2 + 16;
  output = malloc(output_size);
  if (output == NULL)
    return MAIL_CHARCONV_ERROR_MEMORY;

  output_len = 0;
  i = 0;
  in_shift = 0;
  bits = 0;
  bit_count = 0;

  while (i < input_len) {
    unsigned int cp;

    if (mutf7_decode_utf8_char(str, input_len, &i, &cp) < 0) {
      res = MAIL_CHARCONV_ERROR_CONV;
      goto err;
    }

    if (mutf7_is_direct(cp)) {
      if (in_shift) {
        if (mutf7_flush_shift(&output, &output_size, &output_len,
              &bits, &bit_count) < 0)
          goto err;
        in_shift = 0;
      }
      if (mutf7_output_char(&output, &output_size, &output_len,
            (char) cp) < 0)
        goto err;
    }
    else if (cp == '&') {
      if (in_shift) {
        if (mutf7_flush_shift(&output, &output_size, &output_len,
              &bits, &bit_count) < 0)
          goto err;
        in_shift = 0;
      }
      if (mutf7_output_reserve(&output, &output_size, output_len, 2) < 0)
        goto err;
      output[output_len++] = '&';
      output[output_len++] = '-';
    }
    else {
      if (!in_shift) {
        if (mutf7_output_char(&output, &output_size, &output_len, '&') < 0)
          goto err;
        in_shift = 1;
      }
      if (mutf7_encode_shift_codepoint(&output, &output_size, &output_len,
            &bits, &bit_count, cp) < 0)
        goto err;
    }
  }

  if (in_shift) {
    if (mutf7_flush_shift(&output, &output_size, &output_len,
          &bits, &bit_count) < 0)
      goto err;
  }

  output[output_len] = '\0';
  *result = output;
  *result_len = output_len;
  return MAIL_CHARCONV_NO_ERROR;

 err:
  free(output);
  return res;
}

LIBETPAN_EXPORT
char * charconv_encode_mutf7(const char * str)
{
  char * result = NULL;
  size_t length;
  if (str != NULL)
    mutf7_encode(str, strlen(str), &result, &length);
  return result;
}

static int mutf7_charconv(const char * tocode, const char * fromcode,
    const char * str, size_t length, char ** result, size_t * result_len)
{
  if (strcasecmp(tocode, "UTF-8") == 0 &&
      strcasecmp(fromcode, "UTF-7-IMAP") == 0)
    return mutf7_decode(str, length, result, result_len);
  if (strcasecmp(fromcode, "UTF-8") == 0 &&
      strcasecmp(tocode, "UTF-7-IMAP") == 0)
    return mutf7_encode(str, length, result, result_len);
  return MAIL_CHARCONV_ERROR_UNKNOWN_CHARSET;
}

static int charconv_get_output_size(size_t length, size_t * result)
{
  if (length > (((size_t) -1) - 17) / 6)
    return MAIL_CHARCONV_ERROR_MEMORY;

  * result = length * 6 + 16;
  return MAIL_CHARCONV_NO_ERROR;
}

#ifdef HAVE_ICONV
static size_t mail_iconv (iconv_t cd, const char **inbuf, size_t *inbytesleft,
    char **outbuf, size_t *outbytesleft,
    char **inrepls, char *outrepl)
{
  size_t ret = 0, ret1;
  /* XXX - force const to mutable */
  char *ib = (char *) *inbuf;
  size_t ibl = *inbytesleft;
  char *ob = *outbuf;
  size_t obl = *outbytesleft;

  for (;;)
  {
#ifdef HAVE_ICONV_PROTO_CONST
    ret1 = iconv (cd, (const char **) &ib, &ibl, &ob, &obl);
#else
    ret1 = iconv (cd, &ib, &ibl, &ob, &obl);
#endif
    if (ret1 != (size_t)-1)
      ret += ret1;
    if (ibl && obl && errno == EILSEQ)
    {
      if (inrepls)
      {
	/* Try replacing the input */
	char **t;
	for (t = inrepls; *t; t++)
	{
	  char *ib1 = *t;
	  size_t ibl1 = strlen (*t);
	  char *ob1 = ob;
	  size_t obl1 = obl;
#ifdef HAVE_ICONV_PROTO_CONST
	  iconv (cd, (const char **) &ib1, &ibl1, &ob1, &obl1);
#else
	  iconv (cd, &ib1, &ibl1, &ob1, &obl1);
#endif
	  if (!ibl1)
	  {
	    ++ib, --ibl;
	    ob = ob1, obl = obl1;
	    ++ret;
	    break;
	  }
	}
	if (*t)
	  continue;
      }
      if (outrepl)
      {
	/* Try replacing the output */
	size_t n = strlen (outrepl);
	if (n <= obl)
	{
	  memcpy (ob, outrepl, n);
	  ++ib, --ibl;
	  ob += n, obl -= n;
	  ++ret;
	  continue;
	}
      }
    }
    *inbuf = ib, *inbytesleft = ibl;
    *outbuf = ob, *outbytesleft = obl;
    return ret;
  }
}
#endif

#ifdef HAVE_COREFOUNDATION_CHARCONV
#ifndef HAVE_ICONV
static const unsigned short * apple_legacy_charset(const char * name)
{
  size_t index;
  for (index = 0; index < sizeof(apple_legacy_charsets) /
      sizeof(apple_legacy_charsets[0]); index++) {
    if (strcasecmp(name, apple_legacy_charsets[index].name) == 0)
      return apple_legacy_charsets[index].unicode;
  }
  return NULL;
}

static int apple_legacy_charconv(const char * tocode, const char * fromcode,
    const char * str, size_t length, char * result, size_t * result_len)
{
  const unsigned short * table;
  size_t index = 0, written = 0;
  int decode = strcasecmp(tocode, "UTF-8") == 0;
  table = decode ? apple_legacy_charset(fromcode) :
      (strcasecmp(fromcode, "UTF-8") == 0 ? apple_legacy_charset(tocode) : NULL);
  if (table == NULL)
    return MAIL_CHARCONV_ERROR_UNKNOWN_CHARSET;
  while (index < length) {
    unsigned int cp;
    if (decode) {
      size_t needed;
      cp = table[(unsigned char) str[index++]];
      if (cp == 0xffff)
        return MAIL_CHARCONV_ERROR_CONV;
      needed = cp < 0x80 ? 1 : cp < 0x800 ? 2 : 3;
      if (needed > *result_len - written)
        return MAIL_CHARCONV_ERROR_MEMORY;
      if (needed == 1)
        result[written++] = (char) cp;
      else if (needed == 2) {
        result[written++] = (char) (0xc0 | (cp >> 6));
        result[written++] = (char) (0x80 | (cp & 0x3f));
      }
      else {
        result[written++] = (char) (0xe0 | (cp >> 12));
        result[written++] = (char) (0x80 | ((cp >> 6) & 0x3f));
        result[written++] = (char) (0x80 | (cp & 0x3f));
      }
    }
    else {
      size_t byte;
      if (mutf7_decode_utf8_char(str, length, &index, &cp) < 0)
        return MAIL_CHARCONV_ERROR_CONV;
      if (written == *result_len)
        return MAIL_CHARCONV_ERROR_MEMORY;
      for (byte = 0; byte < 256; byte++) {
        if (table[byte] != 0xffff && table[byte] == cp)
          break;
      }
      result[written++] = byte < 256 ? (char) byte : '?';
    }
  }
  result[written] = '\0';
  *result_len = written;
  return MAIL_CHARCONV_NO_ERROR;
}
#endif

static CFStringEncoding apple_charset_encoding(const char * charset)
{
  CFStringRef charset_string;
  CFStringEncoding encoding;

  charset_string = CFStringCreateWithCString(NULL, charset,
      kCFStringEncodingUTF8);
  if (charset_string == NULL)
    return kCFStringEncodingInvalidId;
  encoding = CFStringConvertIANACharSetNameToEncoding(charset_string);
  CFRelease(charset_string);
  return encoding;
}

static int apple_should_try_charset(const char * fromcode)
{
  CFStringEncoding encoding;

#ifndef HAVE_ICONV
  if (apple_legacy_charset(fromcode) != NULL)
    return 1;
#endif
  encoding = apple_charset_encoding(fromcode);
  if (encoding == kCFStringEncodingInvalidId ||
      !CFStringIsEncodingAvailable(encoding))
    return 0;

#ifndef HAVE_ICONV
  /* CoreFoundation supplies the general Apple backend without iconv. */
  return 1;
#else
  return encoding == kCFStringEncodingISO_2022_JP ||
      encoding == kCFStringEncodingISO_2022_JP_1 ||
      encoding == kCFStringEncodingISO_2022_JP_2 ||
      encoding == kCFStringEncodingShiftJIS ||
      encoding == kCFStringEncodingDOSJapanese ||
      encoding == kCFStringEncodingEUC_JP;
#endif
}

static int apple_charconv(const char * tocode, const char * fromcode,
    const char * str, size_t length, char * result, size_t * result_len)
{
  CFStringEncoding from_encoding;
  CFStringEncoding to_encoding;
  CFDataRef source_data;
  CFStringRef string;
  CFDataRef converted_data;
  CFIndex converted_length;
  int res;

#ifndef HAVE_ICONV
  res = apple_legacy_charconv(tocode, fromcode, str, length, result, result_len);
  if (res != MAIL_CHARCONV_ERROR_UNKNOWN_CHARSET)
    return res;
#endif
  if (length > (size_t) LONG_MAX || *result_len > (size_t) LONG_MAX)
    return MAIL_CHARCONV_ERROR_MEMORY;

  from_encoding = apple_charset_encoding(fromcode);
  to_encoding = apple_charset_encoding(tocode);
  if (from_encoding == kCFStringEncodingInvalidId ||
      to_encoding == kCFStringEncodingInvalidId ||
      !CFStringIsEncodingAvailable(from_encoding) ||
      !CFStringIsEncodingAvailable(to_encoding))
    return MAIL_CHARCONV_ERROR_UNKNOWN_CHARSET;

  source_data = CFDataCreate(NULL, (const UInt8 *) str, (CFIndex) length);
  if (source_data == NULL)
    return MAIL_CHARCONV_ERROR_MEMORY;

  string = CFStringCreateFromExternalRepresentation(NULL, source_data,
      from_encoding);
  if (string == NULL) {
    CFRelease(source_data);
    return MAIL_CHARCONV_ERROR_CONV;
  }

  converted_data = CFStringCreateExternalRepresentation(NULL, string,
      to_encoding, (UInt8) '?');
  if (converted_data == NULL) {
    res = MAIL_CHARCONV_ERROR_CONV;
    goto free_string;
  }

  converted_length = CFDataGetLength(converted_data);
  if (converted_length < 0 || (size_t) converted_length > *result_len) {
    res = MAIL_CHARCONV_ERROR_MEMORY;
    goto free_converted_data;
  }

  CFDataGetBytes(converted_data, CFRangeMake(0, converted_length),
      (UInt8 *) result);
  result[converted_length] = '\0';
  *result_len = (size_t) converted_length;
  res = MAIL_CHARCONV_NO_ERROR;

 free_converted_data:
  CFRelease(converted_data);
 free_string:
  CFRelease(string);
  CFRelease(source_data);
  return res;
}
#endif

#ifdef HAVE_ICU
static int icu_should_try_charset(const char * fromcode)
{
#ifndef HAVE_ICONV
  /* ICU is the general conversion backend when iconv is disabled. */
  (void) fromcode;
  return 1;
#else
  return strcasecmp(fromcode, "iso-2022-jp") == 0 ||
      strcasecmp(fromcode, "iso-2022-jp-2") == 0 ||
      strcasecmp(fromcode, "shift_jis") == 0 ||
      strcasecmp(fromcode, "shift-jis") == 0 ||
      strcasecmp(fromcode, "euc-jp") == 0 ||
      strcasecmp(fromcode, "eucjp") == 0;
#endif
}

static int icu_error(UErrorCode err)
{
  if (err == U_MEMORY_ALLOCATION_ERROR || err == U_BUFFER_OVERFLOW_ERROR)
    return MAIL_CHARCONV_ERROR_MEMORY;
  if (err == U_FILE_ACCESS_ERROR || err == U_MISSING_RESOURCE_ERROR ||
      err == U_INVALID_TABLE_FORMAT || err == U_UNSUPPORTED_ERROR)
    return MAIL_CHARCONV_ERROR_UNKNOWN_CHARSET;
  return MAIL_CHARCONV_ERROR_CONV;
}

static UConverter * icu_open_charset(const char * name, UErrorCode * err)
{
  size_t index;
  for (index = 0; index < sizeof(charconv_icu_aliases) /
      sizeof(charconv_icu_aliases[0]); index++) {
    if (strcasecmp(name, charconv_icu_aliases[index].name) == 0) {
      if (charconv_icu_aliases[index].packaged) {
        /* ICU serializes registration; repeated calls are harmless warnings.
         * Our static image also survives an application's u_cleanup(). */
        udata_setAppData("libetpan_charsets", charconv_icu_data.bytes, err);
        if (U_FAILURE(*err))
          return NULL;
        *err = U_ZERO_ERROR;
        return ucnv_openPackage("libetpan_charsets",
            charconv_icu_aliases[index].target, err);
      }
      return ucnv_open(charconv_icu_aliases[index].target, err);
    }
  }
  if (strcasecmp(name, "UCS-2BE") == 0)
    name = "UTF-16BE";
  else if (strcasecmp(name, "UCS-2LE") == 0)
    name = "UTF-16LE";
  else if (strcasecmp(name, "UCS-4BE") == 0)
    name = "UTF-32BE";
  else if (strcasecmp(name, "UCS-4LE") == 0)
    name = "UTF-32LE";
  else if (strcasecmp(name, "WCHAR_T") == 0) {
    if (sizeof(wchar_t) != 4) {
      *err = U_UNSUPPORTED_ERROR;
      return NULL;
    }
    name = U_IS_BIG_ENDIAN ? "UTF-32BE" : "UTF-32LE";
  }
  return ucnv_open(name, err);
}

static int icu_is_ucs2(const char * name)
{
  return strcasecmp(name, "UCS-2BE") == 0 ||
      strcasecmp(name, "UCS-2LE") == 0;
}

static int icu_charconv(const char * tocode, const char * fromcode,
    const char * str, size_t length, char ** result, size_t * result_len)
{
  UErrorCode err = U_ZERO_ERROR;
  UConverter * source = NULL;
  UConverter * target = NULL;
  UChar * unicode = NULL;
  char * output = NULL;
  int32_t unicode_len, output_len;
  int res = MAIL_CHARCONV_NO_ERROR;
  size_t index;

  if (length > (size_t) INT_MAX)
    return MAIL_CHARCONV_ERROR_MEMORY;
  source = icu_open_charset(fromcode, &err);
  if (U_FAILURE(err)) {
    res = icu_error(err);
    goto done;
  }
  err = U_ZERO_ERROR;
  target = icu_open_charset(tocode, &err);
  if (U_FAILURE(err)) {
    res = icu_error(err);
    goto done;
  }
  if (icu_is_ucs2(fromcode)) {
    int big = strcasecmp(fromcode, "UCS-2BE") == 0;
    if (length % 2 != 0) {
      res = MAIL_CHARCONV_ERROR_CONV;
      goto done;
    }
    for (index = 0; index < length; index += 2) {
      unsigned int high = (unsigned char) str[index + (big ? 0 : 1)];
      if (high >= 0xd8 && high <= 0xdf) {
        res = MAIL_CHARCONV_ERROR_CONV;
        goto done;
      }
    }
  }
  err = U_ZERO_ERROR;
  unicode_len = ucnv_toUChars(source, NULL, 0, str, (int32_t) length, &err);
  if (err != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(err)) {
    res = icu_error(err);
    goto done;
  }
  if (unicode_len < 0 || unicode_len == INT_MAX ||
      (size_t) unicode_len + 1 > ((size_t) -1) / sizeof(UChar)) {
    res = MAIL_CHARCONV_ERROR_MEMORY;
    goto done;
  }
  unicode = malloc(((size_t) unicode_len + 1) * sizeof(UChar));
  if (unicode == NULL) {
    res = MAIL_CHARCONV_ERROR_MEMORY;
    goto done;
  }
  err = U_ZERO_ERROR;
  unicode_len = ucnv_toUChars(source, unicode, unicode_len + 1, str,
      (int32_t) length, &err);
  if (U_FAILURE(err)) {
    res = icu_error(err);
    goto done;
  }
  if (icu_is_ucs2(tocode)) {
    for (index = 0; index < (size_t) unicode_len; index++) {
      if (unicode[index] >= 0xd800 && unicode[index] <= 0xdfff) {
        res = MAIL_CHARCONV_ERROR_CONV;
        goto done;
      }
    }
  }
  err = U_ZERO_ERROR;
  output_len = ucnv_fromUChars(target, NULL, 0, unicode, unicode_len, &err);
  if (err != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(err)) {
    res = icu_error(err);
    goto done;
  }
  if (output_len < 0 || output_len == INT_MAX) {
    res = MAIL_CHARCONV_ERROR_MEMORY;
    goto done;
  }
  output = malloc((size_t) output_len + 1);
  if (output == NULL) {
    res = MAIL_CHARCONV_ERROR_MEMORY;
    goto done;
  }
  err = U_ZERO_ERROR;
  output_len = ucnv_fromUChars(target, output, output_len + 1, unicode,
      unicode_len, &err);
  if (U_FAILURE(err)) {
    res = icu_error(err);
    goto done;
  }
  output[output_len] = '\0';
  *result = output;
  *result_len = (size_t) output_len;
  output = NULL;
 done:
  free(output);
  free(unicode);
  if (source != NULL)
    ucnv_close(source);
  if (target != NULL)
    ucnv_close(target);
  return res;
}
#endif

static const char * get_valid_charset(const char * fromcode)
{
  if ((strcasecmp(fromcode, "GB2312") == 0) || (strcasecmp(fromcode, "GB_2312-80") == 0)) {
    fromcode = "GBK";
  }
  else if ((strcasecmp(fromcode, "iso-8859-8-i") == 0) || (strcasecmp(fromcode, "iso_8859-8-i") == 0) ||
           (strcasecmp(fromcode, "iso8859-8-i") == 0)) {
    fromcode = "iso-8859-8";
  }
  else if ((strcasecmp(fromcode, "iso-8859-8-e") == 0) || (strcasecmp(fromcode, "iso_8859-8-e") == 0) ||
           (strcasecmp(fromcode, "iso8859-8-e") == 0)) {
    fromcode = "iso-8859-8";
  }
  else if (strcasecmp(fromcode, "ks_c_5601-1987") == 0) {
    fromcode = "euckr";
  }
  else if (strcasecmp(fromcode, "koi8_r") == 0) {
    fromcode = "koi8-r";
  }
  else if (strcasecmp(fromcode, "iso-2022-jp") == 0) {
    fromcode = "iso-2022-jp-2";
  }
  
  return fromcode;
}

LIBETPAN_EXPORT
int charconv(const char * tocode, const char * fromcode,
    const char * str, size_t length,
    char ** result)
{
#ifdef HAVE_ICONV
	iconv_t conv;
	size_t r;
	char * pout;
	size_t out_size;
	size_t old_out_size;
	size_t count;
#endif
	char * out;
	int res;

  fromcode = get_valid_charset(fromcode);
  
	if (extended_charconv != NULL) {
		size_t		allocated_length;
		size_t		result_length;
		res = charconv_get_output_size(length, &allocated_length);
		if (res != MAIL_CHARCONV_NO_ERROR)
			return res;
		result_length = allocated_length;
		*result = malloc(allocated_length + 1);
		if (*result == NULL) {
			res = MAIL_CHARCONV_ERROR_MEMORY;
		} else {
			res = (*extended_charconv)( tocode, fromcode, str, length, *result, &result_length);
			if (res != MAIL_CHARCONV_NO_ERROR) {
				free( *result);
				*result = NULL;
			} else {
				if (result_length > allocated_length) {
					free(*result);
					return MAIL_CHARCONV_ERROR_MEMORY;
				}
				out = realloc( *result, result_length + 1);
				if (out != NULL) *result = out;
				/* also a cstring, just in case */
				(*result)[result_length] = '\0';
			}
		}
		if (res != MAIL_CHARCONV_ERROR_UNKNOWN_CHARSET)
			return res;
		/* Try the built-in backends for an unsupported charset. */
	}

  {
    size_t converted_length;
    res = mutf7_charconv(tocode, fromcode, str, length, result,
        &converted_length);
    if (res != MAIL_CHARCONV_ERROR_UNKNOWN_CHARSET)
      return res;
  }

#ifdef HAVE_COREFOUNDATION_CHARCONV
	if (apple_should_try_charset(fromcode))
	{
		size_t allocated_length;
		size_t result_length;

		res = charconv_get_output_size(length, &allocated_length);
		if (res != MAIL_CHARCONV_NO_ERROR)
			return res;
		result_length = allocated_length;
		*result = malloc(allocated_length + 1);
		if (*result == NULL)
			return MAIL_CHARCONV_ERROR_MEMORY;
		res = apple_charconv(tocode, fromcode, str, length, *result,
		    &result_length);
		if (res == MAIL_CHARCONV_NO_ERROR) {
			out = realloc(*result, result_length + 1);
			if (out != NULL)
				*result = out;
			return MAIL_CHARCONV_NO_ERROR;
		}
		free(*result);
		*result = NULL;
		if (res != MAIL_CHARCONV_ERROR_UNKNOWN_CHARSET)
			return res;
	}
#endif

#ifdef HAVE_ICU
  if (icu_should_try_charset(fromcode)) {
    size_t result_length;
    res = icu_charconv(tocode, fromcode, str, length, result, &result_length);
    if (res != MAIL_CHARCONV_ERROR_UNKNOWN_CHARSET)
      return res;
  }
#endif

#ifndef HAVE_ICONV
  return MAIL_CHARCONV_ERROR_UNKNOWN_CHARSET;
#else
  
  conv = iconv_open(tocode, fromcode);
  if (conv == (iconv_t) -1) {
    res = MAIL_CHARCONV_ERROR_UNKNOWN_CHARSET;
    goto err;
  }

  res = charconv_get_output_size(length, &out_size); /* UTF-8 can be encoded up to 6 bytes */
  if (res != MAIL_CHARCONV_NO_ERROR)
    goto close_iconv;

  out = malloc(out_size + 1);
  if (out == NULL) {
    res = MAIL_CHARCONV_ERROR_MEMORY;
    goto close_iconv;
  }

  pout = out;
  old_out_size = out_size;

  r = mail_iconv(conv, &str, &length, &pout, &out_size, NULL, "?");

  if (r == (size_t) -1) {
    res = MAIL_CHARCONV_ERROR_CONV;
    goto free;
  }

  iconv_close(conv);

  * pout = '\0';
  count = old_out_size - out_size;
  pout = realloc(out, count + 1);
  if (pout != NULL)
    out = pout;

  * result = out;

  return MAIL_CHARCONV_NO_ERROR;

 free:
  free(out);
 close_iconv:
  iconv_close(conv);
 err:
  return res;
#endif
}

LIBETPAN_EXPORT
int charconv_buffer(const char * tocode, const char * fromcode,
		    const char * str, size_t length,
		    char ** result, size_t * result_len)
{
#ifdef HAVE_ICONV
	iconv_t conv;
	size_t iconv_r;
	int r;
	char * out;
	char * pout;
	size_t out_size;
	size_t old_out_size;
	size_t count;
#endif
	int res;
	MMAPString * mmapstr;

  fromcode = get_valid_charset(fromcode);
  
	if (extended_charconv != NULL) {
		size_t		allocated_length;
		size_t		result_length;
		res = charconv_get_output_size(length, &allocated_length);
		if (res != MAIL_CHARCONV_NO_ERROR)
			return res;
		result_length = allocated_length;
		mmapstr = mmap_string_sized_new( allocated_length + 1);
		*result_len = 0;
		if (mmapstr == NULL) {
			return MAIL_CHARCONV_ERROR_MEMORY;
		} else {
			res = (*extended_charconv)( tocode, fromcode, str, length, mmapstr->str, &result_length);
			if (res != MAIL_CHARCONV_ERROR_UNKNOWN_CHARSET) {
				if (res == MAIL_CHARCONV_NO_ERROR) {
					if (result_length > allocated_length) {
						res = MAIL_CHARCONV_ERROR_MEMORY;
						mmap_string_free(mmapstr);
						return res;
					}
					*result = mmapstr->str;
					res = mmap_string_ref(mmapstr);
					if (res < 0) {
						res = MAIL_CHARCONV_ERROR_MEMORY;
						mmap_string_free(mmapstr);
					} else {
						mmap_string_set_size( mmapstr, result_length);	/* can't fail */
						*result_len = result_length;
					}
				}
                else {
                    mmap_string_free(mmapstr);
                }
			}
            else {
                mmap_string_free(mmapstr);
            }
			if (res != MAIL_CHARCONV_ERROR_UNKNOWN_CHARSET)
				return res;
		}
		/* Try the built-in backends for an unsupported charset. */
	}

  {
    char * converted = NULL;
    size_t converted_length;
    res = mutf7_charconv(tocode, fromcode, str, length, &converted,
        &converted_length);
    if (res == MAIL_CHARCONV_NO_ERROR) {
      mmapstr = mmap_string_new_len(converted, converted_length);
      free(converted);
      if (mmapstr == NULL)
        return MAIL_CHARCONV_ERROR_MEMORY;
      if (mmap_string_ref(mmapstr) < 0) {
        mmap_string_free(mmapstr);
        return MAIL_CHARCONV_ERROR_MEMORY;
      }
      *result = mmapstr->str;
      *result_len = converted_length;
      return MAIL_CHARCONV_NO_ERROR;
    }
    if (res != MAIL_CHARCONV_ERROR_UNKNOWN_CHARSET)
      return res;
  }

#ifdef HAVE_COREFOUNDATION_CHARCONV
	if (apple_should_try_charset(fromcode))
	{
		size_t allocated_length;
		size_t result_length;

		res = charconv_get_output_size(length, &allocated_length);
		if (res != MAIL_CHARCONV_NO_ERROR)
			return res;
		result_length = allocated_length;
		mmapstr = mmap_string_sized_new(allocated_length + 1);
		*result_len = 0;
		if (mmapstr == NULL)
			return MAIL_CHARCONV_ERROR_MEMORY;
		res = apple_charconv(tocode, fromcode, str, length, mmapstr->str,
		    &result_length);
		if (res == MAIL_CHARCONV_NO_ERROR) {
			int r;

			*result = mmapstr->str;
			r = mmap_string_ref(mmapstr);
			if (r < 0) {
				mmap_string_free(mmapstr);
				return MAIL_CHARCONV_ERROR_MEMORY;
			}
			mmap_string_set_size(mmapstr, result_length);
			*result_len = result_length;
			return MAIL_CHARCONV_NO_ERROR;
		}
		mmap_string_free(mmapstr);
		if (res != MAIL_CHARCONV_ERROR_UNKNOWN_CHARSET)
			return res;
	}
#endif

#ifdef HAVE_ICU
  if (icu_should_try_charset(fromcode)) {
    char * converted = NULL;
    size_t converted_length;
    res = icu_charconv(tocode, fromcode, str, length, &converted,
        &converted_length);
    if (res == MAIL_CHARCONV_NO_ERROR) {
      mmapstr = mmap_string_new_len(converted, converted_length);
      free(converted);
      if (mmapstr == NULL)
        return MAIL_CHARCONV_ERROR_MEMORY;
      if (mmap_string_ref(mmapstr) < 0) {
        mmap_string_free(mmapstr);
        return MAIL_CHARCONV_ERROR_MEMORY;
      }
      *result = mmapstr->str;
      *result_len = converted_length;
      return MAIL_CHARCONV_NO_ERROR;
    }
    if (res != MAIL_CHARCONV_ERROR_UNKNOWN_CHARSET)
      return res;
  }
#endif

#ifndef HAVE_ICONV
  return MAIL_CHARCONV_ERROR_UNKNOWN_CHARSET;
#else

  conv = iconv_open(tocode, fromcode);
  if (conv == (iconv_t) -1) {
    res = MAIL_CHARCONV_ERROR_UNKNOWN_CHARSET;
    goto err;
  }

  res = charconv_get_output_size(length, &out_size); /* UTF-8 can be encoded up to 6 bytes */
  if (res != MAIL_CHARCONV_NO_ERROR)
    goto close_iconv;

  mmapstr = mmap_string_sized_new(out_size + 1);
  if (mmapstr == NULL) {
    res = MAIL_CHARCONV_ERROR_MEMORY;
    goto close_iconv;
  }

  out = mmapstr->str;

  pout = out;
  old_out_size = out_size;

  iconv_r = mail_iconv(conv, &str, &length, &pout, &out_size, NULL, "?");

  if (iconv_r == (size_t) -1) {
    res = MAIL_CHARCONV_ERROR_CONV;
    goto free;
  }

  iconv_close(conv);

  * pout = '\0';

  count = old_out_size - out_size;

  r = mmap_string_ref(mmapstr);
  if (r < 0) {
    res = MAIL_CHARCONV_ERROR_MEMORY;
    mmap_string_free(mmapstr);
    goto err;
  }

  * result = out;
  * result_len = count;

  return MAIL_CHARCONV_NO_ERROR;

 free:
  mmap_string_free(mmapstr);
 close_iconv:
  iconv_close(conv);
 err:
  return res;
#endif
}

LIBETPAN_EXPORT
void charconv_buffer_free(char * str)
{
  mmap_string_unref(str);
}
