/*
 * libEtPan! -- a mail stuff library
 */

#ifndef MAILACTIVESYNC_H

#define MAILACTIVESYNC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libetpan/mailactivesync_types.h>
#include <libetpan/mailactivesync_http.h>

LIBETPAN_EXPORT
mailactivesync * mailactivesync_new(int cached,
    const char * cache_directory);

LIBETPAN_EXPORT
void mailactivesync_free(mailactivesync * session);

LIBETPAN_EXPORT
int mailactivesync_connect(mailactivesync * session,
    const char * server_url);

LIBETPAN_EXPORT
int mailactivesync_set_device(mailactivesync * session,
    const char * device_id,
    const char * device_type);

LIBETPAN_EXPORT
int mailactivesync_set_protocol_version(mailactivesync * session,
    const char * version);

LIBETPAN_EXPORT
int mailactivesync_set_policy_key(mailactivesync * session,
    const char * policy_key);

LIBETPAN_EXPORT
int mailactivesync_set_http_transport(mailactivesync * session,
    struct mailactivesync_http_transport * transport);

LIBETPAN_EXPORT
int mailactivesync_login(mailactivesync * session,
    const char * user,
    const char * password);

LIBETPAN_EXPORT
int mailactivesync_login_oauth2(mailactivesync * session,
    const char * user,
    const char * access_token);

LIBETPAN_EXPORT
int mailactivesync_options(mailactivesync * session,
    struct mailactivesync_options ** result);

LIBETPAN_EXPORT
int mailactivesync_folder_sync(mailactivesync * session,
    const char * sync_key,
    struct mailactivesync_folder_sync_result ** result);

LIBETPAN_EXPORT
int mailactivesync_sync(mailactivesync * session,
    struct mailactivesync_sync_request * request,
    struct mailactivesync_sync_result ** result);

LIBETPAN_EXPORT
int mailactivesync_item_operations_fetch(mailactivesync * session,
    const char * collection_id,
    const char * server_id,
    struct mailactivesync_item ** result);

LIBETPAN_EXPORT
int mailactivesync_send_mail(mailactivesync * session,
    const char * mime_message,
    size_t mime_message_len,
    int save_in_sent);

LIBETPAN_EXPORT
int mailactivesync_smart_reply(mailactivesync * session,
    const char * collection_id,
    const char * server_id,
    const char * mime_message,
    size_t mime_message_len,
    int save_in_sent);

LIBETPAN_EXPORT
int mailactivesync_smart_forward(mailactivesync * session,
    const char * collection_id,
    const char * server_id,
    const char * mime_message,
    size_t mime_message_len,
    int save_in_sent);

LIBETPAN_EXPORT
int mailactivesync_move_items(mailactivesync * session,
    clist * moves,
    struct mailactivesync_move_items_result ** result);

LIBETPAN_EXPORT
int mailactivesync_ping(mailactivesync * session,
    struct mailactivesync_ping_request * request,
    struct mailactivesync_ping_result ** result);

#ifdef __cplusplus
}
#endif

#endif
