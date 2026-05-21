#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include <stddef.h>

struct test_file {
  char * path;
  struct test_file * next;
};

struct test_file * test_list_files(const char * root);
void test_free_files(struct test_file * files);

char * test_read_file(const char * path, size_t * length);
int test_file_exists(const char * path);
char * test_path_join(const char * left, const char * right);
char * test_replace_prefix(const char * path, const char * old_prefix,
    const char * new_prefix);
char * test_replace_extension(const char * path, const char * extension);
const char * test_basename(const char * path);
char * test_stem(const char * path);

void test_assert(int condition, const char * message);

#endif
