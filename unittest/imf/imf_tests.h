#ifndef IMF_TESTS_H
#define IMF_TESTS_H

#include <stddef.h>

#include "test_case.h"

size_t imf_parser_test_count(void);
const char * imf_parser_test_name(size_t index);
int imf_parser_test_run_case(size_t index, const char * fixture_root,
    test_failure_callback failure_callback, void * context);
int imf_parser_test_run(void);

#endif
