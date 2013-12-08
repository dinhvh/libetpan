#include "xgmraw.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "mailimap_sender.h"
#include "mailimap.h"
#include "condstore_types.h"
#include "mailimap_keywords.h"
#include "mailimap_parser.h"
#include "qresync.h"
#include "qresync_private.h"

/*
 =>   date            = YYYY/­MM/DD
 */

static int xgmraw_date_send(mailstream * fd, struct mailimap_date * date)
{
  int r;
  
  r = mailimap_number_send(fd, date->dt_year);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  r = mailimap_char_send(fd, '/');
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  r = mailimap_number_send(fd, date->dt_month);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  r = mailimap_char_send(fd, '/');
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  r = mailimap_number_send(fd, date->dt_day);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  return MAILIMAP_NO_ERROR;
}

int xgmraw_search_key_send(mailstream * fd, struct xgmraw_search_key * key)
{
  int r;
  
  switch (key->sk_type) {
    case XGMRAW_SEARCH_KEY_ALL:
      return mailimap_token_send(fd, "in:all");
      
    case XGMRAW_SEARCH_KEY_ANYWHERE:
      return mailimap_token_send(fd, "in:anywhere");
      
    case XGMRAW_SEARCH_KEY_BCC:
      r = mailimap_token_send(fd, "bcc");
      if (r != MAILIMAP_NO_ERROR)
        return r;
      r = mailimap_char_send(fd, ':');
      if (r != MAILIMAP_NO_ERROR)
        return r;
      r = mailimap_astring_send(fd, key->sk_data.sk_bcc);
      if (r != MAILIMAP_NO_ERROR)
        return r;
      return MAILIMAP_NO_ERROR;
      
    case XGMRAW_SEARCH_KEY_BEFORE:
      r = mailimap_token_send(fd, "before");
      if (r != MAILIMAP_NO_ERROR)
        return r;
      r = mailimap_char_send(fd, ':');
      if (r != MAILIMAP_NO_ERROR)
        return r;
      r = xgmraw_date_send(fd, key->sk_data.sk_before);
      if (r != MAILIMAP_NO_ERROR)
        return r;
      return MAILIMAP_NO_ERROR;
      
    case XGMRAW_SEARCH_KEY_CC:
      r = mailimap_token_send(fd, "cc");
      if (r != MAILIMAP_NO_ERROR)
        return r;
      r = mailimap_char_send(fd, ':');
      if (r != MAILIMAP_NO_ERROR)
        return r;
      r = mailimap_astring_send(fd, key->sk_data.sk_cc);
      if (r != MAILIMAP_NO_ERROR)
        return r;
      return MAILIMAP_NO_ERROR;
      
    case XGMRAW_SEARCH_KEY_FILENAME:
      r = mailimap_token_send(fd, "filename");
      if (r != MAILIMAP_NO_ERROR)
        return r;
      r = mailimap_char_send(fd, ':');
      if (r != MAILIMAP_NO_ERROR)
        return r;
      r = mailimap_astring_send(fd, key->sk_data.sk_filename);
      if (r != MAILIMAP_NO_ERROR)
        return r;
      return MAILIMAP_NO_ERROR;
      
    case XGMRAW_SEARCH_KEY_FLAGGED:
      return mailimap_token_send(fd, "is:flagged");
      
    case XGMRAW_SEARCH_KEY_FROM:
      r = mailimap_token_send(fd, "from");
      if (r != MAILIMAP_NO_ERROR)
        return r;
      r = mailimap_char_send(fd, ':');
      if (r != MAILIMAP_NO_ERROR)
        return r;
      r = mailimap_astring_send(fd, key->sk_data.sk_from);
      if (r != MAILIMAP_NO_ERROR)
        return r;
      return MAILIMAP_NO_ERROR;
      
    case XGMRAW_SEARCH_KEY_IN:
      r = mailimap_token_send(fd, "in");
      if (r != MAILIMAP_NO_ERROR)
        return r;
      r = mailimap_char_send(fd, ':');
      if (r != MAILIMAP_NO_ERROR)
        return r;
      r = mailimap_astring_send(fd, key->sk_data.sk_in);
      if (r != MAILIMAP_NO_ERROR)
        return r;
      return MAILIMAP_NO_ERROR;
      
    case XGMRAW_SEARCH_KEY_KEYWORD:
      return mailimap_token_send(fd, key->sk_data.sk_keyword);
      
    case XGMRAW_SEARCH_KEY_LABEL:
      r = mailimap_token_send(fd, "label");
      if (r != MAILIMAP_NO_ERROR)
        return r;
      r = mailimap_char_send(fd, ':');
      if (r != MAILIMAP_NO_ERROR)
        return r;
      r = mailimap_astring_send(fd, key->sk_data.sk_label);
      if (r != MAILIMAP_NO_ERROR)
        return r;
      return MAILIMAP_NO_ERROR;
      
    case XGMRAW_SEARCH_KEY_READ:
      return mailimap_token_send(fd, "is:read");
      
    case XGMRAW_SEARCH_KEY_SINCE:
      r = mailimap_token_send(fd, "after");
      if (r != MAILIMAP_NO_ERROR)
        return r;
      r = mailimap_char_send(fd, ':');
      if (r != MAILIMAP_NO_ERROR)
        return r;
      r = xgmraw_date_send(fd, key->sk_data.sk_since);
      if (r != MAILIMAP_NO_ERROR)
        return r;
      return MAILIMAP_NO_ERROR;
      
    case XGMRAW_SEARCH_KEY_SUBJECT:
      r = mailimap_token_send(fd, "subject");
      if (r != MAILIMAP_NO_ERROR)
        return r;
      r = mailimap_char_send(fd, ':');
      if (r != MAILIMAP_NO_ERROR)
        return r;
      r = mailimap_astring_send(fd, key->sk_data.sk_subject);
      if (r != MAILIMAP_NO_ERROR)
        return r;
      return MAILIMAP_NO_ERROR;
      
    case XGMRAW_SEARCH_KEY_STARRED:
      return mailimap_token_send(fd, "is:starred");
      
    case XGMRAW_SEARCH_KEY_TO:
      r = mailimap_token_send(fd, "to");
      if (r != MAILIMAP_NO_ERROR)
        return r;
      r = mailimap_char_send(fd, ':');
      if (r != MAILIMAP_NO_ERROR)
        return r;
      r = mailimap_astring_send(fd, key->sk_data.sk_to);
      if (r != MAILIMAP_NO_ERROR)
        return r;
      return MAILIMAP_NO_ERROR;
      
    case XGMRAW_SEARCH_KEY_UNREAD:
      return mailimap_token_send(fd, "is:unread");
      
    case XGMRAW_SEARCH_KEY_NOT:
      r = mailimap_char_send(fd, '-');
      if (r != MAILIMAP_NO_ERROR)
        return r;
      r = xgmraw_search_key_send(fd, key->sk_data.sk_not);
      if (r != MAILIMAP_NO_ERROR)
        return r;
      return MAILIMAP_NO_ERROR;
      
    case XGMRAW_SEARCH_KEY_OR:
      r = xgmraw_search_key_send(fd, key->sk_data.sk_or.sk_or1);
      if (r != MAILIMAP_NO_ERROR)
        return r;
      r = mailimap_space_send(fd);
      if (r != MAILIMAP_NO_ERROR)
        return r;
      r = mailimap_token_send(fd, "or");
      if (r != MAILIMAP_NO_ERROR)
        return r;
      r = mailimap_space_send(fd);
      if (r != MAILIMAP_NO_ERROR)
        return r;
      r = xgmraw_search_key_send(fd, key->sk_data.sk_or.sk_or2);
      if (r != MAILIMAP_NO_ERROR)
        return r;
      return MAILIMAP_NO_ERROR;
      
    case XGMRAW_SEARCH_KEY_MULTIPLE:
      return mailimap_struct_spaced_list_send(fd, key->sk_data.sk_multiple,
                                           (mailimap_struct_sender *) xgmraw_search_key_send);
      
    default:
      /* should not happen */
      return MAILIMAP_ERROR_INVAL;
  }
}

int xgmraw_search_send(mailstream * fd, struct xgmraw_search_key * key)
{
  int r;
  
  r = mailimap_token_send(fd, "SEARCH");
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  r = mailimap_token_send(fd, "X-GM-RAW");
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  r = mailimap_char_send(fd, '"');
  if (r != MAILIMAP_NO_ERROR)
    return r;
  r = xgmraw_search_key_send(fd, key);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  r = mailimap_char_send(fd, '"');
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  return MAILIMAP_NO_ERROR;
}

int xgmraw_search(mailimap * session, struct xgmraw_search_key * key, clist ** result)
{
  struct mailimap_response * response;
  int r;
  int error_code;
  
  if (session->imap_state != MAILIMAP_STATE_SELECTED)
    return MAILIMAP_ERROR_BAD_STATE;
  
  r = mailimap_send_current_tag(session);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  r = xgmraw_search_send(session->imap_stream, key);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  r = mailimap_crlf_send(session->imap_stream);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  if (mailstream_flush(session->imap_stream) == -1)
    return MAILIMAP_ERROR_STREAM;
  
  if (mailimap_read_line(session) == NULL)
    return MAILIMAP_ERROR_STREAM;
  
  r = mailimap_parse_response(session, &response);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  * result = session->imap_response_info->rsp_search_result;
  session->imap_response_info->rsp_search_result = NULL;
  
  error_code = response->rsp_resp_done->rsp_data.rsp_tagged->rsp_cond_state->rsp_type;
  
  mailimap_response_free(response);
  
  switch (error_code) {
    case MAILIMAP_RESP_COND_STATE_OK:
      return MAILIMAP_NO_ERROR;
      
    default:
      return MAILIMAP_ERROR_SEARCH;
  }
}
