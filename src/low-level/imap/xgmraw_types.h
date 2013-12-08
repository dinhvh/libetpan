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

#ifndef XGMRAW_TYPES_H

#define XGMRAW_TYPES_H

#include <libetpan/clist.h>

/* this is the condition of the SEARCH operation */

enum {
  XGMRAW_SEARCH_KEY_ALL,        /* all messages */
  XGMRAW_SEARCH_KEY_ANYWHERE,   /* all messages except spam and trash */
  XGMRAW_SEARCH_KEY_BCC,        /* messages whose Bcc field contains the
                                   given string */
  XGMRAW_SEARCH_KEY_BEFORE,     /* messages whose date is earlier
                                   than the specified date */
  XGMRAW_SEARCH_KEY_CC,         /* messages whose Cc field contains the
                                   given string */
  XGMRAW_SEARCH_KEY_FILENAME,   /* messages with an attachment named like the
                                   given string */
  XGMRAW_SEARCH_KEY_FLAGGED,    /* messages with a flag in Gmail */
  XGMRAW_SEARCH_KEY_FROM,       /* messages whose From field contains the
                                   given string */
  XGMRAW_SEARCH_KEY_HAS,        /* messages that have a given attribute */
  XGMRAW_SEARCH_KEY_IN,         /* messages in a given mailbox */
  XGMRAW_SEARCH_KEY_KEYWORD,    /* messages containing the keyword */
  XGMRAW_SEARCH_KEY_LABEL,      /* messages in a given label */
  XGMRAW_SEARCH_KEY_READ,       /* messages marked read in Gmail */
  XGMRAW_SEARCH_KEY_SINCE,      /* messages whose internal date is later
                                   than specified date */
  XGMRAW_SEARCH_KEY_STARRED,    /* messages with a star in Gmail */
  XGMRAW_SEARCH_KEY_SUBJECT,    /* messages whose Subject field contains the
                                   given string */
  XGMRAW_SEARCH_KEY_TO,         /* messages whose To field contains the
                                   given string */
  XGMRAW_SEARCH_KEY_UNREAD,     /* messages marked unread in Gmail */
  XGMRAW_SEARCH_KEY_NOT,        /* not operation of the condition */
  XGMRAW_SEARCH_KEY_OR,         /* or operation between two conditions */
  XGMRAW_SEARCH_KEY_MULTIPLE,   /* the boolean operator between the
                                   conditions is AND */
};

struct xgmraw_search_key {
  int sk_type;
  union {
    char * sk_bcc;
    struct mailimap_date * sk_before;
    char * sk_cc;
    char * sk_filename;
    char * sk_from;
    char * sk_has;
    char * sk_in;
    char * sk_keyword;
    char * sk_label;
    struct mailimap_date * sk_since;
    char * sk_subject;
    char * sk_to;
    struct xgmraw_search_key * sk_not;
    struct {
      struct xgmraw_search_key * sk_or1;
      struct xgmraw_search_key * sk_or2;
    } sk_or;
    clist * sk_multiple; /* list of (struct xgmraw_search_key *) */
  } sk_data;
};

LIBETPAN_EXPORT
struct xgmraw_search_key *
xgmraw_search_key_new(int sk_type,
                      char * sk_bcc, struct mailimap_date * sk_before, char * sk_cc,
                      char * sk_filename, char * sk_from, char * sk_has, char * sk_in,
                      char * sk_keyword, char * sk_label, struct mailimap_date * sk_since,
                      char * sk_subject, char * sk_to, struct xgmraw_search_key * sk_not,
                      struct xgmraw_search_key * sk_or1,
                      struct xgmraw_search_key * sk_or2,
                      clist * sk_multiple);

LIBETPAN_EXPORT
void xgmraw_search_key_free(struct xgmraw_search_key * key);

#endif
