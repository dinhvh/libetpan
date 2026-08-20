/*
 * libEtPan! -- a mail stuff library
 */

#ifndef MAILGMAIL_H

#define MAILGMAIL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libetpan/mailgmail_types.h>

struct mailhttp_transport;

LIBETPAN_EXPORT
mailgmail * mailgmail_new(void);

LIBETPAN_EXPORT
void mailgmail_free(mailgmail * session);

/* Takes ownership of transport, whether wrapping succeeds or fails. */
LIBETPAN_EXPORT
int mailgmail_set_http_transport(mailgmail * session,
    struct mailhttp_transport * transport);

LIBETPAN_EXPORT
int mailgmail_set_user(mailgmail * session, const char * user_id);

LIBETPAN_EXPORT
int mailgmail_set_oauth2_token(mailgmail * session,
    const char * access_token);

LIBETPAN_EXPORT
int mailgmail_set_user_agent(mailgmail * session,
    const char * user_agent);

LIBETPAN_EXPORT
int mailgmail_set_timeout(mailgmail * session, time_t timeout);

LIBETPAN_EXPORT
int mailgmail_get_last_http_status(mailgmail * session);

LIBETPAN_EXPORT
const char * mailgmail_get_last_error_message(mailgmail * session);

LIBETPAN_EXPORT
int mailgmail_get_profile(mailgmail * session,
    struct mailgmail_profile ** result);

LIBETPAN_EXPORT
int mailgmail_list_labels(mailgmail * session,
    struct mailgmail_label_list ** result);

LIBETPAN_EXPORT
int mailgmail_get_label(mailgmail * session,
    const char * label_id,
    struct mailgmail_label ** result);

LIBETPAN_EXPORT
int mailgmail_list_messages(mailgmail * session,
    struct mailgmail_message_list_request * request,
    struct mailgmail_message_list ** result);

LIBETPAN_EXPORT
int mailgmail_get_message(mailgmail * session,
    const char * message_id,
    struct mailgmail_message_get_request * request,
    struct mailgmail_message ** result);

LIBETPAN_EXPORT
int mailgmail_get_attachment(mailgmail * session,
    const char * message_id,
    const char * attachment_id,
    struct mailgmail_attachment ** result);

#ifdef __cplusplus
}
#endif

#endif
