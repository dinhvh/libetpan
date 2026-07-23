/*
 * libEtPan! -- a mail stuff library
 */

#ifndef MAILACTIVESYNC_COMMAND_H

#define MAILACTIVESYNC_COMMAND_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libetpan/mailactivesync_http.h>
#include <libetpan/mailactivesync_types.h>

int mailactivesync_command_options(mailactivesync * session,
    struct mailactivesync_options ** result);

int mailactivesync_command_folder_sync(mailactivesync * session,
    const char * sync_key,
    struct mailactivesync_folder_sync_result ** result);

int mailactivesync_command_sync(mailactivesync * session,
    struct mailactivesync_sync_request * request,
    struct mailactivesync_sync_result ** result);

int mailactivesync_command_provision(mailactivesync * session,
    struct mailactivesync_provision_result ** result);

int mailactivesync_command_settings_set_device_information(
    mailactivesync * session,
    const struct mailactivesync_device_information * device_information,
    struct mailactivesync_settings_result ** result);

int mailactivesync_command_get_item_estimate(mailactivesync * session,
    const char * collection_id,
    const char * sync_key,
    struct mailactivesync_get_item_estimate_result ** result);

int mailactivesync_command_item_operations_fetch(mailactivesync * session,
    const char * collection_id,
    const char * server_id,
    struct mailactivesync_item ** result);

int mailactivesync_command_post(mailactivesync * session,
    const char * command,
    const char * collection_id,
    const unsigned char * request_body,
    size_t request_body_len,
    struct mailactivesync_http_response ** response);

#ifdef __cplusplus
}
#endif

#endif
