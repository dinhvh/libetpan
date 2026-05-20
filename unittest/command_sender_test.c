#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "clist.h"
#include "imap_test_utils.h"
#include "mailimap_sender.h"
#include "mailimap_types.h"
#include "namespace_sender.h"

static struct mailimap_set * make_set(uint32_t first, uint32_t last)
{
  clist * list = clist_new();
  struct mailimap_set_item * item = mailimap_set_item_new(first, last);

  assert(list != NULL);
  assert(item != NULL);
  assert(clist_append(list, item) == 0);
  return mailimap_set_new(list);
}

static struct mailimap_flag_list * make_seen_flag_list(void)
{
  clist * list = clist_new();
  struct mailimap_flag * flag = mailimap_flag_new(MAILIMAP_FLAG_SEEN,
      NULL, NULL);

  assert(list != NULL);
  assert(flag != NULL);
  assert(clist_append(list, flag) == 0);
  return mailimap_flag_list_new(list);
}

static struct mailimap_status_att_list * make_status_att_list(void)
{
  static const int atts[] = {
    MAILIMAP_STATUS_ATT_MESSAGES,
    MAILIMAP_STATUS_ATT_UIDNEXT,
    MAILIMAP_STATUS_ATT_UIDVALIDITY,
    MAILIMAP_STATUS_ATT_UNSEEN,
    MAILIMAP_STATUS_ATT_SIZE
  };
  clist * list = clist_new();
  size_t i;

  assert(list != NULL);
  for (i = 0; i < sizeof(atts) / sizeof(atts[0]); i++) {
    int * value = malloc(sizeof(* value));
    assert(value != NULL);
    *value = atts[i];
    assert(clist_append(list, value) == 0);
  }

  return mailimap_status_att_list_new(list);
}

static struct mailimap_search_key * make_search_all(void)
{
  return mailimap_search_key_new(MAILIMAP_SEARCH_KEY_ALL,
      NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
      NULL, NULL, NULL, NULL, 0, NULL, NULL, NULL, NULL, NULL, NULL,
      0, NULL, NULL, NULL);
}

static int send_crlf(mailstream * stream, int r)
{
  if (r != MAILIMAP_NO_ERROR)
    return r;
  return mailimap_crlf_send(stream);
}

static int send_capability(mailstream * stream, void * context)
{
  (void) context;
  return send_crlf(stream, mailimap_capability_send(stream));
}

static int send_noop(mailstream * stream, void * context)
{
  (void) context;
  return send_crlf(stream, mailimap_noop_send(stream));
}

static int send_logout(mailstream * stream, void * context)
{
  (void) context;
  return send_crlf(stream, mailimap_logout_send(stream));
}

static int send_starttls(mailstream * stream, void * context)
{
  (void) context;
  return send_crlf(stream, mailimap_starttls_send(stream));
}

static int send_authenticate(mailstream * stream, void * context)
{
  (void) context;
  return send_crlf(stream, mailimap_authenticate_send(stream, "PLAIN"));
}

static int send_login(mailstream * stream, void * context)
{
  (void) context;
  return send_crlf(stream, mailimap_login_send(stream, "user", "secret"));
}

static int send_select(mailstream * stream, void * context)
{
  (void) context;
  return send_crlf(stream, mailimap_select_send(stream, "INBOX", 0));
}

static int send_select_condstore(mailstream * stream, void * context)
{
  (void) context;
  return send_crlf(stream, mailimap_select_send(stream, "INBOX", 1));
}

static int send_examine(mailstream * stream, void * context)
{
  (void) context;
  return send_crlf(stream, mailimap_examine_send(stream, "INBOX", 0));
}

static int send_create(mailstream * stream, void * context)
{
  (void) context;
  return send_crlf(stream, mailimap_create_send(stream, "Archive"));
}

static int send_delete(mailstream * stream, void * context)
{
  (void) context;
  return send_crlf(stream, mailimap_delete_send(stream, "Archive"));
}

static int send_rename(mailstream * stream, void * context)
{
  (void) context;
  return send_crlf(stream, mailimap_rename_send(stream, "Archive", "Old"));
}

static int send_subscribe(mailstream * stream, void * context)
{
  (void) context;
  return send_crlf(stream, mailimap_subscribe_send(stream, "Archive"));
}

static int send_unsubscribe(mailstream * stream, void * context)
{
  (void) context;
  return send_crlf(stream, mailimap_unsubscribe_send(stream, "Archive"));
}

static int send_list(mailstream * stream, void * context)
{
  (void) context;
  return send_crlf(stream, mailimap_list_send(stream, "", "*"));
}

static int send_lsub(mailstream * stream, void * context)
{
  (void) context;
  return send_crlf(stream, mailimap_lsub_send(stream, "", "*"));
}

static int send_namespace(mailstream * stream, void * context)
{
  (void) context;
  return send_crlf(stream, mailimap_namespace_send(stream));
}

static int send_status(mailstream * stream, void * context)
{
  struct mailimap_status_att_list * status = make_status_att_list();
  int r = mailimap_status_send(stream, "INBOX", status);

  (void) context;
  mailimap_status_att_list_free(status);
  return send_crlf(stream, r);
}

static int send_append(mailstream * stream, void * context)
{
  struct mailimap_flag_list * flags = make_seen_flag_list();
  struct mailimap_date_time * date_time =
    mailimap_date_time_new(17, 7, 1996, 2, 44, 25, -700);
  int r;

  (void) context;
  assert(date_time != NULL);
  r = mailimap_append_send(stream, "INBOX", flags, date_time, 12);
  mailimap_flag_list_free(flags);
  mailimap_date_time_free(date_time);
  return r;
}

static int send_check(mailstream * stream, void * context)
{
  (void) context;
  return send_crlf(stream, mailimap_check_send(stream));
}

static int send_close(mailstream * stream, void * context)
{
  (void) context;
  return send_crlf(stream, mailimap_close_send(stream));
}

static int send_expunge(mailstream * stream, void * context)
{
  (void) context;
  return send_crlf(stream, mailimap_expunge_send(stream));
}

static int send_search(mailstream * stream, void * context)
{
  struct mailimap_search_key * key = make_search_all();
  int r;

  (void) context;
  assert(key != NULL);
  r = mailimap_search_send(stream, NULL, key);
  mailimap_search_key_free(key);
  return send_crlf(stream, r);
}

static int send_uid_search(mailstream * stream, void * context)
{
  struct mailimap_search_key * key = make_search_all();
  int r;

  (void) context;
  assert(key != NULL);
  r = mailimap_uid_search_send(stream, "UTF-8", key);
  mailimap_search_key_free(key);
  return send_crlf(stream, r);
}

static int send_fetch(mailstream * stream, void * context)
{
  struct mailimap_set * set = make_set(1, 3);
  struct mailimap_fetch_type * fetch_type =
    mailimap_fetch_type_new(MAILIMAP_FETCH_TYPE_FAST, NULL, NULL);
  int r;

  (void) context;
  assert(fetch_type != NULL);
  r = mailimap_fetch_send(stream, set, fetch_type);
  mailimap_fetch_type_free(fetch_type);
  mailimap_set_free(set);
  return send_crlf(stream, r);
}

static int send_uid_fetch(mailstream * stream, void * context)
{
  struct mailimap_set * set = make_set(10, 12);
  struct mailimap_fetch_att * fetch_att =
    mailimap_fetch_att_new(MAILIMAP_FETCH_ATT_UID, NULL, 0, 0, NULL);
  struct mailimap_fetch_type * fetch_type =
    mailimap_fetch_type_new(MAILIMAP_FETCH_TYPE_FETCH_ATT, fetch_att, NULL);
  int r;

  (void) context;
  assert(fetch_att != NULL);
  assert(fetch_type != NULL);
  r = mailimap_uid_fetch_send(stream, set, fetch_type);
  mailimap_fetch_type_free(fetch_type);
  mailimap_set_free(set);
  return send_crlf(stream, r);
}

static int send_store(mailstream * stream, void * context)
{
  struct mailimap_set * set = make_set(1, 3);
  struct mailimap_flag_list * flags = make_seen_flag_list();
  struct mailimap_store_att_flags * store_flags =
    mailimap_store_att_flags_new(1, 1, flags);
  int r;

  (void) context;
  assert(store_flags != NULL);
  r = mailimap_store_send(stream, set, 0, 0, store_flags);
  mailimap_store_att_flags_free(store_flags);
  mailimap_set_free(set);
  return send_crlf(stream, r);
}

static int send_uid_store(mailstream * stream, void * context)
{
  struct mailimap_set * set = make_set(10, 12);
  struct mailimap_flag_list * flags = make_seen_flag_list();
  struct mailimap_store_att_flags * store_flags =
    mailimap_store_att_flags_new(-1, 0, flags);
  int r;

  (void) context;
  assert(store_flags != NULL);
  r = mailimap_uid_store_send(stream, set, 1, 42, store_flags);
  mailimap_store_att_flags_free(store_flags);
  mailimap_set_free(set);
  return send_crlf(stream, r);
}

static int send_copy(mailstream * stream, void * context)
{
  struct mailimap_set * set = make_set(1, 3);
  int r;

  (void) context;
  r = mailimap_copy_send(stream, set, "Archive");
  mailimap_set_free(set);
  return send_crlf(stream, r);
}

static int send_uid_copy(mailstream * stream, void * context)
{
  struct mailimap_set * set = make_set(10, 12);
  int r;

  (void) context;
  r = mailimap_uid_copy_send(stream, set, "Archive");
  mailimap_set_free(set);
  return send_crlf(stream, r);
}

static int send_move(mailstream * stream, void * context)
{
  struct mailimap_set * set = make_set(1, 3);
  int r;

  (void) context;
  r = mailimap_move_send(stream, set, "Archive");
  mailimap_set_free(set);
  return send_crlf(stream, r);
}

static int send_uid_move(mailstream * stream, void * context)
{
  struct mailimap_set * set = make_set(10, 12);
  int r;

  (void) context;
  r = mailimap_uid_move_send(stream, set, "Archive");
  mailimap_set_free(set);
  return send_crlf(stream, r);
}

int main(void)
{
  static const struct {
    const char * expected;
    imap_test_sender * sender;
  } cases[] = {
    { "data/command-sender/capability.imap", send_capability },
    { "data/command-sender/noop.imap", send_noop },
    { "data/command-sender/logout.imap", send_logout },
    { "data/command-sender/starttls.imap", send_starttls },
    { "data/command-sender/authenticate.imap", send_authenticate },
    { "data/command-sender/login.imap", send_login },
    { "data/command-sender/select.imap", send_select },
    { "data/command-sender/select-condstore.imap", send_select_condstore },
    { "data/command-sender/examine.imap", send_examine },
    { "data/command-sender/create.imap", send_create },
    { "data/command-sender/delete.imap", send_delete },
    { "data/command-sender/rename.imap", send_rename },
    { "data/command-sender/subscribe.imap", send_subscribe },
    { "data/command-sender/unsubscribe.imap", send_unsubscribe },
    { "data/command-sender/list.imap", send_list },
    { "data/command-sender/lsub.imap", send_lsub },
    { "data/command-sender/namespace.imap", send_namespace },
    { "data/command-sender/status.imap", send_status },
    { "data/command-sender/append.imap", send_append },
    { "data/command-sender/check.imap", send_check },
    { "data/command-sender/close.imap", send_close },
    { "data/command-sender/expunge.imap", send_expunge },
    { "data/command-sender/search.imap", send_search },
    { "data/command-sender/uid-search.imap", send_uid_search },
    { "data/command-sender/fetch.imap", send_fetch },
    { "data/command-sender/uid-fetch.imap", send_uid_fetch },
    { "data/command-sender/store.imap", send_store },
    { "data/command-sender/uid-store.imap", send_uid_store },
    { "data/command-sender/copy.imap", send_copy },
    { "data/command-sender/uid-copy.imap", send_uid_copy },
    { "data/command-sender/move.imap", send_move },
    { "data/command-sender/uid-move.imap", send_uid_move }
  };
  size_t i;

  for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    imap_test_expect_send_file(cases[i].expected, cases[i].sender, NULL);

  puts("command_sender_test: ok");
  return 0;
}
