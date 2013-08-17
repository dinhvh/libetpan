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

#ifndef ETPANJS_TYPESCONV_H

#define ETPANJS_TYPESCONV_H

#include <node.h>

using namespace v8;

namespace etpanjs {

Handle<Value> getFoldersFromResponseList(const Arguments& args);
Handle<Value> getFoldersFromResponseLsub(const Arguments& args);
Handle<Value> getFetchItemsFromResponse(const Arguments& args);
Handle<Value> getCapabilitiesFromResponse(const Arguments& args);
Handle<Value> getUIDPlusCopyResponseFromResponse(const Arguments& args);
Handle<Value> getUIDPlusAppendResponseFromResponse(const Arguments& args);
Handle<Value> getStatusResponseFromResponse(const Arguments& args);
Handle<Value> getIDResponseFromResponse(const Arguments& args);
Handle<Value> getSelectResponseFromResponse(const Arguments& args);
Handle<Value> getNoopResponseFromResponse(const Arguments& args);
Handle<Value> getSearchResponseFromResponse(const Arguments& args);

}

#endif
