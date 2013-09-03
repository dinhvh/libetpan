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
 * $Id: imapdriver_tools.h,v 1.9 2007/08/08 21:33:30 hoa Exp $
 */

#ifndef IMAPDRIVER_TOOLS_H

#define IMAPDRIVER_TOOLS_H

#include "mailimap.h"
#include "mailmime.h"
#include "imapdriver_types.h"

#ifdef __cplusplus
extern "C" {
#endif

int imap_list_to_list(clist * imap_list, struct mail_list ** result);

int
imap_section_to_imap_section(struct mailmime_section * section, int type,
    struct mailimap_section ** result);

int imap_get_msg_att_info(struct mailimap_msg_att * msg_att,
    uint32_t * puid,
    struct mailimap_envelope ** pimap_envelope,
    char ** preferences,
    size_t * pref_size,
    struct mailimap_msg_att_dynamic ** patt_dyn,
    struct mailimap_body ** pimap_body);

int imap_add_envelope_fetch_att(struct mailimap_fetch_type * fetch_type);

int imap_env_to_fields(struct mailimap_envelope * env,
    char * ref_str, size_t ref_size,
    struct mailimf_fields ** result);

int
imap_fetch_result_to_envelop_list(clist * fetch_result,
    struct mailmessage_list * env_list);

int imap_body_to_body(struct mailimap_body * imap_body,
    struct mailmime ** result);

int imap_msg_list_to_imap_set(clist * msg_list,
    struct mailimap_set ** result);

int imap_flags_to_imap_flags(struct mail_flags * flags,
    struct mailimap_flag_list ** result);

int imap_flags_to_flags(struct mailimap_msg_att_dynamic * att_dyn,
    struct mail_flags ** result);

#ifdef __cplusplus
}
#endif

#endif
