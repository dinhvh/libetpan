#include "test_utils.h"

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static void append_file(struct test_file ** head, struct test_file ** tail,
    const char * path)
{
  struct test_file * item;

  item = calloc(1, sizeof(* item));
  assert(item != NULL);
  item->path = strdup(path);
  assert(item->path != NULL);

  if (*tail == NULL)
    *head = item;
  else
    (*tail)->next = item;
  *tail = item;
}

static void list_files_recursive(const char * root, struct test_file ** head,
    struct test_file ** tail)
{
  DIR * dir;
  struct dirent * ent;

  dir = opendir(root);
  assert(dir != NULL);

  while ((ent = readdir(dir)) != NULL) {
    char * path;
    struct stat st;

    if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
      continue;

    path = test_path_join(root, ent->d_name);
    assert(stat(path, &st) == 0);
    if (S_ISDIR(st.st_mode))
      list_files_recursive(path, head, tail);
    else if (S_ISREG(st.st_mode))
      append_file(head, tail, path);
    free(path);
  }

  closedir(dir);
}

struct test_file * test_list_files(const char * root)
{
  struct test_file * head = NULL;
  struct test_file * tail = NULL;

  list_files_recursive(root, &head, &tail);
  return head;
}

void test_free_files(struct test_file * files)
{
  while (files != NULL) {
    struct test_file * next = files->next;

    free(files->path);
    free(files);
    files = next;
  }
}

char * test_read_file(const char * path, size_t * length)
{
  FILE * f;
  long size;
  char * data;
  size_t read_size;

  f = fopen(path, "rb");
  assert(f != NULL);
  assert(fseek(f, 0, SEEK_END) == 0);
  size = ftell(f);
  assert(size >= 0);
  assert(fseek(f, 0, SEEK_SET) == 0);

  data = malloc((size_t) size + 1);
  assert(data != NULL);
  read_size = fread(data, 1, (size_t) size, f);
  assert(read_size == (size_t) size);
  data[size] = '\0';
  fclose(f);

  if (length != NULL)
    *length = (size_t) size;
  return data;
}

int test_file_exists(const char * path)
{
  struct stat st;

  return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

char * test_path_join(const char * left, const char * right)
{
  size_t left_len = strlen(left);
  size_t right_len = strlen(right);
  int needs_slash = left_len > 0 && left[left_len - 1] != '/';
  char * result;

  result = malloc(left_len + (size_t) needs_slash + right_len + 1);
  assert(result != NULL);
  memcpy(result, left, left_len);
  if (needs_slash)
    result[left_len++] = '/';
  memcpy(result + left_len, right, right_len + 1);
  return result;
}

char * test_replace_prefix(const char * path, const char * old_prefix,
    const char * new_prefix)
{
  size_t old_len = strlen(old_prefix);
  size_t new_len = strlen(new_prefix);
  size_t path_len = strlen(path);
  char * result;

  assert(strncmp(path, old_prefix, old_len) == 0);
  result = malloc(new_len + path_len - old_len + 1);
  assert(result != NULL);
  memcpy(result, new_prefix, new_len);
  memcpy(result + new_len, path + old_len, path_len - old_len + 1);
  return result;
}

char * test_replace_extension(const char * path, const char * extension)
{
  const char * slash = strrchr(path, '/');
  const char * dot = strrchr(path, '.');
  size_t base_len;
  size_t ext_len = strlen(extension);
  char * result;

  if (dot == NULL || (slash != NULL && dot < slash))
    base_len = strlen(path);
  else
    base_len = (size_t) (dot - path);

  result = malloc(base_len + ext_len + 1);
  assert(result != NULL);
  memcpy(result, path, base_len);
  memcpy(result + base_len, extension, ext_len + 1);
  return result;
}

const char * test_basename(const char * path)
{
  const char * slash = strrchr(path, '/');

  if (slash == NULL)
    return path;
  return slash + 1;
}

char * test_stem(const char * path)
{
  const char * base = test_basename(path);
  const char * dot = strrchr(base, '.');
  size_t len = dot == NULL ? strlen(base) : (size_t) (dot - base);
  char * result = malloc(len + 1);

  assert(result != NULL);
  memcpy(result, base, len);
  result[len] = '\0';
  return result;
}

void test_assert(int condition, const char * message)
{
  if (!condition) {
    fprintf(stderr, "%s\n", message);
    abort();
  }
}
