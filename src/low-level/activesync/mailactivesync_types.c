/*
 * libEtPan! -- a mail stuff library
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailactivesync_types.h"
#include "mailactivesync_wbxml.h"

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

static void free_get_item_estimate_collection_item(void * value, void * data)
{
  struct mailactivesync_get_item_estimate_collection * collection;

  (void) data;
  collection = value;
  if (collection == NULL)
    return;
  free(collection->collection_id);
  free(collection->collection_class);
  free(collection);
}

static void free_body_part_item(void * value, void * data)
{
  (void) data;
  mailactivesync_body_part_free(value);
}

static void free_item_item(void * value, void * data)
{
  (void) data;
  mailactivesync_item_free(value);
}

static void free_sync_command_item(void * value, void * data)
{
  (void) data;
  mailactivesync_sync_command_free(value);
}

static void free_body_preference_item(void * value, void * data)
{
  (void) data;
  free(value);
}

static void sync_request_clear_body_preferences(
    struct mailactivesync_sync_request * request)
{
  if (request->body_preferences != NULL) {
    clist_foreach(request->body_preferences, free_body_preference_item, NULL);
    clist_free(request->body_preferences);
  }
  request->body_preferences = NULL;
  request->body_preference = NULL;
}

static void free_sync_command_response_item(void * value, void * data)
{
  (void) data;
  mailactivesync_sync_command_response_free(value);
}

static void free_sync_result_item(void * value, void * data)
{
  (void) data;
  mailactivesync_sync_result_free(value);
}

static void free_move_response_item(void * value, void * data)
{
  (void) data;
  mailactivesync_move_response_free(value);
}

static void free_mail_search_item(void * value, void * data)
{
  (void) data;
  mailactivesync_mail_search_item_free(value);
}

static void free_mail_find_item(void * value, void * data)
{
  (void) data;
  mailactivesync_mail_find_item_free(value);
}

static void free_resolved_recipient_item(void * value, void * data)
{
  (void) data;
  mailactivesync_resolved_recipient_free(value);
}

static void free_resolve_recipients_response_item(void * value, void * data)
{
  (void) data;
  mailactivesync_resolve_recipients_response_free(value);
}

static void free_settings_account_item(void * value, void * data)
{
  (void) data;
  mailactivesync_settings_account_free(value);
}

static void free_validate_cert_certificate_item(void * value, void * data)
{
  (void) data;
  mailactivesync_validate_cert_certificate_free(value);
}

static void free_wbxml_node_item(void * value, void * data)
{
  (void) data;
  mailactivesync_wbxml_node_free(value);
}

static void string_list_free(clist * list)
{
  if (list == NULL)
    return;
  clist_foreach(list, free_string_item, NULL);
  clist_free(list);
}

static int string_list_contains(clist * list, const char * value)
{
  clistiter * cur;

  if ((list == NULL) || (value == NULL))
    return 0;

  for (cur = clist_begin(list); cur != NULL; cur = clist_next(cur)) {
    const char * item;

    item = clist_content(cur);
    if ((item != NULL) && (strcmp(item, value) == 0))
      return 1;
  }

  return 0;
}

void mailactivesync_options_free(struct mailactivesync_options * options)
{
  if (options == NULL)
    return;
  string_list_free(options->protocol_versions);
  string_list_free(options->commands);
  free(options);
}

int mailactivesync_options_supports_command(
    struct mailactivesync_options * options, const char * command)
{
  if (options == NULL)
    return 0;
  return string_list_contains(options->commands, command);
}

int mailactivesync_options_supports_protocol_version(
    struct mailactivesync_options * options, const char * protocol_version)
{
  if (options == NULL)
    return 0;
  return string_list_contains(options->protocol_versions, protocol_version);
}

int mailactivesync_options_best_protocol_version(
    struct mailactivesync_options * options,
    const char * const * preferred_versions, const char ** result)
{
  unsigned int i;

  if ((options == NULL) || (preferred_versions == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  for (i = 0; preferred_versions[i] != NULL; i ++) {
    const char * version;

    version = preferred_versions[i];
    if (mailactivesync_options_supports_protocol_version(options, version)) {
      * result = version;
      return MAILACTIVESYNC_NO_ERROR;
    }
  }

  return MAILACTIVESYNC_ERROR_NOT_IMPLEMENTED;
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

void mailactivesync_folder_mutation_result_free(
    struct mailactivesync_folder_mutation_result * result)
{
  if (result == NULL)
    return;
  free(result->sync_key);
  free(result->server_id);
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
  request->has_rights_management_support = 0;
  request->rights_management_support = 0;
  request->window_size = 0;
  request->has_wait = 0;
  request->wait = 0;
  request->has_heartbeat_interval = 0;
  request->heartbeat_interval = 0;
  request->has_conversation_mode = 0;
  request->conversation_mode = 0;
  request->body_preference = NULL;
  request->body_preferences = NULL;
  request->supported_properties = NULL;
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
  sync_request_clear_body_preferences(request);
  if (request->supported_properties != NULL) {
    clist_foreach(request->supported_properties, free_wbxml_node_item, NULL);
    clist_free(request->supported_properties);
  }
  if (request->client_commands != NULL) {
    clist_foreach(request->client_commands, free_sync_command_item, NULL);
    clist_free(request->client_commands);
  }
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

int mailactivesync_sync_request_set_wait(
    struct mailactivesync_sync_request * request, uint32_t wait)
{
  if (request == NULL)
    return MAILACTIVESYNC_ERROR_BAD_STATE;
  if (wait == 0) {
    request->has_wait = 0;
    request->wait = 0;
    return MAILACTIVESYNC_NO_ERROR;
  }
  if ((wait < 1) || (wait > 59))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  request->has_wait = 1;
  request->wait = wait;
  request->has_heartbeat_interval = 0;
  request->heartbeat_interval = 0;
  return MAILACTIVESYNC_NO_ERROR;
}

int mailactivesync_sync_request_set_heartbeat_interval(
    struct mailactivesync_sync_request * request,
    uint32_t heartbeat_interval)
{
  if (request == NULL)
    return MAILACTIVESYNC_ERROR_BAD_STATE;
  if (heartbeat_interval == 0) {
    request->has_heartbeat_interval = 0;
    request->heartbeat_interval = 0;
    return MAILACTIVESYNC_NO_ERROR;
  }
  if ((heartbeat_interval < 60) || (heartbeat_interval > 3540))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  request->has_heartbeat_interval = 1;
  request->heartbeat_interval = heartbeat_interval;
  request->has_wait = 0;
  request->wait = 0;
  return MAILACTIVESYNC_NO_ERROR;
}

int mailactivesync_sync_request_set_conversation_mode(
    struct mailactivesync_sync_request * request, int conversation_mode)
{
  if (request == NULL)
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  request->has_conversation_mode = 1;
  request->conversation_mode = conversation_mode ? 1 : 0;
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

int mailactivesync_sync_request_set_rights_management_support(
    struct mailactivesync_sync_request * request, int rights_management_support)
{
  if (request == NULL)
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  request->has_rights_management_support = 1;
  request->rights_management_support = rights_management_support ? 1 : 0;
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
  int r;

  if (request == NULL)
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  sync_request_clear_body_preferences(request);
  r = mailactivesync_sync_request_add_body_preference(request, body_type,
      truncation_size);
  if (r != MAILACTIVESYNC_NO_ERROR)
    sync_request_clear_body_preferences(request);
  return r;
}

int mailactivesync_sync_request_add_body_preference(
    struct mailactivesync_sync_request * request, int body_type,
    uint32_t truncation_size)
{
  struct mailactivesync_body_preference * body_preference;

  if (request == NULL)
    return MAILACTIVESYNC_ERROR_BAD_STATE;
  if ((body_type < MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_PLAIN_TEXT) ||
      (body_type > MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_MIME))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  if (request->body_preferences == NULL) {
    request->body_preferences = clist_new();
    if (request->body_preferences == NULL)
      return MAILACTIVESYNC_ERROR_MEMORY;
  }

  body_preference = malloc(sizeof(* body_preference));
  if (body_preference == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  body_preference->type = body_type;
  body_preference->truncation_size = truncation_size;
  body_preference->all_or_none = 0;

  if (clist_append(request->body_preferences, body_preference) < 0) {
    free(body_preference);
    return MAILACTIVESYNC_ERROR_MEMORY;
  }

  request->body_preference = body_preference;

  return MAILACTIVESYNC_NO_ERROR;
}

int mailactivesync_sync_request_set_body_preference_all_or_none(
    struct mailactivesync_sync_request * request, int all_or_none)
{
  if ((request == NULL) || (request->body_preference == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  request->body_preference->all_or_none = all_or_none ? 1 : 0;
  return MAILACTIVESYNC_NO_ERROR;
}

int mailactivesync_sync_request_add_supported_property(
    struct mailactivesync_sync_request * request, uint8_t code_page,
    uint8_t token)
{
  struct mailactivesync_wbxml_node * property;

  if (request == NULL)
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  if (request->supported_properties == NULL) {
    request->supported_properties = clist_new();
    if (request->supported_properties == NULL)
      return MAILACTIVESYNC_ERROR_MEMORY;
  }

  property = mailactivesync_wbxml_node_new(code_page, token);
  if (property == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  if (clist_append(request->supported_properties, property) < 0) {
    mailactivesync_wbxml_node_free(property);
    return MAILACTIVESYNC_ERROR_MEMORY;
  }

  return MAILACTIVESYNC_NO_ERROR;
}

static struct mailactivesync_sync_command * sync_command_new(int type,
    const char * client_id, const char * server_id,
    const char * collection_class)
{
  struct mailactivesync_sync_command * command;

  command = calloc(1, sizeof(* command));
  if (command == NULL)
    return NULL;

  command->type = type;
  command->application_data = clist_new();
  if (command->application_data == NULL)
    goto err;

  if (client_id != NULL) {
    command->client_id = strdup(client_id);
    if (command->client_id == NULL)
      goto err;
  }
  if (server_id != NULL) {
    command->server_id = strdup(server_id);
    if (command->server_id == NULL)
      goto err;
  }
  if (collection_class != NULL) {
    command->collection_class = strdup(collection_class);
    if (command->collection_class == NULL)
      goto err;
  }

  return command;

 err:
  mailactivesync_sync_command_free(command);
  return NULL;
}

struct mailactivesync_sync_command *
mailactivesync_sync_command_add_new(const char * client_id,
    const char * collection_class)
{
  if (client_id == NULL)
    return NULL;

  return sync_command_new(MAILACTIVESYNC_SYNC_COMMAND_ADD, client_id, NULL,
      collection_class);
}

struct mailactivesync_sync_command *
mailactivesync_sync_command_change_new(const char * server_id)
{
  if (server_id == NULL)
    return NULL;

  return sync_command_new(MAILACTIVESYNC_SYNC_COMMAND_CHANGE, NULL,
      server_id, NULL);
}

struct mailactivesync_sync_command *
mailactivesync_sync_command_delete_new(const char * server_id)
{
  if (server_id == NULL)
    return NULL;

  return sync_command_new(MAILACTIVESYNC_SYNC_COMMAND_DELETE, NULL,
      server_id, NULL);
}

struct mailactivesync_sync_command *
mailactivesync_sync_command_fetch_new(const char * server_id)
{
  if (server_id == NULL)
    return NULL;

  return sync_command_new(MAILACTIVESYNC_SYNC_COMMAND_FETCH, NULL,
      server_id, NULL);
}

void mailactivesync_sync_command_free(
    struct mailactivesync_sync_command * command)
{
  if (command == NULL)
    return;
  free(command->client_id);
  free(command->server_id);
  free(command->collection_class);
  if (command->application_data != NULL) {
    clist_foreach(command->application_data, free_wbxml_node_item, NULL);
    clist_free(command->application_data);
  }
  free(command);
}

int mailactivesync_sync_command_add_application_data_node(
    struct mailactivesync_sync_command * command,
    struct mailactivesync_wbxml_node * node)
{
  if ((command == NULL) || (node == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  if (clist_append(command->application_data, node) < 0)
    return MAILACTIVESYNC_ERROR_MEMORY;

  return MAILACTIVESYNC_NO_ERROR;
}

int mailactivesync_sync_command_add_application_data_text(
    struct mailactivesync_sync_command * command,
    uint8_t code_page,
    uint8_t token,
    const char * text)
{
  struct mailactivesync_wbxml_node * node;
  int r;

  if ((command == NULL) || (text == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  node = mailactivesync_wbxml_node_new_text(code_page, token, text);
  if (node == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  r = mailactivesync_sync_command_add_application_data_node(command, node);
  if (r != MAILACTIVESYNC_NO_ERROR)
    mailactivesync_wbxml_node_free(node);

  return r;
}

int mailactivesync_sync_request_add_command(
    struct mailactivesync_sync_request * request,
    struct mailactivesync_sync_command * command)
{
  if ((request == NULL) || (command == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  if (request->client_commands == NULL) {
    request->client_commands = clist_new();
    if (request->client_commands == NULL)
      return MAILACTIVESYNC_ERROR_MEMORY;
  }
  if (clist_append(request->client_commands, command) < 0)
    return MAILACTIVESYNC_ERROR_MEMORY;

  return MAILACTIVESYNC_NO_ERROR;
}

void mailactivesync_sync_command_response_free(
    struct mailactivesync_sync_command_response * response)
{
  if (response == NULL)
    return;
  free(response->client_id);
  free(response->server_id);
  free(response->collection_class);
  mailactivesync_message_free(response->message);
  free(response);
}

static int sync_command_type_is_valid(int type)
{
  switch (type) {
  case MAILACTIVESYNC_SYNC_COMMAND_ADD:
  case MAILACTIVESYNC_SYNC_COMMAND_CHANGE:
  case MAILACTIVESYNC_SYNC_COMMAND_DELETE:
  case MAILACTIVESYNC_SYNC_COMMAND_FETCH:
    return 1;
  default:
    return 0;
  }
}

struct mailactivesync_sync_command_response *
mailactivesync_sync_result_command_response(
    struct mailactivesync_sync_result * result,
    int type,
    const char * client_id,
    const char * server_id)
{
  clistiter * cur;

  if ((result == NULL) || !sync_command_type_is_valid(type))
    return NULL;
  if (result->command_responses == NULL)
    return NULL;

  for (cur = clist_begin(result->command_responses) ; cur != NULL ;
      cur = clist_next(cur)) {
    struct mailactivesync_sync_command_response * response;

    response = clist_content(cur);
    if ((response == NULL) || (response->type != type))
      continue;
    if ((client_id != NULL) && ((response->client_id == NULL) ||
        (strcmp(response->client_id, client_id) != 0)))
      continue;
    if ((server_id != NULL) && ((response->server_id == NULL) ||
        (strcmp(response->server_id, server_id) != 0)))
      continue;
    return response;
  }

  return NULL;
}

int mailactivesync_sync_result_command_response_status_to_error(
    struct mailactivesync_sync_result * result,
    int type,
    const char * client_id,
    const char * server_id)
{
  struct mailactivesync_sync_command_response * response;

  if ((result == NULL) || !sync_command_type_is_valid(type))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  response = mailactivesync_sync_result_command_response(result, type,
      client_id, server_id);
  if (response == NULL)
    return MAILACTIVESYNC_ERROR_PROTOCOL;

  return mailactivesync_sync_status_to_error(response->status);
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
  case 104:
  case 105:
  case 106:
  case 107:
  case 108:
  case 113:
  case 115:
  case 116:
  case 117:
  case 119:
  case 120:
  case 121:
  case 122:
  case 123:
  case 124:
  case 125:
  case 132:
  case 133:
  case 134:
  case 135:
  case 136:
  case 148:
  case 149:
  case 150:
  case 152:
  case 153:
  case 154:
  case 155:
  case 156:
  case 166:
  case 170:
  case 171:
  case 176:
  case 178:
  case 179:
  case 183:
    return MAILACTIVESYNC_ERROR_PROTOCOL;
  case 109:
  case 112:
  case 126:
  case 127:
  case 128:
  case 129:
  case 130:
  case 131:
  case 145:
  case 167:
  case 168:
  case 172:
  case 177:
    return MAILACTIVESYNC_ERROR_CLIENT_DENIED;
  case 110:
  case 111:
  case 114:
  case 169:
    return MAILACTIVESYNC_ERROR_SERVER_BUSY;
  case 137:
  case 138:
    return MAILACTIVESYNC_ERROR_NOT_IMPLEMENTED;
  case 140:
  case 141:
  case 142:
  case 143:
  case 144:
  case 165:
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

struct mailactivesync_sync_result *
mailactivesync_sync_result_collection(
    struct mailactivesync_sync_result * result,
    const char * collection_id)
{
  clistiter * cur;

  if ((result == NULL) || (collection_id == NULL))
    return NULL;

  if ((result->collection_id != NULL) &&
      (strcmp(result->collection_id, collection_id) == 0))
    return result;

  if (result->collections == NULL)
    return NULL;

  for (cur = clist_begin(result->collections) ; cur != NULL ;
      cur = clist_next(cur)) {
    struct mailactivesync_sync_result * collection;

    collection = clist_content(cur);
    if ((collection != NULL) && (collection->collection_id != NULL) &&
        (strcmp(collection->collection_id, collection_id) == 0))
      return collection;
  }

  return NULL;
}

int mailactivesync_sync_result_collection_status_to_error(
    struct mailactivesync_sync_result * result,
    const char * collection_id)
{
  struct mailactivesync_sync_result * collection;

  if ((result == NULL) || (collection_id == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  collection = mailactivesync_sync_result_collection(result, collection_id);
  if (collection == NULL)
    return MAILACTIVESYNC_ERROR_PROTOCOL;

  return mailactivesync_sync_status_to_error(collection->status);
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

int mailactivesync_folder_mutation_status_to_error(int status)
{
  switch (status) {
  case 0:
  case 1:
    return MAILACTIVESYNC_NO_ERROR;
  case 6:
    return MAILACTIVESYNC_ERROR_SERVER_BUSY;
  case 9:
    return MAILACTIVESYNC_ERROR_INVALID_SYNC_KEY;
  case 10:
    return MAILACTIVESYNC_ERROR_PROTOCOL;
  default:
    return mailactivesync_global_status_to_error(status);
  }
}

int mailactivesync_folder_mutation_result_needs_resync(
    struct mailactivesync_folder_mutation_result * result)
{
  if (result == NULL)
    return 0;

  return result->status == 9;
}

int mailactivesync_get_item_estimate_status_to_error(int status)
{
  switch (status) {
  case 0:
  case 1:
    return MAILACTIVESYNC_NO_ERROR;
  case 2:
    return MAILACTIVESYNC_ERROR_FOLDER_RESYNC_REQUIRED;
  case 3:
  case 4:
    return MAILACTIVESYNC_ERROR_INVALID_SYNC_KEY;
  default:
    return mailactivesync_global_status_to_error(status);
  }
}

struct mailactivesync_get_item_estimate_collection *
mailactivesync_get_item_estimate_result_collection(
    struct mailactivesync_get_item_estimate_result * result,
    const char * collection_id)
{
  clistiter * cur;

  if ((result == NULL) || (collection_id == NULL) ||
      (result->collections == NULL))
    return NULL;

  for (cur = clist_begin(result->collections) ; cur != NULL ;
      cur = clist_next(cur)) {
    struct mailactivesync_get_item_estimate_collection * collection;

    collection = clist_content(cur);
    if ((collection != NULL) && (collection->collection_id != NULL) &&
        (strcmp(collection->collection_id, collection_id) == 0))
      return collection;
  }

  return NULL;
}

int mailactivesync_get_item_estimate_result_collection_status_to_error(
    struct mailactivesync_get_item_estimate_result * result,
    const char * collection_id)
{
  struct mailactivesync_get_item_estimate_collection * collection;

  if ((result == NULL) || (collection_id == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  collection = mailactivesync_get_item_estimate_result_collection(result,
      collection_id);
  if (collection == NULL)
    return MAILACTIVESYNC_ERROR_PROTOCOL;

  return mailactivesync_get_item_estimate_status_to_error(collection->status);
}

int mailactivesync_move_items_status_to_error(int status)
{
  switch (status) {
  case 0:
  case 3:
    return MAILACTIVESYNC_NO_ERROR;
  case 1:
  case 2:
    return MAILACTIVESYNC_ERROR_FOLDER_RESYNC_REQUIRED;
  case 4:
    return MAILACTIVESYNC_ERROR_PROTOCOL;
  case 5:
  case 7:
    return MAILACTIVESYNC_ERROR_SERVER_BUSY;
  default:
    return mailactivesync_global_status_to_error(status);
  }
}

int mailactivesync_resolve_recipients_status_to_error(int status)
{
  switch (status) {
  case 0:
  case 1:
    return MAILACTIVESYNC_NO_ERROR;
  case 5:
    return MAILACTIVESYNC_ERROR_PROTOCOL;
  case 6:
    return MAILACTIVESYNC_ERROR_SERVER_BUSY;
  default:
    return mailactivesync_global_status_to_error(status);
  }
}

int mailactivesync_resolve_recipients_response_status_to_error(int status)
{
  switch (status) {
  case 0:
  case 1:
  case 2:
  case 3:
    return MAILACTIVESYNC_NO_ERROR;
  case 4:
    return MAILACTIVESYNC_ERROR_PROTOCOL;
  default:
    return mailactivesync_global_status_to_error(status);
  }
}

int mailactivesync_resolve_recipients_certificates_status_to_error(
    int status)
{
  switch (status) {
  case 0:
  case 1:
  case 7:
    return MAILACTIVESYNC_NO_ERROR;
  case 8:
    return MAILACTIVESYNC_ERROR_PROTOCOL;
  default:
    return mailactivesync_global_status_to_error(status);
  }
}

int mailactivesync_item_operations_status_to_error(int status)
{
  switch (status) {
  case 0:
  case 1:
  case 17:
    return MAILACTIVESYNC_NO_ERROR;
  case 3:
  case 7:
  case 12:
    return MAILACTIVESYNC_ERROR_SERVER_BUSY;
  case 5:
  case 16:
    return MAILACTIVESYNC_ERROR_CLIENT_DENIED;
  case 2:
  case 4:
  case 6:
  case 8:
  case 9:
  case 10:
  case 11:
  case 14:
  case 15:
  case 18:
  case 155:
  case 156:
    return MAILACTIVESYNC_ERROR_PROTOCOL;
  default:
    return mailactivesync_global_status_to_error(status);
  }
}

int mailactivesync_airsyncbase_body_needs_fetch(
    const struct mailactivesync_airsyncbase_body * body)
{
  if (body == NULL)
    return 0;

  return body->truncated != 0;
}

int mailactivesync_body_part_needs_fetch(
    const struct mailactivesync_body_part * body_part)
{
  if (body_part == NULL)
    return 0;

  return (body_part->status == 1) && (body_part->truncated != 0);
}

static int body_part_list_needs_fetch(clist * body_parts)
{
  clistiter * cur;

  if (body_parts == NULL)
    return 0;

  for (cur = clist_begin(body_parts) ; cur != NULL ; cur = clist_next(cur)) {
    if (mailactivesync_body_part_needs_fetch(clist_content(cur)))
      return 1;
  }

  return 0;
}

int mailactivesync_message_needs_body_fetch(
    const struct mailactivesync_message * message)
{
  if (message == NULL)
    return 0;

  return (message->mime_truncated != 0) ||
      mailactivesync_airsyncbase_body_needs_fetch(message->body) ||
      body_part_list_needs_fetch(message->body_parts);
}

int mailactivesync_item_needs_body_fetch(
    const struct mailactivesync_item * item)
{
  if (item == NULL)
    return 0;

  return mailactivesync_airsyncbase_body_needs_fetch(item->body) ||
      body_part_list_needs_fetch(item->body_parts);
}

int mailactivesync_ping_status_to_error(int status)
{
  switch (status) {
  case 0:
  case 1:
  case 2:
  case 5:
  case 6:
    return MAILACTIVESYNC_NO_ERROR;
  case 3:
  case 4:
    return MAILACTIVESYNC_ERROR_PROTOCOL;
  case 7:
    return MAILACTIVESYNC_ERROR_FOLDER_RESYNC_REQUIRED;
  case 8:
    return MAILACTIVESYNC_ERROR_SERVER_BUSY;
  default:
    return mailactivesync_global_status_to_error(status);
  }
}

int mailactivesync_validate_cert_status_to_error(int status)
{
  switch (status) {
  case 0:
  case 1:
    return MAILACTIVESYNC_NO_ERROR;
  case 17:
    return MAILACTIVESYNC_ERROR_PROTOCOL;
  default:
    return mailactivesync_global_status_to_error(status);
  }
}

int mailactivesync_provision_status_to_error(int status)
{
  switch (status) {
  case 0:
  case 1:
    return MAILACTIVESYNC_NO_ERROR;
  case 2:
    return MAILACTIVESYNC_ERROR_PROTOCOL;
  case 3:
    return MAILACTIVESYNC_ERROR_SERVER_BUSY;
  default:
    return mailactivesync_global_status_to_error(status);
  }
}

int mailactivesync_provision_policy_status_to_error(int status)
{
  switch (status) {
  case 0:
  case 1:
  case 2:
    return MAILACTIVESYNC_NO_ERROR;
  case 3:
  case 4:
    return MAILACTIVESYNC_ERROR_PROTOCOL;
  case 5:
    return MAILACTIVESYNC_ERROR_PROVISION_REQUIRED;
  default:
    return mailactivesync_global_status_to_error(status);
  }
}

int mailactivesync_provision_result_status_to_error(
    struct mailactivesync_provision_result * result)
{
  int r;

  if (result == NULL)
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  r = mailactivesync_provision_status_to_error(result->status);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  return mailactivesync_provision_policy_status_to_error(
      result->policy_status);
}

int mailactivesync_settings_status_to_error(int status)
{
  switch (status) {
  case 0:
  case 1:
    return MAILACTIVESYNC_NO_ERROR;
  case 2:
  case 5:
    return MAILACTIVESYNC_ERROR_PROTOCOL;
  case 3:
    return MAILACTIVESYNC_ERROR_CLIENT_DENIED;
  case 4:
    return MAILACTIVESYNC_ERROR_SERVER_BUSY;
  default:
    return mailactivesync_global_status_to_error(status);
  }
}

int mailactivesync_settings_result_status_to_error(
    struct mailactivesync_settings_result * result)
{
  int r;

  if (result == NULL)
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  r = mailactivesync_settings_status_to_error(result->status);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  if (result->device_information_status != 0) {
    r = mailactivesync_settings_status_to_error(
        result->device_information_status);
    if (r != MAILACTIVESYNC_NO_ERROR)
      return r;
  }

  if (result->user_information_status != 0)
    return mailactivesync_settings_status_to_error(
        result->user_information_status);

  return MAILACTIVESYNC_NO_ERROR;
}

int mailactivesync_validate_cert_certificate_status_to_error(int status)
{
  switch (status) {
  case 0:
  case 1:
    return MAILACTIVESYNC_NO_ERROR;
  case 14:
    return MAILACTIVESYNC_ERROR_SERVER_BUSY;
  case 2:
  case 3:
  case 4:
  case 5:
  case 6:
  case 7:
  case 8:
  case 9:
  case 10:
  case 11:
  case 12:
  case 13:
  case 15:
  case 16:
  case 17:
    return MAILACTIVESYNC_ERROR_PROTOCOL;
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
  if (result == NULL)
    return;
  free(result->primary_smtp_address);
  string_list_free(result->smtp_addresses);
  if (result->accounts != NULL) {
    clist_foreach(result->accounts, free_settings_account_item, NULL);
    clist_free(result->accounts);
  }
  free(result);
}

void mailactivesync_settings_account_free(
    struct mailactivesync_settings_account * account)
{
  if (account == NULL)
    return;
  free(account->account_id);
  free(account->account_name);
  free(account->user_display_name);
  free(account->primary_smtp_address);
  string_list_free(account->smtp_addresses);
  free(account);
}

void mailactivesync_get_item_estimate_result_free(
    struct mailactivesync_get_item_estimate_result * result)
{
  if (result == NULL)
    return;
  if (result->collections != NULL) {
    clist_foreach(result->collections,
        free_get_item_estimate_collection_item, NULL);
    clist_free(result->collections);
  }
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
  free(message->display_to);
  free(message->message_class);
  free(message->thread_topic);
  free(message->internet_cpid);
  free(message->content_class);
  string_list_free(message->categories);
  free(message->conversation_id);
  free(message->conversation_index);
  free(message->flag_type);
  free(message->flag_complete_time);
  free(message->mime);
  mailactivesync_airsyncbase_body_free(message->body);
  if (message->body_parts != NULL) {
    clist_foreach(message->body_parts, free_body_part_item, NULL);
    clist_free(message->body_parts);
  }
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

void mailactivesync_body_part_free(struct mailactivesync_body_part * body_part)
{
  if (body_part == NULL)
    return;
  free(body_part->data);
  free(body_part->preview);
  free(body_part);
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
  free(result->collection_id);
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
  if (result->command_responses != NULL) {
    clist_foreach(result->command_responses,
        free_sync_command_response_item, NULL);
    clist_free(result->command_responses);
  }
  if (result->collections != NULL) {
    clist_foreach(result->collections, free_sync_result_item, NULL);
    clist_free(result->collections);
  }
  free(result);
}

void mailactivesync_item_free(struct mailactivesync_item * item)
{
  if (item == NULL)
    return;
  free(item->collection_id);
  free(item->server_id);
  free(item->mime);
  mailactivesync_airsyncbase_body_free(item->body);
  if (item->body_parts != NULL) {
    clist_foreach(item->body_parts, free_body_part_item, NULL);
    clist_free(item->body_parts);
  }
  free(item);
}

void mailactivesync_item_operations_fetch_result_free(
    struct mailactivesync_item_operations_fetch_result * result)
{
  if (result == NULL)
    return;
  if (result->items != NULL) {
    clist_foreach(result->items, free_item_item, NULL);
    clist_free(result->items);
  }
  free(result);
}

void mailactivesync_attachment_data_free(
    struct mailactivesync_attachment_data * data)
{
  if (data == NULL)
    return;
  free(data->file_reference);
  free(data->range);
  free(data->data);
  free(data);
}

void mailactivesync_mail_search_item_free(
    struct mailactivesync_mail_search_item * item)
{
  if (item == NULL)
    return;
  free(item->collection_id);
  free(item->server_id);
  free(item->long_id);
  mailactivesync_message_free(item->message);
  free(item);
}

void mailactivesync_mail_search_result_free(
    struct mailactivesync_mail_search_result * result)
{
  if (result == NULL)
    return;
  free(result->range);
  if (result->items != NULL) {
    clist_foreach(result->items, free_mail_search_item, NULL);
    clist_free(result->items);
  }
  free(result);
}

void mailactivesync_mail_find_item_free(
    struct mailactivesync_mail_find_item * item)
{
  if (item == NULL)
    return;
  free(item->collection_id);
  free(item->server_id);
  free(item->collection_class);
  free(item->preview);
  free(item->display_cc);
  free(item->display_bcc);
  mailactivesync_message_free(item->message);
  free(item);
}

void mailactivesync_mail_find_result_free(
    struct mailactivesync_mail_find_result * result)
{
  if (result == NULL)
    return;
  free(result->store);
  free(result->range);
  if (result->items != NULL) {
    clist_foreach(result->items, free_mail_find_item, NULL);
    clist_free(result->items);
  }
  free(result);
}

void mailactivesync_resolved_recipient_free(
    struct mailactivesync_resolved_recipient * recipient)
{
  if (recipient == NULL)
    return;
  free(recipient->display_name);
  free(recipient->email_address);
  string_list_free(recipient->certificates);
  free(recipient->mini_certificate);
  free(recipient);
}

void mailactivesync_resolve_recipients_response_free(
    struct mailactivesync_resolve_recipients_response * response)
{
  if (response == NULL)
    return;
  free(response->to);
  if (response->recipients != NULL) {
    clist_foreach(response->recipients, free_resolved_recipient_item, NULL);
    clist_free(response->recipients);
  }
  free(response);
}

void mailactivesync_resolve_recipients_result_free(
    struct mailactivesync_resolve_recipients_result * result)
{
  if (result == NULL)
    return;
  if (result->responses != NULL) {
    clist_foreach(result->responses, free_resolve_recipients_response_item,
        NULL);
    clist_free(result->responses);
  }
  free(result);
}

void mailactivesync_validate_cert_certificate_free(
    struct mailactivesync_validate_cert_certificate * certificate)
{
  if (certificate == NULL)
    return;
  string_list_free(certificate->statuses);
  free(certificate);
}

void mailactivesync_validate_cert_result_free(
    struct mailactivesync_validate_cert_result * result)
{
  if (result == NULL)
    return;
  if (result->certificates != NULL) {
    clist_foreach(result->certificates,
        free_validate_cert_certificate_item, NULL);
    clist_free(result->certificates);
  }
  free(result);
}

void mailactivesync_move_items_result_free(
    struct mailactivesync_move_items_result * result)
{
  if (result == NULL)
    return;
  if (result->responses != NULL) {
    clist_foreach(result->responses, free_move_response_item, NULL);
    clist_free(result->responses);
  }
  free(result);
}

void mailactivesync_move_response_free(
    struct mailactivesync_move_response * response)
{
  if (response == NULL)
    return;
  free(response->src_msg_id);
  free(response->src_folder_id);
  free(response->dst_folder_id);
  free(response->dst_msg_id);
  free(response);
}

void mailactivesync_ping_result_free(
    struct mailactivesync_ping_result * result)
{
  if (result == NULL)
    return;
  string_list_free(result->changed_collection_ids);
  free(result);
}
