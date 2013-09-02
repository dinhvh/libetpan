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

#include "typesconv.h"

#include <node.h>
#include <v8.h>
#include <node_buffer.h>
#include <libetpan/libetpan.h>

#include "response.h"

using namespace v8;
using namespace etpanjs;
using namespace node;

enum MessageFlag {
    MessageFlagNone          = 0,
    MessageFlagSeen          = 1 << 0,
    MessageFlagAnswered      = 1 << 1,
    MessageFlagFlagged       = 1 << 2,
    MessageFlagDeleted       = 1 << 3,
    MessageFlagDraft         = 1 << 4,
    MessageFlagMDNSent       = 1 << 5,
    MessageFlagForwarded     = 1 << 6,
    MessageFlagSubmitPending = 1 << 7,
    MessageFlagSubmitted     = 1 << 8,
} ;

static int imap_mailbox_flags_to_flags(struct mailimap_mbx_list_flags * imap_flags);
static MessageFlag flags_from_lep_att_dynamic(struct mailimap_msg_att_dynamic * att_dynamic);
static Handle<Value> getFoldersFromResponseWithOption(const Arguments& args, int dataType);

Handle<Value> etpanjs::getFoldersFromResponseList(const Arguments& args)
{
    return getFoldersFromResponseWithOption(args, MAILIMAP_MAILBOX_DATA_LIST);
}

Handle<Value> etpanjs::getFoldersFromResponseLsub(const Arguments& args)
{
    return getFoldersFromResponseWithOption(args, MAILIMAP_MAILBOX_DATA_LSUB);
}

static Handle<Value> getFoldersFromResponseWithOption(const Arguments& args, int dataType)
{
    HandleScope scope;
    
    Response * obj = ObjectWrap::Unwrap<Response>(args.This());
    struct mailimap_response * response = obj->getResponse();
    
    clist * folders = clist_new();
    if (response->rsp_cont_req_or_resp_data_list != NULL) {
        for(clistiter * cur = clist_begin(response->rsp_cont_req_or_resp_data_list) ; cur != NULL ; cur = clist_next(cur)) {
            struct mailimap_cont_req_or_resp_data * cont_req_or_resp_data;
        
            cont_req_or_resp_data = (struct mailimap_cont_req_or_resp_data *) clist_content(cur);
            if (cont_req_or_resp_data->rsp_type == MAILIMAP_RESP_RESP_DATA) {
                struct mailimap_response_data * resp_data;

                resp_data = cont_req_or_resp_data->rsp_data.rsp_resp_data;
                if (resp_data->rsp_type == MAILIMAP_RESP_DATA_TYPE_MAILBOX_DATA) {
                    struct mailimap_mailbox_data * mb_data;
                
                    mb_data = resp_data->rsp_data.rsp_mailbox_data;
                    if (mb_data->mbd_type == dataType) {
                        clist_append(folders, mb_data->mbd_data.mbd_list);
                    }
                }
            }
        }
    }
    
    unsigned int count = clist_count(folders);
    Handle<Array> array = Array::New(count);
    unsigned int i = 0;
    for(clistiter * iter = clist_begin(folders) ; iter != NULL ; iter = clist_next(iter)) {
        Local<Object> obj = Object::New();
        struct mailimap_mailbox_list * mb_list = (struct mailimap_mailbox_list *) clist_content(iter);
        
        obj->Set(String::NewSymbol("path"), String::New(mb_list->mb_name));
        char delimiter[2];
        delimiter[0] = mb_list->mb_delimiter;
        delimiter[1] = 0;
        obj->Set(String::NewSymbol("delimiter"), String::New(delimiter));
        obj->Set(String::NewSymbol("flags"), Integer::New(imap_mailbox_flags_to_flags(mb_list->mb_flag)));
        array->Set(i, obj);
        i ++;
    }
    
    clist_free(folders);
    
    return scope.Close(array);
}

static Handle<Value> bodyToV8(struct mailimap_body * body);

static void addBodyFieldParameter(Handle<Object> obj, const char * fieldName, struct mailimap_body_fld_param * parameter)
{
    unsigned int count = clist_count(parameter->pa_list);
    Handle<Array> array = Array::New(count);
    int idx = 0;
    for(clistiter * iter = clist_begin(parameter->pa_list) ; iter != NULL ; iter = clist_next(iter)) {
        struct mailimap_single_body_fld_param * param = (struct mailimap_single_body_fld_param *) clist_content(iter);
        Local<Object> item = Object::New();
        item->Set(String::NewSymbol("name"), String::New(param->pa_name));
        item->Set(String::NewSymbol("value"), String::New(param->pa_value));
        array->Set(idx, item);
        idx ++;
    }
    obj->Set(String::NewSymbol(fieldName), array);
}

static void addMIMEFields(Handle<Object> obj, struct mailimap_body_fields * fields)
{
    if (fields->bd_parameter != NULL) {
        addBodyFieldParameter(obj, "content-type-parameters", fields->bd_parameter);
    }
    if (fields->bd_id != NULL) {
        obj->Set(String::NewSymbol("id"), String::New(fields->bd_id));
    }
    if (fields->bd_description != NULL) {
        obj->Set(String::NewSymbol("description"), String::New(fields->bd_description));
    }
    if (fields->bd_encoding != NULL) {
        switch (fields->bd_encoding->enc_type) {
            case MAILIMAP_BODY_FLD_ENC_7BIT:
                obj->Set(String::NewSymbol("encoding"), String::New("7bit"));
                break;
            case MAILIMAP_BODY_FLD_ENC_8BIT:
                obj->Set(String::NewSymbol("encoding"), String::New("8bit"));
                break;
            case MAILIMAP_BODY_FLD_ENC_BINARY:
                obj->Set(String::NewSymbol("encoding"), String::New("binary"));
                break;
            case MAILIMAP_BODY_FLD_ENC_BASE64:
                obj->Set(String::NewSymbol("encoding"), String::New("base64"));
                break;
            case MAILIMAP_BODY_FLD_ENC_QUOTED_PRINTABLE:
                obj->Set(String::NewSymbol("encoding"), String::New("quoted-printable"));
                break;
            case MAILIMAP_BODY_FLD_ENC_OTHER:
                obj->Set(String::NewSymbol("encoding"), String::New(fields->bd_encoding->enc_value));
                break;
        }
    }
}

static void addMedia(Local<Object> obj, struct mailimap_media_basic * media) {
    const char * leftPart = "application";
    const char * rightPart = media->med_subtype;
    
    switch (media->med_type) {
        case MAILIMAP_MEDIA_BASIC_APPLICATION:
        leftPart = "application";
        break;
        case MAILIMAP_MEDIA_BASIC_AUDIO:
        leftPart = "audio";
        break;
        case MAILIMAP_MEDIA_BASIC_IMAGE:
        leftPart = "image";
        break;
        case MAILIMAP_MEDIA_BASIC_MESSAGE:
        leftPart = "message";
        break;
        case MAILIMAP_MEDIA_BASIC_VIDEO:
        leftPart = "video";
        break;
        case MAILIMAP_MEDIA_BASIC_OTHER:
        leftPart = media->med_basic_type;
        break;
    }
    
    size_t len = strlen(leftPart) + strlen(rightPart) + 2;
    char * result = (char *) malloc(len);
    snprintf(result, len, "%s/%s", leftPart, rightPart);
    obj->Set(String::NewSymbol("mimeType"), String::New(result));
    free(result);
}

static void AddAddressList(Handle<Object> obj, const char * name, clist * address_list)
{
    if (address_list == NULL)
        return;
    
    int idx = 0;
    Handle<Array> array = Array::New(clist_count(address_list));
    for(clistiter * iter = clist_begin(address_list) ; iter != NULL ; iter = clist_next(iter)) {
        struct mailimap_address * address = (struct mailimap_address *) clist_content(iter);
        Local<Object> item = Object::New();
        
        if (address->ad_personal_name != NULL) {
            item->Set(String::NewSymbol("name"), String::New(address->ad_personal_name));
        }
        
        char * mailbox = NULL;
        if ((address->ad_mailbox_name != NULL) && (address->ad_host_name != NULL)) {
            size_t len = strlen(address->ad_mailbox_name) + strlen(address->ad_host_name) + 2;
            mailbox = (char *) malloc(len);
            snprintf(mailbox, len, "%s@%s", address->ad_mailbox_name, address->ad_host_name);
        }
        else if (address->ad_mailbox_name != NULL) {
            mailbox = strdup(address->ad_mailbox_name);
        }
        else if (address->ad_host_name != NULL) {
            size_t len = strlen(address->ad_host_name) + 2;
            mailbox = (char *) malloc(len);
            snprintf(mailbox, len, "@%s", address->ad_host_name);
        }
        
        if (mailbox != NULL) {
            item->Set(String::NewSymbol("mailbox"), String::New(mailbox));
            free(mailbox);
        }
        array->Set(idx, item);
        idx ++;
    }
    
    obj->Set(String::NewSymbol(name), array);
}

static Handle<Value> envelopeToV8(struct mailimap_envelope * env)
{
    HandleScope scope;
    
    Local<Object> obj = Object::New();
    if (env->env_date != NULL) {
        obj->Set(String::NewSymbol("date"), String::New(env->env_date));
    }
    if (env->env_subject != NULL) {
        obj->Set(String::NewSymbol("subject"), String::New(env->env_subject));
    }
    if (env->env_from != NULL) {
        AddAddressList(obj, "from", env->env_from->frm_list);
    }
    if (env->env_sender != NULL) {
        AddAddressList(obj, "sender", env->env_sender->snd_list);
    }
    if (env->env_reply_to != NULL) {
        AddAddressList(obj, "replyto", env->env_reply_to->rt_list);
    }
    if (env->env_to != NULL) {
        AddAddressList(obj, "to", env->env_to->to_list);
    }
    if (env->env_cc != NULL) {
        AddAddressList(obj, "cc", env->env_cc->cc_list);
    }
    if (env->env_bcc != NULL) {
        AddAddressList(obj, "bcc", env->env_bcc->bcc_list);
    }
    if (env->env_in_reply_to != NULL) {
        obj->Set(String::NewSymbol("inreplyto"), String::New(env->env_in_reply_to));
    }
    if (env->env_message_id != NULL) {
        obj->Set(String::NewSymbol("msgid"), String::New(env->env_message_id));
    }
    
    return scope.Close(obj);
}

static void AddLanguage(Handle<Object> obj, struct mailimap_body_fld_lang * lang)
{
    Handle<Array> array;
    switch (lang->lg_type) {
        case MAILIMAP_BODY_FLD_LANG_SINGLE: {
            if (lang->lg_data.lg_single == NULL) {
                return;
            }
            array = Array::New(1);
            array->Set(0, String::New(lang->lg_data.lg_single));
            break;
        }
        case MAILIMAP_BODY_FLD_LANG_LIST: {
            if (clist_count(lang->lg_data.lg_list) == 0) {
                return;
            }
            array = Array::New(clist_count(lang->lg_data.lg_list));
            int idx = 0;
            for(clistiter * iter = clist_begin(lang->lg_data.lg_list) ; iter != NULL ;
            iter = clist_next(iter)) {
                char * lang = (char *) clist_content(iter);
                array->Set(idx, String::New(lang));
                idx ++;
            }
            break;
        }
    }
    obj->Set(String::NewSymbol("language"), array);
}

static void AddExtensions(Handle<Object> obj, struct mailimap_body_ext_1part * ext_1part)
{
    if (ext_1part->bd_md5 != NULL) {
        obj->Set(String::NewSymbol("md5"), String::New(ext_1part->bd_md5));
    }
    if (ext_1part->bd_disposition != NULL) {
        obj->Set(String::NewSymbol("disposition-type"), String::New(ext_1part->bd_disposition->dsp_type));
        if (ext_1part->bd_disposition->dsp_attributes != NULL) {
            addBodyFieldParameter(obj, "disposition-parameters", ext_1part->bd_disposition->dsp_attributes);
        }
    }
    if (ext_1part->bd_language != NULL) {
        AddLanguage(obj, ext_1part->bd_language);
    }
    if (ext_1part->bd_loc != NULL) {
        obj->Set(String::NewSymbol("location"), String::New(ext_1part->bd_loc));
    }
    if (ext_1part->bd_extension_list != NULL) {
        // TODO
    }
}

static void AddMultipartExtensions(Handle<Object> obj, struct mailimap_body_ext_mpart * ext_mpart)
{
    if (ext_mpart->bd_parameter != NULL) {
        addBodyFieldParameter(obj, "content-type-parameters", ext_mpart->bd_parameter);
    }
    if (ext_mpart->bd_disposition != NULL) {
        obj->Set(String::NewSymbol("disposition-type"), String::New(ext_mpart->bd_disposition->dsp_type));
        if (ext_mpart->bd_disposition->dsp_attributes != NULL) {
            addBodyFieldParameter(obj, "disposition-parameters", ext_mpart->bd_disposition->dsp_attributes);
        }
    }
    if (ext_mpart->bd_language) {
        AddLanguage(obj, ext_mpart->bd_language);
    }
    if (ext_mpart->bd_loc != NULL) {
        obj->Set(String::NewSymbol("location"), String::New(ext_mpart->bd_loc));
    }
    if (ext_mpart->bd_extension_list != NULL) {
        //TODO
    }
}

static Handle<Value> singlePartToV8(struct mailimap_body_type_1part * part)
{
    HandleScope scope;
    
    Local<Object> obj = Object::New();
    obj->Set(String::NewSymbol("type"), String::NewSymbol("single"));
    switch (part->bd_type) {
        case MAILIMAP_BODY_TYPE_1PART_BASIC: {
            addMedia(obj, part->bd_data.bd_type_basic->bd_media_basic);
            addMIMEFields(obj, part->bd_data.bd_type_basic->bd_fields);
            break;
        }
        case MAILIMAP_BODY_TYPE_1PART_TEXT: {
            // mime type
            size_t len = strlen("text/") + strlen(part->bd_data.bd_type_text->bd_media_text) + 1;
            char * mimeType = (char *) malloc(len);
            snprintf(mimeType, len, "text/%s", part->bd_data.bd_type_text->bd_media_text);
            obj->Set(String::NewSymbol("mimeType"), String::New(mimeType));
            free(mimeType);
            addMIMEFields(obj, part->bd_data.bd_type_basic->bd_fields);
            break;
        }
    }
    AddExtensions(obj, part->bd_ext_1part);
    
    return scope.Close(obj);
}

static Handle<Value> messagePartBodyToV8(struct mailimap_body_type_1part * part)
{
    HandleScope scope;
    Local<Object> obj = Object::New();
    obj->Set(String::NewSymbol("type"), String::NewSymbol("message"));
    addMIMEFields(obj, part->bd_data.bd_type_msg->bd_fields);
    obj->Set(String::NewSymbol("mimeType"), String::New("message/rfc822"));
    obj->Set(String::NewSymbol("env"), envelopeToV8(part->bd_data.bd_type_msg->bd_envelope));
    obj->Set(String::NewSymbol("body"), bodyToV8(part->bd_data.bd_type_msg->bd_body));
    AddExtensions(obj, part->bd_ext_1part);
    return scope.Close(obj);
}

static Handle<Value> multipartToV8(struct mailimap_body_type_mpart * part)
{
    HandleScope scope;
    
    Local<Object> obj = Object::New();
    
    obj->Set(String::NewSymbol("type"), String::NewSymbol("multipart"));
    
    // mime type
    size_t len = strlen("multipart/") + strlen(part->bd_media_subtype) + 1;
    char * mimeType = (char *) malloc(len);
    snprintf(mimeType, len, "multipart/%s", part->bd_media_subtype);
    obj->Set(String::NewSymbol("mimeType"), String::New(mimeType));
    free(mimeType);
    
    int count = clist_count(part->bd_list);
    Handle<Array> array = Array::New(count);
    int idx = 0;
    for(clistiter * iter = clist_begin(part->bd_list) ; iter != NULL ; iter = clist_next(iter)) {
        struct mailimap_body * body = (struct mailimap_body *) clist_content(iter);
        array->Set(idx, bodyToV8(body));
        idx ++;
    }
    obj->Set(String::NewSymbol("subparts"), array);
    
    AddMultipartExtensions(obj, part->bd_ext_mpart);
    
    return scope.Close(obj);
}

static Handle<Value> bodyToV8(struct mailimap_body * body)
{
    switch (body->bd_type) {
        case MAILIMAP_BODY_1PART: {
            switch (body->bd_data.bd_body_1part->bd_type) {
                case MAILIMAP_BODY_TYPE_1PART_BASIC:
                case MAILIMAP_BODY_TYPE_1PART_TEXT:
                    return singlePartToV8(body->bd_data.bd_body_1part);
                case MAILIMAP_BODY_TYPE_1PART_MSG:
                    return messagePartBodyToV8(body->bd_data.bd_body_1part);
            }
            break;
        }
        case MAILIMAP_BODY_MPART: {
            return multipartToV8(body->bd_data.bd_body_mpart);
        }
    }
    //TODO: assert
    return Handle<Value>(NULL);
}

Handle<Value> etpanjs::getFetchItemsFromResponse(const Arguments& args)
{
    HandleScope scope;
    
    Response * obj = ObjectWrap::Unwrap<Response>(args.This());
    struct mailimap_response * response = obj->getResponse();
    
    clist * fetchItems = clist_new();
    if (response->rsp_cont_req_or_resp_data_list != NULL) {
        for(clistiter * cur = clist_begin(response->rsp_cont_req_or_resp_data_list) ; cur != NULL ; cur = clist_next(cur)) {
            struct mailimap_cont_req_or_resp_data * cont_req_or_resp_data;
        
            cont_req_or_resp_data = (struct mailimap_cont_req_or_resp_data *) clist_content(cur);
            if (cont_req_or_resp_data->rsp_type == MAILIMAP_RESP_RESP_DATA) {
                struct mailimap_response_data * resp_data;

                resp_data = cont_req_or_resp_data->rsp_data.rsp_resp_data;
                if (resp_data->rsp_type == MAILIMAP_RESP_DATA_TYPE_MESSAGE_DATA) {
                    struct mailimap_message_data * msg_data;
                
                    msg_data = resp_data->rsp_data.rsp_message_data;
                    if (msg_data->mdt_type == MAILIMAP_MESSAGE_DATA_FETCH) {
                        clist_append(fetchItems, msg_data->mdt_msg_att);
                    }
                }
            }
        }
    }
    
    unsigned int count = clist_count(fetchItems);
    Handle<Array> array = Array::New(count);
    unsigned int idx = 0;
    for(clistiter * cur = clist_begin(fetchItems) ; cur != NULL ; cur = clist_next(cur)) {
        Local<Object> obj = Object::New();
        
        struct mailimap_msg_att * msgAtt = (struct mailimap_msg_att *) clist_content(cur);
        for(clistiter * attIterator = clist_begin(msgAtt->att_list) ; attIterator != NULL ; attIterator = clist_next(attIterator)) {
            struct mailimap_msg_att_item * item = (struct mailimap_msg_att_item *) clist_content(attIterator);
            
            switch (item->att_type) {
                case MAILIMAP_MSG_ATT_ITEM_DYNAMIC: {
                    MessageFlag flags = flags_from_lep_att_dynamic(item->att_data.att_dyn);
                    obj->Set(String::NewSymbol("flags"), Integer::New(flags));
                    break;
                }
                case MAILIMAP_MSG_ATT_ITEM_STATIC: {
                    switch (item->att_data.att_static->att_type) {
                        case MAILIMAP_MSG_ATT_ENVELOPE: {
                            obj->Set(String::NewSymbol("env"), envelopeToV8(item->att_data.att_static->att_data.att_env));
                            break;
                        }
                        case MAILIMAP_MSG_ATT_INTERNALDATE: {
                            obj->Set(String::NewSymbol("receivedDate"), Integer::New(item->att_data.att_static->att_data.att_uid));
                            break;
                        }
                        case MAILIMAP_MSG_ATT_RFC822: {
                            Buffer * buffer = Buffer::New(item->att_data.att_static->att_data.att_rfc822.att_content, item->att_data.att_static->att_data.att_rfc822.att_length);
                            obj->Set(String::NewSymbol("rfc822"), buffer->handle_);
                            break;
                        }
                        case MAILIMAP_MSG_ATT_RFC822_HEADER: {
                            Buffer * buffer = Buffer::New(item->att_data.att_static->att_data.att_rfc822_header.att_content, item->att_data.att_static->att_data.att_rfc822_header.att_length);
                            obj->Set(String::NewSymbol("header"), buffer->handle_);
                            break;
                        }
                        case MAILIMAP_MSG_ATT_RFC822_TEXT: {
                            Buffer * buffer = Buffer::New(item->att_data.att_static->att_data.att_rfc822_text.att_content, item->att_data.att_static->att_data.att_rfc822_text.att_length);
                            obj->Set(String::NewSymbol("text"), buffer->handle_);
                            break;
                        }
                        case MAILIMAP_MSG_ATT_RFC822_SIZE: {
                            obj->Set(String::NewSymbol("size"), Integer::New(item->att_data.att_static->att_data.att_rfc822_size));
                            break;
                        }
                        case MAILIMAP_MSG_ATT_BODY: {
                            obj->Set(String::NewSymbol("bodystructure"), bodyToV8(item->att_data.att_static->att_data.att_body));
                            break;
                        }
                        case MAILIMAP_MSG_ATT_BODYSTRUCTURE: {
                            obj->Set(String::NewSymbol("bodystructure"), bodyToV8(item->att_data.att_static->att_data.att_bodystructure));
                            break;
                        }
                        case MAILIMAP_MSG_ATT_BODY_SECTION: {
                            Buffer * buffer = Buffer::New(item->att_data.att_static->att_data.att_body_section->sec_body_part, item->att_data.att_static->att_data.att_body_section->sec_length);
                            obj->Set(String::NewSymbol("bodysection"), buffer->handle_);
                            break;
                        }
                        case MAILIMAP_MSG_ATT_UID: {
                            //fprintf(stderr, "uid: %i\n", item->att_data.att_static->att_data.att_uid);
                            obj->Set(String::NewSymbol("uid"), Integer::New(item->att_data.att_static->att_data.att_uid));
                            break;
                        }
                    }
                }
                case MAILIMAP_MSG_ATT_ITEM_EXTENSION: {
                    mailimap_extension_data * ext_data = item->att_data.att_extension_data;
                    if (ext_data->ext_extension == &mailimap_extension_condstore) {
                        struct mailimap_condstore_fetch_mod_resp * fetch_data = (struct mailimap_condstore_fetch_mod_resp *) ext_data->ext_data;
                        char valueString[32];
                        snprintf(valueString, sizeof(valueString), "%llu", (unsigned long long) fetch_data->cs_modseq_value);
                        obj->Set(String::NewSymbol("modseq"), String::New(valueString));
                    }
                    else if (ext_data->ext_extension == &mailimap_extension_xgmlabels) {
                        struct mailimap_msg_att_xgmlabels * att = (struct mailimap_msg_att_xgmlabels *) ext_data->ext_data;
                        int count = clist_count(att->att_labels);
                        if (count > 0) {
                            Handle<Array> labels = Array::New(count);
                            int labelIdx = 0;
                            for(clistiter * iter = clist_begin(att->att_labels) ; iter != NULL ; iter = clist_next(iter)) {
                                char * label = (char *) clist_content(iter);
                                labels->Set(labelIdx, String::New(label));
                                labelIdx ++;
                            }
                            obj->Set(String::NewSymbol("gmailLabels"), labels);
                        }
                    }
                    else if (ext_data->ext_extension == &mailimap_extension_xgmmsgid) {
                        uint64_t * p_msgid = (uint64_t *) ext_data->ext_data;
                        char valueString[32];
                        snprintf(valueString, sizeof(valueString), "%llu", (unsigned long long) * p_msgid);
                        obj->Set(String::NewSymbol("gmailMsgID"), String::New(valueString));
                    }
                    else if (ext_data->ext_extension == &mailimap_extension_xgmthrid) {
                        uint64_t * p_thrid = (uint64_t *) ext_data->ext_data;
                        char valueString[32];
                        snprintf(valueString, sizeof(valueString), "%llu", (unsigned long long) * p_thrid);
                        obj->Set(String::NewSymbol("gmailThreadID"), String::New(valueString));
                    }
                    break;
                }
            }
        }
        
        array->Set(idx, obj);
        idx ++;
    }
    return scope.Close(array);
}

enum IMAPFolderFlag {
    IMAPFolderFlagNone        = 0,
    IMAPFolderFlagMarked      = 1 << 0,
    IMAPFolderFlagUnmarked    = 1 << 1,
    IMAPFolderFlagNoSelect    = 1 << 2,
    IMAPFolderFlagNoInferiors = 1 << 3,
    IMAPFolderFlagInbox       = 1 << 4,
    IMAPFolderFlagSentMail    = 1 << 5,
    IMAPFolderFlagStarred     = 1 << 6,
    IMAPFolderFlagAllMail     = 1 << 7,
    IMAPFolderFlagTrash       = 1 << 8,
    IMAPFolderFlagDrafts      = 1 << 9,
    IMAPFolderFlagSpam        = 1 << 10,
    IMAPFolderFlagImportant   = 1 << 11,
    IMAPFolderFlagArchive     = 1 << 12,
    IMAPFolderFlagAll = IMAPFolderFlagAllMail,
    IMAPFolderFlagJunk = IMAPFolderFlagSpam,
    IMAPFolderFlagFlagged = IMAPFolderFlagStarred,
};

static struct {
    const char * name;
    int flag;
} mb_keyword_flag[] = {
    {"Inbox",     IMAPFolderFlagInbox},
    {"AllMail",   IMAPFolderFlagAllMail},
    {"Sent",      IMAPFolderFlagSentMail},
    {"Spam",      IMAPFolderFlagSpam},
    {"Starred",   IMAPFolderFlagStarred},
    {"Trash",     IMAPFolderFlagTrash},
    {"Important", IMAPFolderFlagImportant},
    {"Drafts",    IMAPFolderFlagDrafts},
    {"Archive",   IMAPFolderFlagArchive},
    {"All",       IMAPFolderFlagAll},
    {"Junk",      IMAPFolderFlagJunk},
    {"Flagged",   IMAPFolderFlagFlagged},
};

static int imap_mailbox_flags_to_flags(struct mailimap_mbx_list_flags * imap_flags)
{
    int flags;
    clistiter * cur;
    
    flags = 0;
    if (imap_flags->mbf_type == MAILIMAP_MBX_LIST_FLAGS_SFLAG) {
        switch (imap_flags->mbf_sflag) {
            case MAILIMAP_MBX_LIST_SFLAG_MARKED:
                flags |= IMAPFolderFlagMarked;
                break;
            case MAILIMAP_MBX_LIST_SFLAG_NOSELECT:
                flags |= IMAPFolderFlagNoSelect;
                break;
            case MAILIMAP_MBX_LIST_SFLAG_UNMARKED:
                flags |= IMAPFolderFlagUnmarked;
                break;
        }
    }
    
    for(cur = clist_begin(imap_flags->mbf_oflags) ; cur != NULL ;
        cur = clist_next(cur)) {
        struct mailimap_mbx_list_oflag * oflag;
        
        oflag = (struct mailimap_mbx_list_oflag *) clist_content(cur);
        
        switch (oflag->of_type) {
            case MAILIMAP_MBX_LIST_OFLAG_NOINFERIORS:
                flags |= IMAPFolderFlagNoInferiors;
                break;
                
            case MAILIMAP_MBX_LIST_OFLAG_FLAG_EXT:
                for(unsigned int i = 0 ; i < sizeof(mb_keyword_flag) / sizeof(mb_keyword_flag[0]) ; i ++) {
                    if (strcasecmp(mb_keyword_flag[i].name, oflag->of_flag_ext) == 0) {
                        flags |= mb_keyword_flag[i].flag;
                    }
                }
                break;
        }
    }
    
    return flags;
}

static MessageFlag flag_from_lep(struct mailimap_flag * flag)
{
    switch (flag->fl_type) {
        case MAILIMAP_FLAG_ANSWERED:
            return MessageFlagAnswered;
        case MAILIMAP_FLAG_FLAGGED:
            return MessageFlagFlagged;
        case MAILIMAP_FLAG_DELETED:
            return MessageFlagDeleted;
        case MAILIMAP_FLAG_SEEN:
            return MessageFlagSeen;
        case MAILIMAP_FLAG_DRAFT:
            return MessageFlagDraft;
        case MAILIMAP_FLAG_KEYWORD:
            if (strcasecmp(flag->fl_data.fl_keyword, "$Forwarded") == 0) {
                return MessageFlagForwarded;
            }
            else if (strcasecmp(flag->fl_data.fl_keyword, "$MDNSent") == 0) {
                return MessageFlagMDNSent;
            }
            else if (strcasecmp(flag->fl_data.fl_keyword, "$SubmitPending") == 0) {
                return MessageFlagSubmitPending;
            }
            else if (strcasecmp(flag->fl_data.fl_keyword, "$Submitted") == 0) {
                return MessageFlagSubmitted;
            }
    }
    
    return MessageFlagNone;
}

static MessageFlag flags_from_lep_att_dynamic(struct mailimap_msg_att_dynamic * att_dynamic)
{
    MessageFlag flags;
    clistiter * iter;
    
    if (att_dynamic->att_list == NULL)
        return MessageFlagNone;
    
    flags = MessageFlagNone;
    for(iter = clist_begin(att_dynamic->att_list) ;iter != NULL ; iter = clist_next(iter)) {
        struct mailimap_flag_fetch * flag_fetch;
        struct mailimap_flag * flag;
        
        flag_fetch = (struct mailimap_flag_fetch *) clist_content(iter);
        if (flag_fetch->fl_type != MAILIMAP_FLAG_FETCH_OTHER) {
            continue;
        }
        
        flag = flag_fetch->fl_flag;
        flags = (MessageFlag) (flags | flag_from_lep(flag));
    }
    
    return flags;
}

enum IMAPCapability {
    IMAPCapabilityACL,
    IMAPCapabilityBinary,
    IMAPCapabilityCatenate,
    IMAPCapabilityChildren,
    IMAPCapabilityCompressDeflate,
    IMAPCapabilityCondstore,
    IMAPCapabilityEnable,
    IMAPCapabilityIdle,
    IMAPCapabilityId,
    IMAPCapabilityLiteralPlus,
    IMAPCapabilityMultiAppend,
    IMAPCapabilityNamespace,
    IMAPCapabilityQResync,
    IMAPCapabilityQuota,
    IMAPCapabilitySort,
    IMAPCapabilityStartTLS,
    IMAPCapabilityThreadOrderedSubject,
    IMAPCapabilityThreadReferences,
    IMAPCapabilityUIDPlus,
    IMAPCapabilityUnselect,
    IMAPCapabilityXList,
    IMAPCapabilityAuthAnonymous,
    IMAPCapabilityAuthCRAMMD5,
    IMAPCapabilityAuthDigestMD5,
    IMAPCapabilityAuthExternal,
    IMAPCapabilityAuthGSSAPI,
    IMAPCapabilityAuthKerberosV4,
    IMAPCapabilityAuthLogin,
    IMAPCapabilityAuthNTLM,
    IMAPCapabilityAuthOTP,
    IMAPCapabilityAuthPlain,
    IMAPCapabilityAuthSKey,
    IMAPCapabilityAuthSRP,
    IMAPCapabilityXOAuth2,
    IMAPCapabilityGmail,
};

struct {
    const char * name;
    IMAPCapability value;
} capability_names[] = {
    {"ACL", IMAPCapabilityACL},
    {"BINARY", IMAPCapabilityBinary},
    {"CATENATE", IMAPCapabilityCatenate},
    {"CHILDREN", IMAPCapabilityChildren},
    {"COMPRESS", IMAPCapabilityCompressDeflate},
    {"CONDSTORE", IMAPCapabilityCondstore},
    {"ENABLE", IMAPCapabilityEnable},
    {"IDLE", IMAPCapabilityIdle},
    {"ID", IMAPCapabilityId},
    {"LITERAL+", IMAPCapabilityLiteralPlus},
    {"MULTIAPPEND", IMAPCapabilityMultiAppend},
    {"NAMESPACE", IMAPCapabilityNamespace},
    {"QRESYNC", IMAPCapabilityQResync},
    {"QUOTA", IMAPCapabilityQuota},
    {"SORT", IMAPCapabilitySort},
    {"STARTTLS", IMAPCapabilityStartTLS},
    {"THREAD=ORDEREDSUBJECT", IMAPCapabilityThreadOrderedSubject},
    {"THREAD=REFERENCES", IMAPCapabilityThreadReferences},
    {"UIDPLUS", IMAPCapabilityUIDPlus},
    {"UNSELECT", IMAPCapabilityUnselect},
    {"XLIST", IMAPCapabilityXList},
    {"X-GM-EXT-1", IMAPCapabilityGmail},
};

struct {
    const char * name;
    IMAPCapability value;
} capability_auth_names[] = {
    {"ANONYMOUS", IMAPCapabilityAuthAnonymous},
    {"CRAM-MD5", IMAPCapabilityAuthCRAMMD5},
    {"DIGEST-MD5", IMAPCapabilityAuthDigestMD5},
    {"EXTERNAL", IMAPCapabilityAuthExternal},
    {"GSSAPI", IMAPCapabilityAuthGSSAPI},
    {"KERBEROS_V4", IMAPCapabilityAuthKerberosV4},
    {"LOGIN", IMAPCapabilityAuthLogin},
    {"NTLM", IMAPCapabilityAuthNTLM},
    {"OTP", IMAPCapabilityAuthOTP},
    {"PLAIN", IMAPCapabilityAuthPlain},
    {"SKEY", IMAPCapabilityAuthSKey},
    {"SRP", IMAPCapabilityAuthSRP},
    {"XOAUTH2", IMAPCapabilityXOAuth2},
};

Handle<Value> etpanjs::getCapabilitiesFromResponse(const Arguments& args)
{
    HandleScope scope;
    
    Response * obj = ObjectWrap::Unwrap<Response>(args.This());
    struct mailimap_capability_data * imap_capability = NULL;
    if (obj->getResponse() != NULL) {
        struct mailimap_response * response = obj->getResponse();
    
        if (response->rsp_cont_req_or_resp_data_list != NULL) {
            for(clistiter * cur = clist_begin(response->rsp_cont_req_or_resp_data_list) ; cur != NULL ; cur = clist_next(cur)) {
                struct mailimap_cont_req_or_resp_data * cont_req_or_resp_data;
        
                cont_req_or_resp_data = (struct mailimap_cont_req_or_resp_data *) clist_content(cur);
                if (cont_req_or_resp_data->rsp_type == MAILIMAP_RESP_RESP_DATA) {
                    struct mailimap_response_data * resp_data;

                    resp_data = cont_req_or_resp_data->rsp_data.rsp_resp_data;

                    switch (resp_data->rsp_type) {
                        case MAILIMAP_RESP_DATA_TYPE_CAPABILITY_DATA:
                        if (imap_capability == NULL) {
                            imap_capability = resp_data->rsp_data.rsp_capability_data;
                        }
                        break;
                    }
                }
            }
        }
        if (response->rsp_resp_done->rsp_type == MAILIMAP_RESP_DONE_TYPE_TAGGED) {
            if (response->rsp_resp_done->rsp_data.rsp_tagged->rsp_cond_state->rsp_text != NULL) {
                if (response->rsp_resp_done->rsp_data.rsp_tagged->rsp_cond_state->rsp_text->rsp_code != NULL) {
                    if (response->rsp_resp_done->rsp_data.rsp_tagged->rsp_cond_state->rsp_text->rsp_code->rc_type == MAILIMAP_RESP_TEXT_CODE_CAPABILITY_DATA) {
                        if (imap_capability == NULL) {
                            imap_capability = response->rsp_resp_done->rsp_data.rsp_tagged->rsp_cond_state->rsp_text->rsp_code->rc_data.rc_cap_data;
                        }
                    }
                }
            }
        }
    }
    else if (obj->getGreeting() != NULL) {
        struct mailimap_greeting * greeting = obj->getGreeting();
        
        if (greeting->gr_data.gr_auth != NULL) {
            if (greeting->gr_data.gr_auth->rsp_text->rsp_code != NULL) {
                if (greeting->gr_data.gr_auth->rsp_text->rsp_code->rc_type == MAILIMAP_RESP_TEXT_CODE_CAPABILITY_DATA) {
                    if (imap_capability == NULL) {
                        imap_capability = greeting->gr_data.gr_auth->rsp_text->rsp_code->rc_data.rc_cap_data;
                    }
                }
            }
        }
    }
    else if (obj->getContReq() != NULL) {
        struct mailimap_continue_req * contreq = obj->getContReq();
        
        if (contreq->cr_type == MAILIMAP_CONTINUE_REQ_TEXT) {
            if (contreq->cr_data.cr_text != NULL) {
                if (contreq->cr_data.cr_text->rsp_code != NULL) {
                    if (contreq->cr_data.cr_text->rsp_code->rc_type == MAILIMAP_RESP_TEXT_CODE_CAPABILITY_DATA) {
                        if (imap_capability == NULL) {
                            imap_capability = contreq->cr_data.cr_text->rsp_code->rc_data.rc_cap_data;
                        }
                    }
                }
            }
        }
    }
    
    if (imap_capability == NULL) {
        return Handle<Value>();
    }
    
    int count = 0;
    for(clistiter * iter = clist_begin(imap_capability->cap_list) ; iter != NULL ; iter = clist_next(iter)) {
        struct mailimap_capability * cap = (struct mailimap_capability *) clist_content(iter);
        switch (cap->cap_type) {
            case MAILIMAP_CAPABILITY_AUTH_TYPE: {
                //fprintf(stderr, "auth: %s\n", cap->cap_data.cap_auth_type);
                for(unsigned int i = 0 ; i < sizeof(capability_auth_names) / sizeof(capability_auth_names[0]) ; i ++) {
                    if (strcmp(cap->cap_data.cap_auth_type, capability_auth_names[i].name) == 0) {
                        count ++;
                    }
                }
                break;
            }
            case MAILIMAP_CAPABILITY_NAME: {
                //fprintf(stderr, "cap: %s\n", cap->cap_data.cap_name);
                for(unsigned int i = 0 ; i < sizeof(capability_names) / sizeof(capability_names[0]) ; i ++) {
                    if (strcmp(cap->cap_data.cap_name, capability_names[i].name) == 0) {
                        count ++;
                    }
                }
                break;
            }
        }
    }
    Handle<Array> array = Array::New(count);
    int idx = 0;
    for(clistiter * iter = clist_begin(imap_capability->cap_list) ; iter != NULL ; iter = clist_next(iter)) {
        struct mailimap_capability * cap = (struct mailimap_capability *) clist_content(iter);
        switch (cap->cap_type) {
            case MAILIMAP_CAPABILITY_AUTH_TYPE: {
                for(unsigned int i = 0 ; i < sizeof(capability_auth_names) / sizeof(capability_auth_names[0]) ; i ++) {
                    if (strcmp(cap->cap_data.cap_auth_type, capability_auth_names[i].name) == 0) {
                        array->Set(idx, Integer::New(capability_auth_names[i].value));
                        idx ++;
                    }
                }
                break;
            }
            case MAILIMAP_CAPABILITY_NAME: {
                for(unsigned int i = 0 ; i < sizeof(capability_names) / sizeof(capability_names[0]) ; i ++) {
                    if (strcmp(cap->cap_data.cap_name, capability_names[i].name) == 0) {
                        array->Set(idx, Integer::New(capability_names[i].value));
                        idx ++;
                    }
                }
                break;
            }
        }
    }
    return scope.Close(array);
}

Handle<Value> imap_set_to_range(struct mailimap_set * uids)
{
    HandleScope scope;
    int count = clist_count(uids->set_list);
    Handle<Array> array = Array::New(count);
    int idx = 0;
    for(clistiter * iter = clist_begin(uids->set_list) ; iter != NULL ; iter = clist_next(iter)) {
        Handle<Array> resultItem = Array::New(2);
        struct mailimap_set_item * item = (struct mailimap_set_item *) clist_content(iter);
        resultItem->Set(0, Integer::New(item->set_first));
        resultItem->Set(1, Integer::New(item->set_last));
        
        array->Set(idx, resultItem);
        idx ++;
    }
    return scope.Close(array);
}

Handle<Value> etpanjs::getUIDPlusCopyResponseFromResponse(const Arguments& args)
{
    HandleScope scope;
    struct mailimap_uidplus_resp_code_copy * copy_info = NULL;
    
    Response * obj = ObjectWrap::Unwrap<Response>(args.This());
    if (obj->getResponse() != NULL) {
        struct mailimap_response * response = obj->getResponse();
        
        if (response->rsp_resp_done->rsp_type == MAILIMAP_RESP_DONE_TYPE_TAGGED) {
            if (response->rsp_resp_done->rsp_data.rsp_tagged->rsp_cond_state->rsp_text != NULL) {
                if (response->rsp_resp_done->rsp_data.rsp_tagged->rsp_cond_state->rsp_text->rsp_code != NULL) {
                    if (response->rsp_resp_done->rsp_data.rsp_tagged->rsp_cond_state->rsp_text->rsp_code->rc_type == MAILIMAP_RESP_TEXT_CODE_EXTENSION) {
                        struct mailimap_extension_data * data = response->rsp_resp_done->rsp_data.rsp_tagged->rsp_cond_state->rsp_text->rsp_code->rc_data.rc_ext_data;
                        if (data->ext_extension == &mailimap_extension_uidplus) {
                            if (data->ext_type == MAILIMAP_UIDPLUS_RESP_CODE_COPY) {
                                copy_info = (struct mailimap_uidplus_resp_code_copy *) data->ext_data;
                            }
                        }
                    }
                }
            }
        }
    }
    
    if (copy_info == NULL) {
        return Handle<Value>();
    }
    
    Local<Object> result = Object::New();
    result->Set(String::NewSymbol("uidvalidity"), Integer::New(copy_info->uid_uidvalidity));
    result->Set(String::NewSymbol("sourceUids"), imap_set_to_range(copy_info->uid_source_set));
    result->Set(String::NewSymbol("destUids"), imap_set_to_range(copy_info->uid_dest_set));
    return scope.Close(result);
}

Handle<Value> etpanjs::getUIDPlusAppendResponseFromResponse(const Arguments& args)
{
    HandleScope scope;
    struct mailimap_uidplus_resp_code_apnd * append_info = NULL;
    
    Response * obj = ObjectWrap::Unwrap<Response>(args.This());
    if (obj->getResponse() != NULL) {
        struct mailimap_response * response = obj->getResponse();
        
        if (response->rsp_resp_done->rsp_type == MAILIMAP_RESP_DONE_TYPE_TAGGED) {
            if (response->rsp_resp_done->rsp_data.rsp_tagged->rsp_cond_state->rsp_text != NULL) {
                if (response->rsp_resp_done->rsp_data.rsp_tagged->rsp_cond_state->rsp_text->rsp_code != NULL) {
                    if (response->rsp_resp_done->rsp_data.rsp_tagged->rsp_cond_state->rsp_text->rsp_code->rc_type == MAILIMAP_RESP_TEXT_CODE_EXTENSION) {
                        struct mailimap_extension_data * data = response->rsp_resp_done->rsp_data.rsp_tagged->rsp_cond_state->rsp_text->rsp_code->rc_data.rc_ext_data;
                        if (data->ext_extension == &mailimap_extension_uidplus) {
                            if (data->ext_type == MAILIMAP_UIDPLUS_RESP_CODE_APND) {
                                append_info = (struct mailimap_uidplus_resp_code_apnd *) data->ext_data;
                            }
                        }
                    }
                }
            }
        }
    }
    
    if (append_info == NULL) {
        return Handle<Value>();
    }
    
    Local<Object> result = Object::New();
    result->Set(String::NewSymbol("uidvalidity"), Integer::New(append_info->uid_uidvalidity));
    result->Set(String::NewSymbol("appendUids"), imap_set_to_range(append_info->uid_set));
    return scope.Close(result);
}

Handle<Value> etpanjs::getStatusResponseFromResponse(const Arguments& args)
{
    HandleScope scope;
    
    Response * obj = ObjectWrap::Unwrap<Response>(args.This());
    struct mailimap_response * response = obj->getResponse();
    struct mailimap_mailbox_data_status * statusData = NULL;
    
    if (response->rsp_cont_req_or_resp_data_list != NULL) {
        for(clistiter * cur = clist_begin(response->rsp_cont_req_or_resp_data_list) ; cur != NULL ; cur = clist_next(cur)) {
            struct mailimap_cont_req_or_resp_data * cont_req_or_resp_data;
        
            cont_req_or_resp_data = (struct mailimap_cont_req_or_resp_data *) clist_content(cur);
            if (cont_req_or_resp_data->rsp_type == MAILIMAP_RESP_RESP_DATA) {
                struct mailimap_response_data * resp_data;

                resp_data = cont_req_or_resp_data->rsp_data.rsp_resp_data;
                if (resp_data->rsp_type == MAILIMAP_RESP_DATA_TYPE_MAILBOX_DATA) {
                    struct mailimap_mailbox_data * mb_data;
                
                    mb_data = resp_data->rsp_data.rsp_mailbox_data;
                    if (mb_data->mbd_type == MAILIMAP_MAILBOX_DATA_STATUS) {
                        statusData = mb_data->mbd_data.mbd_status;
                    }
                }
            }
        }
    }
    
    if (statusData == NULL) {
        return Handle<Value>();
    }
    
    Local<Object> result = Object::New();
    for(clistiter * iter = clist_begin(statusData->st_info_list) ; iter != NULL ; iter = clist_next(iter)) {
        struct mailimap_status_info * info = (struct mailimap_status_info *) clist_content(iter);
        switch (info->st_att) {
            case MAILIMAP_STATUS_ATT_MESSAGES: {
                result->Set(String::NewSymbol("messages"), Integer::New(info->st_value));
                break;
            }
            case MAILIMAP_STATUS_ATT_RECENT: {
                result->Set(String::NewSymbol("recent"), Integer::New(info->st_value));
                break;
            }
            case MAILIMAP_STATUS_ATT_UIDNEXT: {
                result->Set(String::NewSymbol("uidnext"), Integer::New(info->st_value));
                break;
            }
            case MAILIMAP_STATUS_ATT_UIDVALIDITY: {
                result->Set(String::NewSymbol("uidvalidity"), Integer::New(info->st_value));
                break;
            }
            case MAILIMAP_STATUS_ATT_UNSEEN: {
                result->Set(String::NewSymbol("unseen"), Integer::New(info->st_value));
                break;
            }
            case MAILIMAP_STATUS_ATT_EXTENSION: {
                struct mailimap_extension_data * data = info->st_ext_data;
                if (data->ext_extension == &mailimap_extension_condstore) {
                    struct mailimap_condstore_status_info * status_info = (struct mailimap_condstore_status_info *) data->ext_data;
                    char value_string[32];
                    snprintf(value_string, sizeof(value_string), "%llu", (unsigned long long) status_info->cs_highestmodseq_value);
                    result->Set(String::NewSymbol("highestmodseq"), String::New(value_string));
                }
                break;
            }
        }
    }
    return scope.Close(result);
}

Handle<Value> etpanjs::getIDResponseFromResponse(const Arguments& args)
{
    HandleScope scope;
    
    Response * obj = ObjectWrap::Unwrap<Response>(args.This());
    struct mailimap_response * response = obj->getResponse();
    struct mailimap_id_params_list * server_info = NULL;
    
    if (response->rsp_cont_req_or_resp_data_list != NULL) {
        for(clistiter * cur = clist_begin(response->rsp_cont_req_or_resp_data_list) ; cur != NULL ; cur = clist_next(cur)) {
            struct mailimap_cont_req_or_resp_data * cont_req_or_resp_data;
        
            cont_req_or_resp_data = (struct mailimap_cont_req_or_resp_data *) clist_content(cur);
            if (cont_req_or_resp_data->rsp_type == MAILIMAP_RESP_RESP_DATA) {
                struct mailimap_response_data * resp_data;

                resp_data = cont_req_or_resp_data->rsp_data.rsp_resp_data;
                if (resp_data->rsp_type == MAILIMAP_RESP_DATA_TYPE_EXTENSION_DATA) {
                    struct mailimap_extension_data * ext_data;
                
                    ext_data = resp_data->rsp_data.rsp_extension_data;
                    if (ext_data->ext_extension == &mailimap_extension_id) {
                        server_info = (struct mailimap_id_params_list *) ext_data->ext_data;
                    }
                }
            }
        }
    }
    
    if (server_info == NULL) {
        return Handle<Value>();
    }
    
    Local<Object> result = Object::New();
    for(clistiter * iter = clist_begin(server_info->idpa_list) ; iter != NULL ; iter = clist_next(iter)) {
        struct mailimap_id_param * info = (struct mailimap_id_param *) clist_content(iter);
        result->Set(String::New(info->idpa_name), String::New(info->idpa_value));
    }
    
    return scope.Close(result);
}

Handle<Value> etpanjs::getSelectResponseFromResponse(const Arguments& args)
{
    HandleScope scope;
    
    Response * obj = ObjectWrap::Unwrap<Response>(args.This());
    struct mailimap_response * response = obj->getResponse();
    
    Local<Object> result = Object::New();
    
    if (response->rsp_cont_req_or_resp_data_list != NULL) {
        for(clistiter * cur = clist_begin(response->rsp_cont_req_or_resp_data_list) ; cur != NULL ; cur = clist_next(cur)) {
            struct mailimap_cont_req_or_resp_data * cont_req_or_resp_data;
        
            cont_req_or_resp_data = (struct mailimap_cont_req_or_resp_data *) clist_content(cur);
            if (cont_req_or_resp_data->rsp_type == MAILIMAP_RESP_RESP_DATA) {
                struct mailimap_response_data * resp_data;

                resp_data = cont_req_or_resp_data->rsp_data.rsp_resp_data;
                if (resp_data->rsp_type == MAILIMAP_RESP_DATA_TYPE_MAILBOX_DATA) {
                    struct mailimap_mailbox_data * mb_data;
                
                    mb_data = resp_data->rsp_data.rsp_mailbox_data;
                    if (mb_data->mbd_type == MAILIMAP_MAILBOX_DATA_EXISTS) {
                        result->Set(String::NewSymbol("exists"), Integer::New(mb_data->mbd_data.mbd_exists));
                    }
                }
            }
        }
    }
    if (response->rsp_resp_done->rsp_type == MAILIMAP_RESP_DONE_TYPE_TAGGED) {
        if (response->rsp_resp_done->rsp_data.rsp_tagged->rsp_cond_state->rsp_text != NULL) {
            if (response->rsp_resp_done->rsp_data.rsp_tagged->rsp_cond_state->rsp_text->rsp_code != NULL) {
                if (response->rsp_resp_done->rsp_data.rsp_tagged->rsp_cond_state->rsp_text->rsp_code->rc_type == MAILIMAP_RESP_TEXT_CODE_UIDNEXT) {
                    result->Set(String::NewSymbol("uidnext"), Integer::New(response->rsp_resp_done->rsp_data.rsp_tagged->rsp_cond_state->rsp_text->rsp_code->rc_data.rc_uidnext));
                }
                else if (response->rsp_resp_done->rsp_data.rsp_tagged->rsp_cond_state->rsp_text->rsp_code->rc_type == MAILIMAP_RESP_TEXT_CODE_UIDVALIDITY) {
                    result->Set(String::NewSymbol("uidvalidity"), Integer::New(response->rsp_resp_done->rsp_data.rsp_tagged->rsp_cond_state->rsp_text->rsp_code->rc_data.rc_uidvalidity));
                }
            }
        }
    }
    
    return scope.Close(result);
}

Handle<Value> etpanjs::getNoopResponseFromResponse(const Arguments& args)
{
    HandleScope scope;
    
    Response * obj = ObjectWrap::Unwrap<Response>(args.This());
    struct mailimap_response * response = obj->getResponse();
    
    Local<Object> result = Object::New();
    
    if (response->rsp_cont_req_or_resp_data_list != NULL) {
        for(clistiter * cur = clist_begin(response->rsp_cont_req_or_resp_data_list) ; cur != NULL ; cur = clist_next(cur)) {
            struct mailimap_cont_req_or_resp_data * cont_req_or_resp_data;
        
            cont_req_or_resp_data = (struct mailimap_cont_req_or_resp_data *) clist_content(cur);
            if (cont_req_or_resp_data->rsp_type == MAILIMAP_RESP_RESP_DATA) {
                struct mailimap_response_data * resp_data;

                resp_data = cont_req_or_resp_data->rsp_data.rsp_resp_data;
                if (resp_data->rsp_type == MAILIMAP_RESP_DATA_TYPE_MAILBOX_DATA) {
                    struct mailimap_mailbox_data * mb_data;
                
                    mb_data = resp_data->rsp_data.rsp_mailbox_data;
                    if (mb_data->mbd_type == MAILIMAP_MAILBOX_DATA_EXISTS) {
                        result->Set(String::NewSymbol("exists"), Integer::New(mb_data->mbd_data.mbd_exists));
                    }
                }
            }
        }
    }
    
    return scope.Close(result);
}

Handle<Value> etpanjs::getSearchResponseFromResponse(const Arguments& args)
{
    HandleScope scope;
    
    Response * obj = ObjectWrap::Unwrap<Response>(args.This());
    struct mailimap_response * response = obj->getResponse();
    clist * search_result = NULL;
    
    if (response->rsp_cont_req_or_resp_data_list != NULL) {
        for(clistiter * cur = clist_begin(response->rsp_cont_req_or_resp_data_list) ; cur != NULL ; cur = clist_next(cur)) {
            struct mailimap_cont_req_or_resp_data * cont_req_or_resp_data;
        
            cont_req_or_resp_data = (struct mailimap_cont_req_or_resp_data *) clist_content(cur);
            if (cont_req_or_resp_data->rsp_type == MAILIMAP_RESP_RESP_DATA) {
                struct mailimap_response_data * resp_data;

                resp_data = cont_req_or_resp_data->rsp_data.rsp_resp_data;
                if (resp_data->rsp_type == MAILIMAP_RESP_DATA_TYPE_MAILBOX_DATA) {
                    struct mailimap_mailbox_data * mb_data;
                
                    mb_data = resp_data->rsp_data.rsp_mailbox_data;
                    if (mb_data->mbd_type == MAILIMAP_MAILBOX_DATA_SEARCH) {
                        search_result = mb_data->mbd_data.mbd_search;
                    }
                }
            }
        }
    }
    
    if (search_result == NULL) {
        return Handle<Value>();
    }
    
    unsigned int count = clist_count(search_result);
    Handle<Array> array = Array::New(count);
    unsigned int i = 0;
    for(clistiter * iter = clist_begin(search_result) ; iter != NULL ; iter = clist_next(iter)) {
        uint32_t * value = (uint32_t *) clist_content(iter);
        array->Set(i, Integer::New(* value));
        i ++;
    }
    return scope.Close(array);
}
