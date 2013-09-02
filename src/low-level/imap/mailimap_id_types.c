#include "mailimap_id_types.h"

#include <stdlib.h>

struct mailimap_id_params_list * mailimap_id_params_list_new(clist * items)
{
  struct mailimap_id_params_list * list;
  
  list = malloc(sizeof(* list));
  if (list == NULL)
    return NULL;
  
  list->idpa_list = items;
  
  return list;
}

void mailimap_id_params_list_free(struct mailimap_id_params_list * list)
{
  clist_foreach(list->idpa_list, (clist_func) mailimap_id_param_free, NULL);
  clist_free(list->idpa_list);
  free(list);
}

struct mailimap_id_param * mailimap_id_param_new(char * name, char * value)
{
  struct mailimap_id_param * param;
  
  param = malloc(sizeof(* param));
  if (param == NULL)
    return NULL;
  
  param->idpa_name = name;
  param->idpa_value = value;
  
  return param;
}

void mailimap_id_param_free(struct mailimap_id_param * param)
{
  free(param->idpa_name);
  free(param->idpa_value);
  free(param);
}

struct mailimap_id_params_list * mailimap_id_params_list_new_empty(void)
{
  clist * items;
  struct mailimap_id_params_list * list;
  
  items = clist_new();
  if (items == NULL)
    return NULL;
  
  list = mailimap_id_params_list_new(items);
  if (list == NULL) {
    clist_free(items);
    return NULL;
  }
  
  return list;
}

int mailimap_id_params_list_add_name_value(struct mailimap_id_params_list * list, char * name, char * value)
{
  struct mailimap_id_param * param;
  int r;
  
  param = mailimap_id_param_new(name, value);
  if (param == NULL)
    return -1;
  
  r = clist_append(list->idpa_list, param);
  if (r < 0) {
    mailimap_id_param_free(param);
    return -1;
  }
  
  return 0;
}

