#ifndef ACTIVESYNC_TESTS_H
#define ACTIVESYNC_TESTS_H

#include <stddef.h>

#include "test_case.h"

size_t activesync_wbxml_test_count(void);
const char * activesync_wbxml_test_name(size_t index);
int activesync_wbxml_test_run_case(size_t index,
    test_failure_callback failure_callback, void * context);
int activesync_wbxml_test_run(void);

size_t activesync_http_test_count(void);
const char * activesync_http_test_name(size_t index);
int activesync_http_test_run_case(size_t index,
    test_failure_callback failure_callback, void * context);
int activesync_http_test_run(void);

size_t activesync_sample_test_count(void);
const char * activesync_sample_test_name(size_t index);
int activesync_sample_test_run_case(size_t index,
    test_failure_callback failure_callback, void * context);
int activesync_sample_test_run(void);

#endif
