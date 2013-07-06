#include "mailsmtp_oauth2.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "base64.h"
#include "mailsmtp_private.h"

#define SMTP_STRING_SIZE 513

LIBETPAN_EXPORT
int mailsmtp_oauth2_authenticate(mailsmtp * session, const char * auth_user,
    const char * access_token)
{
  int r;
  char command[SMTP_STRING_SIZE];
  char * ptr;
  char * full_auth_string;
  char * full_auth_string_b64;
  int auth_user_len;
  int access_token_len;
  int full_auth_string_len;
  int res;
  
  full_auth_string = NULL;
  full_auth_string_b64 = NULL;
  
  /* Build client response string */
  auth_user_len = strlen(auth_user);
  access_token_len = strlen(access_token);
  full_auth_string_len = 5 + auth_user_len + 1 + 12 + access_token_len + 2;
  full_auth_string = malloc(full_auth_string_len + 1);
  if (full_auth_string == NULL) {
    res = MAILSMTP_ERROR_MEMORY;
    goto free;
  }
  
  ptr = memcpy(full_auth_string, "user=", 5);
  ptr = memcpy(ptr + 5, auth_user, auth_user_len);
  ptr = memcpy(ptr + auth_user_len, "\1auth=Bearer ", 13);
  ptr = memcpy(ptr + 13, access_token, access_token_len);
  ptr = memcpy(ptr + access_token_len, "\1\1\0", 3);
  
  /* Convert to base64 */
  full_auth_string_b64 = encode_base64(full_auth_string, full_auth_string_len);
  if (full_auth_string_b64 == NULL) {
    res = MAILSMTP_ERROR_MEMORY;
    goto free;
  }
  
  snprintf(command, SMTP_STRING_SIZE, "AUTH XOAUTH2 %s\r\n", full_auth_string_b64);
  r = mailsmtp_send_command(session, command);
  if (r == -1) {
    res = MAILSMTP_ERROR_STREAM;
    goto free;
  }
  
  r = mailsmtp_read_response(session);
  switch (r) {
  case 220:
  case 235:
    res = MAILSMTP_NO_ERROR;
    goto free;

  case 334:
    /* AUTH in progress */
    
    /* There's probably an error, send an empty line as acknowledgement. */
    snprintf(command, SMTP_STRING_SIZE, "\r\n");
    r = mailsmtp_send_command(session, command);
    if (r == -1) {
      res = MAILSMTP_ERROR_STREAM;
      goto free;
    }
    
    r = mailsmtp_read_response(session);
    switch (r) {
      case 535:
        res = MAILSMTP_ERROR_AUTH_LOGIN;
        goto free;
      
      default:
        res = MAILSMTP_ERROR_UNEXPECTED_CODE;
        goto free;
    }
    break;

  default:
    res = MAILSMTP_ERROR_UNEXPECTED_CODE;
    goto free;
  }
  
  free:
   free(full_auth_string);
   free(full_auth_string_b64);
   return res;
}
