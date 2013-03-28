//
//  mailimap_sort_types.c
//  libetpan
//
//  Created by Pitiphong Phongpattranont on 28/3/56 BE.
//
//

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailimap_sort_types.h"
#include "mmapstring.h"
#include "mail.h"
#include "mailimap_extension.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>


struct mailimap_sort_key *
mailimap_sort_key_new(int sortk_type,
                      _Bool is_reverse,
                      clist * sortk_multiple) {
  struct mailimap_sort_key * key;
  
  key = malloc(sizeof(* key));
  if (key == NULL)
    return NULL;
  
  key->sortk_type = sortk_type;
  key->sortk_is_reverse = is_reverse;
  
  if (sortk_type == MAILIMAP_SORT_KEY_MULTIPLE) {
    key->sortk_multiple = sortk_multiple;
  }
  
  return key;
}


void mailimap_sort_key_free(struct mailimap_sort_key * key) {
  if (key->sortk_type == MAILIMAP_SORT_KEY_MULTIPLE) {
    clist_foreach(key->sortk_multiple,
                  (clist_func) mailimap_sort_key_free, NULL);
    clist_free(key->sortk_multiple);
  }
  
  free(key);
}


struct mailimap_sort_key *
mailimap_sort_key_new_arrival(_Bool is_reverse) {
  return mailimap_sort_key_new(MAILIMAP_SORT_KEY_ARRIVAL, is_reverse, NULL);
}

struct mailimap_sort_key *
mailimap_sort_key_new_cc(_Bool is_reverse) {
  return mailimap_sort_key_new(MAILIMAP_SORT_KEY_CC, is_reverse, NULL);
}

struct mailimap_sort_key *
mailimap_sort_key_new_date(_Bool is_reverse) {
  return mailimap_sort_key_new(MAILIMAP_SORT_KEY_DATE, is_reverse, NULL);
}

struct mailimap_sort_key *
mailimap_sort_key_new_from(_Bool is_reverse) {
  return mailimap_sort_key_new(MAILIMAP_SORT_KEY_FROM, is_reverse, NULL);
}

struct mailimap_sort_key *
mailimap_sort_key_new_size(_Bool is_reverse) {
  return mailimap_sort_key_new(MAILIMAP_SORT_KEY_SIZE, is_reverse, NULL);
}

struct mailimap_sort_key *
mailimap_sort_key_new_subject(_Bool is_reverse) {
  return mailimap_sort_key_new(MAILIMAP_SORT_KEY_SUBJECT, is_reverse, NULL);
}

struct mailimap_sort_key *
mailimap_sort_key_new_to(_Bool is_reverse) {
  return mailimap_sort_key_new(MAILIMAP_SORT_KEY_TO, is_reverse, NULL);
}

struct mailimap_sort_key *
mailimap_sort_key_new_multiple(clist * keys) {
  return mailimap_sort_key_new(MAILIMAP_SORT_KEY_MULTIPLE, false, keys);
}


struct mailimap_sort_key *
mailimap_sort_key_new_multiple_empty(void)
{
  clist * list;
  
  list = clist_new();
  if (list == NULL)
    return NULL;
  
  return mailimap_sort_key_new_multiple(list);
}

int
mailimap_sort_key_multiple_add(struct mailimap_sort_key * keys,
                               struct mailimap_sort_key * key_item)
{
  int r;
	
  r = clist_append(keys->sortk_multiple, key_item);
  if (r < 0)
    return MAILIMAP_ERROR_MEMORY;
  
  return MAILIMAP_NO_ERROR;
}


