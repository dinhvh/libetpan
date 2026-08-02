/*
 * libEtPan! -- a mail stuff library
 */

#ifndef MAILGMAIL_PRIVATE_H

#define MAILGMAIL_PRIVATE_H

#include "mailgmail_types.h"

struct mailgmail_http_transport;

struct mailgmail {
  char * gmail_user_id;
  char * gmail_oauth_token;
  char * gmail_user_agent;
  time_t gmail_timeout;
  int gmail_last_http_status;
  char * gmail_last_error_message;
  struct mailgmail_http_transport * gmail_http_transport;
};

#endif
