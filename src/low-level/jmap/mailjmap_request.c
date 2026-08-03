/*
 * libEtPan! -- a mail stuff library
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailjmap_request.h"

#include <libetpan/clist.h>

#include <stdlib.h>
#include <string.h>

#define MAILJMAP_CAPABILITY_CORE "urn:ietf:params:jmap:core"

struct mailjmap_method_call {
  char * name;
  mailjmap_json_value * arguments;
  char * call_id;
};

struct mailjmap_request {
  clist * capabilities; /* char * */
  clist * method_calls; /* struct mailjmap_method_call * */
};

static char * dup_string(const char * value)
{
  if (value == NULL)
    return NULL;

  return strdup(value);
}

static void string_list_free(clist * list)
{
  clistiter * cur;

  if (list == NULL)
    return;

  for (cur = clist_begin(list); cur != NULL; cur = clist_next(cur))
    free(clist_content(cur));
  clist_free(list);
}

static int string_list_contains(clist * list, const char * value)
{
  clistiter * cur;

  if ((list == NULL) || (value == NULL))
    return 0;

  for (cur = clist_begin(list); cur != NULL; cur = clist_next(cur)) {
    const char * item;

    item = clist_content(cur);
    if ((item != NULL) && (strcmp(item, value) == 0))
      return 1;
  }

  return 0;
}

static void method_call_free(struct mailjmap_method_call * call)
{
  if (call == NULL)
    return;

  free(call->name);
  mailjmap_json_free(call->arguments);
  free(call->call_id);
  free(call);
}

static void method_call_list_free(clist * list)
{
  clistiter * cur;

  if (list == NULL)
    return;

  for (cur = clist_begin(list); cur != NULL; cur = clist_next(cur))
    method_call_free(clist_content(cur));
  clist_free(list);
}

struct mailjmap_request * mailjmap_request_new(void)
{
  struct mailjmap_request * request;

  request = malloc(sizeof(* request));
  if (request == NULL)
    return NULL;

  request->capabilities = clist_new();
  request->method_calls = clist_new();
  if ((request->capabilities == NULL) || (request->method_calls == NULL)) {
    mailjmap_request_free(request);
    return NULL;
  }

  return request;
}

void mailjmap_request_free(struct mailjmap_request * request)
{
  if (request == NULL)
    return;

  string_list_free(request->capabilities);
  method_call_list_free(request->method_calls);
  free(request);
}

int mailjmap_request_add_capability(struct mailjmap_request * request,
    const char * capability)
{
  char * copy;

  if ((request == NULL) || (capability == NULL) || (* capability == '\0'))
    return MAILJMAP_ERROR_BAD_STATE;

  if ((strcmp(capability, MAILJMAP_CAPABILITY_CORE) == 0) ||
      string_list_contains(request->capabilities, capability))
    return MAILJMAP_NO_ERROR;

  copy = dup_string(capability);
  if (copy == NULL)
    return MAILJMAP_ERROR_MEMORY;

  if (clist_append(request->capabilities, copy) < 0) {
    free(copy);
    return MAILJMAP_ERROR_MEMORY;
  }

  return MAILJMAP_NO_ERROR;
}

int mailjmap_request_add_call_json(struct mailjmap_request * request,
    const char * name, mailjmap_json_value * arguments,
    const char * call_id)
{
  struct mailjmap_method_call * call;

  if ((request == NULL) || (name == NULL) || (* name == '\0') ||
      (arguments == NULL) || !mailjmap_json_is_object(arguments) ||
      (call_id == NULL) || (* call_id == '\0'))
    return MAILJMAP_ERROR_BAD_STATE;

  call = malloc(sizeof(* call));
  if (call == NULL)
    return MAILJMAP_ERROR_MEMORY;

  call->name = dup_string(name);
  call->arguments = arguments;
  call->call_id = dup_string(call_id);
  if ((call->name == NULL) || (call->call_id == NULL)) {
    call->arguments = NULL;
    method_call_free(call);
    return MAILJMAP_ERROR_MEMORY;
  }

  if (clist_append(request->method_calls, call) < 0) {
    call->arguments = NULL;
    method_call_free(call);
    return MAILJMAP_ERROR_MEMORY;
  }

  return MAILJMAP_NO_ERROR;
}

static int object_set_string(mailjmap_json_value * object,
    const char * key, const char * string)
{
  mailjmap_json_value * value;
  int r;

  value = NULL;
  r = mailjmap_json_new_string(string, &value);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  r = mailjmap_json_object_set_new(object, key, value);
  if (r != MAILJMAP_NO_ERROR) {
    mailjmap_json_free(value);
    return r;
  }

  return MAILJMAP_NO_ERROR;
}

int mailjmap_request_add_call_empty(struct mailjmap_request * request,
    const char * name, const char * call_id)
{
  mailjmap_json_value * arguments;
  int r;

  arguments = NULL;
  r = mailjmap_json_new_object(&arguments);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  r = mailjmap_request_add_call_json(request, name, arguments, call_id);
  if (r != MAILJMAP_NO_ERROR) {
    mailjmap_json_free(arguments);
    return r;
  }

  return MAILJMAP_NO_ERROR;
}

int mailjmap_request_add_string_argument(mailjmap_json_value * arguments,
    const char * key, const char * value)
{
  if ((arguments == NULL) || !mailjmap_json_is_object(arguments) ||
      (key == NULL) || (* key == '\0') || (value == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  return object_set_string(arguments, key, value);
}

int mailjmap_request_arguments_set_result_reference(
    mailjmap_json_value * arguments,
    const char * key,
    const char * result_of,
    const char * name,
    const char * path)
{
  mailjmap_json_value * reference;
  int r;

  if ((arguments == NULL) || !mailjmap_json_is_object(arguments) ||
      (key == NULL) || (* key == '\0') || (key[0] != '#') ||
      (result_of == NULL) || (* result_of == '\0') ||
      (name == NULL) || (* name == '\0') ||
      (path == NULL) || (* path == '\0'))
    return MAILJMAP_ERROR_BAD_STATE;

  reference = NULL;
  r = mailjmap_json_new_object(&reference);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  r = object_set_string(reference, "resultOf", result_of);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = object_set_string(reference, "name", name);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = object_set_string(reference, "path", path);
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  r = mailjmap_json_object_set_new(arguments, key, reference);
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_json_free(reference);
  return r;
}

const char * mailjmap_request_method_name_for_call_id(
    struct mailjmap_request * request,
    const char * call_id)
{
  clistiter * cur;

  if ((request == NULL) || (call_id == NULL))
    return NULL;

  for (cur = clist_begin(request->method_calls); cur != NULL;
       cur = clist_next(cur)) {
    struct mailjmap_method_call * call;

    call = clist_content(cur);
    if ((call != NULL) && (call->call_id != NULL) &&
        (strcmp(call->call_id, call_id) == 0))
      return call->name;
  }

  return NULL;
}

static int append_string_value(mailjmap_json_value * array,
    const char * string)
{
  mailjmap_json_value * value;
  int r;

  value = NULL;
  r = mailjmap_json_new_string(string, &value);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  r = mailjmap_json_array_append_new(array, value);
  if (r != MAILJMAP_NO_ERROR) {
    mailjmap_json_free(value);
    return r;
  }

  return MAILJMAP_NO_ERROR;
}

static int build_using(struct mailjmap_request * request,
    mailjmap_json_value ** result)
{
  mailjmap_json_value * array;
  clistiter * cur;
  int r;

  array = NULL;
  r = mailjmap_json_new_array(&array);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  r = append_string_value(array, MAILJMAP_CAPABILITY_CORE);
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  for (cur = clist_begin(request->capabilities); cur != NULL;
       cur = clist_next(cur)) {
    r = append_string_value(array, clist_content(cur));
    if (r != MAILJMAP_NO_ERROR)
      goto err;
  }

  * result = array;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_json_free(array);
  return r;
}

static int build_method_call(struct mailjmap_method_call * call,
    mailjmap_json_value ** result)
{
  mailjmap_json_value * array;
  mailjmap_json_value * value;
  int r;

  array = NULL;
  value = NULL;

  r = mailjmap_json_new_array(&array);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  r = append_string_value(array, call->name);
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  r = mailjmap_json_deep_copy(call->arguments, &value);
  if (r != MAILJMAP_NO_ERROR)
    goto err;
  r = mailjmap_json_array_append_new(array, value);
  value = NULL;
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  r = append_string_value(array, call->call_id);
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  * result = array;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_json_free(value);
  mailjmap_json_free(array);
  return r;
}

static int build_method_calls(struct mailjmap_request * request,
    mailjmap_json_value ** result)
{
  mailjmap_json_value * array;
  clistiter * cur;
  int r;

  array = NULL;
  r = mailjmap_json_new_array(&array);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  for (cur = clist_begin(request->method_calls); cur != NULL;
       cur = clist_next(cur)) {
    mailjmap_json_value * call_value;

    call_value = NULL;
    r = build_method_call(clist_content(cur), &call_value);
    if (r != MAILJMAP_NO_ERROR)
      goto err;

    r = mailjmap_json_array_append_new(array, call_value);
    if (r != MAILJMAP_NO_ERROR) {
      mailjmap_json_free(call_value);
      goto err;
    }
  }

  * result = array;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_json_free(array);
  return r;
}

int mailjmap_request_serialize(struct mailjmap_request * request,
    mailjmap_json_value ** result)
{
  mailjmap_json_value * root;
  mailjmap_json_value * using_array;
  mailjmap_json_value * method_calls;
  int r;

  if ((request == NULL) || (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  root = NULL;
  using_array = NULL;
  method_calls = NULL;

  r = mailjmap_json_new_object(&root);
  if (r != MAILJMAP_NO_ERROR)
    return r;

  r = build_using(request, &using_array);
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  r = mailjmap_json_object_set_new(root, "using", using_array);
  using_array = NULL;
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  r = build_method_calls(request, &method_calls);
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  r = mailjmap_json_object_set_new(root, "methodCalls", method_calls);
  method_calls = NULL;
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  * result = root;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_json_free(method_calls);
  mailjmap_json_free(using_array);
  mailjmap_json_free(root);
  return r;
}
