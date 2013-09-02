#ifndef MAILSMTP_OAUTH2_H

#define MAILSMTP_OAUTH2_H

#include <libetpan/mailsmtp_types.h>

LIBETPAN_EXPORT
int mailsmtp_oauth2_authenticate(mailsmtp * session, const char * auth_user,
    const char * access_token);

#endif
