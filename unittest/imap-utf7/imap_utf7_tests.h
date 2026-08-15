#ifndef IMAP_UTF7_TESTS_H
#define IMAP_UTF7_TESTS_H

#include <stddef.h>

#include "test_case.h"

size_t imap_utf7_test_count(void);
const char * imap_utf7_test_name(size_t index);
int imap_utf7_test_run(size_t index, test_failure_callback failure_callback,
    void * context);

#endif
