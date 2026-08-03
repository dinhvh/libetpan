/*
 * libEtPan! -- a mail stuff library
 */

#ifndef MAILACTIVESYNC_TYPES_H

#define MAILACTIVESYNC_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libetpan/libetpan-config.h>
#include <libetpan/clist.h>

#include <stddef.h>
#include <stdint.h>

enum {
  MAILACTIVESYNC_NO_ERROR = 0,
  MAILACTIVESYNC_ERROR_BAD_STATE,
  MAILACTIVESYNC_ERROR_UNAUTHORIZED,
  MAILACTIVESYNC_ERROR_STREAM,
  MAILACTIVESYNC_ERROR_HTTP,
  MAILACTIVESYNC_ERROR_PROTOCOL,
  MAILACTIVESYNC_ERROR_PARSE,
  MAILACTIVESYNC_ERROR_MEMORY,
  MAILACTIVESYNC_ERROR_SSL,
  MAILACTIVESYNC_ERROR_NOT_IMPLEMENTED,
  MAILACTIVESYNC_ERROR_HTTP_UNAVAILABLE,
  MAILACTIVESYNC_ERROR_PROVISION_REQUIRED,
  MAILACTIVESYNC_ERROR_REDIRECT,
  MAILACTIVESYNC_ERROR_RESPONSE_NOT_WBXML,
  MAILACTIVESYNC_ERROR_INVALID_SYNC_KEY,
  MAILACTIVESYNC_ERROR_FOLDER_RESYNC_REQUIRED,
  MAILACTIVESYNC_ERROR_ACCOUNT_RESYNC_REQUIRED,
  MAILACTIVESYNC_ERROR_SERVER_BUSY,
  MAILACTIVESYNC_ERROR_CLIENT_DENIED
};

typedef struct mailactivesync mailactivesync;
struct mailactivesync_http_transport;
struct mailactivesync_wbxml_node;

struct mailactivesync {
  char * as_server_url;
  char * as_login;
  char * as_password;
  char * as_oauth_token;
  char * as_device_id;
  char * as_device_type;
  char * as_protocol_version;
  char * as_policy_key;
  char * as_user_agent;
  char * as_last_redirect_url;
  char * as_last_authenticate_header;
  int as_connected;
  int as_authenticated;
  clist * as_advertised_commands;
  struct mailactivesync_http_transport * as_http_transport;
};

struct mailactivesync_options {
  clist * protocol_versions; /* char * */
  clist * commands;          /* char * */
};

struct mailactivesync_folder {
  char * server_id;
  char * parent_id;
  char * display_name;
  int type;
};

struct mailactivesync_folder_sync_result {
  char * sync_key;
  int status;
  clist * added;   /* struct mailactivesync_folder * */
  clist * updated; /* struct mailactivesync_folder * */
  clist * deleted; /* char * server_id */
};

struct mailactivesync_folder_mutation_result {
  char * sync_key;
  char * server_id;
  int status;
};

struct mailactivesync_body_preference {
  int type;
  uint32_t truncation_size;
  int all_or_none;
};

enum {
  MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_PLAIN_TEXT = 1,
  MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_HTML = 2,
  MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_RTF = 3,
  MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_MIME = 4
};

struct mailactivesync_airsyncbase_body {
  int type;
  char * data;
  size_t data_len;
  uint32_t estimated_data_size;
  int truncated;
  int native_body_type;
  char * content_type;
  char * preview;
  clist * attachments; /* struct mailactivesync_attachment * */
};

struct mailactivesync_body_part {
  int status;
  int type;
  char * data;
  size_t data_len;
  uint32_t estimated_data_size;
  int truncated;
  char * preview;
};

struct mailactivesync_attachment {
  char * display_name;
  char * file_reference;
  int method;
  char * content_id;
  char * content_location;
  int is_inline;
  char * content_type;
  uint32_t estimated_data_size;
};

struct mailactivesync_message {
  char * server_id;
  char * subject;
  char * from;
  char * to;
  char * cc;
  char * reply_to;
  char * date_received;
  char * display_to;
  char * message_class;
  char * thread_topic;
  char * internet_cpid;
  char * content_class;
  clist * categories; /* char * */
  char * conversation_id;
  size_t conversation_id_len;
  char * conversation_index;
  size_t conversation_index_len;
  uint32_t estimated_size;
  int importance;
  int read;
  int flagged;
  int flag_status;
  char * flag_type;
  char * flag_complete_time;
  char * mime;
  size_t mime_len;
  uint32_t mime_size;
  int mime_truncated;
  struct mailactivesync_airsyncbase_body * body;
  clist * body_parts; /* struct mailactivesync_body_part * */
};

struct mailactivesync_draft {
  const char * to;
  const char * cc;
  const char * bcc;
  const char * reply_to;
  const char * subject;
  int has_importance;
  int importance;
  int has_read;
  int read;
  int body_type;
  const char * body;
  clist * attachments; /* struct mailactivesync_draft_attachment * */
};

struct mailactivesync_draft_attachment {
  const char * client_id;
  int method;
  const char * content_type;
  const unsigned char * content;
  size_t content_len;
  const char * display_name;
  const char * content_id;
  int is_inline;
};

struct mailactivesync_sync_request {
  char * collection_id;
  char * sync_key;
  char * collection_class;
  int get_changes;
  int has_deletes_as_moves;
  int deletes_as_moves;
  int has_filter_type;
  uint32_t filter_type;
  int has_conflict;
  uint32_t conflict;
  int has_rights_management_support;
  int rights_management_support;
  uint32_t window_size;
  int has_wait;
  uint32_t wait;
  int has_heartbeat_interval;
  uint32_t heartbeat_interval;
  int has_conversation_mode;
  int conversation_mode;
  struct mailactivesync_body_preference * body_preference;
  clist * body_preferences; /* struct mailactivesync_body_preference * */
  clist * supported_properties; /* struct mailactivesync_wbxml_node * */
  clist * client_commands;
};

enum {
  MAILACTIVESYNC_SYNC_COMMAND_ADD = 1,
  MAILACTIVESYNC_SYNC_COMMAND_CHANGE,
  MAILACTIVESYNC_SYNC_COMMAND_DELETE,
  MAILACTIVESYNC_SYNC_COMMAND_FETCH
};

struct mailactivesync_sync_command {
  int type;
  char * client_id;
  char * server_id;
  char * collection_class;
  clist * application_data; /* struct mailactivesync_wbxml_node * */
};

struct mailactivesync_sync_command_response {
  int type;
  int status;
  char * client_id;
  char * server_id;
  char * collection_class;
  struct mailactivesync_message * message;
};

struct mailactivesync_sync_result {
  char * collection_id;
  char * sync_key;
  int status;
  int more_available;
  int empty_response;
  int sync_key_from_response;
  uint32_t limit;
  clist * added;   /* struct mailactivesync_message * */
  clist * changed; /* struct mailactivesync_message * */
  clist * deleted; /* char * server_id */
  clist * command_responses; /* struct mailactivesync_sync_command_response * */
  clist * collections; /* struct mailactivesync_sync_result * */
};

struct mailactivesync_provision_result {
  int status;
  int policy_status;
  char * policy_key;
};

struct mailactivesync_device_information {
  const char * model;
  const char * imei;
  const char * friendly_name;
  const char * os;
  const char * os_language;
  const char * phone_number;
  const char * user_agent;
  const char * mobile_operator;
};

struct mailactivesync_settings_account {
  char * account_id;
  char * account_name;
  char * user_display_name;
  int send_disabled;
  char * primary_smtp_address;
  clist * smtp_addresses; /* char * */
};

struct mailactivesync_settings_result {
  int status;
  int device_information_status;
  int user_information_status;
  char * primary_smtp_address;
  clist * smtp_addresses; /* char * */
  clist * accounts; /* struct mailactivesync_settings_account * */
};

struct mailactivesync_get_item_estimate_collection_request {
  const char * collection_id;
  const char * sync_key;
  const char * collection_class;
  int has_filter_type;
  uint32_t filter_type;
  int has_conversation_mode;
  int conversation_mode;
};

struct mailactivesync_get_item_estimate_collection {
  char * collection_id;
  char * collection_class;
  int status;
  uint32_t estimate;
};

struct mailactivesync_get_item_estimate_result {
  int status;
  int collection_status;
  uint32_t estimate;
  int empty_response;
  clist * collections; /* struct mailactivesync_get_item_estimate_collection * */
};

struct mailactivesync_item {
  char * collection_id;
  char * server_id;
  int status;
  char * mime;
  size_t mime_len;
  struct mailactivesync_airsyncbase_body * body;
  clist * body_parts; /* struct mailactivesync_body_part * */
};

struct mailactivesync_item_operations_fetch_request {
  const char * collection_id;
  const char * server_id;
  int body_part_type;
  uint32_t truncation_size;
};

struct mailactivesync_item_operations_fetch_result {
  int status;
  clist * items; /* struct mailactivesync_item * */
};

struct mailactivesync_attachment_data {
  int status;
  char * file_reference;
  char * range;
  uint32_t total;
  char * data;
  size_t data_len;
};

struct mailactivesync_mail_search_request {
  const char * collection_id;
  const char * free_text;
  uint32_t range_start;
  uint32_t range_end;
  int rebuild_results;
};

struct mailactivesync_mail_search_item {
  char * collection_id;
  char * server_id;
  char * long_id;
  struct mailactivesync_message * message;
};

struct mailactivesync_mail_search_result {
  int status;
  char * range;
  uint32_t total;
  clist * items; /* struct mailactivesync_mail_search_item * */
};

struct mailactivesync_mail_find_request {
  const char * search_id;
  const char * collection_id;
  const char * query;
  uint32_t range_start;
  uint32_t range_end;
  int deep_traversal;
};

struct mailactivesync_mail_find_item {
  char * collection_id;
  char * server_id;
  char * collection_class;
  char * preview;
  char * display_cc;
  char * display_bcc;
  int has_attachments;
  struct mailactivesync_message * message;
};

struct mailactivesync_mail_find_result {
  int status;
  int response_status;
  char * store;
  char * range;
  uint32_t total;
  clist * items; /* struct mailactivesync_mail_find_item * */
};

struct mailactivesync_resolve_recipients_request {
  clist * recipients; /* char * */
  uint32_t max_ambiguous_recipients;
  int certificate_retrieval;
  uint32_t max_certificates;
};

struct mailactivesync_resolved_recipient {
  int type;
  char * display_name;
  char * email_address;
  int certificates_status;
  uint32_t certificate_count;
  clist * certificates; /* char * */
  char * mini_certificate;
};

struct mailactivesync_resolve_recipients_response {
  char * to;
  int status;
  uint32_t recipient_count;
  clist * recipients; /* struct mailactivesync_resolved_recipient * */
};

struct mailactivesync_resolve_recipients_result {
  int status;
  clist * responses; /* struct mailactivesync_resolve_recipients_response * */
};

struct mailactivesync_validate_cert_request {
  clist * certificates; /* char * */
  clist * certificate_chain; /* char * */
  int check_crl;
};

struct mailactivesync_validate_cert_certificate {
  clist * statuses; /* int * */
};

struct mailactivesync_validate_cert_result {
  int status;
  clist * certificates; /* struct mailactivesync_validate_cert_certificate * */
};

struct mailactivesync_move {
  char * src_msg_id;
  char * src_folder_id;
  char * dst_folder_id;
};

struct mailactivesync_move_response {
  char * src_msg_id;
  char * src_folder_id;
  char * dst_folder_id;
  char * dst_msg_id;
  int status;
};

struct mailactivesync_move_items_result {
  int status;
  clist * responses; /* struct mailactivesync_move_response * */
};

struct mailactivesync_composemail_request {
  const char * client_id;
  const char * collection_id;
  const char * server_id;
  const char * long_id;
  const char * instance_id;
  const char * mime_message;
  size_t mime_message_len;
  int save_in_sent;
  int replace_mime;
};

struct mailactivesync_ping_request {
  uint32_t heartbeat_interval;
  clist * collection_ids; /* char * */
};

struct mailactivesync_ping_result {
  int status;
  uint32_t heartbeat_interval;
  uint32_t max_folders;
  clist * changed_collection_ids; /* char * */
};

LIBETPAN_EXPORT
void mailactivesync_options_free(struct mailactivesync_options * options);

LIBETPAN_EXPORT
int mailactivesync_options_supports_command(
    struct mailactivesync_options * options, const char * command);

LIBETPAN_EXPORT
int mailactivesync_options_supports_protocol_version(
    struct mailactivesync_options * options, const char * protocol_version);

LIBETPAN_EXPORT
int mailactivesync_options_best_protocol_version(
    struct mailactivesync_options * options,
    const char * const * preferred_versions, const char ** result);

LIBETPAN_EXPORT
struct mailactivesync_folder *
mailactivesync_folder_new(char * server_id, char * parent_id,
    char * display_name, int type);

LIBETPAN_EXPORT
void mailactivesync_folder_free(struct mailactivesync_folder * folder);

LIBETPAN_EXPORT
void mailactivesync_folder_sync_result_free(
    struct mailactivesync_folder_sync_result * result);

LIBETPAN_EXPORT
void mailactivesync_folder_mutation_result_free(
    struct mailactivesync_folder_mutation_result * result);

LIBETPAN_EXPORT
struct mailactivesync_sync_request *
mailactivesync_sync_request_new(const char * collection_id,
    const char * sync_key);

LIBETPAN_EXPORT
void mailactivesync_sync_request_free(
    struct mailactivesync_sync_request * request);

LIBETPAN_EXPORT
int mailactivesync_sync_request_set_get_changes(
    struct mailactivesync_sync_request * request, int get_changes);

LIBETPAN_EXPORT
int mailactivesync_sync_request_set_window_size(
    struct mailactivesync_sync_request * request, uint32_t window_size);

LIBETPAN_EXPORT
int mailactivesync_sync_request_set_wait(
    struct mailactivesync_sync_request * request, uint32_t wait);

LIBETPAN_EXPORT
int mailactivesync_sync_request_set_heartbeat_interval(
    struct mailactivesync_sync_request * request,
    uint32_t heartbeat_interval);

LIBETPAN_EXPORT
int mailactivesync_sync_request_set_conversation_mode(
    struct mailactivesync_sync_request * request, int conversation_mode);

LIBETPAN_EXPORT
int mailactivesync_sync_request_set_collection_class(
    struct mailactivesync_sync_request * request,
    const char * collection_class);

LIBETPAN_EXPORT
int mailactivesync_sync_request_set_deletes_as_moves(
    struct mailactivesync_sync_request * request, int deletes_as_moves);

LIBETPAN_EXPORT
int mailactivesync_sync_request_set_filter_type(
    struct mailactivesync_sync_request * request, uint32_t filter_type);

LIBETPAN_EXPORT
int mailactivesync_sync_request_set_conflict(
    struct mailactivesync_sync_request * request, uint32_t conflict);

LIBETPAN_EXPORT
int mailactivesync_sync_request_set_rights_management_support(
    struct mailactivesync_sync_request * request,
    int rights_management_support);

LIBETPAN_EXPORT
int mailactivesync_sync_request_set_mime_body_preference(
    struct mailactivesync_sync_request * request, uint32_t truncation_size);

LIBETPAN_EXPORT
int mailactivesync_sync_request_set_body_preference(
    struct mailactivesync_sync_request * request, int body_type,
    uint32_t truncation_size);

LIBETPAN_EXPORT
int mailactivesync_sync_request_add_body_preference(
    struct mailactivesync_sync_request * request, int body_type,
    uint32_t truncation_size);

LIBETPAN_EXPORT
int mailactivesync_sync_request_set_body_preference_all_or_none(
    struct mailactivesync_sync_request * request, int all_or_none);

LIBETPAN_EXPORT
int mailactivesync_sync_request_add_supported_property(
    struct mailactivesync_sync_request * request,
    uint8_t code_page,
    uint8_t token);

LIBETPAN_EXPORT
struct mailactivesync_sync_command *
mailactivesync_sync_command_add_new(const char * client_id,
    const char * collection_class);

LIBETPAN_EXPORT
struct mailactivesync_sync_command *
mailactivesync_sync_command_change_new(const char * server_id);

LIBETPAN_EXPORT
struct mailactivesync_sync_command *
mailactivesync_sync_command_delete_new(const char * server_id);

LIBETPAN_EXPORT
struct mailactivesync_sync_command *
mailactivesync_sync_command_fetch_new(const char * server_id);

LIBETPAN_EXPORT
void mailactivesync_sync_command_free(
    struct mailactivesync_sync_command * command);

LIBETPAN_EXPORT
int mailactivesync_sync_command_add_application_data_node(
    struct mailactivesync_sync_command * command,
    struct mailactivesync_wbxml_node * node);

LIBETPAN_EXPORT
int mailactivesync_sync_command_add_application_data_text(
    struct mailactivesync_sync_command * command,
    uint8_t code_page,
    uint8_t token,
    const char * text);

LIBETPAN_EXPORT
int mailactivesync_sync_request_add_command(
    struct mailactivesync_sync_request * request,
    struct mailactivesync_sync_command * command);

LIBETPAN_EXPORT
void mailactivesync_sync_command_response_free(
    struct mailactivesync_sync_command_response * response);

LIBETPAN_EXPORT
struct mailactivesync_sync_command_response *
mailactivesync_sync_result_command_response(
    struct mailactivesync_sync_result * result,
    int type,
    const char * client_id,
    const char * server_id);

LIBETPAN_EXPORT
int mailactivesync_sync_result_command_response_status_to_error(
    struct mailactivesync_sync_result * result,
    int type,
    const char * client_id,
    const char * server_id);

LIBETPAN_EXPORT
int mailactivesync_global_status_to_error(int status);

LIBETPAN_EXPORT
int mailactivesync_sync_status_to_error(int status);

LIBETPAN_EXPORT
struct mailactivesync_sync_result *
mailactivesync_sync_result_collection(
    struct mailactivesync_sync_result * result,
    const char * collection_id);

LIBETPAN_EXPORT
int mailactivesync_sync_result_collection_status_to_error(
    struct mailactivesync_sync_result * result,
    const char * collection_id);

LIBETPAN_EXPORT
int mailactivesync_folder_sync_status_to_error(int status);

LIBETPAN_EXPORT
int mailactivesync_folder_mutation_status_to_error(int status);

LIBETPAN_EXPORT
int mailactivesync_folder_mutation_result_needs_resync(
    struct mailactivesync_folder_mutation_result * result);

LIBETPAN_EXPORT
int mailactivesync_get_item_estimate_status_to_error(int status);

LIBETPAN_EXPORT
struct mailactivesync_get_item_estimate_collection *
mailactivesync_get_item_estimate_result_collection(
    struct mailactivesync_get_item_estimate_result * result,
    const char * collection_id);

LIBETPAN_EXPORT
int mailactivesync_get_item_estimate_result_collection_status_to_error(
    struct mailactivesync_get_item_estimate_result * result,
    const char * collection_id);

LIBETPAN_EXPORT
int mailactivesync_move_items_status_to_error(int status);

LIBETPAN_EXPORT
int mailactivesync_resolve_recipients_status_to_error(int status);

LIBETPAN_EXPORT
int mailactivesync_resolve_recipients_response_status_to_error(int status);

LIBETPAN_EXPORT
int mailactivesync_resolve_recipients_certificates_status_to_error(
    int status);

int mailactivesync_item_operations_status_to_error(int status);

LIBETPAN_EXPORT
int mailactivesync_airsyncbase_body_needs_fetch(
    const struct mailactivesync_airsyncbase_body * body);

LIBETPAN_EXPORT
int mailactivesync_body_part_needs_fetch(
    const struct mailactivesync_body_part * body_part);

LIBETPAN_EXPORT
int mailactivesync_message_needs_body_fetch(
    const struct mailactivesync_message * message);

LIBETPAN_EXPORT
int mailactivesync_item_needs_body_fetch(
    const struct mailactivesync_item * item);

LIBETPAN_EXPORT
int mailactivesync_ping_status_to_error(int status);

LIBETPAN_EXPORT
int mailactivesync_provision_status_to_error(int status);

LIBETPAN_EXPORT
int mailactivesync_provision_policy_status_to_error(int status);

LIBETPAN_EXPORT
int mailactivesync_provision_result_status_to_error(
    struct mailactivesync_provision_result * result);

LIBETPAN_EXPORT
int mailactivesync_settings_status_to_error(int status);

LIBETPAN_EXPORT
int mailactivesync_settings_result_status_to_error(
    struct mailactivesync_settings_result * result);

int mailactivesync_validate_cert_status_to_error(int status);

LIBETPAN_EXPORT
int mailactivesync_validate_cert_certificate_status_to_error(int status);

LIBETPAN_EXPORT
void mailactivesync_provision_result_free(
    struct mailactivesync_provision_result * result);

LIBETPAN_EXPORT
void mailactivesync_settings_result_free(
    struct mailactivesync_settings_result * result);
void mailactivesync_settings_account_free(
    struct mailactivesync_settings_account * account);

LIBETPAN_EXPORT
void mailactivesync_get_item_estimate_result_free(
    struct mailactivesync_get_item_estimate_result * result);

LIBETPAN_EXPORT
void mailactivesync_message_free(struct mailactivesync_message * message);

void mailactivesync_airsyncbase_body_free(
    struct mailactivesync_airsyncbase_body * body);

LIBETPAN_EXPORT
void mailactivesync_body_part_free(struct mailactivesync_body_part * body_part);

LIBETPAN_EXPORT
void mailactivesync_attachment_free(
    struct mailactivesync_attachment * attachment);

LIBETPAN_EXPORT
void mailactivesync_sync_result_free(
    struct mailactivesync_sync_result * result);

LIBETPAN_EXPORT
void mailactivesync_item_free(struct mailactivesync_item * item);

LIBETPAN_EXPORT
void mailactivesync_item_operations_fetch_result_free(
    struct mailactivesync_item_operations_fetch_result * result);

LIBETPAN_EXPORT
void mailactivesync_attachment_data_free(
    struct mailactivesync_attachment_data * data);

LIBETPAN_EXPORT
void mailactivesync_mail_search_item_free(
    struct mailactivesync_mail_search_item * item);

LIBETPAN_EXPORT
void mailactivesync_mail_search_result_free(
    struct mailactivesync_mail_search_result * result);

LIBETPAN_EXPORT
void mailactivesync_mail_find_item_free(
    struct mailactivesync_mail_find_item * item);

LIBETPAN_EXPORT
void mailactivesync_mail_find_result_free(
    struct mailactivesync_mail_find_result * result);

void mailactivesync_resolved_recipient_free(
    struct mailactivesync_resolved_recipient * recipient);

LIBETPAN_EXPORT
void mailactivesync_resolve_recipients_response_free(
    struct mailactivesync_resolve_recipients_response * response);

LIBETPAN_EXPORT
void mailactivesync_resolve_recipients_result_free(
    struct mailactivesync_resolve_recipients_result * result);

LIBETPAN_EXPORT
void mailactivesync_validate_cert_certificate_free(
    struct mailactivesync_validate_cert_certificate * certificate);

LIBETPAN_EXPORT
void mailactivesync_validate_cert_result_free(
    struct mailactivesync_validate_cert_result * result);

LIBETPAN_EXPORT
void mailactivesync_move_response_free(
    struct mailactivesync_move_response * response);

LIBETPAN_EXPORT
void mailactivesync_move_items_result_free(
    struct mailactivesync_move_items_result * result);

LIBETPAN_EXPORT
void mailactivesync_ping_result_free(
    struct mailactivesync_ping_result * result);

#ifdef __cplusplus
}
#endif

#endif
