#include "xgmthrid.h"

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

enum {
    MAILIMAP_XGMTHRID_TYPE_THRID
};

static int
mailimap_xgmthrid_extension_parse(int calling_parser, mailstream * fd,
                                   MMAPString * buffer, size_t * indx,
                                   struct mailimap_extension_data ** result,
                                   size_t progr_rate, progress_function * progr_fun);

static void
mailimap_xgmthrid_extension_data_free(struct mailimap_extension_data * ext_data);

LIBETPAN_EXPORT
struct mailimap_extension_api mailimap_extension_xgmthrid = {
    /* name */          "X-GM-THRID",
    /* extension_id */  MAILIMAP_EXTENSION_XGMTHRID,
    /* parser */        mailimap_xgmthrid_extension_parse,
    /* free */          mailimap_xgmthrid_extension_data_free
};

static int fetch_data_xgmthrid_parse(mailstream * fd,
                                      MMAPString * buffer, size_t * indx,
                                      uint64_t * result, size_t progr_rate, progress_function * progr_fun)
{
    size_t cur_token;
    uint64_t thrid;
    uint32_t uid;
    int r;
    
    cur_token = * indx;
    
    r = mailimap_token_case_insensitive_parse(fd, buffer,
                                              &cur_token, "X-GM-THRID");
    if (r != MAILIMAP_NO_ERROR)
        return r;
    
    r = mailimap_space_parse(fd, buffer, &cur_token);
    if (r != MAILIMAP_NO_ERROR)
        return r;
    
    r = mailimap_uint64_parse(fd, buffer, &cur_token, &thrid);
    if (r != MAILIMAP_NO_ERROR)
        return r;
    
    * indx = cur_token;
    * result = thrid;
    
    return MAILIMAP_NO_ERROR;
}

static int
mailimap_xgmthrid_extension_parse(int calling_parser, mailstream * fd,
                                   MMAPString * buffer, size_t * indx,
                                   struct mailimap_extension_data ** result,
                                   size_t progr_rate, progress_function * progr_fun)
{
    size_t cur_token;
    uint64_t thrid;
    uint64_t * data_thrid;
    struct mailimap_extension_data * ext_data;
    int r;
    
    cur_token = * indx;
    
    switch (calling_parser)
    {
        case MAILIMAP_EXTENDED_PARSER_MAILBOX_DATA:
            
            r = fetch_data_xgmthrid_parse(fd, buffer, &cur_token, &thrid, progr_rate, progr_fun);
            if (r != MAILIMAP_NO_ERROR)
                return r;
            
            data_thrid = malloc(sizeof(* data_thrid));
            if (data_thrid == NULL) {
              return MAILIMAP_ERROR_MEMORY;
            }
            * data_thrid = thrid;
              
            ext_data = mailimap_extension_data_new(&mailimap_extension_xgmthrid,
                                                   MAILIMAP_XGMTHRID_TYPE_THRID, data_thrid);
            if (ext_data == NULL) {
              free(data_thrid);
              return MAILIMAP_ERROR_MEMORY;
            }
            
            * result = ext_data;
            * indx = cur_token;
            
            return MAILIMAP_NO_ERROR;
            
        default:
            return MAILIMAP_ERROR_PARSE;
    }
}

static void
mailimap_xgmthrid_extension_data_free(struct mailimap_extension_data * ext_data)
{
    free(ext_data->ext_data);
    free(ext_data);
}

struct mailimap_fetch_att * mailimap_fetch_att_new_xgmthrid(void)
{
  char * keyword;
  struct mailimap_fetch_att * att;
  
  keyword = strdup("X-GM-THRID");
  if (keyword == NULL)
    return NULL;
  
  att = mailimap_fetch_att_new_extension(keyword);
  if (att == NULL) {
    free(keyword);
    return NULL;
  }
  
  return att;
}
