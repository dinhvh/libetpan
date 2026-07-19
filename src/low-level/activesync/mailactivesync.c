/*
 * libEtPan! -- a mail stuff library
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailactivesync.h"

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

mailactivesync * mailactivesync_new(int cached,
    const char * cache_directory)
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
  session->as_connected = 0;
  session->as_authenticated = 0;
  session->as_cached = cached;
  session->as_cache_directory = NULL;

  if (cache_directory != NULL) {
    session->as_cache_directory = strdup(cache_directory);
    if (session->as_cache_directory == NULL)
      goto err;
  }

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
  free(session->as_cache_directory);
  free(session);
}

int mailactivesync_connect(mailactivesync * session,
    const char * server_url)
{
  int r;

  if ((session == NULL) || (server_url == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  r = set_string(&session->as_server_url, server_url);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  session->as_connected = 1;
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

  return MAILACTIVESYNC_ERROR_NOT_IMPLEMENTED;
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

  return MAILACTIVESYNC_ERROR_NOT_IMPLEMENTED;
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

  return MAILACTIVESYNC_ERROR_NOT_IMPLEMENTED;
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

  return MAILACTIVESYNC_ERROR_NOT_IMPLEMENTED;
}

int mailactivesync_send_mail(mailactivesync * session,
    const char * mime_message,
    size_t mime_message_len,
    int save_in_sent)
{
  int r;

  (void) mime_message_len;
  (void) save_in_sent;

  if (mime_message == NULL)
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  r = require_ready(session);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  return MAILACTIVESYNC_ERROR_NOT_IMPLEMENTED;
}

int mailactivesync_smart_reply(mailactivesync * session,
    const char * collection_id,
    const char * server_id,
    const char * mime_message,
    size_t mime_message_len,
    int save_in_sent)
{
  int r;

  (void) mime_message_len;
  (void) save_in_sent;

  if ((collection_id == NULL) || (server_id == NULL) || (mime_message == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  r = require_ready(session);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  return MAILACTIVESYNC_ERROR_NOT_IMPLEMENTED;
}

int mailactivesync_smart_forward(mailactivesync * session,
    const char * collection_id,
    const char * server_id,
    const char * mime_message,
    size_t mime_message_len,
    int save_in_sent)
{
  int r;

  (void) mime_message_len;
  (void) save_in_sent;

  if ((collection_id == NULL) || (server_id == NULL) || (mime_message == NULL))
    return MAILACTIVESYNC_ERROR_BAD_STATE;

  r = require_ready(session);
  if (r != MAILACTIVESYNC_NO_ERROR)
    return r;

  return MAILACTIVESYNC_ERROR_NOT_IMPLEMENTED;
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

  return MAILACTIVESYNC_ERROR_NOT_IMPLEMENTED;
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

  return MAILACTIVESYNC_ERROR_NOT_IMPLEMENTED;
}
