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
  int as_cached;
  char * as_cache_directory;
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
  char * message_class;
  uint32_t estimated_size;
  int read;
  int flagged;
  char * mime;
  size_t mime_len;
  struct mailactivesync_airsyncbase_body * body;
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
  uint32_t window_size;
  struct mailactivesync_body_preference * body_preference;
  clist * client_commands;
};

struct mailactivesync_sync_result {
  char * sync_key;
  int status;
  int more_available;
  int empty_response;
  int sync_key_from_response;
  clist * added;   /* struct mailactivesync_message * */
  clist * changed; /* struct mailactivesync_message * */
  clist * deleted; /* char * server_id */
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

struct mailactivesync_settings_result {
  int status;
  int device_information_status;
};

struct mailactivesync_get_item_estimate_result {
  int status;
  int collection_status;
  uint32_t estimate;
  int empty_response;
};

struct mailactivesync_item {
  char * server_id;
  char * mime;
  size_t mime_len;
  struct mailactivesync_airsyncbase_body * body;
};

struct mailactivesync_move {
  char * src_msg_id;
  char * src_folder_id;
  char * dst_folder_id;
};

struct mailactivesync_move_items_result {
  int status;
  clist * responses;
};

struct mailactivesync_ping_request {
  uint32_t heartbeat_interval;
  clist * collection_ids; /* char * */
};

struct mailactivesync_ping_result {
  int status;
  clist * changed_collection_ids; /* char * */
};

LIBETPAN_EXPORT
void mailactivesync_options_free(struct mailactivesync_options * options);

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
int mailactivesync_sync_request_set_mime_body_preference(
    struct mailactivesync_sync_request * request, uint32_t truncation_size);

LIBETPAN_EXPORT
int mailactivesync_sync_request_set_body_preference(
    struct mailactivesync_sync_request * request, int body_type,
    uint32_t truncation_size);

LIBETPAN_EXPORT
int mailactivesync_global_status_to_error(int status);

LIBETPAN_EXPORT
int mailactivesync_sync_status_to_error(int status);

LIBETPAN_EXPORT
int mailactivesync_folder_sync_status_to_error(int status);

LIBETPAN_EXPORT
void mailactivesync_provision_result_free(
    struct mailactivesync_provision_result * result);

LIBETPAN_EXPORT
void mailactivesync_settings_result_free(
    struct mailactivesync_settings_result * result);

LIBETPAN_EXPORT
void mailactivesync_get_item_estimate_result_free(
    struct mailactivesync_get_item_estimate_result * result);

LIBETPAN_EXPORT
void mailactivesync_message_free(struct mailactivesync_message * message);

LIBETPAN_EXPORT
void mailactivesync_airsyncbase_body_free(
    struct mailactivesync_airsyncbase_body * body);

LIBETPAN_EXPORT
void mailactivesync_attachment_free(
    struct mailactivesync_attachment * attachment);

LIBETPAN_EXPORT
void mailactivesync_sync_result_free(
    struct mailactivesync_sync_result * result);

LIBETPAN_EXPORT
void mailactivesync_item_free(struct mailactivesync_item * item);

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
