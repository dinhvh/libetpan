#include <stdio.h>

#include "imap_tests.h"

int main(void)
{
  imap_response_data_test_run();
  imap_response_done_test_run();
  imap_unsupported_response_test_run();
  imap_command_sender_test_run();
  imap_command_parameter_sender_test_run();

  puts("imap_test: ok");
  return 0;
}
