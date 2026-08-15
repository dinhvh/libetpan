#ifndef COMMAND_SENDER_TEST_H
#define COMMAND_SENDER_TEST_H

#include <stddef.h>

#include "test_case.h"

size_t imap_command_sender_test_count(void);
const char * imap_command_sender_test_name(size_t index);
int imap_command_sender_test_run_case(size_t index, const char * fixture_root,
    test_failure_callback failure_callback, void * context);
int imap_command_sender_test_run(void);

#endif
