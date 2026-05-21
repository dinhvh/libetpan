#include <stdio.h>

#include "mime_tests.h"

int main(void)
{
  mime_parser_test_run();

  puts("mime_test: ok");
  return 0;
}
