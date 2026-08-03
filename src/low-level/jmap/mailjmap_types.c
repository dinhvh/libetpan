/*
 * libEtPan! -- a mail stuff library
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailjmap_types.h"

#include <stdlib.h>
#include <string.h>

static char * dup_string(const char * value)
{
  if (value == NULL)
    return NULL;

  return strdup(value);
}

static void free_string_item(void * value, void * data)
{
  (void) data;
  free(value);
}

static void string_list_free(clist * list)
{
  if (list == NULL)
    return;

  clist_foreach(list, free_string_item, NULL);
  clist_free(list);
}

struct mailjmap_session_capability *
mailjmap_session_capability_new(const char * capability, const char * json)
{
  struct mailjmap_session_capability * item;

  if ((capability == NULL) || (json == NULL))
    return NULL;

  item = malloc(sizeof(* item));
  if (item == NULL)
    return NULL;

  item->capability = dup_string(capability);
  item->json = dup_string(json);
  if ((item->capability == NULL) || (item->json == NULL)) {
    mailjmap_session_capability_free(item);
    return NULL;
  }

  return item;
}

void mailjmap_session_capability_free(
    struct mailjmap_session_capability * capability)
{
  if (capability == NULL)
    return;

  free(capability->capability);
  free(capability->json);
  free(capability);
}

static void free_capability_item(void * value, void * data)
{
  (void) data;
  mailjmap_session_capability_free(value);
}

static void capability_list_free(clist * list)
{
  if (list == NULL)
    return;

  clist_foreach(list, free_capability_item, NULL);
  clist_free(list);
}

struct mailjmap_session_account *
mailjmap_session_account_new(const char * account_id)
{
  struct mailjmap_session_account * account;

  if (account_id == NULL)
    return NULL;

  account = malloc(sizeof(* account));
  if (account == NULL)
    return NULL;

  account->account_id = dup_string(account_id);
  account->name = NULL;
  account->is_personal = 0;
  account->is_read_only = 0;
  account->capabilities = clist_new();
  account->capability_details = clist_new();
  if ((account->account_id == NULL) || (account->capabilities == NULL) ||
      (account->capability_details == NULL)) {
    mailjmap_session_account_free(account);
    return NULL;
  }

  return account;
}

void mailjmap_session_account_free(
    struct mailjmap_session_account * account)
{
  if (account == NULL)
    return;

  free(account->account_id);
  free(account->name);
  string_list_free(account->capabilities);
  capability_list_free(account->capability_details);
  free(account);
}

struct mailjmap_session_primary_account *
mailjmap_session_primary_account_new(const char * capability,
    const char * account_id)
{
  struct mailjmap_session_primary_account * primary_account;

  if ((capability == NULL) || (account_id == NULL))
    return NULL;

  primary_account = malloc(sizeof(* primary_account));
  if (primary_account == NULL)
    return NULL;

  primary_account->capability = dup_string(capability);
  primary_account->account_id = dup_string(account_id);
  if ((primary_account->capability == NULL) ||
      (primary_account->account_id == NULL)) {
    mailjmap_session_primary_account_free(primary_account);
    return NULL;
  }

  return primary_account;
}

void mailjmap_session_primary_account_free(
    struct mailjmap_session_primary_account * primary_account)
{
  if (primary_account == NULL)
    return;

  free(primary_account->capability);
  free(primary_account->account_id);
  free(primary_account);
}

static void free_account_item(void * value, void * data)
{
  (void) data;
  mailjmap_session_account_free(value);
}

static void free_primary_account_item(void * value, void * data)
{
  (void) data;
  mailjmap_session_primary_account_free(value);
}

struct mailjmap_session * mailjmap_session_new(void)
{
  struct mailjmap_session * session;

  session = malloc(sizeof(* session));
  if (session == NULL)
    return NULL;

  session->api_url = NULL;
  session->upload_url = NULL;
  session->download_url = NULL;
  session->event_source_url = NULL;
  session->session_state = NULL;
  session->capabilities = clist_new();
  session->capability_details = clist_new();
  session->accounts = clist_new();
  session->primary_accounts = clist_new();
  if ((session->capabilities == NULL) ||
      (session->capability_details == NULL) || (session->accounts == NULL) ||
      (session->primary_accounts == NULL)) {
    mailjmap_session_free(session);
    return NULL;
  }

  return session;
}

void mailjmap_session_free(struct mailjmap_session * session)
{
  if (session == NULL)
    return;

  free(session->api_url);
  free(session->upload_url);
  free(session->download_url);
  free(session->event_source_url);
  free(session->session_state);
  string_list_free(session->capabilities);
  capability_list_free(session->capability_details);
  if (session->accounts != NULL) {
    clist_foreach(session->accounts, free_account_item, NULL);
    clist_free(session->accounts);
  }
  if (session->primary_accounts != NULL) {
    clist_foreach(session->primary_accounts, free_primary_account_item, NULL);
    clist_free(session->primary_accounts);
  }
  free(session);
}

struct mailjmap_blob_upload * mailjmap_blob_upload_new(void)
{
  struct mailjmap_blob_upload * upload;

  upload = malloc(sizeof(* upload));
  if (upload == NULL)
    return NULL;

  upload->account_id = NULL;
  upload->blob_id = NULL;
  upload->type = NULL;
  upload->name = NULL;
  upload->size = 0;
  return upload;
}

void mailjmap_blob_upload_free(struct mailjmap_blob_upload * upload)
{
  if (upload == NULL)
    return;

  free(upload->account_id);
  free(upload->blob_id);
  free(upload->type);
  free(upload->name);
  free(upload);
}
