#include "xgmmsgid.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "clist.h"
#include "mailimap_types_helper.h"
#include "mailimap_extension.h"
#include "mailimap_keywords.h"
#include "mailimap_parser.h"
#include "mailimap_sender.h"
#include "mailimap.h"

struct mailimap_fetch_att * mailimap_fetch_att_new_xgmmsgid(void)
{
  char * keyword;
  struct mailimap_fetch_att * att;

  keyword = strdup("X-GM-MSGID");
  if (keyword == NULL)
    return NULL;

  att = mailimap_fetch_att_new_extension(keyword);
  if (att == NULL) {
    free(keyword);
    return NULL;
  }

  return att;
}

int mailimap_has_xgmmsgid(mailimap * session)
{
  return mailimap_has_extension(session, "X-GM-EXT-1");
}

static int
mailimap_xgmmsgid_extension_parse(int calling_parser, mailstream * fd,
                                   MMAPString * buffer, size_t * indx,
                                   struct mailimap_extension_data ** result,
                                   size_t progr_rate, progress_function * progr_fun);

static void
mailimap_xgmmsgid_extension_data_free(struct mailimap_extension_data * ext_data);

LIBETPAN_EXPORT
struct mailimap_extension_api mailimap_extension_xgmmsgid = {
    /* name */          "X-GM-MSGID",
    /* extension_id */  MAILIMAP_EXTENSION_XGMMSGID,
    /* parser */        mailimap_xgmmsgid_extension_parse,
    /* free */          mailimap_xgmmsgid_extension_data_free
};

static int fetch_data_xgmmsgid_parse(mailstream * fd,
                                      MMAPString * buffer, size_t * indx,
                                      uint64_t * result, size_t progr_rate, progress_function * progr_fun)
{
    size_t cur_token;
    uint64_t msgid;
    int r;
    
    cur_token = * indx;
    
    r = mailimap_token_case_insensitive_parse(fd, buffer,
                                              &cur_token, "X-GM-MSGID");
    if (r != MAILIMAP_NO_ERROR)
        return r;
    
    r = mailimap_space_parse(fd, buffer, &cur_token);
    if (r != MAILIMAP_NO_ERROR)
        return r;
    
    r = mailimap_uint64_parse(fd, buffer, &cur_token, &msgid);
    if (r != MAILIMAP_NO_ERROR)
        return r;
    
    * indx = cur_token;
    * result = msgid;
    
    return MAILIMAP_NO_ERROR;
}

static int
mailimap_xgmmsgid_extension_parse(int calling_parser, mailstream * fd,
                                   MMAPString * buffer, size_t * indx,
                                   struct mailimap_extension_data ** result,
                                   size_t progr_rate, progress_function * progr_fun)
{
    size_t cur_token;
    uint64_t msgid;
    uint64_t * data_msgid;
    struct mailimap_extension_data * ext_data;
    int r;
    
    cur_token = * indx;
    
    switch (calling_parser)
    {
        case MAILIMAP_EXTENDED_PARSER_FETCH_DATA:
            
            r = fetch_data_xgmmsgid_parse(fd, buffer, &cur_token, &msgid, progr_rate, progr_fun);
            if (r != MAILIMAP_NO_ERROR)
              return r;
        
            data_msgid = malloc(sizeof(* data_msgid));
            if (data_msgid == NULL) {
              return MAILIMAP_ERROR_MEMORY;
            }
            * data_msgid = msgid;
            
            ext_data = mailimap_extension_data_new(&mailimap_extension_xgmmsgid,
                                                   MAILIMAP_XGMMSGID_TYPE_MSGID, data_msgid);
            if (ext_data == NULL) {
                free(data_msgid);
                return MAILIMAP_ERROR_MEMORY;
            }
            
            * result = ext_data;
            * indx = cur_token;
            
            return MAILIMAP_NO_ERROR;
            
        default:
            return MAILIMAP_ERROR_PARSE;
    }
}

/*
    Free mailimap_extension_data created for the xgmmsgid extension, including anything pointed to by
 ext_data->ext_data
 
static void
mailimap_xgmmsgid_extension_data_free(struct mailimap_extension_data * ext_data)
{
    if (ext_data == NULL)
	return;
    
    switch (ext_data->ext_type)
    {
	case MAILIMAP_XGMMSGID_TYPE_MSGID:
	    free(ext_data->ext_data); // msgid stashed in ext_data
	    break;
    }
    
    free (ext_data);

}
 */
static void
mailimap_xgmmsgid_extension_data_free(struct mailimap_extension_data * ext_data)
{
  free(ext_data->ext_data);
  free(ext_data);
}

int mailimap_fetch_xgmmsgid(mailimap * session,
                            struct mailimap_set * set,
                            clist ** results)
{
    struct mailimap_response * response;
    int r;
    int error_code;
    
    if (session->imap_state != MAILIMAP_STATE_SELECTED)
        return MAILIMAP_ERROR_BAD_STATE;
    
    r = mailimap_send_current_tag(session);
    if (r != MAILIMAP_NO_ERROR)
        return r;
    
    r = mailimap_token_send(session->imap_stream, "FETCH");
    if (r != MAILIMAP_NO_ERROR)
        return r;
    r = mailimap_space_send(session->imap_stream);
    if (r != MAILIMAP_NO_ERROR)
        return r;
    r = mailimap_set_send(session->imap_stream, set);
    if (r != MAILIMAP_NO_ERROR)
        return r;
    r = mailimap_space_send(session->imap_stream);
    if (r != MAILIMAP_NO_ERROR)
        return r;
    
    r = mailimap_token_send(session->imap_stream, "(X-GM-MSGID)");
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
    
    error_code = response->rsp_resp_done->rsp_data.rsp_tagged->rsp_cond_state->rsp_type;
    
    *results = response->rsp_cont_req_or_resp_data_list;

    mailimap_response_free(response);
    
    switch (error_code) {
        case MAILIMAP_RESP_COND_STATE_OK:
            return MAILIMAP_NO_ERROR;
            
        default:
            return MAILIMAP_ERROR_FETCH;
    }
}
