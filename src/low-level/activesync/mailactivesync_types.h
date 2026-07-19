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
  MAILACTIVESYNC_ERROR_NOT_IMPLEMENTED
};

typedef struct mailactivesync mailactivesync;

struct mailactivesync {
  char * as_server_url;
  char * as_login;
  char * as_password;
  char * as_oauth_token;
  char * as_device_id;
  char * as_device_type;
  char * as_protocol_version;
  char * as_policy_key;
  int as_connected;
  int as_authenticated;
  int as_cached;
  char * as_cache_directory;
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
};

struct mailactivesync_sync_request {
  char * collection_id;
  char * sync_key;
  int get_changes;
  uint32_t window_size;
  struct mailactivesync_body_preference * body_preference;
  clist * client_commands;
};

struct mailactivesync_sync_result {
  char * sync_key;
  int status;
  int more_available;
  clist * added;   /* struct mailactivesync_message * */
  clist * changed; /* struct mailactivesync_message * */
  clist * deleted; /* char * server_id */
};

struct mailactivesync_item {
  char * server_id;
  char * mime;
  size_t mime_len;
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
int mailactivesync_sync_request_set_mime_body_preference(
    struct mailactivesync_sync_request * request, uint32_t truncation_size);

LIBETPAN_EXPORT
void mailactivesync_message_free(struct mailactivesync_message * message);

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
