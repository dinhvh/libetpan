/*
 * libEtPan! -- a mail stuff library
 */

#ifndef MAILJMAP_PRIVATE_H

#define MAILJMAP_PRIVATE_H

#include "mailjmap.h"

struct mailjmap {
  int jmap_cached;
  char * jmap_cache_directory;
  char * jmap_session_url;
  char * jmap_api_url;
  char * jmap_upload_url;
  char * jmap_download_url;
  char * jmap_user;
  char * jmap_oauth_token;
  time_t jmap_timeout;
  int jmap_last_http_status;
  char * jmap_last_error_message;
  char * jmap_last_problem_type;
  char * jmap_last_method_name;
  char * jmap_last_method_call_id;
  char * jmap_last_method_error_type;
  char * jmap_last_method_error_description;
  struct mailjmap_http_transport * jmap_http_transport;
};

void mailjmap_private_clear_last_error(mailjmap * session);
int mailjmap_private_set_last_error_message_if_empty(mailjmap * session,
    const char * message);
int mailjmap_private_remember_http_problem_diagnostics(mailjmap * session,
    struct mailjmap_http_response * http_response);
int mailjmap_private_add_auth_header(mailjmap * session,
    struct mailjmap_http_request * request);
int mailjmap_private_normalize_parse_error(int r);

#endif
