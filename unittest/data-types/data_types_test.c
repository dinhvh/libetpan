#include "data_types_tests.h"

struct test_group {
  size_t (* count)(void);
  const char * (* name)(size_t index);
  int (* run_case)(size_t index, test_failure_callback failure_callback,
      void * context);
};

static const struct test_group groups[] = {
  { md5_test_count, md5_test_name, md5_test_run_case },
  { base64_test_count, base64_test_name, base64_test_run_case },
  { carray_test_count, carray_test_name, carray_test_run_case },
  { mailstream_test_count, mailstream_test_name, mailstream_test_run_case },
};

size_t data_types_test_count(void)
{
  size_t count;
  size_t i;

  count = 0;
  for (i = 0; i < sizeof(groups) / sizeof(groups[0]); i++)
    count += groups[i].count();
  return count;
}

const char * data_types_test_name(size_t index)
{
  size_t i;

  for (i = 0; i < sizeof(groups) / sizeof(groups[0]); i++) {
    size_t count;

    count = groups[i].count();
    if (index < count)
      return groups[i].name(index);
    index -= count;
  }

  return NULL;
}

int data_types_test_run_case(size_t index,
    test_failure_callback failure_callback, void * context)
{
  size_t i;

  for (i = 0; i < sizeof(groups) / sizeof(groups[0]); i++) {
    size_t count;

    count = groups[i].count();
    if (index < count)
      return groups[i].run_case(index, failure_callback, context);
    index -= count;
  }

  if (failure_callback != NULL)
    failure_callback(__FILE__, __LINE__, "index < data_types_test_count()",
        "test case index is out of range", context);
  return -1;
}

int data_types_test_run(void)
{
  size_t index;

  for (index = 0; index < data_types_test_count(); index++) {
    if (data_types_test_run_case(index, NULL, NULL) != 0)
      return -1;
  }
  return 0;
}
