#ifndef PLAINTEXT_RENDERING_TESTS_H
#define PLAINTEXT_RENDERING_TESTS_H

#include "test_case.h"

int plaintext_rendering_test_run_case(const char * relative_path,
    const char * fixture_root, test_failure_callback failure_callback,
    void * context);
int plaintext_rendering_test_run(const char * fixture_root);

#endif
