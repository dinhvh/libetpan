/*
 * libEtPan! -- a mail stuff library
 *
 * Copyright (C) 2015, DINH Viet Hoa, Volker Birk
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
 * $Id: quoted_printable.c,v 1.0 2015/03/08 20:56:00 fdik Exp $
 */

#include "quoted_printable.h"

#include <string.h>
#include <stdio.h>

#include "mmapstring.h"

static inline int to_be_quoted(const char * word, size_t size, int subject)
{
  int do_quote;
  const char * cur;
  size_t i;

  do_quote = 0;
  cur = word;

  for(i = 0 ; i < size ; i ++) {
    if (* cur == '=')
      do_quote = 1;

    if (!subject) {
      switch (* cur) {
        case ',':
        case ':':
        case '!':
        case '"':
        case '#':
        case '$':
        case '@':
        case '[':
        case '\\':
        case ']':
        case '^':
        case '`':
        case '{':
        case '|':
        case '}':
        case '~':
        case '=':
        case '?':
        case '_':
          do_quote = 1;
          break;
      }
    }
    if (((unsigned char) * cur) >= 128)
      do_quote = 1;
    cur ++;
  }

  return do_quote;
}

#define MAX_IMF_LINE 72

static inline void quote_word(const char * display_charset, MMAPString *
    mmapstr, const char * word, size_t size)
{
  const char * cur;
  size_t i;
  char hex[4];
  int col;

  mmap_string_append(mmapstr, "=?");
  mmap_string_append(mmapstr, display_charset);
  mmap_string_append(mmapstr, "?Q?");

  col = (int) mmapstr->len;
  cur = word;

  for(i = 0 ; i < size ; i ++) {
    int do_quote_char;
    do_quote_char = 0;

    switch (* cur) {
      case ',':
      case ':':
      case '!':
      case '"':
      case '#':
      case '$':
      case '@':
      case '[':
      case '\\':
      case ']':
      case '^':
      case '`':
      case '{':
      case '|':
      case '}':
      case '~':
      case '=':
      case '?':
      case '_':
        do_quote_char = 1;
        break;
      default:
        if (((unsigned char) * cur) >= 128)
          do_quote_char = 1;
        break;
    }

    if (do_quote_char) {
      snprintf(hex, 4, "=%2.2X", (unsigned char) * cur);
      mmap_string_append(mmapstr, hex);
      col += 3;
    }
    else {
      if (* cur == ' ') {
        mmap_string_append_c(mmapstr, '_');
      }
      else {
        mmap_string_append_c(mmapstr, * cur);
      }

      col += 3;
    }

    cur ++;
  }

  mmap_string_append(mmapstr, "?=");
}

static inline void get_word(const char * begin, const char ** pend, int
    subject, int * pto_be_quoted)
{
  const char * cur;

  cur = begin;
  while ((* cur != ' ') && (* cur != '\t') && (* cur != '\0')) {
    cur ++;
  }

  while (((* cur == ' ') || (* cur == '\t')) && (* cur != '\0')) {
    cur ++;
  }

  if (cur - begin + 1 /* minimum column of string in a folded header */
      > MAX_IMF_LINE)
    * pto_be_quoted = 1;
  else
    * pto_be_quoted = to_be_quoted(begin, cur - begin, subject);

  * pend = cur;
}

LIBETPAN_EXPORT
char * etpan_make_full_quoted_printable(const char * display_charset,
    const char * phrase)
{
  int needs_quote;
  char * str;

  needs_quote = to_be_quoted(phrase, strlen(phrase), 0);

  if (needs_quote) {
    MMAPString * mmapstr;
    mmapstr = mmap_string_new("");
    quote_word(display_charset, mmapstr, phrase, strlen(phrase));
    str = strdup(mmapstr->str);
    mmap_string_free(mmapstr);
  }
  else {
    str = strdup(phrase);
  }

  return str;
}

LIBETPAN_EXPORT
char * etpan_make_quoted_printable(const char * display_charset, const
    char * phrase, int subject)
{
  char * str;
  const char * cur;
  MMAPString * mmapstr;

  mmapstr = mmap_string_new("");

  cur = phrase;
  while (* cur != '\0') {
    const char * begin;
    const char * end;
    int do_quote;
    int quote_words;

    begin = cur;
    end = begin;
    quote_words = 0;
    do_quote = 1;

    while (* cur != '\0') {
      get_word(cur, &cur, subject, &do_quote);
      if (do_quote) {
        quote_words = 1;
        end = cur;
      }
      else
        break;
      if (* cur != '\0')
        cur ++;
    }

    if (quote_words) {
      quote_word(display_charset, mmapstr, begin, end - begin);
      if ((* end == ' ') || (* end == '\t')) {
        mmap_string_append_c(mmapstr, * end);
        end ++;
      }
      if (* end != '\0') {
        mmap_string_append_len(mmapstr, end, cur - end);
      }
    }
    else {
      mmap_string_append_len(mmapstr, begin, cur - begin);
    }

    if ((* cur == ' ') || (* cur == '\t')) {
      mmap_string_append_c(mmapstr, * cur);
      cur ++;
    }
  }

  str = strdup(mmapstr->str);
  mmap_string_free(mmapstr);

  return str;
}

