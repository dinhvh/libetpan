/*
 *  xlist.c
 *  libetpan
 *
 *  Created by DINH Viêt Hoà on 30/3/11.
 *  Copyright 2011 __MyCompanyName__. All rights reserved.
 *
 */

#include "xlist.h"

#include <stdlib.h>
#include "mailimap.h"
#include "mailimap_extension.h"
#include "mailimap_extension_types.h"
#include "mailimap_sender.h"
#include "mailimap_parser.h"
#include "mailimap_keywords.h"

enum {
    MAILIMAP_XLIST_TYPE_XLIST
};

static int
mailimap_xlist_extension_parse(int calling_parser, mailstream * fd,
                               MMAPString * buffer, size_t * indx,
                               struct mailimap_extension_data ** result,
                               size_t progr_rate, progress_function * progr_fun);

static void
mailimap_xlist_extension_data_free(struct mailimap_extension_data * ext_data);

LIBETPAN_EXPORT
struct mailimap_extension_api mailimap_extension_xlist = {
  /* name */          "XLIST",
  /* extension_id */  MAILIMAP_EXTENSION_XLIST,
  /* parser */        mailimap_xlist_extension_parse,
  /* free */          mailimap_xlist_extension_data_free
};

static int mailimap_xlist_send(mailstream * fd,
                               const char * mb, const char * list_mb)
{
  int r;
  
  r = mailimap_token_send(fd, "XLIST");
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  r = mailimap_mailbox_send(fd, mb);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  r = mailimap_list_mailbox_send(fd, list_mb);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  return MAILIMAP_NO_ERROR;
}

LIBETPAN_EXPORT
int mailimap_xlist(mailimap * session, const char * mb,
                   const char * list_mb, clist ** result)
{
  struct mailimap_response * response;
  int r;
  int error_code;
  clistiter * cur;
  int res;
  clist * result_list;
  
  if ((session->imap_state != MAILIMAP_STATE_AUTHENTICATED) &&
      (session->imap_state != MAILIMAP_STATE_SELECTED))
    return MAILIMAP_ERROR_BAD_STATE;
  
  r = mailimap_send_current_tag(session);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  r = mailimap_xlist_send(session->imap_stream, mb, list_mb);
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
  
  result_list = clist_new();
  if (result_list == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free_response;
  }
  
  for(cur = clist_begin(session->imap_response_info->rsp_extension_list) ; cur != NULL ; cur = clist_next(cur)) {
    struct mailimap_extension_data * ext_data;
    struct mailimap_mailbox_list * mailbox_list;
    
    ext_data = clist_content(cur);
    if (ext_data->ext_extension->ext_id != MAILIMAP_EXTENSION_XLIST) {
      continue;
    }
    if (ext_data->ext_type != MAILIMAP_XLIST_TYPE_XLIST) {
      continue;
    }
    
    mailbox_list = ext_data->ext_data;
    r = clist_append(result_list, mailbox_list);
    if (r < 0) {
      res = MAILIMAP_ERROR_MEMORY;
      goto free_list;
    }
    ext_data->ext_data = NULL;
  }
  
  * result = result_list;
  
  error_code = response->rsp_resp_done->rsp_data.rsp_tagged->rsp_cond_state->rsp_type;
  
  mailimap_response_free(response);
  
  switch (error_code) {
    case MAILIMAP_RESP_COND_STATE_OK:
      return MAILIMAP_NO_ERROR;
      
    default:
      return MAILIMAP_ERROR_LIST;
  }
  
free_list:
  for(cur = clist_begin(result_list) ; cur != NULL ; cur = clist_next(cur)) {
    struct mailimap_mailbox_list * mailbox_list;
    
    mailbox_list = clist_content(cur);
    mailimap_mailbox_list_free(mailbox_list);
  }
  clist_free(result_list);
free_response:
  mailimap_response_free(response);
err:
  return res;
}

static int
mailimap_mailbox_data_xlist_parse(mailstream * fd, MMAPString * buffer,
                                  size_t * indx,
                                  struct mailimap_mailbox_list ** result,
                                  size_t progr_rate,
                                  progress_function * progr_fun)
{
  size_t cur_token;
  struct mailimap_mailbox_list * mb_list;
  int r;
  int res;
  
  cur_token = * indx;
  
  r = mailimap_token_case_insensitive_parse(fd, buffer, &cur_token, "XLIST");
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    return r;
  }
  
  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    return r;
  }
  
  r = mailimap_mailbox_list_parse(fd, buffer, &cur_token, &mb_list,
                                  progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    return r;
  }
  
  * result = mb_list;
  * indx = cur_token;
  
  return MAILIMAP_NO_ERROR;
}

static int
mailimap_xlist_extension_parse(int calling_parser, mailstream * fd,
                               MMAPString * buffer, size_t * indx,
                               struct mailimap_extension_data ** result,
                               size_t progr_rate, progress_function * progr_fun)
{
  int r;
  struct mailimap_mailbox_list * xlist_data = NULL;
  struct mailimap_extension_data * ext_data;
  void * data;
  int type;
  size_t cur_token;
  
  cur_token = * indx;
  
  switch (calling_parser)
  {
    case MAILIMAP_EXTENDED_PARSER_RESPONSE_DATA:
      r = mailimap_mailbox_data_xlist_parse(fd, buffer, &cur_token,
                                            &xlist_data, progr_rate, progr_fun);
      if (r == MAILIMAP_NO_ERROR) {
        type = MAILIMAP_XLIST_TYPE_XLIST;
        data = xlist_data;
      }
      
      if (r != MAILIMAP_NO_ERROR) {
        return r;
      }
      
      ext_data = mailimap_extension_data_new(&mailimap_extension_xlist,
                                             type, data);
      if (ext_data == NULL) {
        if (xlist_data != NULL)
          mailimap_mailbox_list_free(xlist_data);
        return MAILIMAP_ERROR_MEMORY;
      }
      
      * result = ext_data;
      * indx = cur_token;
      
      return MAILIMAP_NO_ERROR;
      
    default:
      /* return a MAILIMAP_ERROR_PARSE if the extension
       doesn't extend calling_parser. */
      return MAILIMAP_ERROR_PARSE;
  }
}

LIBETPAN_EXPORT
int mailimap_has_xlist(mailimap * session)
{
  return mailimap_has_extension(session, "XLIST");
}

static void
mailimap_xlist_extension_data_free(struct mailimap_extension_data * ext_data)
{
  if (ext_data->ext_data != NULL) {
    mailimap_mailbox_list_free((struct mailimap_mailbox_list *) ext_data->ext_data);
  }
  free(ext_data);
}
