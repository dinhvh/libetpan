/*
 * libEtPan! -- a mail library
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
 * $Id: mailprivacy_smime.h,v 1.5 2007/10/30 00:40:39 hoa Exp $
 */

#ifndef MAILPRIVACY_SMIME_H

#define MAILPRIVACY_SMIME_H

#include <libetpan/mailprivacy_types.h>

LIBETPAN_EXPORT
int mailprivacy_smime_init(struct mailprivacy * privacy);

LIBETPAN_EXPORT
void mailprivacy_smime_done(struct mailprivacy * privacy);

LIBETPAN_EXPORT
void mailprivacy_smime_set_cert_dir(struct mailprivacy * privacy,
    char * directory);


/*
  set directory where certificates of authority certifications are
  stored.
*/

LIBETPAN_EXPORT
void mailprivacy_smime_set_CA_dir(struct mailprivacy * privacy,
    char * directory);


/*
  to disable the verification of signers certificates of a
  signed message.
*/

LIBETPAN_EXPORT
void mailprivacy_smime_set_CA_check(struct mailprivacy * privacy,
    int enabled);


/*
  to store certificates of signed messages
*/

LIBETPAN_EXPORT
void mailprivacy_smime_set_store_cert(struct mailprivacy * privacy,
    int enabled);

/*
  set directory where private keys are stored.
  name of the files in that directory must be in form :
  [email-address]-private-key.pem
*/

LIBETPAN_EXPORT
void mailprivacy_smime_set_private_keys_dir(struct mailprivacy * privacy,
    char * directory);


LIBETPAN_EXPORT
clist * mailprivacy_smime_encryption_id_list(struct mailprivacy * privacy,
    mailmessage * msg);

LIBETPAN_EXPORT
void mailprivacy_smime_encryption_id_list_clear(struct mailprivacy * privacy,
    mailmessage * msg);

LIBETPAN_EXPORT
int mailprivacy_smime_set_encryption_id(struct mailprivacy * privacy,
    char * user_id, char * passphrase);

#endif
