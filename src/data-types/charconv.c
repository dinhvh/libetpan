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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

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

LIBETPAN_EXPORT
char * charconv_decode_mutf7(const char * str)
{
  size_t input_len;
  char * output;
  size_t output_len;
  size_t i;

  if (str == NULL)
    return NULL;

  input_len = strlen(str);
  output = malloc(input_len * 4 + 1);
  if (output == NULL)
    return NULL;

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
      if (bytes == NULL)
        goto err;
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
  return output;

 err:
  free(output);
  return NULL;
}

static int mutf7_output_reserve(char ** output, size_t * output_size,
    size_t output_len, size_t needed)
{
  char * new_output;
  size_t new_size;

  if (output_len + needed + 1 <= *output_size)
    return 0;

  new_size = *output_size;
  while (output_len + needed + 1 > new_size)
    new_size *= 2;

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

LIBETPAN_EXPORT
char * charconv_encode_mutf7(const char * str)
{
  char * output;
  size_t output_size;
  size_t output_len;
  size_t input_len;
  size_t i;
  int in_shift;
  unsigned int bits;
  unsigned int bit_count;

  if (str == NULL)
    return NULL;

  input_len = strlen(str);
  output_size = input_len * 2 + 16;
  output = malloc(output_size);
  if (output == NULL)
    return NULL;

  output_len = 0;
  i = 0;
  in_shift = 0;
  bits = 0;
  bit_count = 0;

  while (i < input_len) {
    unsigned int cp;

    if (mutf7_decode_utf8_char(str, input_len, &i, &cp) < 0)
      goto err;

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
  return output;

 err:
  free(output);
  return NULL;
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
		size_t		result_length;
		result_length = length * 6;
		*result = malloc( length * 6 + 1);
		if (*result == NULL) {
			res = MAIL_CHARCONV_ERROR_MEMORY;
		} else {
			res = (*extended_charconv)( tocode, fromcode, str, length, *result, &result_length);
			if (res != MAIL_CHARCONV_NO_ERROR) {
				free( *result);
				*result = NULL;
			} else {
				out = realloc( *result, result_length + 1);
				if (out != NULL) *result = out;
				/* also a cstring, just in case */
				(*result)[result_length] = '\0';
			}
		}
		if (res != MAIL_CHARCONV_ERROR_UNKNOWN_CHARSET)
			return res;
		/* else, let's try with iconv, if available */
	}

#ifndef HAVE_ICONV
  return MAIL_CHARCONV_ERROR_UNKNOWN_CHARSET;
#else
  
  conv = iconv_open(tocode, fromcode);
  if (conv == (iconv_t) -1) {
    res = MAIL_CHARCONV_ERROR_UNKNOWN_CHARSET;
    goto err;
  }

  out_size = 6 * length; /* UTF-8 can be encoded up to 6 bytes */

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
		size_t		result_length;
		result_length = length * 6;
		mmapstr = mmap_string_sized_new( result_length + 1);
		*result_len = 0;
		if (mmapstr == NULL) {
			res = MAIL_CHARCONV_ERROR_MEMORY;
		} else {
			res = (*extended_charconv)( tocode, fromcode, str, length, mmapstr->str, &result_length);
			if (res != MAIL_CHARCONV_ERROR_UNKNOWN_CHARSET) {
				if (res == MAIL_CHARCONV_NO_ERROR) {
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
			return res;
		}
		/* else, let's try with iconv, if available */
	}

#ifndef HAVE_ICONV
  return MAIL_CHARCONV_ERROR_UNKNOWN_CHARSET;
#else

  conv = iconv_open(tocode, fromcode);
  if (conv == (iconv_t) -1) {
    res = MAIL_CHARCONV_ERROR_UNKNOWN_CHARSET;
    goto err;
  }

  out_size = 6 * length; /* UTF-8 can be encoded up to 6 bytes */

  mmapstr = mmap_string_sized_new(out_size + 1);
  if (mmapstr == NULL) {
    res = MAIL_CHARCONV_ERROR_MEMORY;
    goto err;
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
    goto free;
  }

  * result = out;
  * result_len = count;

  return MAIL_CHARCONV_NO_ERROR;

 free:
  mmap_string_free(mmapstr);
 err:
  return res;
#endif
}

LIBETPAN_EXPORT
void charconv_buffer_free(char * str)
{
  mmap_string_unref(str);
}
