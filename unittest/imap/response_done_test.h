#ifndef RESPONSE_DONE_TEST_H
#define RESPONSE_DONE_TEST_H

#include "test_case.h"

int imap_response_done_test_case(const char * fixture_root,
    const char * fixture_name, int compressed, int cond_type,
    unsigned expected_elements, test_failure_callback failure_callback,
    void * context);
int imap_response_done_test_run(void);

#endif
