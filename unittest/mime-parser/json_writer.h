#ifndef MIME_PARSER_JSON_WRITER_H
#define MIME_PARSER_JSON_WRITER_H

#include <time.h>

#include "mmapstring.h"

struct json_writer {
  MMAPString * out;
};

void json_writer_append(struct json_writer * writer, const char * str);
void json_writer_comma_if_needed(struct json_writer * writer);
void json_writer_string(struct json_writer * writer, const char * str);
void json_writer_time(struct json_writer * writer, time_t value);

#endif
