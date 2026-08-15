#ifndef IDLE_TEST_H
#define IDLE_TEST_H

#include <stddef.h>

#include "test_case.h"

size_t imap_idle_test_count(void);
const char * imap_idle_test_name(size_t index);
int imap_idle_test_run_case(size_t index,
    test_failure_callback failure_callback, void * context);
int imap_idle_test_run(void);

#endif
