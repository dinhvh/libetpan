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

int mailactivesync_command_folder_create(mailactivesync * session,
    const char * sync_key,
    const char * parent_id,
    const char * display_name,
    int type,
    struct mailactivesync_folder_mutation_result ** result);

int mailactivesync_command_folder_update(mailactivesync * session,
    const char * sync_key,
    const char * server_id,
    const char * parent_id,
    const char * display_name,
    struct mailactivesync_folder_mutation_result ** result);

int mailactivesync_command_folder_delete(mailactivesync * session,
    const char * sync_key,
    const char * server_id,
    struct mailactivesync_folder_mutation_result ** result);

int mailactivesync_command_sync(mailactivesync * session,
    struct mailactivesync_sync_request * request,
    struct mailactivesync_sync_result ** result);

int mailactivesync_command_sync_multi(mailactivesync * session,
    clist * requests,
    struct mailactivesync_sync_result ** result);

int mailactivesync_command_provision(mailactivesync * session,
    struct mailactivesync_provision_result ** result);

int mailactivesync_command_settings_set_device_information(
    mailactivesync * session,
    const struct mailactivesync_device_information * device_information,
    struct mailactivesync_settings_result ** result);

int mailactivesync_command_settings_get_user_information(
    mailactivesync * session,
    struct mailactivesync_settings_result ** result);

int mailactivesync_command_get_item_estimate(mailactivesync * session,
    const char * collection_id,
    const char * sync_key,
    struct mailactivesync_get_item_estimate_result ** result);

int mailactivesync_command_get_item_estimate_multi(mailactivesync * session,
    clist * collections,
    struct mailactivesync_get_item_estimate_result ** result);

int mailactivesync_command_item_operations_fetch(mailactivesync * session,
    const char * collection_id,
    const char * server_id,
    struct mailactivesync_item ** result);

int mailactivesync_command_item_operations_fetch_body_part(
    mailactivesync * session,
    const char * collection_id,
    const char * server_id,
    int body_type,
    uint32_t truncation_size,
    struct mailactivesync_item ** result);

int mailactivesync_command_item_operations_fetch_multi(
    mailactivesync * session,
    clist * requests,
    struct mailactivesync_item_operations_fetch_result ** result);

int mailactivesync_command_item_operations_fetch_attachment(
    mailactivesync * session,
    const char * file_reference,
    const char * range,
    struct mailactivesync_attachment_data ** result);

int mailactivesync_command_mail_search(mailactivesync * session,
    const struct mailactivesync_mail_search_request * request,
    struct mailactivesync_mail_search_result ** result);

int mailactivesync_command_mail_find(mailactivesync * session,
    const struct mailactivesync_mail_find_request * request,
    struct mailactivesync_mail_find_result ** result);

int mailactivesync_command_resolve_recipients(mailactivesync * session,
    clist * recipients,
    uint32_t max_ambiguous_recipients,
    struct mailactivesync_resolve_recipients_result ** result);

int mailactivesync_command_resolve_recipients_ext(mailactivesync * session,
    const struct mailactivesync_resolve_recipients_request * request,
    struct mailactivesync_resolve_recipients_result ** result);

int mailactivesync_command_validate_cert(mailactivesync * session,
    const struct mailactivesync_validate_cert_request * request,
    struct mailactivesync_validate_cert_result ** result);

int mailactivesync_command_send_mail(mailactivesync * session,
    const char * mime_message,
    size_t mime_message_len,
    int save_in_sent);

int mailactivesync_command_send_mail_ext(mailactivesync * session,
    const struct mailactivesync_composemail_request * request);

int mailactivesync_command_smart_reply(mailactivesync * session,
    const char * collection_id,
    const char * server_id,
    const char * mime_message,
    size_t mime_message_len,
    int save_in_sent);

int mailactivesync_command_smart_reply_ext(mailactivesync * session,
    const struct mailactivesync_composemail_request * request);

int mailactivesync_command_smart_forward(mailactivesync * session,
    const char * collection_id,
    const char * server_id,
    const char * mime_message,
    size_t mime_message_len,
    int save_in_sent);

int mailactivesync_command_smart_forward_ext(mailactivesync * session,
    const struct mailactivesync_composemail_request * request);

int mailactivesync_command_move_items(mailactivesync * session,
    clist * moves,
    struct mailactivesync_move_items_result ** result);

int mailactivesync_command_ping(mailactivesync * session,
    struct mailactivesync_ping_request * request,
    struct mailactivesync_ping_result ** result);

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
