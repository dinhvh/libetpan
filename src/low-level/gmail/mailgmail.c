/*
 * libEtPan! -- a mail stuff library
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailgmail.h"
#include "mailgmail_http.h"
#include "mailgmail_parser.h"
#include "mailgmail_private.h"
#include "mailgmail_url.h"

#include <libetpan/mmapstring.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char * dup_string(const char * value)
{
  if (value == NULL)
    return NULL;

  return strdup(value);
}

static int replace_string(char ** target, const char * value)
{
  char * copy;

  if (target == NULL)
    return MAILGMAIL_ERROR_BAD_STATE;

  copy = NULL;
  if (value != NULL) {
    copy = dup_string(value);
    if (copy == NULL)
      return MAILGMAIL_ERROR_MEMORY;
  }

  free(* target);
  * target = copy;
  return MAILGMAIL_NO_ERROR;
}

static void clear_last_error(mailgmail * session)
{
  if (session == NULL)
    return;

  session->gmail_last_http_status = 0;
  free(session->gmail_last_error_message);
  session->gmail_last_error_message = NULL;
}

static int set_last_error_message(mailgmail * session, const char * message)
{
  if (session == NULL)
    return MAILGMAIL_ERROR_BAD_STATE;

  free(session->gmail_last_error_message);
  session->gmail_last_error_message = NULL;

  if (message != NULL) {
    session->gmail_last_error_message = dup_string(message);
    if (session->gmail_last_error_message == NULL)
      return MAILGMAIL_ERROR_MEMORY;
  }

  return MAILGMAIL_NO_ERROR;
}

mailgmail * mailgmail_new(void)
{
  mailgmail * session;
  int r;

  session = malloc(sizeof(* session));
  if (session == NULL)
    return NULL;

  session->gmail_user_id = NULL;
  session->gmail_oauth_token = NULL;
  session->gmail_user_agent = NULL;
  session->gmail_timeout = 60;
  session->gmail_last_http_status = 0;
  session->gmail_last_error_message = NULL;
  session->gmail_http_transport = NULL;

  r = replace_string(&session->gmail_user_id, "me");
  if (r != MAILGMAIL_NO_ERROR) {
    mailgmail_free(session);
    return NULL;
  }

  r = mailgmail_http_transport_new_default(&session->gmail_http_transport);
  if (r != MAILGMAIL_NO_ERROR)
    session->gmail_http_transport = NULL;

  return session;
}

void mailgmail_free(mailgmail * session)
{
  if (session == NULL)
    return;

  free(session->gmail_user_id);
  free(session->gmail_oauth_token);
  free(session->gmail_user_agent);
  free(session->gmail_last_error_message);
  mailgmail_http_transport_free(session->gmail_http_transport);
  free(session);
}

int mailgmail_set_user(mailgmail * session, const char * user_id)
{
  if (session == NULL)
    return MAILGMAIL_ERROR_BAD_STATE;

  if ((user_id == NULL) || (* user_id == '\0'))
    user_id = "me";

  return replace_string(&session->gmail_user_id, user_id);
}

int mailgmail_set_oauth2_token(mailgmail * session,
    const char * access_token)
{
  if (session == NULL)
    return MAILGMAIL_ERROR_BAD_STATE;

  return replace_string(&session->gmail_oauth_token, access_token);
}

int mailgmail_set_user_agent(mailgmail * session,
    const char * user_agent)
{
  if (session == NULL)
    return MAILGMAIL_ERROR_BAD_STATE;

  return replace_string(&session->gmail_user_agent, user_agent);
}

int mailgmail_set_timeout(mailgmail * session, time_t timeout)
{
  if (session == NULL)
    return MAILGMAIL_ERROR_BAD_STATE;

  session->gmail_timeout = timeout;
  return MAILGMAIL_NO_ERROR;
}

int mailgmail_get_last_http_status(mailgmail * session)
{
  if (session == NULL)
    return 0;

  return session->gmail_last_http_status;
}

const char * mailgmail_get_last_error_message(mailgmail * session)
{
  if (session == NULL)
    return NULL;

  return session->gmail_last_error_message;
}

static int append_auth_header_value(MMAPString * buffer,
    const char * access_token)
{
  if ((buffer == NULL) || (access_token == NULL))
    return MAILGMAIL_ERROR_BAD_STATE;

  if (mmap_string_append(buffer, "Bearer ") == NULL)
    return MAILGMAIL_ERROR_MEMORY;
  if (mmap_string_append(buffer, access_token) == NULL)
    return MAILGMAIL_ERROR_MEMORY;
  return MAILGMAIL_NO_ERROR;
}

static int add_common_headers(mailgmail * session,
    struct mailgmail_http_request * request)
{
  MMAPString * auth;
  int r;

  if ((session == NULL) || (request == NULL))
    return MAILGMAIL_ERROR_BAD_STATE;

  if ((session->gmail_oauth_token == NULL) ||
      (* session->gmail_oauth_token == '\0'))
    return MAILGMAIL_ERROR_UNAUTHORIZED;

  auth = mmap_string_sized_new(strlen(session->gmail_oauth_token) + 16);
  if (auth == NULL)
    return MAILGMAIL_ERROR_MEMORY;

  r = append_auth_header_value(auth, session->gmail_oauth_token);
  if (r != MAILGMAIL_NO_ERROR) {
    mmap_string_free(auth);
    return r;
  }

  r = mailgmail_http_request_add_header(request, "Authorization", auth->str);
  mmap_string_free(auth);
  if (r != MAILGMAIL_NO_ERROR)
    return r;

  r = mailgmail_http_request_add_header(request, "Accept", "application/json");
  if (r != MAILGMAIL_NO_ERROR)
    return r;

  if (session->gmail_user_agent != NULL) {
    r = mailgmail_http_request_add_header(request, "User-Agent",
        session->gmail_user_agent);
    if (r != MAILGMAIL_NO_ERROR)
      return r;
  }

  return MAILGMAIL_NO_ERROR;
}

static int error_for_http_status(int status_code)
{
  if ((status_code >= 200) && (status_code < 300))
    return MAILGMAIL_NO_ERROR;

  switch (status_code) {
  case 401:
    return MAILGMAIL_ERROR_UNAUTHORIZED;
  case 403:
    return MAILGMAIL_ERROR_FORBIDDEN;
  case 404:
    return MAILGMAIL_ERROR_NOT_FOUND;
  case 409:
    return MAILGMAIL_ERROR_CONFLICT;
  case 429:
    return MAILGMAIL_ERROR_RATE_LIMITED;
  default:
    if (status_code >= 500)
      return MAILGMAIL_ERROR_SERVER;
    return MAILGMAIL_ERROR_HTTP;
  }
}

static int perform_get(mailgmail * session, char * url,
    struct mailgmail_http_response ** result)
{
  struct mailgmail_http_request * request;
  struct mailgmail_http_response * response;
  char * error_message;
  int r;

  if ((session == NULL) || (url == NULL) || (result == NULL))
    return MAILGMAIL_ERROR_BAD_STATE;

  * result = NULL;
  clear_last_error(session);

  if (session->gmail_http_transport == NULL)
    return MAILGMAIL_ERROR_HTTP_UNAVAILABLE;

  request = mailgmail_http_request_new("GET", url);
  if (request == NULL)
    return MAILGMAIL_ERROR_MEMORY;
  request->timeout = session->gmail_timeout;

  r = add_common_headers(session, request);
  if (r != MAILGMAIL_NO_ERROR)
    goto cleanup_request;

  response = NULL;
  r = mailgmail_http_perform(session->gmail_http_transport, request,
      &response);
  if (r != MAILGMAIL_NO_ERROR)
    goto cleanup_request;

  session->gmail_last_http_status = response->status_code;
  r = error_for_http_status(response->status_code);
  if (r != MAILGMAIL_NO_ERROR) {
    error_message = NULL;
    if ((response->body != NULL) && (response->body_len > 0)) {
      if (mailgmail_parser_parse_error_message((const char *) response->body,
          response->body_len, &error_message) == MAILGMAIL_NO_ERROR)
        set_last_error_message(session, error_message);
      free(error_message);
    }
    mailgmail_http_response_free(response);
    goto cleanup_request;
  }

  * result = response;
  r = MAILGMAIL_NO_ERROR;

 cleanup_request:
  mailgmail_http_request_free(request);
  return r;
}

int mailgmail_get_profile(mailgmail * session,
    struct mailgmail_profile ** result)
{
  char * url;
  struct mailgmail_http_response * response;
  int r;

  if ((session == NULL) || (result == NULL))
    return MAILGMAIL_ERROR_BAD_STATE;

  * result = NULL;
  url = NULL;
  r = mailgmail_url_profile_get(session, &url);
  if (r != MAILGMAIL_NO_ERROR)
    return r;

  response = NULL;
  r = perform_get(session, url, &response);
  free(url);
  if (r != MAILGMAIL_NO_ERROR)
    return r;

  r = mailgmail_parser_parse_profile((const char *) response->body,
      response->body_len, result);
  mailgmail_http_response_free(response);
  return r;
}

int mailgmail_list_labels(mailgmail * session,
    struct mailgmail_label_list ** result)
{
  char * url;
  struct mailgmail_http_response * response;
  int r;

  if ((session == NULL) || (result == NULL))
    return MAILGMAIL_ERROR_BAD_STATE;

  * result = NULL;
  url = NULL;
  r = mailgmail_url_labels_list(session, &url);
  if (r != MAILGMAIL_NO_ERROR)
    return r;

  response = NULL;
  r = perform_get(session, url, &response);
  free(url);
  if (r != MAILGMAIL_NO_ERROR)
    return r;

  r = mailgmail_parser_parse_label_list((const char *) response->body,
      response->body_len, result);
  mailgmail_http_response_free(response);
  return r;
}

int mailgmail_get_label(mailgmail * session,
    const char * label_id,
    struct mailgmail_label ** result)
{
  char * url;
  struct mailgmail_http_response * response;
  int r;

  if ((session == NULL) || (label_id == NULL) || (result == NULL))
    return MAILGMAIL_ERROR_BAD_STATE;

  * result = NULL;
  url = NULL;
  r = mailgmail_url_label_get(session, label_id, &url);
  if (r != MAILGMAIL_NO_ERROR)
    return r;

  response = NULL;
  r = perform_get(session, url, &response);
  free(url);
  if (r != MAILGMAIL_NO_ERROR)
    return r;

  r = mailgmail_parser_parse_label((const char *) response->body,
      response->body_len, result);
  mailgmail_http_response_free(response);
  return r;
}

int mailgmail_list_messages(mailgmail * session,
    struct mailgmail_message_list_request * request,
    struct mailgmail_message_list ** result)
{
  char * url;
  struct mailgmail_http_response * response;
  int r;

  if ((session == NULL) || (result == NULL))
    return MAILGMAIL_ERROR_BAD_STATE;

  * result = NULL;
  url = NULL;
  r = mailgmail_url_messages_list(session, request, &url);
  if (r != MAILGMAIL_NO_ERROR)
    return r;

  response = NULL;
  r = perform_get(session, url, &response);
  free(url);
  if (r != MAILGMAIL_NO_ERROR)
    return r;

  r = mailgmail_parser_parse_message_list((const char *) response->body,
      response->body_len, result);
  mailgmail_http_response_free(response);
  return r;
}

int mailgmail_get_message(mailgmail * session,
    const char * message_id,
    struct mailgmail_message_get_request * request,
    struct mailgmail_message ** result)
{
  char * url;
  struct mailgmail_http_response * response;
  int r;

  if ((session == NULL) || (message_id == NULL) || (result == NULL))
    return MAILGMAIL_ERROR_BAD_STATE;

  * result = NULL;
  url = NULL;
  r = mailgmail_url_message_get(session, message_id, request, &url);
  if (r != MAILGMAIL_NO_ERROR)
    return r;

  response = NULL;
  r = perform_get(session, url, &response);
  free(url);
  if (r != MAILGMAIL_NO_ERROR)
    return r;

  r = mailgmail_parser_parse_message((const char *) response->body,
      response->body_len, result);
  mailgmail_http_response_free(response);
  return r;
}

int mailgmail_get_attachment(mailgmail * session,
    const char * message_id,
    const char * attachment_id,
    struct mailgmail_attachment ** result)
{
  char * url;
  struct mailgmail_http_response * response;
  int r;

  if ((session == NULL) || (message_id == NULL) ||
      (attachment_id == NULL) || (result == NULL))
    return MAILGMAIL_ERROR_BAD_STATE;

  * result = NULL;
  url = NULL;
  r = mailgmail_url_attachment_get(session, message_id, attachment_id, &url);
  if (r != MAILGMAIL_NO_ERROR)
    return r;

  response = NULL;
  r = perform_get(session, url, &response);
  free(url);
  if (r != MAILGMAIL_NO_ERROR)
    return r;

  r = mailgmail_parser_parse_attachment((const char *) response->body,
      response->body_len, result);
  mailgmail_http_response_free(response);
  return r;
}
