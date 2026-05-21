#ifndef MIME_PARSER_MIME_SERIALIZER_H
#define MIME_PARSER_MIME_SERIALIZER_H

#include "mailimf_types.h"
#include "mailmime.h"
#include "mmapstring.h"

void mime_serializer_set_context(const char * expected_json,
    struct mailimf_fields * header_fields);
MMAPString * mime_serializer_serialize_message(struct mailmime * mime);

#endif
