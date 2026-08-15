#ifndef CHARSET_DETECTION_TESTS_H
#define CHARSET_DETECTION_TESTS_H

#include <stddef.h>

#include "test_case.h"

size_t charset_detection_test_count(void);
const char * charset_detection_test_name(size_t index);
int charset_detection_test_run_case(size_t index, const char * fixture_root,
    test_failure_callback failure_callback, void * context);
int charset_detection_test_run(const char * fixture_root);

#endif
