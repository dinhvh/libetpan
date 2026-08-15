#ifndef TEST_CASE_H
#define TEST_CASE_H

typedef void (*test_failure_callback)(const char * file, unsigned line,
    const char * expression, const char * message, void * context);

#define TEST_CHECK(condition, message) \
  do { \
    if (!(condition)) { \
      if (failure_callback != NULL) \
        failure_callback(__FILE__, __LINE__, #condition, message, context); \
      result = -1; \
      goto cleanup; \
    } \
  } while (0)

#endif
