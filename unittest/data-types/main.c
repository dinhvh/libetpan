#include <stdio.h>

#include "data_types_tests.h"

static void report_failure(const char * file, unsigned line,
    const char * expression, const char * message, void * context)
{
  const char * test_name;

  test_name = context;
  fprintf(stderr, "%s:%u: %s: %s (%s)\n", file, line, test_name, message,
      expression);
}

int main(void)
{
  size_t index;

  for (index = 0; index < data_types_test_count(); index++) {
    const char * name;

    name = data_types_test_name(index);
    if (data_types_test_run_case(index, report_failure, (void *) name) != 0)
      return 1;
  }
  puts("data_types_test: ok");
  return 0;
}
