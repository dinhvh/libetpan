#include <stdio.h>

#include "charset_detection_tests.h"

int main(void)
{
  if (charset_detection_test_run("data/input") != 0)
    return 1;
  printf("charset_detection_test: %lu fixtures checked\n",
      (unsigned long) charset_detection_test_count());
  return 0;
}
