#ifndef MIME_PARSER_SERIALIZATION_TESTS_H
#define MIME_PARSER_SERIALIZATION_TESTS_H

#include "test_case.h"

int mime_parser_serialization_test_run_case(const char * relative_path,
    const char * fixture_root, test_failure_callback failure_callback,
    void * context);
int mime_parser_serialization_test_run(const char * fixture_root);

#endif
