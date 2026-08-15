#include <assert.h>
#include <stdio.h>

#include "imap_utf7_tests.h"

int main(void)
{
  size_t index;

  for (index = 0; index < imap_utf7_test_count(); index++)
    assert(imap_utf7_test_run(index, NULL, NULL) == 0);

  puts("imap_utf7_test: ok");
  return 0;
}
