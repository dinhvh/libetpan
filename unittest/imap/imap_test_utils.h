#ifndef IMAP_TEST_UTILS_H
#define IMAP_TEST_UTILS_H

#include <stdbool.h>

#include "mailimap_types.h"
#include "mailstream.h"
#include "mmapstring.h"

typedef int imap_test_sender(mailstream * stream, void * context);

int imap_test_parse_response_file(const char * path, bool compressed,
    struct mailimap_response ** result);

int imap_test_parse_response_data_file(const char * path, bool compressed,
    struct mailimap_response_data ** result);

void imap_test_expect_send_file(const char * path, imap_test_sender * sender,
    void * context);

mailstream * imap_test_stream_from_string(const char * input);
mailstream * imap_test_stream_from_string_with_output(const char * input,
    MMAPString ** output);

#endif
