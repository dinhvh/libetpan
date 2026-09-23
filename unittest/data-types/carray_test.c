#include "data_types_tests.h"

#include "carray.h"

static int check_carray_growth_clears_slots(
    test_failure_callback failure_callback, void * context)
{
  carray /* int * */ * array;
  int result;
  static int first;
  static int stale;

  result = 0;
  array = carray_new(8);
  TEST_CHECK(array != NULL, "carray_new failed");
  TEST_CHECK(carray_set_size(array, 6) == 0, "carray_set_size failed");
  carray_set(array, 0, &first);
  carray_set(array, 2, &stale);
  carray_set(array, 3, &stale);
  carray_set(array, 4, &stale);
  carray_set(array, 5, &stale);

  TEST_CHECK(carray_set_size(array, 2) == 0, "carray_set_size shrink failed");
  TEST_CHECK(carray_set_size(array, 6) == 0, "carray_set_size grow failed");
  TEST_CHECK(carray_get(array, 0) == &first, "existing slot was changed");
  TEST_CHECK(carray_get(array, 1) == NULL, "new slot was not cleared");
  TEST_CHECK(carray_get(array, 2) == NULL, "re-exposed slot was not cleared");
  TEST_CHECK(carray_get(array, 3) == NULL, "re-exposed slot was not cleared");
  TEST_CHECK(carray_get(array, 4) == NULL, "re-exposed slot was not cleared");
  TEST_CHECK(carray_get(array, 5) == NULL, "re-exposed slot was not cleared");

 cleanup:
  if (array != NULL)
    carray_free(array);
  return result;
}

struct carray_case {
  const char * name;
  int (* run)(test_failure_callback failure_callback, void * context);
};

static const struct carray_case cases[] = {
  { "carray growth clears slots", check_carray_growth_clears_slots },
};

size_t carray_test_count(void)
{
  return sizeof(cases) / sizeof(cases[0]);
}

const char * carray_test_name(size_t index)
{
  if (index >= carray_test_count())
    return NULL;
  return cases[index].name;
}

int carray_test_run_case(size_t index,
    test_failure_callback failure_callback, void * context)
{
  if (index >= carray_test_count()) {
    if (failure_callback != NULL)
      failure_callback(__FILE__, __LINE__, "index < carray_test_count()",
          "test case index is out of range", context);
    return -1;
  }

  return cases[index].run(failure_callback, context);
}
