/*
 * libEtPan! -- a mail stuff library
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailactivesync_types.h"

#include <stdlib.h>
#include <string.h>

static void free_string_item(void * value, void * data)
{
  (void) data;
  free(value);
}

static void free_folder_item(void * value, void * data)
{
  (void) data;
  mailactivesync_folder_free(value);
}

static void free_message_item(void * value, void * data)
{
  (void) data;
  mailactivesync_message_free(value);
}

static void free_attachment_item(void * value, void * data)
{
  (void) data;
  mailactivesync_attachment_free(value);
}

static void string_list_free(clist * list)
{
  if (list == NULL)
    return;
  clist_foreach(list, free_string_item, NULL);
  clist_free(list);
}

void mailactivesync_options_free(struct mailactivesync_options * options)
{
  if (options == NULL)
    return;
  string_list_free(options->protocol_versions);
  string_list_free(options->commands);
  free(options);
}

struct mailactivesync_folder *
mailactivesync_folder_new(char * server_id, char * parent_id,
    char * display_name, int type)
{
  struct mailactivesync_folder * folder;

  folder = malloc(sizeof(* folder));
  if (folder == NULL)
    return NULL;

  folder->server_id = server_id;
  folder->parent_id = parent_id;
  folder->display_name = display_name;
  folder->type = type;

  return folder;
}

void mailactivesync_folder_free(struct mailactivesync_folder * folder)
{
  if (folder == NULL)
    return;
  free(folder->server_id);
  free(folder->parent_id);
  free(folder->display_name);
  free(folder);
}

void mailactivesync_folder_sync_result_free(
    struct mailactivesync_folder_sync_result * result)
{
  if (result == NULL)
    return;
  free(result->sync_key);
  if (result->added != NULL) {
    clist_foreach(result->added, free_folder_item, NULL);
    clist_free(result->added);
  }
  if (result->updated != NULL) {
    clist_foreach(result->updated, free_folder_item, NULL);
    clist_free(result->updated);
  }
  string_list_free(result->deleted);
  free(result);
}

struct mailactivesync_sync_request *
mailactivesync_sync_request_new(const char * collection_id,
    const char * sync_key)
{
  struct mailactivesync_sync_request * request;

  request = malloc(sizeof(* request));
  if (request == NULL)
    return NULL;

  request->collection_id = NULL;
  request->sync_key = NULL;
  request->collection_class = NULL;
  request->get_changes = 1;
  request->has_deletes_as_moves = 0;
  request->deletes_as_moves = 0;
  request->has_filter_type = 0;
  request->filter_type = 0;
  request->has_conflict = 0;
  request->conflict = 0;
  request->window_size = 0;
  request->body_preference = NULL;
  request->client_commands = NULL;

  if (collection_id != NULL) {
    request->collection_id = strdup(collection_id);
    if (request->collection_id == NULL)
      goto err;
  }

  if (sync_key != NULL) {
    request->sync_key = strdup(sync_key);
    if (request->sync_key == NULL)
      goto err;
  }

  return request;

 err:
  mailactivesync_sync_request_free(request);
  return NULL;
}

void mailactivesync_sync_request_free(
    struct mailactivesync_sync_request * request)
{
  if (request == NULL)
    return;
  free(request->collection_id);
  free(request->sync_key);
  free(request->collection_class);
  free(request->body_preference);
  if (request->client_commands != NULL)
    clist_free(request->client_commands);
  free(request);
}

int mailactivesync_sync_request_set_get_changes(
    struct mailactivesync_sync_request * request, int get_changes)
{
  if (request == NULL)
    return MAILACTIVESYNC_ERROR_BAD_STATE;
  request->get_changes = get_changes;
  return MAILACTIVESYNC_NO_ERROR;
}

int mailactivesync_sync_request_set_window_size(
    struct mailactivesync_sync_request * request, uint32_t window_size)
{
  if (request == NULL)
    return MAILACTIVESYNC_ERROR_BAD_STATE;
  request->window_size = window_size;
  return MAILACTIVESYNC_NO_ERROR;
}

int mailactivesync_sync_request_set_collection_class(
    struct mailactivesync_sync_request * request,
    const char * collection_class)
{
  char * copy;

  if (request == NULL)
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  copy = NULL;
  if (collection_class != NULL) {
    copy = strdup(collection_class);
    if (copy == NULL)
      return MAILACTIVESYNC_ERROR_MEMORY;
  }

  free(request->collection_class);
  request->collection_class = copy;
  return MAILACTIVESYNC_NO_ERROR;
}

int mailactivesync_sync_request_set_deletes_as_moves(
    struct mailactivesync_sync_request * request, int deletes_as_moves)
{
  if (request == NULL)
    return MAILACTIVESYNC_ERROR_BAD_STATE;
  request->has_deletes_as_moves = 1;
  request->deletes_as_moves = deletes_as_moves;
  return MAILACTIVESYNC_NO_ERROR;
}

int mailactivesync_sync_request_set_filter_type(
    struct mailactivesync_sync_request * request, uint32_t filter_type)
{
  if (request == NULL)
    return MAILACTIVESYNC_ERROR_BAD_STATE;
  request->has_filter_type = 1;
  request->filter_type = filter_type;
  return MAILACTIVESYNC_NO_ERROR;
}

int mailactivesync_sync_request_set_conflict(
    struct mailactivesync_sync_request * request, uint32_t conflict)
{
  if (request == NULL)
    return MAILACTIVESYNC_ERROR_BAD_STATE;
  request->has_conflict = 1;
  request->conflict = conflict;
  return MAILACTIVESYNC_NO_ERROR;
}

int mailactivesync_sync_request_set_mime_body_preference(
    struct mailactivesync_sync_request * request, uint32_t truncation_size)
{
  return mailactivesync_sync_request_set_body_preference(request,
      MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_MIME, truncation_size);
}

int mailactivesync_sync_request_set_body_preference(
    struct mailactivesync_sync_request * request, int body_type,
    uint32_t truncation_size)
{
  struct mailactivesync_body_preference * body_preference;

  if (request == NULL)
    return MAILACTIVESYNC_ERROR_BAD_STATE;
  if ((body_type < MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_PLAIN_TEXT) ||
      (body_type > MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_MIME))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  if (request->body_preference == NULL) {
    body_preference = malloc(sizeof(* body_preference));
    if (body_preference == NULL)
      return MAILACTIVESYNC_ERROR_MEMORY;
    request->body_preference = body_preference;
  }

  request->body_preference->type = body_type;
  request->body_preference->truncation_size = truncation_size;
  request->body_preference->all_or_none = 0;

  return MAILACTIVESYNC_NO_ERROR;
}

int mailactivesync_global_status_to_error(int status)
{
  switch (status) {
  case 0:
  case 1:
    return MAILACTIVESYNC_NO_ERROR;
  case 101:
  case 102:
  case 103:
    return MAILACTIVESYNC_ERROR_PROTOCOL;
  case 109:
  case 112:
  case 126:
  case 127:
  case 128:
  case 129:
  case 130:
  case 131:
    return MAILACTIVESYNC_ERROR_CLIENT_DENIED;
  case 110:
    return MAILACTIVESYNC_ERROR_SERVER_BUSY;
  case 141:
  case 142:
  case 143:
  case 144:
    return MAILACTIVESYNC_ERROR_PROVISION_REQUIRED;
  default:
    return MAILACTIVESYNC_ERROR_PROTOCOL;
  }
}

int mailactivesync_sync_status_to_error(int status)
{
  switch (status) {
  case 0:
  case 1:
  case 7:
  case 9:
    return MAILACTIVESYNC_NO_ERROR;
  case 3:
    return MAILACTIVESYNC_ERROR_FOLDER_RESYNC_REQUIRED;
  case 12:
    return MAILACTIVESYNC_ERROR_ACCOUNT_RESYNC_REQUIRED;
  case 4:
  case 5:
  case 6:
  case 8:
    return MAILACTIVESYNC_ERROR_PROTOCOL;
  default:
    return mailactivesync_global_status_to_error(status);
  }
}

int mailactivesync_folder_sync_status_to_error(int status)
{
  switch (status) {
  case 0:
  case 1:
    return MAILACTIVESYNC_NO_ERROR;
  case 9:
    return MAILACTIVESYNC_ERROR_ACCOUNT_RESYNC_REQUIRED;
  default:
    return mailactivesync_global_status_to_error(status);
  }
}

void mailactivesync_provision_result_free(
    struct mailactivesync_provision_result * result)
{
  if (result == NULL)
    return;
  free(result->policy_key);
  free(result);
}

void mailactivesync_settings_result_free(
    struct mailactivesync_settings_result * result)
{
  free(result);
}

void mailactivesync_get_item_estimate_result_free(
    struct mailactivesync_get_item_estimate_result * result)
{
  free(result);
}

void mailactivesync_message_free(struct mailactivesync_message * message)
{
  if (message == NULL)
    return;
  free(message->server_id);
  free(message->subject);
  free(message->from);
  free(message->to);
  free(message->cc);
  free(message->reply_to);
  free(message->date_received);
  free(message->message_class);
  free(message->mime);
  mailactivesync_airsyncbase_body_free(message->body);
  free(message);
}

void mailactivesync_airsyncbase_body_free(
    struct mailactivesync_airsyncbase_body * body)
{
  if (body == NULL)
    return;
  free(body->data);
  free(body->content_type);
  free(body->preview);
  if (body->attachments != NULL) {
    clist_foreach(body->attachments, free_attachment_item, NULL);
    clist_free(body->attachments);
  }
  free(body);
}

void mailactivesync_attachment_free(
    struct mailactivesync_attachment * attachment)
{
  if (attachment == NULL)
    return;
  free(attachment->display_name);
  free(attachment->file_reference);
  free(attachment->content_id);
  free(attachment->content_location);
  free(attachment->content_type);
  free(attachment);
}

void mailactivesync_sync_result_free(
    struct mailactivesync_sync_result * result)
{
  if (result == NULL)
    return;
  free(result->sync_key);
  if (result->added != NULL) {
    clist_foreach(result->added, free_message_item, NULL);
    clist_free(result->added);
  }
  if (result->changed != NULL) {
    clist_foreach(result->changed, free_message_item, NULL);
    clist_free(result->changed);
  }
  string_list_free(result->deleted);
  free(result);
}

void mailactivesync_item_free(struct mailactivesync_item * item)
{
  if (item == NULL)
    return;
  free(item->server_id);
  free(item->mime);
  mailactivesync_airsyncbase_body_free(item->body);
  free(item);
}

void mailactivesync_move_items_result_free(
    struct mailactivesync_move_items_result * result)
{
  if (result == NULL)
    return;
  if (result->responses != NULL)
    clist_free(result->responses);
  free(result);
}

void mailactivesync_ping_result_free(
    struct mailactivesync_ping_result * result)
{
  if (result == NULL)
    return;
  string_list_free(result->changed_collection_ids);
  free(result);
}
