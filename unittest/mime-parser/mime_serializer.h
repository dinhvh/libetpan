#ifndef MIME_PARSER_MIME_SERIALIZER_H
#define MIME_PARSER_MIME_SERIALIZER_H

#include "mailmime.h"
#include "mmapstring.h"

void mime_serializer_set_context(const char * original_data,
    const char * expected_json, const char * mime_version_name,
    const char * content_type);
MMAPString * mime_serializer_serialize_message(struct mailmime * mime);

#endif
