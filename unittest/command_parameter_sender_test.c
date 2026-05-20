#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clist.h"
#include "imap_test_utils.h"
#include "mailimap_sender.h"
#include "mailimap_types.h"

static char * copy_string(const char * str)
{
  size_t len = strlen(str) + 1;
  char * result = malloc(len);

  assert(result != NULL);
  memcpy(result, str, len);
  return result;
}

static int * copy_int(int value)
{
  int * result = malloc(sizeof(* result));

  assert(result != NULL);
  *result = value;
  return result;
}

static struct mailimap_set * make_set_item(uint32_t first, uint32_t last)
{
  clist * list = clist_new();
  struct mailimap_set_item * item = mailimap_set_item_new(first, last);

  assert(list != NULL);
  assert(item != NULL);
  assert(clist_append(list, item) == 0);
  return mailimap_set_new(list);
}

static struct mailimap_set * make_two_item_set(void)
{
  clist * list = clist_new();

  assert(list != NULL);
  assert(clist_append(list, mailimap_set_item_new(1, 1)) == 0);
  assert(clist_append(list, mailimap_set_item_new(3, 5)) == 0);
  return mailimap_set_new(list);
}

static struct mailimap_flag_list * make_all_flag_list(void)
{
  clist * list = clist_new();

  assert(list != NULL);
  assert(clist_append(list, mailimap_flag_new(MAILIMAP_FLAG_ANSWERED,
        NULL, NULL)) == 0);
  assert(clist_append(list, mailimap_flag_new(MAILIMAP_FLAG_FLAGGED,
        NULL, NULL)) == 0);
  assert(clist_append(list, mailimap_flag_new(MAILIMAP_FLAG_DELETED,
        NULL, NULL)) == 0);
  assert(clist_append(list, mailimap_flag_new(MAILIMAP_FLAG_SEEN,
        NULL, NULL)) == 0);
  assert(clist_append(list, mailimap_flag_new(MAILIMAP_FLAG_DRAFT,
        NULL, NULL)) == 0);
  assert(clist_append(list, mailimap_flag_new(MAILIMAP_FLAG_KEYWORD,
        copy_string("$Forwarded"), NULL)) == 0);
  assert(clist_append(list, mailimap_flag_new(MAILIMAP_FLAG_EXTENSION,
        NULL, copy_string("\\Foo"))) == 0);
  return mailimap_flag_list_new(list);
}

static struct mailimap_status_att_list * make_full_status_att_list(void)
{
  static const int atts[] = {
    MAILIMAP_STATUS_ATT_MESSAGES,
    MAILIMAP_STATUS_ATT_RECENT,
    MAILIMAP_STATUS_ATT_UIDNEXT,
    MAILIMAP_STATUS_ATT_UIDVALIDITY,
    MAILIMAP_STATUS_ATT_UNSEEN,
    MAILIMAP_STATUS_ATT_HIGHESTMODSEQ,
    MAILIMAP_STATUS_ATT_SIZE
  };
  clist * list = clist_new();
  size_t i;

  assert(list != NULL);
  for (i = 0; i < sizeof(atts) / sizeof(atts[0]); i++)
    assert(clist_append(list, copy_int(atts[i])) == 0);

  return mailimap_status_att_list_new(list);
}

static struct mailimap_date * make_date(void)
{
  struct mailimap_date * date = mailimap_date_new(17, 7, 1996);

  assert(date != NULL);
  return date;
}

static struct mailimap_search_key * search_key(int type)
{
  return mailimap_search_key_new(type,
      NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
      NULL, NULL, NULL, NULL, 0, NULL, NULL, NULL, NULL, NULL, NULL,
      0, NULL, NULL, NULL);
}

static struct mailimap_section * make_header_section(void)
{
  struct mailimap_section_msgtext * msgtext =
    mailimap_section_msgtext_new(MAILIMAP_SECTION_MSGTEXT_HEADER, NULL);
  struct mailimap_section_spec * spec;

  assert(msgtext != NULL);
  spec = mailimap_section_spec_new(MAILIMAP_SECTION_SPEC_SECTION_MSGTEXT,
      msgtext, NULL, NULL);
  assert(spec != NULL);
  return mailimap_section_new(spec);
}

static struct mailimap_section * make_part_mime_section(void)
{
  clist * ids = clist_new();
  struct mailimap_section_part * part;
  struct mailimap_section_text * text;
  struct mailimap_section_spec * spec;

  assert(ids != NULL);
  assert(clist_append(ids, copy_int(1)) == 0);
  assert(clist_append(ids, copy_int(2)) == 0);

  part = mailimap_section_part_new(ids);
  assert(part != NULL);
  text = mailimap_section_text_new(MAILIMAP_SECTION_TEXT_MIME, NULL);
  assert(text != NULL);
  spec = mailimap_section_spec_new(MAILIMAP_SECTION_SPEC_SECTION_PART,
      NULL, part, text);
  assert(spec != NULL);
  return mailimap_section_new(spec);
}

static int send_crlf(mailstream * stream, int r)
{
  if (r != MAILIMAP_NO_ERROR)
    return r;
  return mailimap_crlf_send(stream);
}

static int send_select_quoted_mailbox(mailstream * stream, void * context)
{
  (void) context;
  return send_crlf(stream, mailimap_select_send(stream, "Project Mail", 0));
}

static int send_login_quoted_credentials(mailstream * stream, void * context)
{
  (void) context;
  return send_crlf(stream, mailimap_login_send(stream, "user name",
        "sec ret"));
}

static int send_append_minimal(mailstream * stream, void * context)
{
  (void) context;
  return mailimap_append_send(stream, "Project Mail", NULL, NULL, 5);
}

static int send_copy_wildcard_set(mailstream * stream, void * context)
{
  struct mailimap_set * set = make_set_item(0, 0);
  int r;

  (void) context;
  r = mailimap_copy_send(stream, set, "Archive 2026");
  mailimap_set_free(set);
  return send_crlf(stream, r);
}

static int send_uid_copy_open_range(mailstream * stream, void * context)
{
  struct mailimap_set * set = make_set_item(10, 0);
  int r;

  (void) context;
  r = mailimap_uid_copy_send(stream, set, "Archive 2026");
  mailimap_set_free(set);
  return send_crlf(stream, r);
}

static int send_fetch_sections(mailstream * stream, void * context)
{
  clist * list = clist_new();
  struct mailimap_set * set = make_two_item_set();
  struct mailimap_fetch_type * fetch_type;
  int r;

  (void) context;
  assert(list != NULL);
  assert(clist_append(list, mailimap_fetch_att_new(
        MAILIMAP_FETCH_ATT_BODY_SECTION, make_header_section(), 0, 0,
        NULL)) == 0);
  assert(clist_append(list, mailimap_fetch_att_new(
        MAILIMAP_FETCH_ATT_BODY_PEEK_SECTION, make_part_mime_section(), 4, 8,
        NULL)) == 0);
  fetch_type = mailimap_fetch_type_new(MAILIMAP_FETCH_TYPE_FETCH_ATT_LIST,
      NULL, list);
  assert(fetch_type != NULL);

  r = mailimap_fetch_send(stream, set, fetch_type);
  mailimap_fetch_type_free(fetch_type);
  mailimap_set_free(set);
  return send_crlf(stream, r);
}

static int send_fetch_static_attrs(mailstream * stream, void * context)
{
  static const int attrs[] = {
    MAILIMAP_FETCH_ATT_ENVELOPE,
    MAILIMAP_FETCH_ATT_FLAGS,
    MAILIMAP_FETCH_ATT_INTERNALDATE,
    MAILIMAP_FETCH_ATT_RFC822,
    MAILIMAP_FETCH_ATT_RFC822_HEADER,
    MAILIMAP_FETCH_ATT_RFC822_SIZE,
    MAILIMAP_FETCH_ATT_RFC822_TEXT,
    MAILIMAP_FETCH_ATT_BODY,
    MAILIMAP_FETCH_ATT_BODYSTRUCTURE,
    MAILIMAP_FETCH_ATT_UID
  };
  clist * list = clist_new();
  struct mailimap_set * set = make_set_item(1, 1);
  struct mailimap_fetch_type * fetch_type;
  size_t i;
  int r;

  (void) context;
  assert(list != NULL);
  for (i = 0; i < sizeof(attrs) / sizeof(attrs[0]); i++)
    assert(clist_append(list, mailimap_fetch_att_new(attrs[i], NULL, 0, 0,
          NULL)) == 0);

  fetch_type = mailimap_fetch_type_new(MAILIMAP_FETCH_TYPE_FETCH_ATT_LIST,
      NULL, list);
  assert(fetch_type != NULL);

  r = mailimap_fetch_send(stream, set, fetch_type);
  mailimap_fetch_type_free(fetch_type);
  mailimap_set_free(set);
  return send_crlf(stream, r);
}

static int send_store_all_flags(mailstream * stream, void * context)
{
  struct mailimap_set * set = make_set_item(0, 0);
  struct mailimap_flag_list * flags = make_all_flag_list();
  struct mailimap_store_att_flags * store =
    mailimap_store_att_flags_new(0, 0, flags);
  int r;

  (void) context;
  assert(store != NULL);
  r = mailimap_store_send(stream, set, 0, 0, store);
  mailimap_store_att_flags_free(store);
  mailimap_set_free(set);
  return send_crlf(stream, r);
}

static int send_status_all_attrs(mailstream * stream, void * context)
{
  struct mailimap_status_att_list * status = make_full_status_att_list();
  int r;

  (void) context;
  r = mailimap_status_send(stream, "Project Mail", status);
  mailimap_status_att_list_free(status);
  return send_crlf(stream, r);
}

static int send_search_strings(mailstream * stream, void * context)
{
  struct mailimap_search_key * key = mailimap_search_key_new(
      MAILIMAP_SEARCH_KEY_FROM,
      NULL, NULL, NULL, NULL, copy_string("Alice Smith"), NULL,
      NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL,
      NULL, NULL, NULL, NULL, NULL, 0, NULL, NULL, NULL);
  int r;

  (void) context;
  assert(key != NULL);
  r = mailimap_search_send(stream, "UTF-8", key);
  mailimap_search_key_free(key);
  return send_crlf(stream, r);
}

static int send_search_dates_and_sizes(mailstream * stream, void * context)
{
  clist * list = clist_new();
  struct mailimap_search_key * key;
  int r;

  (void) context;
  assert(list != NULL);
  assert(clist_append(list, mailimap_search_key_new(
        MAILIMAP_SEARCH_KEY_BEFORE, NULL, make_date(), NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL,
        NULL, NULL, NULL, NULL, NULL, 0, NULL, NULL, NULL)) == 0);
  assert(clist_append(list, mailimap_search_key_new(
        MAILIMAP_SEARCH_KEY_LARGER, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 4096, NULL,
        NULL, NULL, NULL, NULL, NULL, 0, NULL, NULL, NULL)) == 0);
  assert(clist_append(list, mailimap_search_key_new(
        MAILIMAP_SEARCH_KEY_SMALLER, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL,
        NULL, NULL, NULL, NULL, NULL, 8192, NULL, NULL, NULL)) == 0);

  key = mailimap_search_key_new(MAILIMAP_SEARCH_KEY_MULTIPLE,
      NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
      NULL, NULL, NULL, NULL, 0, NULL, NULL, NULL, NULL, NULL, NULL,
      0, NULL, NULL, list);
  assert(key != NULL);
  r = mailimap_search_send(stream, NULL, key);
  mailimap_search_key_free(key);
  return send_crlf(stream, r);
}

static int send_search_boolean_and_sets(mailstream * stream, void * context)
{
  struct mailimap_search_key * seen = search_key(MAILIMAP_SEARCH_KEY_SEEN);
  struct mailimap_search_key * deleted =
    search_key(MAILIMAP_SEARCH_KEY_DELETED);
  struct mailimap_search_key * or_key;
  struct mailimap_search_key * not_key;
  struct mailimap_search_key * uid_key;
  clist * list = clist_new();
  struct mailimap_search_key * multiple;
  int r;

  (void) context;
  assert(seen != NULL);
  assert(deleted != NULL);
  or_key = mailimap_search_key_new(MAILIMAP_SEARCH_KEY_OR,
      NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
      NULL, NULL, NULL, NULL, 0, NULL, seen, deleted, NULL, NULL, NULL,
      0, NULL, NULL, NULL);
  assert(or_key != NULL);

  not_key = mailimap_search_key_new(MAILIMAP_SEARCH_KEY_NOT,
      NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
      NULL, NULL, NULL, NULL, 0, search_key(MAILIMAP_SEARCH_KEY_FLAGGED),
      NULL, NULL, NULL, NULL, NULL, 0, NULL, NULL, NULL);
  assert(not_key != NULL);

  uid_key = mailimap_search_key_new(MAILIMAP_SEARCH_KEY_UID,
      NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
      NULL, NULL, NULL, NULL, 0, NULL, NULL, NULL, NULL, NULL, NULL,
      0, make_set_item(10, 0), NULL, NULL);
  assert(uid_key != NULL);

  assert(list != NULL);
  assert(clist_append(list, or_key) == 0);
  assert(clist_append(list, not_key) == 0);
  assert(clist_append(list, uid_key) == 0);

  multiple = mailimap_search_key_new(MAILIMAP_SEARCH_KEY_MULTIPLE,
      NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
      NULL, NULL, NULL, NULL, 0, NULL, NULL, NULL, NULL, NULL, NULL,
      0, NULL, NULL, list);
  assert(multiple != NULL);
  r = mailimap_search_send(stream, NULL, multiple);
  mailimap_search_key_free(multiple);
  return send_crlf(stream, r);
}

static int send_search_all_string_keys(mailstream * stream, void * context)
{
  clist * list = clist_new();
  struct mailimap_search_key * multiple;
  int r;

  (void) context;
  assert(list != NULL);
  assert(clist_append(list, mailimap_search_key_new(
        MAILIMAP_SEARCH_KEY_BCC, copy_string("bcc value"), NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        0, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, NULL, NULL)) == 0);
  assert(clist_append(list, mailimap_search_key_new(
        MAILIMAP_SEARCH_KEY_BODY, NULL, NULL, copy_string("body value"),
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        0, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, NULL, NULL)) == 0);
  assert(clist_append(list, mailimap_search_key_new(
        MAILIMAP_SEARCH_KEY_CC, NULL, NULL, NULL, copy_string("cc value"),
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0,
        NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, NULL, NULL)) == 0);
  assert(clist_append(list, mailimap_search_key_new(
        MAILIMAP_SEARCH_KEY_SUBJECT, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, copy_string("subject value"), NULL, NULL, NULL, NULL,
        NULL, 0, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, NULL,
        NULL)) == 0);
  assert(clist_append(list, mailimap_search_key_new(
        MAILIMAP_SEARCH_KEY_TEXT, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, copy_string("text value"), NULL, NULL, NULL,
        NULL, 0, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, NULL,
        NULL)) == 0);
  assert(clist_append(list, mailimap_search_key_new(
        MAILIMAP_SEARCH_KEY_TO, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, copy_string("to value"), NULL, NULL, NULL, 0,
        NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, NULL, NULL)) == 0);

  multiple = mailimap_search_key_new(MAILIMAP_SEARCH_KEY_MULTIPLE,
      NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
      NULL, NULL, NULL, NULL, 0, NULL, NULL, NULL, NULL, NULL, NULL,
      0, NULL, NULL, list);
  assert(multiple != NULL);
  r = mailimap_search_send(stream, "UTF-8", multiple);
  mailimap_search_key_free(multiple);
  return send_crlf(stream, r);
}

static int send_search_keyword_header_dates(mailstream * stream, void * context)
{
  clist * list = clist_new();
  struct mailimap_search_key * multiple;
  int r;

  (void) context;
  assert(list != NULL);
  assert(clist_append(list, mailimap_search_key_new(
        MAILIMAP_SEARCH_KEY_KEYWORD, NULL, NULL, NULL, NULL, NULL,
        copy_string("$Forwarded"), NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, 0, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL,
        NULL, NULL)) == 0);
  assert(clist_append(list, mailimap_search_key_new(
        MAILIMAP_SEARCH_KEY_UNKEYWORD, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, copy_string("$Junk"), NULL,
        NULL, 0, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, NULL,
        NULL)) == 0);
  assert(clist_append(list, mailimap_search_key_new(
        MAILIMAP_SEARCH_KEY_HEADER, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, copy_string("X-Test"),
        copy_string("value"), 0, NULL, NULL, NULL, NULL, NULL, NULL, 0,
        NULL, NULL, NULL)) == 0);
  assert(clist_append(list, mailimap_search_key_new(
        MAILIMAP_SEARCH_KEY_ON, NULL, NULL, NULL, NULL, NULL, NULL,
        make_date(), NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL,
        NULL, NULL, NULL, NULL, NULL, 0, NULL, NULL, NULL)) == 0);
  assert(clist_append(list, mailimap_search_key_new(
        MAILIMAP_SEARCH_KEY_SINCE, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, make_date(), NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL,
        NULL, NULL, NULL, NULL, NULL, 0, NULL, NULL, NULL)) == 0);
  assert(clist_append(list, mailimap_search_key_new(
        MAILIMAP_SEARCH_KEY_SENTBEFORE, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL,
        NULL, NULL, make_date(), NULL, NULL, 0, NULL, NULL, NULL)) == 0);
  assert(clist_append(list, mailimap_search_key_new(
        MAILIMAP_SEARCH_KEY_SENTON, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, NULL,
        NULL, NULL, make_date(), NULL, 0, NULL, NULL, NULL)) == 0);
  assert(clist_append(list, mailimap_search_key_new(
        MAILIMAP_SEARCH_KEY_SENTSINCE, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL,
        NULL, NULL, NULL, NULL, make_date(), 0, NULL, NULL, NULL)) == 0);

  multiple = mailimap_search_key_new(MAILIMAP_SEARCH_KEY_MULTIPLE,
      NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
      NULL, NULL, NULL, NULL, 0, NULL, NULL, NULL, NULL, NULL, NULL,
      0, NULL, NULL, list);
  assert(multiple != NULL);
  r = mailimap_search_send(stream, "UTF-8", multiple);
  mailimap_search_key_free(multiple);
  return send_crlf(stream, r);
}

static int send_search_all_flag_keys(mailstream * stream, void * context)
{
  static const int types[] = {
    MAILIMAP_SEARCH_KEY_ALL,
    MAILIMAP_SEARCH_KEY_ANSWERED,
    MAILIMAP_SEARCH_KEY_DELETED,
    MAILIMAP_SEARCH_KEY_FLAGGED,
    MAILIMAP_SEARCH_KEY_NEW,
    MAILIMAP_SEARCH_KEY_OLD,
    MAILIMAP_SEARCH_KEY_RECENT,
    MAILIMAP_SEARCH_KEY_SEEN,
    MAILIMAP_SEARCH_KEY_UNANSWERED,
    MAILIMAP_SEARCH_KEY_UNDELETED,
    MAILIMAP_SEARCH_KEY_UNFLAGGED,
    MAILIMAP_SEARCH_KEY_UNSEEN,
    MAILIMAP_SEARCH_KEY_DRAFT,
    MAILIMAP_SEARCH_KEY_UNDRAFT
  };
  clist * list = clist_new();
  struct mailimap_search_key * multiple;
  size_t i;
  int r;

  (void) context;
  assert(list != NULL);
  for (i = 0; i < sizeof(types) / sizeof(types[0]); i++)
    assert(clist_append(list, search_key(types[i])) == 0);

  multiple = mailimap_search_key_new(MAILIMAP_SEARCH_KEY_MULTIPLE,
      NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
      NULL, NULL, NULL, NULL, 0, NULL, NULL, NULL, NULL, NULL, NULL,
      0, NULL, NULL, list);
  assert(multiple != NULL);
  r = mailimap_search_send(stream, NULL, multiple);
  mailimap_search_key_free(multiple);
  return send_crlf(stream, r);
}

int main(void)
{
  static const struct {
    const char * expected;
    imap_test_sender * sender;
  } cases[] = {
    { "data/command-parameters/select-quoted-mailbox.imap",
      send_select_quoted_mailbox },
    { "data/command-parameters/login-quoted-credentials.imap",
      send_login_quoted_credentials },
    { "data/command-parameters/append-minimal.imap", send_append_minimal },
    { "data/command-parameters/copy-wildcard-set.imap",
      send_copy_wildcard_set },
    { "data/command-parameters/uid-copy-open-range.imap",
      send_uid_copy_open_range },
    { "data/command-parameters/fetch-sections.imap", send_fetch_sections },
    { "data/command-parameters/fetch-static-attrs.imap",
      send_fetch_static_attrs },
    { "data/command-parameters/store-all-flags.imap", send_store_all_flags },
    { "data/command-parameters/status-all-attrs.imap", send_status_all_attrs },
    { "data/command-parameters/search-strings.imap", send_search_strings },
    { "data/command-parameters/search-dates-and-sizes.imap",
      send_search_dates_and_sizes },
    { "data/command-parameters/search-boolean-and-sets.imap",
      send_search_boolean_and_sets },
    { "data/command-parameters/search-all-string-keys.imap",
      send_search_all_string_keys },
    { "data/command-parameters/search-keyword-header-dates.imap",
      send_search_keyword_header_dates },
    { "data/command-parameters/search-all-flag-keys.imap",
      send_search_all_flag_keys }
  };
  size_t i;

  for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    imap_test_expect_send_file(cases[i].expected, cases[i].sender, NULL);

  puts("command_parameter_sender_test: ok");
  return 0;
}
