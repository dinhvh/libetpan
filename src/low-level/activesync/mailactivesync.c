/*
 * libEtPan! -- a mail stuff library
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailactivesync.h"
#include "mailactivesync_command.h"
#include "mailactivesync_codes.h"
#include "mailactivesync_http.h"
#include "mailactivesync_wbxml.h"

#include <libetpan/mmapstring.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int set_string(char ** target, const char * value)
{
  char * dup_value;

  if (value == NULL) {
    dup_value = NULL;
  }
  else {
    dup_value = strdup(value);
    if (dup_value == NULL)
      return MAILACTIVESYNC_ERROR_MEMORY;
  }

  free(* target);
  * target = dup_value;

  return MAILACTIVESYNC_NO_ERROR;
}

static void free_string_item(void * value, void * data)
{
  (void) data;
  free(value);
}

static void string_list_free(clist * list)
{
  if (list == NULL)
    return;
  clist_foreach(list, free_string_item, NULL);
  clist_free(list);
}

static int string_ends_with(const char * str, const char * suffix)
{
  size_t str_len;
  size_t suffix_len;

  str_len = strlen(str);
  suffix_len = strlen(suffix);
  if (str_len < suffix_len)
    return 0;

  return strcmp(str + str_len - suffix_len, suffix) == 0;
}

static int normalize_server_url(char ** result, const char * server_url)
{
  static const char active_sync_path[] = "/Microsoft-Server-ActiveSync";
  MMAPString * buffer;
  size_t len;
  char * normalized;

  if ((result == NULL) || (server_url == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  buffer = mmap_string_sized_new(strlen(server_url) + sizeof(active_sync_path));
  if (buffer == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  if (mmap_string_append(buffer, server_url) == NULL) {
    mmap_string_free(buffer);
    return MAILACTIVESYNC_ERROR_MEMORY;
  }

  len = buffer->len;
  while ((len > 0) && isspace((unsigned char) buffer->str[len - 1]))
    len --;
  mmap_string_truncate(buffer, len);

  if (!string_ends_with(buffer->str, active_sync_path)) {
    if ((buffer->len > 0) && (buffer->str[buffer->len - 1] == '/'))
      mmap_string_truncate(buffer, buffer->len - 1);
    if (mmap_string_append(buffer, active_sync_path) == NULL) {
      mmap_string_free(buffer);
      return MAILACTIVESYNC_ERROR_MEMORY;
    }
  }

  normalized = strdup(buffer->str);
  mmap_string_free(buffer);
  if (normalized == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  * result = normalized;
  return MAILACTIVESYNC_NO_ERROR;
}

mailactivesync * mailactivesync_new(void)
{
  mailactivesync * session;

  session = malloc(sizeof(* session));
  if (session == NULL)
    return NULL;

  session->as_server_url = NULL;
  session->as_login = NULL;
  session->as_password = NULL;
  session->as_oauth_token = NULL;
  session->as_device_id = NULL;
  session->as_device_type = NULL;
  session->as_protocol_version = NULL;
  session->as_policy_key = NULL;
  session->as_user_agent = NULL;
  session->as_last_redirect_url = NULL;
  session->as_last_authenticate_header = NULL;
  session->as_connected = 0;
  session->as_authenticated = 0;
  session->as_advertised_commands = NULL;
  session->as_http_transport = NULL;

  if (mailactivesync_set_protocol_version(session, "16.1") !=
      MAILACTIVESYNC_NO_ERROR)
    goto err;

  return session;

 err:
  mailactivesync_free(session);
  return NULL;
}

void mailactivesync_free(mailactivesync * session)
{
  if (session == NULL)
    return;
  free(session->as_server_url);
  free(session->as_login);
  free(session->as_password);
  free(session->as_oauth_token);
  free(session->as_device_id);
  free(session->as_device_type);
  free(session->as_protocol_version);
  free(session->as_policy_key);
  free(session->as_user_agent);
  free(session->as_last_redirect_url);
  free(session->as_last_authenticate_header);
  string_list_free(session->as_advertised_commands);
  mailactivesync_http_transport_free(session->as_http_transport);
  free(session);
}

int mailactivesync_connect(mailactivesync * session,
    const char * server_url)
{
  int r;
  char * normalized_url;

  if ((session == NULL) || (server_url == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  normalized_url = NULL;
  r = normalize_server_url(&normalized_url, server_url);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  free(session->as_server_url);
  session->as_server_url = normalized_url;
  string_list_free(session->as_advertised_commands);
  session->as_advertised_commands = NULL;

  if (session->as_http_transport == NULL) {
    r = mailactivesync_http_transport_new_default(
        &session->as_http_transport);
    if (r != MAILACTIVESYNC_NO_ERROR)
      return r;
  }

  session->as_connected = 1;
  return MAILACTIVESYNC_NO_ERROR;
}

int mailactivesync_set_http_transport(mailactivesync * session,
    struct mailactivesync_http_transport * transport)
{
  if (session == NULL)
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  mailactivesync_http_transport_free(session->as_http_transport);
  session->as_http_transport = transport;
  return MAILACTIVESYNC_NO_ERROR;
}

int mailactivesync_set_device(mailactivesync * session,
    const char * device_id,
    const char * device_type)
{
  int r;

  if (session == NULL)
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  r = set_string(&session->as_device_id, device_id);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  return set_string(&session->as_device_type, device_type);
}

int mailactivesync_set_protocol_version(mailactivesync * session,
    const char * version)
{
  if (session == NULL)
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  return set_string(&session->as_protocol_version, version);
}

int mailactivesync_set_policy_key(mailactivesync * session,
    const char * policy_key)
{
  if (session == NULL)
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  return set_string(&session->as_policy_key, policy_key);
}

int mailactivesync_set_user_agent(mailactivesync * session,
    const char * user_agent)
{
  if (session == NULL)
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  return set_string(&session->as_user_agent, user_agent);
}

const char * mailactivesync_get_last_redirect_url(mailactivesync * session)
{
  if (session == NULL)
    return NULL;

  return session->as_last_redirect_url;
}

const char * mailactivesync_get_last_authenticate_header(
    mailactivesync * session)
{
  if (session == NULL)
    return NULL;

  return session->as_last_authenticate_header;
}

int mailactivesync_login(mailactivesync * session,
    const char * user,
    const char * password)
{
  int r;

  if ((session == NULL) || (user == NULL) || (password == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  r = set_string(&session->as_login, user);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  r = set_string(&session->as_password, password);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  r = set_string(&session->as_oauth_token, NULL);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  session->as_authenticated = 1;
  return MAILACTIVESYNC_NO_ERROR;
}

int mailactivesync_login_oauth2(mailactivesync * session,
    const char * user,
    const char * access_token)
{
  int r;

  if ((session == NULL) || (user == NULL) || (access_token == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  r = set_string(&session->as_login, user);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  r = set_string(&session->as_oauth_token, access_token);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  r = set_string(&session->as_password, NULL);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  session->as_authenticated = 1;
  return MAILACTIVESYNC_NO_ERROR;
}

int mailactivesync_set_oauth2_token(mailactivesync * session,
    const char * access_token)
{
  if ((session == NULL) || (access_token == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  return set_string(&session->as_oauth_token, access_token);
}

static int require_ready(mailactivesync * session)
{
  if ((session == NULL) || !session->as_connected ||
      !session->as_authenticated)
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  return MAILACTIVESYNC_NO_ERROR;
}

int mailactivesync_options(mailactivesync * session,
    struct mailactivesync_options ** result)
{
  int r;

  if (result == NULL)
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;

  r = require_ready(session);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  return mailactivesync_command_options(session, result);
}

int mailactivesync_folder_sync(mailactivesync * session,
    const char * sync_key,
    struct mailactivesync_folder_sync_result ** result)
{
  int r;

  if ((sync_key == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;

  r = require_ready(session);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  return mailactivesync_command_folder_sync(session, sync_key, result);
}

int mailactivesync_folder_resync(mailactivesync * session,
    struct mailactivesync_folder_sync_result ** result)
{
  return mailactivesync_folder_sync(session, "0", result);
}

int mailactivesync_folder_create(mailactivesync * session,
    const char * sync_key,
    const char * parent_id,
    const char * display_name,
    int type,
    struct mailactivesync_folder_mutation_result ** result)
{
  int r;

  if ((sync_key == NULL) || (parent_id == NULL) ||
      (display_name == NULL) || (type <= 0) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;

  r = require_ready(session);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  return mailactivesync_command_folder_create(session, sync_key, parent_id,
      display_name, type, result);
}

int mailactivesync_folder_update(mailactivesync * session,
    const char * sync_key,
    const char * server_id,
    const char * parent_id,
    const char * display_name,
    struct mailactivesync_folder_mutation_result ** result)
{
  int r;

  if ((sync_key == NULL) || (server_id == NULL) || (parent_id == NULL) ||
      (display_name == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;

  r = require_ready(session);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  return mailactivesync_command_folder_update(session, sync_key, server_id,
      parent_id, display_name, result);
}

int mailactivesync_folder_delete(mailactivesync * session,
    const char * sync_key,
    const char * server_id,
    struct mailactivesync_folder_mutation_result ** result)
{
  int r;

  if ((sync_key == NULL) || (server_id == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;

  r = require_ready(session);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  return mailactivesync_command_folder_delete(session, sync_key, server_id,
      result);
}

int mailactivesync_sync(mailactivesync * session,
    struct mailactivesync_sync_request * request,
    struct mailactivesync_sync_result ** result)
{
  int r;

  if ((request == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;

  r = require_ready(session);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  return mailactivesync_command_sync(session, request, result);
}

int mailactivesync_sync_multi(mailactivesync * session,
    clist * requests,
    struct mailactivesync_sync_result ** result)
{
  clistiter * cur;
  int first;
  int r;

  if ((requests == NULL) || (clist_count(requests) == 0) ||
      (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  first = 1;
  for (cur = clist_begin(requests); cur != NULL; cur = clist_next(cur)) {
    struct mailactivesync_sync_request * request;

    request = clist_content(cur);
    if ((request == NULL) || (request->collection_id == NULL))
      return MAILACTIVESYNC_ERROR_BAD_STATE;
    if (!first && (request->has_wait || request->has_heartbeat_interval))
      return MAILACTIVESYNC_ERROR_BAD_STATE;
    first = 0;
  }

  * result = NULL;

  r = require_ready(session);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  return mailactivesync_command_sync_multi(session, requests, result);
}

int mailactivesync_provision(mailactivesync * session,
    struct mailactivesync_provision_result ** result)
{
  int r;

  if (result == NULL)
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;

  r = require_ready(session);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  return mailactivesync_command_provision(session, result);
}

int mailactivesync_settings_set_device_information(mailactivesync * session,
    const struct mailactivesync_device_information * device_information,
    struct mailactivesync_settings_result ** result)
{
  int r;

  if ((device_information == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;

  r = require_ready(session);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  return mailactivesync_command_settings_set_device_information(session,
      device_information, result);
}

int mailactivesync_settings_get_user_information(mailactivesync * session,
    struct mailactivesync_settings_result ** result)
{
  int r;

  if (result == NULL)
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;

  r = require_ready(session);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  return mailactivesync_command_settings_get_user_information(session, result);
}

int mailactivesync_get_item_estimate(mailactivesync * session,
    const char * collection_id,
    const char * sync_key,
    struct mailactivesync_get_item_estimate_result ** result)
{
  int r;

  if ((collection_id == NULL) || (sync_key == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;

  r = require_ready(session);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  return mailactivesync_command_get_item_estimate(session, collection_id,
      sync_key, result);
}

int mailactivesync_get_item_estimate_multi(mailactivesync * session,
    clist * collections,
    struct mailactivesync_get_item_estimate_result ** result)
{
  int r;

  if ((collections == NULL) || (clist_count(collections) == 0) ||
      (clist_count(collections) > 300) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;

  r = require_ready(session);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  return mailactivesync_command_get_item_estimate_multi(session, collections,
      result);
}

int mailactivesync_item_operations_fetch(mailactivesync * session,
    const char * collection_id,
    const char * server_id,
    struct mailactivesync_item ** result)
{
  int r;

  if ((collection_id == NULL) || (server_id == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;

  r = require_ready(session);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  return mailactivesync_command_item_operations_fetch(session, collection_id,
      server_id, result);
}

static int session_supports_body_part_preference(mailactivesync * session)
{
  if ((session == NULL) || (session->as_protocol_version == NULL))
    return 1;
  return (strcmp(session->as_protocol_version, "14.1") == 0) ||
      (strcmp(session->as_protocol_version, "16.0") == 0) ||
      (strcmp(session->as_protocol_version, "16.1") == 0);
}

int mailactivesync_item_operations_fetch_body_part(mailactivesync * session,
    const char * collection_id,
    const char * server_id,
    int body_type,
    uint32_t truncation_size,
    struct mailactivesync_item ** result)
{
  int r;

  if ((collection_id == NULL) || (server_id == NULL) ||
      (body_type < MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_PLAIN_TEXT) ||
      (body_type > MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_MIME) ||
      (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;

  r = require_ready(session);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  if (!session_supports_body_part_preference(session))
    return MAILACTIVESYNC_ERROR_NOT_IMPLEMENTED;

  return mailactivesync_command_item_operations_fetch_body_part(session,
      collection_id, server_id, body_type, truncation_size, result);
}

int mailactivesync_item_operations_fetch_multi(mailactivesync * session,
    clist * requests,
    struct mailactivesync_item_operations_fetch_result ** result)
{
  clistiter * cur;
  int needs_body_part_preference;
  int r;

  if ((requests == NULL) || (clist_count(requests) == 0) ||
      (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  needs_body_part_preference = 0;
  for (cur = clist_begin(requests); cur != NULL; cur = clist_next(cur)) {
    struct mailactivesync_item_operations_fetch_request * request;

    request = clist_content(cur);
    if ((request == NULL) || (request->collection_id == NULL) ||
        (request->server_id == NULL) || (request->body_part_type < 0) ||
        (request->body_part_type >
            MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_MIME))
      return MAILACTIVESYNC_ERROR_BAD_STATE;
    if (request->body_part_type != 0)
      needs_body_part_preference = 1;
  }

  * result = NULL;

  r = require_ready(session);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  if (needs_body_part_preference && !session_supports_body_part_preference(
      session))
    return MAILACTIVESYNC_ERROR_NOT_IMPLEMENTED;

  return mailactivesync_command_item_operations_fetch_multi(session,
      requests, result);
}

int mailactivesync_item_operations_fetch_attachment(mailactivesync * session,
    const char * file_reference,
    const char * range,
    struct mailactivesync_attachment_data ** result)
{
  int r;

  if ((file_reference == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;

  r = require_ready(session);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  return mailactivesync_command_item_operations_fetch_attachment(session,
      file_reference, range, result);
}

static int mail_search_protocol_supported(mailactivesync * session)
{
  return (session != NULL) && (session->as_protocol_version != NULL) &&
      (strcmp(session->as_protocol_version, "2.5") != 0);
}

int mailactivesync_mail_search(mailactivesync * session,
    const struct mailactivesync_mail_search_request * request,
    struct mailactivesync_mail_search_result ** result)
{
  int r;

  if ((request == NULL) || (request->collection_id == NULL) ||
      (request->free_text == NULL) || (request->range_end <
      request->range_start) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;

  r = require_ready(session);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;
  if (!mail_search_protocol_supported(session))
    return MAILACTIVESYNC_ERROR_NOT_IMPLEMENTED;

  return mailactivesync_command_mail_search(session, request, result);
}

static int mail_find_protocol_supported(mailactivesync * session)
{
  return (session != NULL) && (session->as_protocol_version != NULL) &&
      (strcmp(session->as_protocol_version, "16.1") == 0);
}

int mailactivesync_mail_find(mailactivesync * session,
    const struct mailactivesync_mail_find_request * request,
    struct mailactivesync_mail_find_result ** result)
{
  int r;

  if ((request == NULL) || (request->search_id == NULL) ||
      (request->collection_id == NULL) || (request->query == NULL) ||
      (request->range_end < request->range_start) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;

  r = require_ready(session);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;
  if (!mail_find_protocol_supported(session))
    return MAILACTIVESYNC_ERROR_NOT_IMPLEMENTED;

  return mailactivesync_command_mail_find(session, request, result);
}

int mailactivesync_resolve_recipients(mailactivesync * session,
    clist * recipients,
    uint32_t max_ambiguous_recipients,
    struct mailactivesync_resolve_recipients_result ** result)
{
  struct mailactivesync_resolve_recipients_request request;

  memset(&request, 0, sizeof(request));
  request.recipients = recipients;
  request.max_ambiguous_recipients = max_ambiguous_recipients;

  return mailactivesync_resolve_recipients_ext(session, &request, result);
}

int mailactivesync_resolve_recipients_ext(mailactivesync * session,
    const struct mailactivesync_resolve_recipients_request * request,
    struct mailactivesync_resolve_recipients_result ** result)
{
  int r;

  if ((request == NULL) || (request->recipients == NULL) ||
      (clist_count(request->recipients) == 0) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;

  r = require_ready(session);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  return mailactivesync_command_resolve_recipients_ext(session, request,
      result);
}

int mailactivesync_validate_cert(mailactivesync * session,
    const struct mailactivesync_validate_cert_request * request,
    struct mailactivesync_validate_cert_result ** result)
{
  int r;

  if ((request == NULL) || (request->certificates == NULL) ||
      (clist_count(request->certificates) == 0) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;

  r = require_ready(session);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  return mailactivesync_command_validate_cert(session, request, result);
}

static int sync_result_command_status(
    struct mailactivesync_sync_result * result,
    int type,
    const char * server_id)
{
  if ((result == NULL) || (result->command_responses == NULL) ||
      (clist_count(result->command_responses) == 0))
    return MAILACTIVESYNC_NO_ERROR;

  return mailactivesync_sync_result_command_response_status_to_error(result,
      type, NULL, server_id);
}

static int run_single_sync_command(mailactivesync * session,
    const char * collection_id,
    const char * sync_key,
    struct mailactivesync_sync_command * command,
    int deletes_as_moves_set,
    int deletes_as_moves,
    struct mailactivesync_sync_result ** result)
{
  struct mailactivesync_sync_request * request;
  int command_type;
  char * server_id;
  int r;

  if ((collection_id == NULL) || (sync_key == NULL) || (command == NULL) ||
      (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;
  command_type = command->type;
  server_id = command->server_id != NULL ? strdup(command->server_id) : NULL;
  if ((command->server_id != NULL) && (server_id == NULL)) {
    mailactivesync_sync_command_free(command);
    return MAILACTIVESYNC_ERROR_MEMORY;
  }

  request = mailactivesync_sync_request_new(collection_id, sync_key);
  if (request == NULL) {
    mailactivesync_sync_command_free(command);
    free(server_id);
    return MAILACTIVESYNC_ERROR_MEMORY;
  }

  r = mailactivesync_sync_request_set_collection_class(request, "Email");
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;
  if (deletes_as_moves_set) {
    r = mailactivesync_sync_request_set_deletes_as_moves(request,
        deletes_as_moves);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto cleanup;
  }
  r = mailactivesync_sync_request_add_command(request, command);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;
  command = NULL;

  r = mailactivesync_sync(session, request, result);
  if (r == MAILACTIVESYNC_NO_ERROR)
    r = sync_result_command_status(* result, command_type, server_id);

 cleanup:
  mailactivesync_sync_command_free(command);
  mailactivesync_sync_request_free(request);
  free(server_id);
  return r;
}

static int add_child_text_checked(struct mailactivesync_wbxml_node * parent,
    uint8_t code_page,
    uint8_t token,
    const char * text)
{
  struct mailactivesync_wbxml_node * child;
  int r;

  child = mailactivesync_wbxml_node_new_text(code_page, token, text);
  if (child == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  r = mailactivesync_wbxml_node_add_child(parent, child);
  if (r != MAILACTIVESYNC_NO_ERROR)
    mailactivesync_wbxml_node_free(child);

  return r;
}

static int add_child_opaque_checked(struct mailactivesync_wbxml_node * parent,
    uint8_t code_page,
    uint8_t token,
    const unsigned char * data,
    size_t len)
{
  struct mailactivesync_wbxml_node * child;
  int r;

  child = mailactivesync_wbxml_node_new_opaque(code_page, token, data, len);
  if (child == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  r = mailactivesync_wbxml_node_add_child(parent, child);
  if (r != MAILACTIVESYNC_NO_ERROR)
    mailactivesync_wbxml_node_free(child);

  return r;
}

static int add_child_empty_checked(struct mailactivesync_wbxml_node * parent,
    uint8_t code_page,
    uint8_t token)
{
  struct mailactivesync_wbxml_node * child;
  int r;

  child = mailactivesync_wbxml_node_new(code_page, token);
  if (child == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  r = mailactivesync_wbxml_node_add_child(parent, child);
  if (r != MAILACTIVESYNC_NO_ERROR)
    mailactivesync_wbxml_node_free(child);

  return r;
}

static int draft_protocol_supported(mailactivesync * session)
{
  return (session != NULL) && (session->as_protocol_version != NULL) &&
      ((strcmp(session->as_protocol_version, "16.0") == 0) ||
      (strcmp(session->as_protocol_version, "16.1") == 0));
}

static int add_optional_text(
    struct mailactivesync_sync_command * command,
    uint8_t code_page,
    uint8_t token,
    const char * text)
{
  if (text == NULL)
    return MAILACTIVESYNC_NO_ERROR;

  return mailactivesync_sync_command_add_application_data_text(command,
      code_page, token, text);
}

static int draft_add_attachment_node(
    struct mailactivesync_wbxml_node * attachments_node,
    const struct mailactivesync_draft_attachment * attachment)
{
  struct mailactivesync_wbxml_node * add_node;
  char value[16];
  int r;

  if ((attachments_node == NULL) || (attachment == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;
  if ((attachment->client_id == NULL) || (attachment->display_name == NULL) ||
      ((attachment->content == NULL) && (attachment->content_len > 0)) ||
      ((attachment->method != 1) && (attachment->method != 5)))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  add_node = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_ADD);
  if (add_node == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  r = add_child_text_checked(add_node, MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_CLIENT_ID, attachment->client_id);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;
  snprintf(value, sizeof(value), "%d", attachment->method);
  r = add_child_text_checked(add_node, MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_METHOD, value);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;
  r = add_child_opaque_checked(add_node, MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_CONTENT, attachment->content,
      attachment->content_len);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;
  r = add_child_text_checked(add_node, MAILACTIVESYNC_CP_AIRSYNCBASE,
      MAILACTIVESYNC_AIRSYNCBASE_DISPLAY_NAME, attachment->display_name);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;
  if (attachment->content_type != NULL) {
    r = add_child_text_checked(add_node, MAILACTIVESYNC_CP_AIRSYNCBASE,
        MAILACTIVESYNC_AIRSYNCBASE_CONTENT_TYPE, attachment->content_type);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto cleanup;
  }
  if (attachment->content_id != NULL) {
    r = add_child_text_checked(add_node, MAILACTIVESYNC_CP_AIRSYNCBASE,
        MAILACTIVESYNC_AIRSYNCBASE_CONTENT_ID, attachment->content_id);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto cleanup;
  }
  if (attachment->is_inline) {
    r = add_child_empty_checked(add_node, MAILACTIVESYNC_CP_AIRSYNCBASE,
        MAILACTIVESYNC_AIRSYNCBASE_IS_INLINE);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto cleanup;
  }

  r = mailactivesync_wbxml_node_add_child(attachments_node, add_node);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;

  return MAILACTIVESYNC_NO_ERROR;

 cleanup:
  mailactivesync_wbxml_node_free(add_node);
  return r;
}

static int draft_add_attachments(
    struct mailactivesync_sync_command * command,
    clist * attachments)
{
  struct mailactivesync_wbxml_node * attachments_node;
  clistiter * cur;
  int r;

  if ((attachments == NULL) || (clist_count(attachments) == 0))
    return MAILACTIVESYNC_NO_ERROR;
  if (command == NULL)
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  attachments_node = mailactivesync_wbxml_node_new(
      MAILACTIVESYNC_CP_AIRSYNCBASE, MAILACTIVESYNC_AIRSYNCBASE_ATTACHMENTS);
  if (attachments_node == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  for (cur = clist_begin(attachments); cur != NULL; cur = clist_next(cur)) {
    r = draft_add_attachment_node(attachments_node, clist_content(cur));
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto cleanup;
  }

  r = mailactivesync_sync_command_add_application_data_node(command,
      attachments_node);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup;

  return MAILACTIVESYNC_NO_ERROR;

 cleanup:
  mailactivesync_wbxml_node_free(attachments_node);
  return r;
}

static int draft_validate_application_data(
    const struct mailactivesync_draft * draft)
{
  if (draft == NULL)
    return MAILACTIVESYNC_ERROR_BAD_STATE;
  if ((draft->body_type != 0) &&
      ((draft->body_type < MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_PLAIN_TEXT) ||
      (draft->body_type > MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_MIME)))
    return MAILACTIVESYNC_ERROR_BAD_STATE;
  if ((draft->body != NULL) && (draft->body_type == 0))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  if (draft->body_type == MAILACTIVESYNC_AIRSYNCBASE_BODY_TYPE_MIME) {
    if ((draft->to != NULL) || (draft->cc != NULL) ||
        (draft->bcc != NULL) || (draft->reply_to != NULL) ||
        (draft->subject != NULL))
      return MAILACTIVESYNC_ERROR_BAD_STATE;
  }

  return MAILACTIVESYNC_NO_ERROR;
}

static int draft_add_application_data(
    struct mailactivesync_sync_command * command,
    const struct mailactivesync_draft * draft)
{
  struct mailactivesync_wbxml_node * body_node;
  char value[16];
  int r;

  if ((command == NULL) || (draft == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;
  r = draft_validate_application_data(draft);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  r = add_optional_text(command, MAILACTIVESYNC_CP_EMAIL,
      MAILACTIVESYNC_EMAIL_TO, draft->to);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;
  r = add_optional_text(command, MAILACTIVESYNC_CP_EMAIL,
      MAILACTIVESYNC_EMAIL_CC, draft->cc);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;
  r = add_optional_text(command, MAILACTIVESYNC_CP_EMAIL2,
      MAILACTIVESYNC_EMAIL2_BCC, draft->bcc);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;
  r = add_optional_text(command, MAILACTIVESYNC_CP_EMAIL,
      MAILACTIVESYNC_EMAIL_REPLY_TO, draft->reply_to);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;
  r = add_optional_text(command, MAILACTIVESYNC_CP_EMAIL,
      MAILACTIVESYNC_EMAIL_SUBJECT, draft->subject);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  if (draft->has_importance) {
    snprintf(value, sizeof(value), "%d", draft->importance);
    r = mailactivesync_sync_command_add_application_data_text(command,
        MAILACTIVESYNC_CP_EMAIL, MAILACTIVESYNC_EMAIL_IMPORTANCE, value);
    if (r != MAILACTIVESYNC_NO_ERROR)
      return r;
  }
  if (draft->has_read) {
    r = mailactivesync_sync_command_add_application_data_text(command,
        MAILACTIVESYNC_CP_EMAIL, MAILACTIVESYNC_EMAIL_READ,
        draft->read ? "1" : "0");
    if (r != MAILACTIVESYNC_NO_ERROR)
      return r;
  }

  if (draft->body != NULL) {
    body_node = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_AIRSYNCBASE,
        MAILACTIVESYNC_AIRSYNCBASE_BODY);
    if (body_node == NULL)
      return MAILACTIVESYNC_ERROR_MEMORY;

    snprintf(value, sizeof(value), "%d", draft->body_type);
    r = add_child_text_checked(body_node, MAILACTIVESYNC_CP_AIRSYNCBASE,
        MAILACTIVESYNC_AIRSYNCBASE_TYPE, value);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto cleanup_body;
    r = add_child_text_checked(body_node, MAILACTIVESYNC_CP_AIRSYNCBASE,
        MAILACTIVESYNC_AIRSYNCBASE_DATA, draft->body);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto cleanup_body;
    r = mailactivesync_sync_command_add_application_data_node(command,
        body_node);
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto cleanup_body;
    body_node = NULL;
  }

  r = draft_add_attachments(command, draft->attachments);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  return MAILACTIVESYNC_NO_ERROR;

 cleanup_body:
  mailactivesync_wbxml_node_free(body_node);
  return r;
}

int mailactivesync_mark_read(mailactivesync * session,
    const char * collection_id,
    const char * sync_key,
    const char * server_id,
    int read,
    struct mailactivesync_sync_result ** result)
{
  struct mailactivesync_sync_command * command;
  int r;

  if ((collection_id == NULL) || (sync_key == NULL) || (server_id == NULL) ||
      (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  r = require_ready(session);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  command = mailactivesync_sync_command_change_new(server_id);
  if (command == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  r = mailactivesync_sync_command_add_application_data_text(command,
      MAILACTIVESYNC_CP_EMAIL, MAILACTIVESYNC_EMAIL_READ, read ? "1" : "0");
  if (r != MAILACTIVESYNC_NO_ERROR) {
    mailactivesync_sync_command_free(command);
    return r;
  }

  return run_single_sync_command(session, collection_id, sync_key, command,
      0, 0, result);
}

int mailactivesync_set_flagged(mailactivesync * session,
    const char * collection_id,
    const char * sync_key,
    const char * server_id,
    int flagged,
    struct mailactivesync_sync_result ** result)
{
  struct mailactivesync_sync_command * command;
  struct mailactivesync_wbxml_node * flag_node;
  int r;

  if ((collection_id == NULL) || (sync_key == NULL) || (server_id == NULL) ||
      (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  r = require_ready(session);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  command = mailactivesync_sync_command_change_new(server_id);
  if (command == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  flag_node = mailactivesync_wbxml_node_new(MAILACTIVESYNC_CP_EMAIL,
      MAILACTIVESYNC_EMAIL_FLAG);
  if (flag_node == NULL) {
    mailactivesync_sync_command_free(command);
    return MAILACTIVESYNC_ERROR_MEMORY;
  }

  r = add_child_text_checked(flag_node, MAILACTIVESYNC_CP_EMAIL,
      MAILACTIVESYNC_EMAIL_STATUS, flagged ? "2" : "0");
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup_flag;
  if (flagged) {
    r = add_child_text_checked(flag_node, MAILACTIVESYNC_CP_EMAIL,
        MAILACTIVESYNC_EMAIL_FLAG_TYPE, "Flag for follow up");
    if (r != MAILACTIVESYNC_NO_ERROR)
      goto cleanup_flag;
  }

  r = mailactivesync_sync_command_add_application_data_node(command,
      flag_node);
  if (r != MAILACTIVESYNC_NO_ERROR)
    goto cleanup_flag;
  flag_node = NULL;

  return run_single_sync_command(session, collection_id, sync_key, command,
      0, 0, result);

 cleanup_flag:
  mailactivesync_wbxml_node_free(flag_node);
  mailactivesync_sync_command_free(command);
  return r;
}

int mailactivesync_delete_message(mailactivesync * session,
    const char * collection_id,
    const char * sync_key,
    const char * server_id,
    int deletes_as_moves,
    struct mailactivesync_sync_result ** result)
{
  struct mailactivesync_sync_command * command;
  int r;

  if ((collection_id == NULL) || (sync_key == NULL) || (server_id == NULL) ||
      (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  r = require_ready(session);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  command = mailactivesync_sync_command_delete_new(server_id);
  if (command == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  return run_single_sync_command(session, collection_id, sync_key, command,
      1, deletes_as_moves, result);
}

int mailactivesync_add_draft(mailactivesync * session,
    const char * collection_id,
    const char * sync_key,
    const char * client_id,
    const struct mailactivesync_draft * draft,
    struct mailactivesync_sync_result ** result)
{
  struct mailactivesync_sync_command * command;
  int r;

  if ((collection_id == NULL) || (sync_key == NULL) || (client_id == NULL) ||
      (draft == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  r = require_ready(session);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;
  if (!draft_protocol_supported(session))
    return MAILACTIVESYNC_ERROR_NOT_IMPLEMENTED;

  command = mailactivesync_sync_command_add_new(client_id, "Email");
  if (command == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  r = draft_add_application_data(command, draft);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    mailactivesync_sync_command_free(command);
    return r;
  }

  return run_single_sync_command(session, collection_id, sync_key, command,
      0, 0, result);
}

int mailactivesync_update_draft(mailactivesync * session,
    const char * collection_id,
    const char * sync_key,
    const char * server_id,
    const struct mailactivesync_draft * draft,
    struct mailactivesync_sync_result ** result)
{
  struct mailactivesync_sync_command * command;
  int r;

  if ((collection_id == NULL) || (sync_key == NULL) || (server_id == NULL) ||
      (draft == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  r = require_ready(session);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;
  if (!draft_protocol_supported(session))
    return MAILACTIVESYNC_ERROR_NOT_IMPLEMENTED;

  command = mailactivesync_sync_command_change_new(server_id);
  if (command == NULL)
    return MAILACTIVESYNC_ERROR_MEMORY;

  r = draft_add_application_data(command, draft);
  if (r != MAILACTIVESYNC_NO_ERROR) {
    mailactivesync_sync_command_free(command);
    return r;
  }

  return run_single_sync_command(session, collection_id, sync_key, command,
      0, 0, result);
}

int mailactivesync_send_mail(mailactivesync * session,
    const char * mime_message,
    size_t mime_message_len,
    int save_in_sent)
{
  struct mailactivesync_composemail_request request;

  memset(&request, 0, sizeof(request));
  request.mime_message = mime_message;
  request.mime_message_len = mime_message_len;
  request.save_in_sent = save_in_sent;

  return mailactivesync_send_mail_ext(session, &request);
}

int mailactivesync_send_mail_ext(mailactivesync * session,
    const struct mailactivesync_composemail_request * request)
{
  int r;

  if ((request == NULL) || (request->mime_message == NULL) ||
      (request->collection_id != NULL) || (request->server_id != NULL) ||
      (request->long_id != NULL) || (request->instance_id != NULL) ||
      request->replace_mime)
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  r = require_ready(session);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  return mailactivesync_command_send_mail_ext(session, request);
}

int mailactivesync_smart_reply(mailactivesync * session,
    const char * collection_id,
    const char * server_id,
    const char * mime_message,
    size_t mime_message_len,
    int save_in_sent)
{
  struct mailactivesync_composemail_request request;

  memset(&request, 0, sizeof(request));
  request.collection_id = collection_id;
  request.server_id = server_id;
  request.mime_message = mime_message;
  request.mime_message_len = mime_message_len;
  request.save_in_sent = save_in_sent;

  return mailactivesync_smart_reply_ext(session, &request);
}

int mailactivesync_smart_reply_ext(mailactivesync * session,
    const struct mailactivesync_composemail_request * request)
{
  int r;

  if ((request == NULL) || (request->mime_message == NULL) ||
      ((request->long_id == NULL) &&
      ((request->collection_id == NULL) || (request->server_id == NULL))) ||
      ((request->long_id != NULL) &&
      ((request->collection_id != NULL) || (request->server_id != NULL))))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  r = require_ready(session);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  return mailactivesync_command_smart_reply_ext(session, request);
}

int mailactivesync_smart_forward(mailactivesync * session,
    const char * collection_id,
    const char * server_id,
    const char * mime_message,
    size_t mime_message_len,
    int save_in_sent)
{
  struct mailactivesync_composemail_request request;

  memset(&request, 0, sizeof(request));
  request.collection_id = collection_id;
  request.server_id = server_id;
  request.mime_message = mime_message;
  request.mime_message_len = mime_message_len;
  request.save_in_sent = save_in_sent;

  return mailactivesync_smart_forward_ext(session, &request);
}

int mailactivesync_smart_forward_ext(mailactivesync * session,
    const struct mailactivesync_composemail_request * request)
{
  int r;

  if ((request == NULL) || (request->mime_message == NULL) ||
      ((request->long_id == NULL) &&
      ((request->collection_id == NULL) || (request->server_id == NULL))) ||
      ((request->long_id != NULL) &&
      ((request->collection_id != NULL) || (request->server_id != NULL))))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  r = require_ready(session);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  return mailactivesync_command_smart_forward_ext(session, request);
}

int mailactivesync_move_items(mailactivesync * session,
    clist * moves,
    struct mailactivesync_move_items_result ** result)
{
  int r;

  if ((moves == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;

  r = require_ready(session);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  return mailactivesync_command_move_items(session, moves, result);
}

int mailactivesync_ping(mailactivesync * session,
    struct mailactivesync_ping_request * request,
    struct mailactivesync_ping_result ** result)
{
  int r;

  if ((request == NULL) || (result == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  * result = NULL;

  r = require_ready(session);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  return mailactivesync_command_ping(session, request, result);
}
