/*
 * libEtPan! -- a mail stuff library
 */

#ifndef MAILGMAIL_URL_H

#define MAILGMAIL_URL_H

#include "mailgmail_types.h"

int mailgmail_url_messages_list(mailgmail * session,
    struct mailgmail_message_list_request * request,
    char ** result);

int mailgmail_url_message_get(mailgmail * session,
    const char * message_id,
    struct mailgmail_message_get_request * request,
    char ** result);

int mailgmail_url_labels_list(mailgmail * session, char ** result);

int mailgmail_url_label_get(mailgmail * session,
    const char * label_id,
    char ** result);

int mailgmail_url_profile_get(mailgmail * session, char ** result);

int mailgmail_url_attachment_get(mailgmail * session,
    const char * message_id,
    const char * attachment_id,
    char ** result);

#endif
