#ifndef DATA_TYPES_TESTS_H
#define DATA_TYPES_TESTS_H

#include <stddef.h>

#include "test_case.h"

size_t data_types_test_count(void);
const char * data_types_test_name(size_t index);
int data_types_test_run_case(size_t index,
    test_failure_callback failure_callback, void * context);
int data_types_test_run(void);

#endif
