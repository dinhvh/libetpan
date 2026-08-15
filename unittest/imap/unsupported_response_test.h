#ifndef UNSUPPORTED_RESPONSE_TEST_H
#define UNSUPPORTED_RESPONSE_TEST_H

#include <stddef.h>

#include "test_case.h"

size_t imap_unsupported_response_test_count(void);
const char * imap_unsupported_response_test_name(size_t index);
int imap_unsupported_response_test_run_case(size_t index, int compressed,
    const char * fixture_root, test_failure_callback failure_callback,
    void * context);
int imap_unsupported_response_test_run(void);

#endif
