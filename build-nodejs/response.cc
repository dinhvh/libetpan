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

#include "response.h"
#include "typesconv.h"

using namespace etpanjs;

Persistent<FunctionTemplate> Response::responseTemplate;

Response::Response()
{
    mResponse = NULL;
    mGreeting = NULL;
    mContReq = NULL;
    mIdleDataResponse = NULL;
}

Response::~Response()
{
    if (mIdleDataResponse != NULL) {
        clist_foreach(mIdleDataResponse,
        (clist_func) mailimap_response_data_free, NULL);
        clist_free(mIdleDataResponse);
    }
    if (mContReq != NULL) {
        mailimap_continue_req_free(mContReq);
    }
    if (mGreeting != NULL) {
        mailimap_greeting_free(mGreeting);
    }
    if (mResponse != NULL) {
        mailimap_response_free(mResponse);
    }
}

void Response::Init(Handle<Object> exports)
{
    Local<FunctionTemplate> tpl = FunctionTemplate::New();

    responseTemplate = Persistent<FunctionTemplate>::New(tpl);
    responseTemplate->InstanceTemplate()->SetInternalFieldCount(1);  
    responseTemplate->SetClassName(String::NewSymbol("Response"));
    
    responseTemplate->PrototypeTemplate()->Set(String::NewSymbol("getFoldersFromResponseList"), FunctionTemplate::New(getFoldersFromResponseList)->GetFunction());
    responseTemplate->PrototypeTemplate()->Set(String::NewSymbol("getFoldersFromResponseLsub"), FunctionTemplate::New(getFoldersFromResponseLsub)->GetFunction());
    responseTemplate->PrototypeTemplate()->Set(String::NewSymbol("getFetchItemsFromResponse"), FunctionTemplate::New(getFetchItemsFromResponse)->GetFunction());
    responseTemplate->PrototypeTemplate()->Set(String::NewSymbol("getCapabilitiesFromResponse"), FunctionTemplate::New(getCapabilitiesFromResponse)->GetFunction());
    responseTemplate->PrototypeTemplate()->Set(String::NewSymbol("getUIDPlusCopyResponseFromResponse"), FunctionTemplate::New(getUIDPlusCopyResponseFromResponse)->GetFunction());
    responseTemplate->PrototypeTemplate()->Set(String::NewSymbol("getUIDPlusAppendResponseFromResponse"), FunctionTemplate::New(getUIDPlusAppendResponseFromResponse)->GetFunction());
    responseTemplate->PrototypeTemplate()->Set(String::NewSymbol("getStatusResponseFromResponse"), FunctionTemplate::New(getStatusResponseFromResponse)->GetFunction());
    responseTemplate->PrototypeTemplate()->Set(String::NewSymbol("getIDResponseFromResponse"), FunctionTemplate::New(getIDResponseFromResponse)->GetFunction());
    responseTemplate->PrototypeTemplate()->Set(String::NewSymbol("getSelectResponseFromResponse"), FunctionTemplate::New(getSelectResponseFromResponse)->GetFunction());
    responseTemplate->PrototypeTemplate()->Set(String::NewSymbol("getNoopResponseFromResponse"), FunctionTemplate::New(getNoopResponseFromResponse)->GetFunction());
    responseTemplate->PrototypeTemplate()->Set(String::NewSymbol("getSearchResponseFromResponse"), FunctionTemplate::New(getSearchResponseFromResponse)->GetFunction());
    
    exports->Set(String::NewSymbol("Response"), responseTemplate->GetFunction());
}

Handle<Object> Response::New(const Arguments& args)
{
    HandleScope scope;
    Response * response = new Response();
    response->Wrap(args.This());
    args.This()->Set(String::NewSymbol("result"), args[0]->ToInteger());
    args.This()->Set(String::NewSymbol("type"), args[1]->ToInteger());
    args.This()->Set(String::NewSymbol("hasIdleData"), args[2]->ToBoolean());
    return args.This();
}

Handle<Object> Response::New(int result, int type, bool hasIdleData)
{
    HandleScope scope;
    Handle<Object> instance = responseTemplate->InstanceTemplate()->NewInstance();
    Response * response = new Response();
    response->Wrap(instance);
    instance->Set(String::NewSymbol("result"), Integer::New(result));
    instance->Set(String::NewSymbol("type"), Integer::New(type));
    instance->Set(String::NewSymbol("hasIdleData"), Boolean::New(hasIdleData));
    return scope.Close(instance);
}

void Response::setResponse(struct mailimap_response * response)
{
    mResponse = response;
}

struct mailimap_response * Response::getResponse()
{
    return mResponse;
}

void Response::setGreeting(struct mailimap_greeting * greeting)
{
    mGreeting = greeting;
}

struct mailimap_greeting * Response::getGreeting()
{
    return mGreeting;
}

void Response::setContReq(struct mailimap_continue_req * contReq)
{
    mContReq = contReq;
}

struct mailimap_continue_req * Response::getContReq()
{
    return mContReq;
}

void Response::setIdleDataResponse(clist * idleDataResponse)
{
    mIdleDataResponse = idleDataResponse;
}

clist * Response::getIdleDataResponse()
{
    return mIdleDataResponse;
}
