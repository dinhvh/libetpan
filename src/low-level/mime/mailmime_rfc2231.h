/*
 * libEtPan! -- a mail stuff library
 */

#ifndef MAILMIME_RFC2231_H

#define MAILMIME_RFC2231_H

#include <libetpan/clist.h>

int mailmime_rfc2231_normalize_parameters(
    clist /* struct mailmime_parameter * */ * parameters);

int mailmime_rfc2231_normalize_disposition_parameters(
    clist /* struct mailmime_disposition_parm * */ * parameters);

int mailmime_rfc2231_parameter_has_base(const char * name,
    const char * base_name);

#endif
