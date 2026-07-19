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
  request->get_changes = 1;
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

int mailactivesync_sync_request_set_mime_body_preference(
    struct mailactivesync_sync_request * request, uint32_t truncation_size)
{
  struct mailactivesync_body_preference * body_preference;

  if (request == NULL)
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  if (request->body_preference == NULL) {
    body_preference = malloc(sizeof(* body_preference));
    if (body_preference == NULL)
      return MAILACTIVESYNC_ERROR_MEMORY;
    request->body_preference = body_preference;
  }

  request->body_preference->type = 4;
  request->body_preference->truncation_size = truncation_size;
  request->body_preference->all_or_none = 0;

  return MAILACTIVESYNC_NO_ERROR;
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
  free(body);
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
