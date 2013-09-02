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

#ifndef ETPANJS_RESPONSE_H
#define ETPANJS_RESPONSE_H

#include <node.h>
#include <libetpan/libetpan.h>

using namespace v8;

namespace etpanjs {

    class Response : public node::ObjectWrap {
    public:
        Response();
        virtual ~Response();
    
        static Persistent<FunctionTemplate> responseTemplate;

        static void Init(Handle<Object> exports);
        static Handle<Object> New(const Arguments& args);
        static Handle<Object> New(int result, int type, bool hasIdleData);
        
        virtual void setResponse(struct mailimap_response * response);
        virtual struct mailimap_response * getResponse();
        
        virtual void setGreeting(struct mailimap_greeting * greeting);
        virtual struct mailimap_greeting * getGreeting();
        
        virtual void setContReq(struct mailimap_continue_req * contReq);
        virtual struct mailimap_continue_req * getContReq();
        
        virtual void setIdleDataResponse(clist * idleDataResponse);
        virtual clist * getIdleDataResponse();
        
    private:
        // general response
        struct mailimap_response * mResponse;
        // greeting response on connection
        struct mailimap_greeting * mGreeting;
        // continue-req response
        struct mailimap_continue_req * mContReq;
        // idle data response
        clist * mIdleDataResponse;
    };

}

#endif

