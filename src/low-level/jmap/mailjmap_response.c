/*
 * libEtPan! -- a mail stuff library
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailjmap_response.h"
#include "mailjmap_types.h"

#include <stdlib.h>
#include <string.h>

static char * dup_string(const char * value)
{
  if (value == NULL)
    return NULL;

  return strdup(value);
}

struct mailjmap_method_response *
mailjmap_method_response_new(const char * name,
    mailjson_value * arguments, const char * call_id)
{
  struct mailjmap_method_response * response;

  if ((name == NULL) || (* name == '\0') || (arguments == NULL) ||
      !mailjson_is_object(arguments) ||
      (call_id == NULL) || (* call_id == '\0'))
    return NULL;

  response = malloc(sizeof(* response));
  if (response == NULL)
    return NULL;

  response->name = dup_string(name);
  response->arguments = arguments;
  response->call_id = dup_string(call_id);
  if ((response->name == NULL) || (response->call_id == NULL)) {
    response->arguments = NULL;
    mailjmap_method_response_free(response);
    return NULL;
  }

  return response;
}

void mailjmap_method_response_free(
    struct mailjmap_method_response * response)
{
  if (response == NULL)
    return;

  free(response->name);
  mailjson_free(response->arguments);
  free(response->call_id);
  free(response);
}

static void method_response_list_free(clist * list)
{
  clistiter * cur;

  if (list == NULL)
    return;

  for (cur = clist_begin(list); cur != NULL; cur = clist_next(cur))
    mailjmap_method_response_free(clist_content(cur));
  clist_free(list);
}

struct mailjmap_response * mailjmap_response_new(void)
{
  struct mailjmap_response * response;

  response = malloc(sizeof(* response));
  if (response == NULL)
    return NULL;

  response->session_state = NULL;
  response->method_responses = clist_new();
  response->error_type = NULL;
  response->error_detail = NULL;
  if (response->method_responses == NULL) {
    mailjmap_response_free(response);
    return NULL;
  }

  return response;
}

void mailjmap_response_free(struct mailjmap_response * response)
{
  if (response == NULL)
    return;

  free(response->session_state);
  method_response_list_free(response->method_responses);
  free(response->error_type);
  free(response->error_detail);
  free(response);
}

static int normalize_parse_error(int r)
{
  if (r == MAILJMAP_ERROR_BAD_STATE)
    return MAILJMAP_ERROR_PROTOCOL;

  return r;
}

static int parse_method_response_entry(mailjson_value * entry,
    struct mailjmap_response * parsed)
{
  mailjson_value * name_value;
  mailjson_value * args_value;
  mailjson_value * call_id_value;
  mailjson_value * args_copy;
  struct mailjmap_method_response * method_response;
  char * name;
  char * call_id;
  int r;

  if (!mailjson_is_array(entry))
    return MAILJMAP_ERROR_PROTOCOL;
  if (mailjson_array_size(entry) != 3)
    return MAILJMAP_ERROR_PROTOCOL;

  name_value = NULL;
  args_value = NULL;
  call_id_value = NULL;
  args_copy = NULL;
  name = NULL;
  call_id = NULL;

  r = mailjson_array_get(entry, 0, &name_value);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = mailjson_array_get(entry, 1, &args_value);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = mailjson_array_get(entry, 2, &call_id_value);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;

  if (!mailjson_is_string(name_value) ||
      !mailjson_is_object(args_value) ||
      !mailjson_is_string(call_id_value)) {
    r = MAILJMAP_ERROR_PROTOCOL;
    goto cleanup;
  }

  r = mailjson_string_dup(name_value, &name);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = mailjson_string_dup(call_id_value, &call_id);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = mailjson_deep_copy(args_value, &args_copy);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;

  method_response = mailjmap_method_response_new(name, args_copy, call_id);
  if (method_response == NULL) {
    r = MAILJMAP_ERROR_MEMORY;
    goto cleanup;
  }
  args_copy = NULL;

  if (clist_append(parsed->method_responses, method_response) < 0) {
    mailjmap_method_response_free(method_response);
    r = MAILJMAP_ERROR_MEMORY;
    goto cleanup;
  }

  r = MAILJMAP_NO_ERROR;

 cleanup:
  mailjson_free(args_copy);
  mailjson_free(call_id_value);
  mailjson_free(args_value);
  mailjson_free(name_value);
  free(call_id);
  free(name);
  return normalize_parse_error(r);
}

static int parse_method_responses(mailjson_value * root,
    struct mailjmap_response * parsed, int * has_method_responses)
{
  mailjson_value * method_responses;
  size_t count;
  size_t i;
  int r;

  if (has_method_responses != NULL)
    * has_method_responses = 0;

  method_responses = NULL;
  r = mailjson_object_get(root, "methodResponses", &method_responses);
  if (r != MAILJMAP_NO_ERROR)
    return normalize_parse_error(r);
  if (method_responses == NULL)
    return MAILJMAP_NO_ERROR;
  if (!mailjson_is_array(method_responses)) {
    mailjson_free(method_responses);
    return MAILJMAP_ERROR_PROTOCOL;
  }
  if (has_method_responses != NULL)
    * has_method_responses = 1;

  count = mailjson_array_size(method_responses);
  for (i = 0; i < count; i ++) {
    mailjson_value * entry;

    entry = NULL;
    r = mailjson_array_get(method_responses, i, &entry);
    if (r == MAILJMAP_NO_ERROR)
      r = parse_method_response_entry(entry, parsed);
    mailjson_free(entry);
    if (r != MAILJMAP_NO_ERROR) {
      mailjson_free(method_responses);
      return normalize_parse_error(r);
    }
  }

  mailjson_free(method_responses);
  return MAILJMAP_NO_ERROR;
}

static int parse_problem_fields(mailjson_value * root,
    struct mailjmap_response * parsed)
{
  int r;

  r = mailjson_object_get_string_dup(root, "type",
      &parsed->error_type);
  if (r != MAILJMAP_NO_ERROR)
    return normalize_parse_error(r);

  r = mailjson_object_get_string_dup(root, "detail",
      &parsed->error_detail);
  if (r != MAILJMAP_NO_ERROR)
    return normalize_parse_error(r);

  if (parsed->error_detail == NULL) {
    r = mailjson_object_get_string_dup(root, "title",
        &parsed->error_detail);
    if (r != MAILJMAP_NO_ERROR)
      return normalize_parse_error(r);
  }

  return MAILJMAP_NO_ERROR;
}

int mailjmap_response_parse(const char * data, size_t data_len,
    struct mailjmap_response ** result)
{
  mailjson_value * root;
  struct mailjmap_response * parsed;
  int has_method_responses;
  int r;

  if ((data == NULL) || (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  * result = NULL;
  root = NULL;
  parsed = NULL;

  r = mailjson_parse(data, data_len, &root);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  if (!mailjson_is_object(root)) {
    r = MAILJMAP_ERROR_PROTOCOL;
    goto err;
  }

  parsed = mailjmap_response_new();
  if (parsed == NULL) {
    r = MAILJMAP_ERROR_MEMORY;
    goto err;
  }

  r = mailjson_object_get_string_dup(root, "sessionState",
      &parsed->session_state);
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  has_method_responses = 0;
  r = parse_method_responses(root, parsed, &has_method_responses);
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  r = parse_problem_fields(root, parsed);
  if (r != MAILJMAP_NO_ERROR)
    goto err;

  if ((parsed->error_type == NULL) &&
      ((parsed->session_state == NULL) || !has_method_responses)) {
    r = MAILJMAP_ERROR_PROTOCOL;
    goto err;
  }

  mailjson_free(root);
  * result = parsed;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_response_free(parsed);
  mailjson_free(root);
  return normalize_parse_error(r);
}
