/*
 * libEtPan! -- a mail stuff library
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailjmap.h"
#include "mailjmap_json.h"
#include "mailjmap_private.h"
#include "mailjmap_request.h"
#include "mailjmap_response.h"

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
    return MAILJMAP_ERROR_BAD_STATE;

  copy = NULL;
  if (value != NULL) {
    copy = dup_string(value);
    if (copy == NULL)
      return MAILJMAP_ERROR_MEMORY;
  }

  free(* target);
  * target = copy;
  return MAILJMAP_NO_ERROR;
}

void mailjmap_private_clear_last_error(mailjmap * session)
{
  if (session == NULL)
    return;

  session->jmap_last_http_status = 0;
  free(session->jmap_last_error_message);
  session->jmap_last_error_message = NULL;
  free(session->jmap_last_problem_type);
  session->jmap_last_problem_type = NULL;
  free(session->jmap_last_method_name);
  session->jmap_last_method_name = NULL;
  free(session->jmap_last_method_call_id);
  session->jmap_last_method_call_id = NULL;
  free(session->jmap_last_method_error_type);
  session->jmap_last_method_error_type = NULL;
  free(session->jmap_last_method_error_description);
  session->jmap_last_method_error_description = NULL;
}

static int set_last_error_message(mailjmap * session, const char * message)
{
  if (session == NULL)
    return MAILJMAP_ERROR_BAD_STATE;

  free(session->jmap_last_error_message);
  session->jmap_last_error_message = NULL;

  if (message != NULL) {
    session->jmap_last_error_message = dup_string(message);
    if (session->jmap_last_error_message == NULL)
      return MAILJMAP_ERROR_MEMORY;
  }

  return MAILJMAP_NO_ERROR;
}

int mailjmap_private_set_last_error_message_if_empty(mailjmap * session,
    const char * message)
{
  if (session == NULL)
    return MAILJMAP_ERROR_BAD_STATE;
  if (session->jmap_last_error_message != NULL)
    return MAILJMAP_NO_ERROR;

  return set_last_error_message(session, message);
}

static int remember_last_problem(mailjmap * session,
    struct mailjmap_response * response)
{
  int r;

  if ((session == NULL) || (response == NULL) ||
      (response->error_type == NULL))
    return MAILJMAP_NO_ERROR;

  r = replace_string(&session->jmap_last_problem_type,
      response->error_type);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  return set_last_error_message(session, response->error_detail);
}

static int jmap_problem_type_has_suffix(const char * problem_type,
    const char * suffix)
{
  size_t problem_len;
  size_t suffix_len;

  if ((problem_type == NULL) || (suffix == NULL))
    return 0;

  problem_len = strlen(problem_type);
  suffix_len = strlen(suffix);
  if (problem_len < suffix_len)
    return 0;

  return strcmp(problem_type + problem_len - suffix_len, suffix) == 0;
}

static int jmap_problem_type_error_code(const char * problem_type,
    int default_error)
{
  if (jmap_problem_type_has_suffix(problem_type, "unknownCapability"))
    return MAILJMAP_ERROR_CAPABILITY;
  if (jmap_problem_type_has_suffix(problem_type, "limit"))
    return MAILJMAP_ERROR_LIMIT;
  if (jmap_problem_type_has_suffix(problem_type, "notJSON") ||
      jmap_problem_type_has_suffix(problem_type, "notRequest"))
    return MAILJMAP_ERROR_PROTOCOL;

  return default_error;
}

static int remember_last_method_error(mailjmap * session,
    struct mailjmap_request * request,
    struct mailjmap_method_response * method_response)
{
  const char * method_name;
  char * type;
  char * description;
  int r;

  if ((session == NULL) || (method_response == NULL) ||
      (strcmp(method_response->name, "error") != 0))
    return MAILJMAP_NO_ERROR;

  type = NULL;
  description = NULL;
  method_name = mailjmap_request_method_name_for_call_id(request,
      method_response->call_id);

  r = mailjmap_json_object_get_string_dup(method_response->arguments, "type",
      &type);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = mailjmap_json_object_get_string_dup(method_response->arguments,
      "description", &description);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;

  r = replace_string(&session->jmap_last_method_name,
      method_name != NULL ? method_name : method_response->name);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = replace_string(&session->jmap_last_method_call_id,
      method_response->call_id);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = replace_string(&session->jmap_last_method_error_type, type);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = replace_string(&session->jmap_last_method_error_description,
      description);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = set_last_error_message(session,
      description != NULL ? description : type);

 cleanup:
  free(description);
  free(type);
  return r;
}

static int remember_response_diagnostics(mailjmap * session,
    struct mailjmap_request * request,
    struct mailjmap_response * response)
{
  clistiter * cur;
  int r;

  r = remember_last_problem(session, response);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  if ((response == NULL) || (response->method_responses == NULL))
    return MAILJMAP_NO_ERROR;

  for (cur = clist_begin(response->method_responses); cur != NULL;
       cur = clist_next(cur)) {
    struct mailjmap_method_response * method_response;

    method_response = clist_content(cur);
    if ((method_response != NULL) &&
        (strcmp(method_response->name, "error") == 0))
      return remember_last_method_error(session, request, method_response);
  }

  return MAILJMAP_NO_ERROR;
}

int mailjmap_private_remember_http_problem_diagnostics(mailjmap * session,
    struct mailjmap_http_response * http_response)
{
  struct mailjmap_response * jmap_response;
  int r;

  if ((session == NULL) || (http_response == NULL) ||
      (http_response->body == NULL) || (http_response->body_len == 0))
    return MAILJMAP_NO_ERROR;

  jmap_response = NULL;
  r = mailjmap_response_parse((const char *) http_response->body,
      http_response->body_len, &jmap_response);
  if (r == MAILJMAP_NO_ERROR)
    r = remember_last_problem(session, jmap_response);
  else
    r = MAILJMAP_NO_ERROR;

  mailjmap_response_free(jmap_response);
  return r;
}

mailjmap * mailjmap_new(int cached, const char * cache_directory)
{
  mailjmap * session;
  int r;

  session = malloc(sizeof(* session));
  if (session == NULL)
    return NULL;

  session->jmap_cached = cached;
  session->jmap_cache_directory = NULL;
  session->jmap_session_url = NULL;
  session->jmap_api_url = NULL;
  session->jmap_upload_url = NULL;
  session->jmap_download_url = NULL;
  session->jmap_user = NULL;
  session->jmap_oauth_token = NULL;
  session->jmap_timeout = 60;
  session->jmap_last_http_status = 0;
  session->jmap_last_error_message = NULL;
  session->jmap_last_problem_type = NULL;
  session->jmap_last_method_name = NULL;
  session->jmap_last_method_call_id = NULL;
  session->jmap_last_method_error_type = NULL;
  session->jmap_last_method_error_description = NULL;
  session->jmap_http_transport = NULL;

  r = replace_string(&session->jmap_cache_directory, cache_directory);
  if (r != MAILJMAP_NO_ERROR) {
    mailjmap_free(session);
    return NULL;
  }

  r = mailjmap_http_transport_new_default(&session->jmap_http_transport);
  if (r != MAILJMAP_NO_ERROR)
    session->jmap_http_transport = NULL;

  return session;
}

void mailjmap_free(mailjmap * session)
{
  if (session == NULL)
    return;

  free(session->jmap_cache_directory);
  free(session->jmap_session_url);
  free(session->jmap_api_url);
  free(session->jmap_upload_url);
  free(session->jmap_download_url);
  free(session->jmap_user);
  free(session->jmap_oauth_token);
  free(session->jmap_last_error_message);
  free(session->jmap_last_problem_type);
  free(session->jmap_last_method_name);
  free(session->jmap_last_method_call_id);
  free(session->jmap_last_method_error_type);
  free(session->jmap_last_method_error_description);
  mailjmap_http_transport_free(session->jmap_http_transport);
  free(session);
}

int mailjmap_set_http_transport(mailjmap * session,
    struct mailjmap_http_transport * transport)
{
  if (session == NULL)
    return MAILJMAP_ERROR_BAD_STATE;

  mailjmap_http_transport_free(session->jmap_http_transport);
  session->jmap_http_transport = transport;
  return MAILJMAP_NO_ERROR;
}

int mailjmap_connect(mailjmap * session, const char * session_url)
{
  if ((session == NULL) || (session_url == NULL) || (* session_url == '\0'))
    return MAILJMAP_ERROR_BAD_STATE;

  return replace_string(&session->jmap_session_url, session_url);
}

int mailjmap_login_oauth2(mailjmap * session,
    const char * user, const char * access_token)
{
  int r;

  if (session == NULL)
    return MAILJMAP_ERROR_BAD_STATE;

  r = replace_string(&session->jmap_user, user);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  return mailjmap_set_oauth2_token(session, access_token);
}

int mailjmap_set_oauth2_token(mailjmap * session,
    const char * access_token)
{
  if (session == NULL)
    return MAILJMAP_ERROR_BAD_STATE;

  return replace_string(&session->jmap_oauth_token, access_token);
}

int mailjmap_set_timeout(mailjmap * session, time_t timeout)
{
  if (session == NULL)
    return MAILJMAP_ERROR_BAD_STATE;

  session->jmap_timeout = timeout;
  return MAILJMAP_NO_ERROR;
}

int mailjmap_get_last_http_status(mailjmap * session)
{
  if (session == NULL)
    return 0;

  return session->jmap_last_http_status;
}

const char * mailjmap_get_last_error_message(mailjmap * session)
{
  if (session == NULL)
    return NULL;

  return session->jmap_last_error_message;
}

const char * mailjmap_get_last_problem_type(mailjmap * session)
{
  if (session == NULL)
    return NULL;

  return session->jmap_last_problem_type;
}

const char * mailjmap_get_last_method_name(mailjmap * session)
{
  if (session == NULL)
    return NULL;

  return session->jmap_last_method_name;
}

const char * mailjmap_get_last_method_call_id(mailjmap * session)
{
  if (session == NULL)
    return NULL;

  return session->jmap_last_method_call_id;
}

const char * mailjmap_get_last_method_error_type(mailjmap * session)
{
  if (session == NULL)
    return NULL;

  return session->jmap_last_method_error_type;
}

const char * mailjmap_get_last_method_error_description(mailjmap * session)
{
  if (session == NULL)
    return NULL;

  return session->jmap_last_method_error_description;
}

static int append_auth_header_value(char ** result, const char * access_token)
{
  char * value;
  size_t len;

  if ((result == NULL) || (access_token == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  len = strlen("Bearer ") + strlen(access_token) + 1;
  value = malloc(len);
  if (value == NULL)
    return MAILJMAP_ERROR_MEMORY;

  snprintf(value, len, "Bearer %s", access_token);
  * result = value;
  return MAILJMAP_NO_ERROR;
}

int mailjmap_private_add_auth_header(mailjmap * session,
    struct mailjmap_http_request * request)
{
  char * auth;
  int r;

  if ((session == NULL) || (request == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  if ((session->jmap_oauth_token == NULL) ||
      (* session->jmap_oauth_token == '\0'))
    return MAILJMAP_ERROR_AUTHENTICATION;

  auth = NULL;
  r = append_auth_header_value(&auth, session->jmap_oauth_token);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  r = mailjmap_http_request_add_header(request, "Authorization", auth);
  free(auth);
  return r;
}

static int add_auth_header_if_available(mailjmap * session,
    struct mailjmap_http_request * request)
{
  if ((session == NULL) || (request == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  if ((session->jmap_oauth_token == NULL) ||
      (* session->jmap_oauth_token == '\0'))
    return MAILJMAP_NO_ERROR;

  return mailjmap_private_add_auth_header(session, request);
}

static int domain_from_input(const char * domain_or_email,
    const char ** domain, size_t * domain_len)
{
  const char * at;
  const char * value;
  size_t len;

  if ((domain_or_email == NULL) || (domain == NULL) || (domain_len == NULL) ||
      (* domain_or_email == '\0'))
    return MAILJMAP_ERROR_BAD_STATE;

  at = strrchr(domain_or_email, '@');
  value = (at == NULL) ? domain_or_email : at + 1;
  len = strlen(value);
  if (len == 0)
    return MAILJMAP_ERROR_BAD_STATE;
  if (strchr(value, '/') != NULL)
    return MAILJMAP_ERROR_BAD_STATE;

  * domain = value;
  * domain_len = len;
  return MAILJMAP_NO_ERROR;
}

static int build_discovery_url(const char * domain_or_email, char ** result)
{
  const char * domain;
  size_t domain_len;
  size_t url_len;
  char * url;
  int r;

  if (result == NULL)
    return MAILJMAP_ERROR_BAD_STATE;

  * result = NULL;
  domain = NULL;
  domain_len = 0;
  r = domain_from_input(domain_or_email, &domain, &domain_len);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  url_len = strlen("https://") + domain_len + strlen("/.well-known/jmap") + 1;
  url = malloc(url_len);
  if (url == NULL)
    return MAILJMAP_ERROR_MEMORY;

  snprintf(url, url_len, "https://%.*s/.well-known/jmap",
      (int) domain_len, domain);
  * result = url;
  return MAILJMAP_NO_ERROR;
}

static int string_list_append(clist * list, const char * value)
{
  char * copy;

  if ((list == NULL) || (value == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  copy = dup_string(value);
  if (copy == NULL)
    return MAILJMAP_ERROR_MEMORY;

  if (clist_append(list, copy) < 0) {
    free(copy);
    return MAILJMAP_ERROR_MEMORY;
  }

  return MAILJMAP_NO_ERROR;
}

struct parse_capability_context {
  clist * names;
  clist * details;
};

static int append_capability_detail(const char * key,
    mailjmap_json_value * value, void * context)
{
  struct parse_capability_context * capability_context;
  struct mailjmap_session_capability * detail;
  char * json;
  size_t json_len;
  int r;

  if ((key == NULL) || (value == NULL) || (context == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  capability_context = context;
  r = string_list_append(capability_context->names, key);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  json = NULL;
  r = mailjmap_json_serialize(value,
      MAILJMAP_JSON_SERIALIZE_COMPACT | MAILJMAP_JSON_SERIALIZE_SORT_KEYS,
      &json, &json_len);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  detail = mailjmap_session_capability_new(key, json);
  free(json);
  if (detail == NULL)
    return MAILJMAP_ERROR_MEMORY;

  if (clist_append(capability_context->details, detail) < 0) {
    mailjmap_session_capability_free(detail);
    return MAILJMAP_ERROR_MEMORY;
  }

  return MAILJMAP_NO_ERROR;
}

static int parse_session_capabilities(mailjmap_json_value * root,
    struct mailjmap_session * parsed)
{
  struct parse_capability_context context;
  mailjmap_json_value * capabilities;
  int r;

  capabilities = NULL;
  r = mailjmap_json_object_get(root, "capabilities", &capabilities);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  if (capabilities == NULL)
    return MAILJMAP_ERROR_PROTOCOL;
  if (!mailjmap_json_is_object(capabilities)) {
    mailjmap_json_free(capabilities);
    return MAILJMAP_ERROR_PROTOCOL;
  }

  context.names = parsed->capabilities;
  context.details = parsed->capability_details;
  r = mailjmap_json_object_foreach(capabilities, append_capability_detail,
      &context);
  mailjmap_json_free(capabilities);
  return r;
}

static int parse_account_capabilities(mailjmap_json_value * account_value,
    struct mailjmap_session_account * account)
{
  struct parse_capability_context context;
  mailjmap_json_value * capabilities;
  int r;

  capabilities = NULL;
  r = mailjmap_json_object_get(account_value, "accountCapabilities",
      &capabilities);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  if (capabilities == NULL)
    return MAILJMAP_NO_ERROR;
  if (!mailjmap_json_is_object(capabilities)) {
    mailjmap_json_free(capabilities);
    return MAILJMAP_ERROR_PROTOCOL;
  }

  context.names = account->capabilities;
  context.details = account->capability_details;
  r = mailjmap_json_object_foreach(capabilities, append_capability_detail,
      &context);
  mailjmap_json_free(capabilities);
  return r;
}

static int parse_account_entry(const char * key, mailjmap_json_value * value,
    void * context)
{
  struct mailjmap_session * parsed;
  struct mailjmap_session_account * account;
  int r;

  parsed = context;
  if (!mailjmap_json_is_object(value))
    return MAILJMAP_ERROR_PROTOCOL;

  account = mailjmap_session_account_new(key);
  if (account == NULL)
    return MAILJMAP_ERROR_MEMORY;

  r = mailjmap_json_object_get_string_dup(value, "name", &account->name);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = mailjmap_json_object_get_boolean(value, "isPersonal",
      &account->is_personal);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = mailjmap_json_object_get_boolean(value, "isReadOnly",
      &account->is_read_only);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = parse_account_capabilities(value, account);
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  if (clist_append(parsed->accounts, account) < 0) {
    r = MAILJMAP_ERROR_MEMORY;
    goto err;
  }

  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_session_account_free(account);
  return r;
}

static int parse_session_accounts(mailjmap_json_value * root,
    struct mailjmap_session * parsed)
{
  mailjmap_json_value * accounts;
  int r;

  accounts = NULL;
  r = mailjmap_json_object_get(root, "accounts", &accounts);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  if (accounts == NULL)
    return MAILJMAP_ERROR_PROTOCOL;
  if (!mailjmap_json_is_object(accounts)) {
    mailjmap_json_free(accounts);
    return MAILJMAP_ERROR_PROTOCOL;
  }

  r = mailjmap_json_object_foreach(accounts, parse_account_entry, parsed);
  mailjmap_json_free(accounts);
  return r;
}

static int parse_primary_account_entry(const char * key,
    mailjmap_json_value * value, void * context)
{
  struct mailjmap_session * parsed;
  struct mailjmap_session_primary_account * primary_account;
  char * account_id;
  int r;

  parsed = context;
  if (mailjmap_json_is_null(value))
    return MAILJMAP_NO_ERROR;
  if (!mailjmap_json_is_string(value))
    return MAILJMAP_ERROR_PROTOCOL;

  account_id = NULL;
  r = mailjmap_json_string_dup(value, &account_id);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  primary_account = mailjmap_session_primary_account_new(key, account_id);
  free(account_id);
  if (primary_account == NULL)
    return MAILJMAP_ERROR_MEMORY;

  if (clist_append(parsed->primary_accounts, primary_account) < 0) {
    mailjmap_session_primary_account_free(primary_account);
    return MAILJMAP_ERROR_MEMORY;
  }

  return MAILJMAP_NO_ERROR;
}

static int parse_session_primary_accounts(mailjmap_json_value * root,
    struct mailjmap_session * parsed)
{
  mailjmap_json_value * primary_accounts;
  int r;

  primary_accounts = NULL;
  r = mailjmap_json_object_get(root, "primaryAccounts", &primary_accounts);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  if (primary_accounts == NULL)
    return MAILJMAP_ERROR_PROTOCOL;
  if (!mailjmap_json_is_object(primary_accounts)) {
    mailjmap_json_free(primary_accounts);
    return MAILJMAP_ERROR_PROTOCOL;
  }

  r = mailjmap_json_object_foreach(primary_accounts,
      parse_primary_account_entry, parsed);
  mailjmap_json_free(primary_accounts);
  return r;
}

int mailjmap_private_normalize_parse_error(int r)
{
  if (r == MAILJMAP_ERROR_BAD_STATE)
    return MAILJMAP_ERROR_PROTOCOL;

  return r;
}

static int parse_session_object(const char * data, size_t data_len,
    struct mailjmap_session ** result)
{
  mailjmap_json_value * root;
  struct mailjmap_session * parsed;
  int r;

  if ((data == NULL) || (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  root = NULL;
  parsed = NULL;

  r = mailjmap_json_parse(data, data_len, &root);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  if (!mailjmap_json_is_object(root)) {
    r = MAILJMAP_ERROR_PROTOCOL;
    goto err;
  }

  parsed = mailjmap_session_new();
  if (parsed == NULL) {
    r = MAILJMAP_ERROR_MEMORY;
    goto err;
  }

  r = mailjmap_json_object_get_string_dup(root, "apiUrl",
      &parsed->api_url);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = mailjmap_json_object_get_string_dup(root, "uploadUrl",
      &parsed->upload_url);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = mailjmap_json_object_get_string_dup(root, "downloadUrl",
      &parsed->download_url);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = mailjmap_json_object_get_string_dup(root, "eventSourceUrl",
      &parsed->event_source_url);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = mailjmap_json_object_get_string_dup(root, "sessionState",
      &parsed->session_state);
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  if ((parsed->api_url == NULL) || (parsed->upload_url == NULL) ||
      (parsed->download_url == NULL) || (parsed->session_state == NULL)) {
    r = MAILJMAP_ERROR_PROTOCOL;
    goto err;
  }

  r = parse_session_capabilities(root, parsed);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = parse_session_accounts(root, parsed);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = parse_session_primary_accounts(root, parsed);
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  mailjmap_json_free(root);
  * result = parsed;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_json_free(root);
  mailjmap_session_free(parsed);
  return mailjmap_private_normalize_parse_error(r);
}

static int remember_session_urls(mailjmap * session,
    struct mailjmap_session * parsed)
{
  int r;

  if ((session == NULL) || (parsed == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  r = replace_string(&session->jmap_api_url, parsed->api_url);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  r = replace_string(&session->jmap_upload_url, parsed->upload_url);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  return replace_string(&session->jmap_download_url, parsed->download_url);
}

int mailjmap_get_session(mailjmap * session,
    struct mailjmap_session ** result)
{
  struct mailjmap_http_request * request;
  struct mailjmap_http_response * response;
  int r;

  if ((session == NULL) || (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  * result = NULL;
  mailjmap_private_clear_last_error(session);

  if ((session->jmap_session_url == NULL) ||
      (* session->jmap_session_url == '\0'))
    return MAILJMAP_ERROR_BAD_STATE;
  if (session->jmap_http_transport == NULL)
    return MAILJMAP_ERROR_HTTP;

  request = mailjmap_http_request_new("GET", session->jmap_session_url);
  if (request == NULL)
    return MAILJMAP_ERROR_MEMORY;
  response = NULL;
  request->timeout = session->jmap_timeout;

  r = mailjmap_http_request_set_accept_type(request, "application/json");
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = mailjmap_private_add_auth_header(session, request);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;

  r = mailjmap_http_perform(session->jmap_http_transport, request, &response);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;

  session->jmap_last_http_status = response->status_code;
  if (response->status_code < 200 || response->status_code >= 300) {
    r = mailjmap_private_remember_http_problem_diagnostics(session, response);
    if (r != MAILJMAP_NO_ERROR)
      goto cleanup;
  }
  if (response->status_code == 401 || response->status_code == 403) {
    mailjmap_private_set_last_error_message_if_empty(session,
        "JMAP authentication failed");
    r = MAILJMAP_ERROR_AUTHENTICATION;
    goto cleanup;
  }
  if (response->status_code < 200 || response->status_code >= 300) {
    mailjmap_private_set_last_error_message_if_empty(session,
        "JMAP Session request failed");
    r = MAILJMAP_ERROR_HTTP;
    goto cleanup;
  }

  r = parse_session_object((const char *) response->body, response->body_len,
      result);
  if (r == MAILJMAP_NO_ERROR)
    r = remember_session_urls(session, * result);

 cleanup:
  mailjmap_http_response_free(response);
  mailjmap_http_request_free(request);
  return r;
}

int mailjmap_discover(mailjmap * session,
    const char * domain_or_email,
    struct mailjmap_session ** result)
{
  struct mailjmap_http_request * request;
  struct mailjmap_http_response * response;
  char * discovery_url;
  const char * session_url;
  int r;

  if ((session == NULL) || (domain_or_email == NULL) || (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  * result = NULL;
  mailjmap_private_clear_last_error(session);

  if (session->jmap_http_transport == NULL)
    return MAILJMAP_ERROR_HTTP;

  request = NULL;
  response = NULL;
  discovery_url = NULL;

  r = build_discovery_url(domain_or_email, &discovery_url);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;

  request = mailjmap_http_request_new("GET", discovery_url);
  if (request == NULL) {
    r = MAILJMAP_ERROR_MEMORY;
    goto cleanup;
  }
  request->timeout = session->jmap_timeout;

  r = mailjmap_http_request_set_accept_type(request, "application/json");
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = add_auth_header_if_available(session, request);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;

  r = mailjmap_http_perform(session->jmap_http_transport, request, &response);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;

  session->jmap_last_http_status = response->status_code;
  if (response->status_code < 200 || response->status_code >= 300) {
    r = mailjmap_private_remember_http_problem_diagnostics(session, response);
    if (r != MAILJMAP_NO_ERROR)
      goto cleanup;
  }
  if (response->status_code == 401 || response->status_code == 403) {
    mailjmap_private_set_last_error_message_if_empty(session,
        "JMAP authentication failed");
    r = MAILJMAP_ERROR_AUTHENTICATION;
    goto cleanup;
  }
  if (response->status_code == 404) {
    mailjmap_private_set_last_error_message_if_empty(session,
        "JMAP discovery endpoint not found");
    r = MAILJMAP_ERROR_DISCOVERY;
    goto cleanup;
  }
  if (response->status_code < 200 || response->status_code >= 300) {
    mailjmap_private_set_last_error_message_if_empty(session,
        "JMAP discovery request failed");
    r = MAILJMAP_ERROR_DISCOVERY;
    goto cleanup;
  }

  r = parse_session_object((const char *) response->body, response->body_len,
      result);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;

  r = remember_session_urls(session, * result);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;

  session_url = (response->final_url != NULL) ? response->final_url :
      discovery_url;
  r = replace_string(&session->jmap_session_url, session_url);

 cleanup:
  if ((r != MAILJMAP_NO_ERROR) && (result != NULL)) {
    mailjmap_session_free(* result);
    * result = NULL;
  }
  free(discovery_url);
  mailjmap_http_response_free(response);
  mailjmap_http_request_free(request);
  return r;
}

int mailjmap_call(mailjmap * session,
    struct mailjmap_request * jmap_request,
    struct mailjmap_response ** result)
{
  struct mailjmap_http_request * request;
  struct mailjmap_http_response * response;
  mailjmap_json_value * envelope;
  char * body;
  size_t body_len;
  int r;

  if ((session == NULL) || (jmap_request == NULL) || (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  * result = NULL;
  mailjmap_private_clear_last_error(session);

  if ((session->jmap_api_url == NULL) || (* session->jmap_api_url == '\0'))
    return MAILJMAP_ERROR_BAD_STATE;
  if (session->jmap_http_transport == NULL)
    return MAILJMAP_ERROR_HTTP;

  request = NULL;
  response = NULL;
  envelope = NULL;
  body = NULL;
  body_len = 0;

  r = mailjmap_request_serialize(jmap_request, &envelope);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;

  r = mailjmap_json_serialize(envelope, MAILJMAP_JSON_SERIALIZE_COMPACT,
      &body, &body_len);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;

  request = mailjmap_http_request_new("POST", session->jmap_api_url);
  if (request == NULL) {
    r = MAILJMAP_ERROR_MEMORY;
    goto cleanup;
  }
  request->timeout = session->jmap_timeout;

  r = mailjmap_http_request_set_content_type(request, "application/json");
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = mailjmap_http_request_set_accept_type(request, "application/json");
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = mailjmap_http_request_set_body(request, (const unsigned char *) body,
      body_len);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = mailjmap_private_add_auth_header(session, request);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;

  r = mailjmap_http_perform(session->jmap_http_transport, request, &response);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;

  session->jmap_last_http_status = response->status_code;
  if (response->status_code < 200 || response->status_code >= 300) {
    r = mailjmap_private_remember_http_problem_diagnostics(session, response);
    if (r != MAILJMAP_NO_ERROR)
      goto cleanup;
  }
  if (response->status_code == 401 || response->status_code == 403) {
    mailjmap_private_set_last_error_message_if_empty(session,
        "JMAP authentication failed");
    r = MAILJMAP_ERROR_AUTHENTICATION;
    goto cleanup;
  }
  if (response->status_code == 429) {
    mailjmap_private_set_last_error_message_if_empty(session,
        "JMAP request rate limited");
    r = MAILJMAP_ERROR_LIMIT;
    goto cleanup;
  }
  if (response->status_code < 200 || response->status_code >= 300) {
    mailjmap_private_set_last_error_message_if_empty(session,
        "JMAP API request failed");
    r = jmap_problem_type_error_code(session->jmap_last_problem_type,
        MAILJMAP_ERROR_HTTP);
    goto cleanup;
  }

  r = mailjmap_response_parse((const char *) response->body,
      response->body_len, result);
  if (r == MAILJMAP_NO_ERROR)
    r = remember_response_diagnostics(session, jmap_request, * result);

 cleanup:
  free(body);
  mailjmap_json_free(envelope);
  mailjmap_http_response_free(response);
  mailjmap_http_request_free(request);
  return r;
}
