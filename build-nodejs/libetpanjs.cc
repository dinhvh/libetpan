/*
 * libEtPan! -- a mail stuff library
 *
 * Copyright (C) 2001, 2013 - DINH Viet Hoa
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the libEtPan! project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <node.h>
#include <v8.h>
#include <node_buffer.h>
#include <libetpan/libetpan.h>

#include "response.h"

using namespace v8;
using namespace etpanjs;
using namespace node;

extern "C" {
typedef int mailimap_struct_parser(mailstream * fd, MMAPString * buffer,
    				   size_t * indx, void * result,
    				   size_t progr_rate,
    				   progress_function * progr_fun);
typedef void mailimap_struct_destructor(void * result);

extern int mailimap_greeting_parse(mailstream * fd, MMAPString * buffer,
                            size_t * indx,
                            struct mailimap_greeting ** result,
                            size_t progr_rate,
                            progress_function * progr_fun);

extern int
mailimap_response_parse_with_context(mailstream * fd, MMAPString * buffer,
                                     size_t * indx, struct mailimap_response ** result,
                                     mailprogress_function * body_progr_fun,
                                     mailprogress_function * items_progr_fun,
                                     void * context,
                                     mailimap_msg_att_handler * msg_att_handler,
                                     void * msg_att_context);

extern int
mailimap_continue_req_parse(mailstream * fd, MMAPString * buffer,
                            size_t * indx,
                            struct mailimap_continue_req ** result,
                            size_t progr_rate,
                            progress_function * progr_fun);
                            
extern int
mailimap_response_data_parse(mailstream * fd, MMAPString * buffer,
                            size_t * indx,
                            struct mailimap_response_data ** result,
                            size_t progr_rate,
                            progress_function * progr_fun);

extern int
mailimap_struct_multiple_parse(mailstream * fd, MMAPString * buffer,
                            			       size_t * indx, clist ** result,
                            			       mailimap_struct_parser * parser,
                            			       mailimap_struct_destructor * destructor,
                            			       size_t progr_rate,
                            			       progress_function * progr_fun);
}

enum {
    PARSER_ENABLE_RESPONSE = 1 << 0,
    PARSER_ENABLE_GREETING = 1 << 1,
    PARSER_CONT_REQ = 1 << 2,
};

Handle<Value> responseParse(const Arguments& args)
{
  HandleScope scope;
  
  if (args.Length() < 2) {
    ThrowException(Exception::TypeError(String::New("Wrong number of arguments")));
    return scope.Close(Undefined());
  }

  Handle<Object> buffer = node::Buffer::New(args[0]->ToString());
  int parserFlags = args[1]->ToInteger()->Value();
  char * bytes = node::Buffer::Data(buffer);
  size_t length = node::Buffer::Length(buffer);
  
  MMAPString * str = mmap_string_new_len(bytes, length);
  size_t indx = 0;
  int r = MAILIMAP_ERROR_PARSE;
  
  struct mailimap_response * response = NULL;
  struct mailimap_greeting * greeting = NULL;
  struct mailimap_continue_req * contReq = NULL;
  clist * idleDataResponse = NULL;
  int done = 0;
  int type = 0;
  
  fprintf(stderr, "parser flags: %i\n", parserFlags);
  if (((parserFlags & PARSER_ENABLE_GREETING) != 0) && !done) {
      fprintf(stderr, "try greeting\n");
      r = mailimap_greeting_parse(NULL, str, &indx, &greeting, 0, NULL);
      if (r == MAILIMAP_NO_ERROR) {
          done = 1;
          type = PARSER_ENABLE_GREETING;
      }
  }
  else {
      if (((parserFlags & PARSER_CONT_REQ) != 0) && !done) {
          fprintf(stderr, "try response data list\n");
          r = mailimap_struct_multiple_parse(NULL, str, &indx, &idleDataResponse,
              (mailimap_struct_parser *) mailimap_response_data_parse,
              (mailimap_struct_destructor *) mailimap_response_data_free,
              0, NULL);
          if ((r == MAILIMAP_NO_ERROR) || (r == MAILIMAP_ERROR_PARSE)) {
              fprintf(stderr, "try cont req\n");
              r = mailimap_continue_req_parse(NULL, str, &indx, &contReq, 0, NULL);
              if (r == MAILIMAP_NO_ERROR) {
                  done = 1;
                  type = PARSER_CONT_REQ;
              }
          }
      }
      if (((parserFlags & PARSER_ENABLE_RESPONSE) != 0) && !done) {
          fprintf(stderr, "try response\n");
          r = mailimap_response_parse_with_context(NULL, str, &indx, &response, NULL, NULL, NULL, NULL, NULL);
          if (r == MAILIMAP_NO_ERROR) {
              done = 1;
              type = PARSER_ENABLE_RESPONSE;
          }
      }
  }
  
  if (!done) {
      if (r == MAILIMAP_ERROR_NEEDS_MORE_DATA) {
          fprintf(stderr, "response: needs more data\n");
      }
      else {
          fprintf(stderr, "response: parser error %i\n", r);
      }
  }
  
  Handle<Object> responseWrapper = Response::New(r, type, idleDataResponse != NULL);
  Response * obj = ObjectWrap::Unwrap<Response>(responseWrapper);
  obj->setResponse(response);
  obj->setGreeting(greeting);
  obj->setContReq(contReq);
  obj->setIdleDataResponse(idleDataResponse);
  
  if (response != NULL) {
      bool success = true;
      if (response->rsp_resp_done->rsp_type == MAILIMAP_RESP_DONE_TYPE_FATAL) {
          success = false;
      }
      if (response->rsp_resp_done->rsp_data.rsp_tagged->rsp_cond_state->rsp_type ==
          MAILIMAP_RESP_COND_STATE_BAD) {
          success = false;
      }
      if (!success) {
          responseWrapper->Set(String::NewSymbol("result"), Integer::New(-1));
      }
  }
  if (greeting != NULL) {
      if (greeting->gr_type != MAILIMAP_GREETING_RESP_COND_AUTH) {
          responseWrapper->Set(String::NewSymbol("result"), Integer::New(-1));
      }
  }
  
  if (r == MAILIMAP_NO_ERROR) {
    fprintf(stderr, "response: parsing successful\n");
  }
  
  return scope.Close(responseWrapper);
}


void init(Handle<Object> target) {
  NODE_SET_METHOD(target, "responseParse", responseParse);
  Response::Init(target);
}

NODE_MODULE(etpan, init)
