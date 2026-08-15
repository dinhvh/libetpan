#include <stdio.h>

#include "data_types_tests.h"

int main(void)
{
  if (data_types_test_run() != 0)
    return 1;
  puts("data_types_test: ok");
  return 0;
}
