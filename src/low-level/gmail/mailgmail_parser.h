/*
 * libEtPan! -- a mail stuff library
 */

#ifndef MAILGMAIL_PARSER_H

#define MAILGMAIL_PARSER_H

#include "mailgmail_types.h"

int mailgmail_parser_parse_error_message(const char * data, size_t len,
    char ** result);

int mailgmail_parser_parse_profile(const char * data, size_t len,
    struct mailgmail_profile ** result);

int mailgmail_parser_parse_label_list(const char * data, size_t len,
    struct mailgmail_label_list ** result);

int mailgmail_parser_parse_label(const char * data, size_t len,
    struct mailgmail_label ** result);

int mailgmail_parser_parse_message_list(const char * data, size_t len,
    struct mailgmail_message_list ** result);

int mailgmail_parser_parse_message(const char * data, size_t len,
    struct mailgmail_message ** result);

int mailgmail_parser_parse_attachment(const char * data, size_t len,
    struct mailgmail_attachment ** result);

#endif
