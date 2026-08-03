/*
 * libEtPan! -- a mail stuff library
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailjmap_response.h"

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
    mailjmap_json_value * arguments, const char * call_id)
{
  struct mailjmap_method_response * response;

  if ((name == NULL) || (* name == '\0') || (arguments == NULL) ||
      !mailjmap_json_is_object(arguments) ||
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
  mailjmap_json_free(response->arguments);
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

static int parse_method_response_entry(mailjmap_json_value * entry,
    struct mailjmap_response * parsed)
{
  mailjmap_json_value * name_value;
  mailjmap_json_value * args_value;
  mailjmap_json_value * call_id_value;
  mailjmap_json_value * args_copy;
  struct mailjmap_method_response * method_response;
  char * name;
  char * call_id;
  int r;

  if (!mailjmap_json_is_array(entry))
    return MAILJMAP_ERROR_PROTOCOL;
  if (mailjmap_json_array_size(entry) != 3)
    return MAILJMAP_ERROR_PROTOCOL;

  name_value = NULL;
  args_value = NULL;
  call_id_value = NULL;
  args_copy = NULL;
  name = NULL;
  call_id = NULL;

  r = mailjmap_json_array_get(entry, 0, &name_value);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = mailjmap_json_array_get(entry, 1, &args_value);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = mailjmap_json_array_get(entry, 2, &call_id_value);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;

  if (!mailjmap_json_is_string(name_value) ||
      !mailjmap_json_is_object(args_value) ||
      !mailjmap_json_is_string(call_id_value)) {
    r = MAILJMAP_ERROR_PROTOCOL;
    goto cleanup;
  }

  r = mailjmap_json_string_dup(name_value, &name);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = mailjmap_json_string_dup(call_id_value, &call_id);
  if (r != MAILJMAP_NO_ERROR)
    goto cleanup;
  r = mailjmap_json_deep_copy(args_value, &args_copy);
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
  mailjmap_json_free(args_copy);
  mailjmap_json_free(call_id_value);
  mailjmap_json_free(args_value);
  mailjmap_json_free(name_value);
  free(call_id);
  free(name);
  return normalize_parse_error(r);
}

static int parse_method_responses(mailjmap_json_value * root,
    struct mailjmap_response * parsed, int * has_method_responses)
{
  mailjmap_json_value * method_responses;
  size_t count;
  size_t i;
  int r;

  if (has_method_responses != NULL)
    * has_method_responses = 0;

  method_responses = NULL;
  r = mailjmap_json_object_get(root, "methodResponses", &method_responses);
  if (r != MAILJMAP_NO_ERROR)
    return normalize_parse_error(r);
  if (method_responses == NULL)
    return MAILJMAP_NO_ERROR;
  if (!mailjmap_json_is_array(method_responses)) {
    mailjmap_json_free(method_responses);
    return MAILJMAP_ERROR_PROTOCOL;
  }
  if (has_method_responses != NULL)
    * has_method_responses = 1;

  count = mailjmap_json_array_size(method_responses);
  for (i = 0; i < count; i ++) {
    mailjmap_json_value * entry;

    entry = NULL;
    r = mailjmap_json_array_get(method_responses, i, &entry);
    if (r == MAILJMAP_NO_ERROR)
      r = parse_method_response_entry(entry, parsed);
    mailjmap_json_free(entry);
    if (r != MAILJMAP_NO_ERROR) {
      mailjmap_json_free(method_responses);
      return normalize_parse_error(r);
    }
  }

  mailjmap_json_free(method_responses);
  return MAILJMAP_NO_ERROR;
}

static int parse_problem_fields(mailjmap_json_value * root,
    struct mailjmap_response * parsed)
{
  int r;

  r = mailjmap_json_object_get_string_dup(root, "type",
      &parsed->error_type);
  if (r != MAILJMAP_NO_ERROR)
    return normalize_parse_error(r);

  r = mailjmap_json_object_get_string_dup(root, "detail",
      &parsed->error_detail);
  if (r != MAILJMAP_NO_ERROR)
    return normalize_parse_error(r);

  if (parsed->error_detail == NULL) {
    r = mailjmap_json_object_get_string_dup(root, "title",
        &parsed->error_detail);
    if (r != MAILJMAP_NO_ERROR)
      return normalize_parse_error(r);
  }

  return MAILJMAP_NO_ERROR;
}

int mailjmap_response_parse(const char * data, size_t data_len,
    struct mailjmap_response ** result)
{
  mailjmap_json_value * root;
  struct mailjmap_response * parsed;
  int has_method_responses;
  int r;

  if ((data == NULL) || (result == NULL))
    return MAILJMAP_ERROR_BAD_STATE;

  * result = NULL;
  root = NULL;
  parsed = NULL;

  r = mailjmap_json_parse(data, data_len, &root);
  if (r != MAILJMAP_NO_ERROR)
    return r;
  if (!mailjmap_json_is_object(root)) {
    r = MAILJMAP_ERROR_PROTOCOL;
    goto err;
  }

  parsed = mailjmap_response_new();
  if (parsed == NULL) {
    r = MAILJMAP_ERROR_MEMORY;
    goto err;
  }

  r = mailjmap_json_object_get_string_dup(root, "sessionState",
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

  mailjmap_json_free(root);
  * result = parsed;
  return MAILJMAP_NO_ERROR;

 err:
  mailjmap_response_free(parsed);
  mailjmap_json_free(root);
  return normalize_parse_error(r);
}
