#ifndef MIME_BUILDER_TESTS_H
#define MIME_BUILDER_TESTS_H

#include <stddef.h>

#include "test_case.h"

size_t mime_builder_test_count(void);
const char * mime_builder_test_name(size_t index);
int mime_builder_test_run_case(size_t index, const char * fixture_root,
    test_failure_callback failure_callback, void * context);
int mime_builder_test_run(const char * fixture_root);

#endif
