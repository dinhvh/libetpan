#ifndef MIME_BUILDER_HELPERS_H
#define MIME_BUILDER_HELPERS_H

#include "mailmime.h"

struct mailmime * mime_builder_new_part(int type, const char * content_type,
    struct mailmime_fields * fields);
struct mailmime_fields * mime_builder_new_body_fields(int encoding,
    int disposition_type, const char * filename, const char * content_id);
struct mailmime * mime_builder_new_text_part(const char * content_type,
    int encoding, const char * body);
struct mailmime * mime_builder_new_attachment_part(const char * filename,
    const char * content_id);
void mime_builder_add_part(struct mailmime * parent, struct mailmime * child);
struct mailmime * mime_builder_wrap_message(struct mailmime * body,
    const char * subject, int multiple_recipients);

#endif
