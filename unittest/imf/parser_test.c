#include "imf_tests.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mailimf.h"
#include "mmapstring.h"

static MMAPString * read_fixture(const char * path)
{
  FILE * f;
  MMAPString * result;
  int ch;
  int previous = 0;

  f = fopen(path, "rb");
  assert(f != NULL);

  result = mmap_string_new("");
  assert(result != NULL);

  while ((ch = fgetc(f)) != EOF) {
    if (ch == '\n' && previous != '\r')
      assert(mmap_string_append_c(result, '\r') != NULL);
    assert(mmap_string_append_c(result, (char) ch) != NULL);
    previous = ch;
  }

  fclose(f);
  return result;
}

static void assert_parse_consumes(int r, size_t indx, const char * input)
{
  assert(r == MAILIMF_NO_ERROR);
  if (indx != strlen(input)) {
    fprintf(stderr, "parse stopped at %lu of %lu for: %s\n",
        (unsigned long) indx, (unsigned long) strlen(input), input);
    assert(indx == strlen(input));
  }
}

static struct mailimf_field * field_at(struct mailimf_fields * fields,
    int index)
{
  clistiter * iter;
  int i;

  iter = clist_begin(fields->fld_list);
  for (i = 0; i < index; i++)
    iter = clist_next(iter);

  assert(iter != NULL);
  return clist_content(iter);
}

static void check_date_time(void)
{
  static const char * dates[] = {
    "Fri, 21 Nov 1997 09:55:06 -0600",
    "Tue, 1 Jul 2003 10:52:37 +0200",
    "21 Nov 1997 09:55:06 -0600",
    "Thu,\r\n  13\r\n  Feb\r\n  1969\r\n  23:32\r\n  -0330"
  };
  struct mailimf_date_time * date_time;
  size_t i;

  for (i = 0; i < sizeof(dates) / sizeof(dates[0]); i++) {
    size_t indx = 0;
    int r;

    date_time = NULL;
    r = mailimf_date_time_parse(dates[i], strlen(dates[i]), &indx,
        &date_time);
    assert_parse_consumes(r, indx, dates[i]);
    assert(date_time != NULL);
    mailimf_date_time_free(date_time);
  }

  {
    const char * input = "Fri, 21 Nov 1997 09:55:06 -0600";
    size_t indx = 0;
    int r;

    date_time = NULL;
    r = mailimf_date_time_parse(input, strlen(input), &indx, &date_time);
    assert_parse_consumes(r, indx, input);
    assert(date_time->dt_day == 21);
    assert(date_time->dt_month == 11);
    assert(date_time->dt_year == 1997);
    assert(date_time->dt_hour == 9);
    assert(date_time->dt_min == 55);
    assert(date_time->dt_sec == 6);
    assert(date_time->dt_zone == -600);
    mailimf_date_time_free(date_time);
  }
}

static void check_lexical_tokens(void)
{
  struct mailimf_fields * fields;
  struct mailimf_field * field;
  char * value;
  size_t indx;
  int r;

  indx = 0;
  r = mailimf_fws_parse(" \t\r\n more", strlen(" \t\r\n more"), &indx);
  assert(r == MAILIMF_NO_ERROR);
  assert(indx == strlen(" \t\r\n "));

  indx = 0;
  r = mailimf_cfws_parse("(one (two))\r\n \t", strlen("(one (two))\r\n \t"),
      &indx);
  assert(r == MAILIMF_NO_ERROR);
  assert(indx == strlen("(one (two))"));

  indx = 0;
  value = NULL;
  r = mailimf_atom_parse("simple atom", strlen("simple atom"), &indx,
      &value);
  assert(r == MAILIMF_NO_ERROR);
  assert(strcmp(value, "simple") == 0);
  assert(indx == strlen("simple"));
  free(value);

  indx = 0;
  value = NULL;
  r = mailimf_quoted_string_parse("\"quoted \\\"value\\\"\"",
      strlen("\"quoted \\\"value\\\"\""), &indx, &value);
  assert_parse_consumes(r, indx, "\"quoted \\\"value\\\"\"");
  assert(strcmp(value, "quoted \"value\"") == 0);
  free(value);

  indx = 0;
  value = NULL;
  r = mailimf_word_parse("\"quoted\" rest", strlen("\"quoted\" rest"),
      &indx, &value);
  assert(r == MAILIMF_NO_ERROR);
  assert(strcmp(value, "quoted") == 0);
  assert(indx == strlen("\"quoted\""));
  free(value);

  indx = 0;
  fields = NULL;
  r = mailimf_fields_parse("Subject: one\r\n\tfolded\r\n",
      strlen("Subject: one\r\n\tfolded\r\n"), &indx,
      &fields);
  assert_parse_consumes(r, indx, "Subject: one\r\n\tfolded\r\n");
  assert(clist_count(fields->fld_list) == 1);
  field = field_at(fields, 0);
  assert(field->fld_type == MAILIMF_FIELD_SUBJECT);
  assert(strstr(field->fld_data.fld_subject->sbj_value, "one") != NULL);
  assert(strstr(field->fld_data.fld_subject->sbj_value, "folded") != NULL);
  mailimf_fields_free(fields);
}

static void check_mailbox(const char * input, const char * display_name,
    const char * addr_spec)
{
  struct mailimf_mailbox * mailbox = NULL;
  size_t indx = 0;
  int r;

  r = mailimf_mailbox_parse(input, strlen(input), &indx, &mailbox);
  assert_parse_consumes(r, indx, input);
  assert(mailbox != NULL);

  if (display_name == NULL)
    assert(mailbox->mb_display_name == NULL);
  else
    assert(strcmp(mailbox->mb_display_name, display_name) == 0);
  assert(strcmp(mailbox->mb_addr_spec, addr_spec) == 0);

  mailimf_mailbox_free(mailbox);
}

static void check_address_forms(void)
{
  struct mailimf_address_list * address_list;
  struct mailimf_address * address;
  struct mailimf_mailbox_list * mailbox_list;
  struct mailimf_fields * fields;
  struct mailimf_field * field;
  struct mailimf_mailbox * mailbox;
  size_t indx;
  int r;

  check_mailbox("simple@example.com", NULL, "simple@example.com");
  check_mailbox("\"Joe Q. Public\" <john.q.public@example.com>",
      "Joe Q. Public", "john.q.public@example.com");
  check_mailbox("Pete <pete@silly.test>", "Pete", "pete@silly.test");
  check_mailbox("\"Giant; \\\"Big\\\" Box\" <sysservices@example.net>",
      "Giant; \"Big\" Box", "sysservices@example.net");
  check_mailbox("user@[127.0.0.1]", NULL, "user@[127.0.0.1]");
  check_mailbox("admin <>", "admin", "");
  check_mailbox("\"Folded\" <wangshiyin@\r\n 258.com>", "Folded",
      "wangshiyin@258.com");

  indx = 0;
  r = mailimf_fields_parse("From: admin <>\r\n",
      strlen("From: admin <>\r\n"), &indx, &fields);
  assert_parse_consumes(r, indx, "From: admin <>\r\n");
  field = field_at(fields, 0);
  assert(field->fld_type == MAILIMF_FIELD_FROM);
  mailbox = clist_content(clist_begin(
          field->fld_data.fld_from->frm_mb_list->mb_list));
  assert(strcmp(mailbox->mb_display_name, "admin") == 0);
  assert(strcmp(mailbox->mb_addr_spec, "") == 0);
  mailimf_fields_free(fields);

  indx = 0;
  mailbox_list = NULL;
  r = mailimf_mailbox_list_parse("a@example.com, b@example.net",
      strlen("a@example.com, b@example.net"), &indx, &mailbox_list);
  assert_parse_consumes(r, indx, "a@example.com, b@example.net");
  assert(clist_count(mailbox_list->mb_list) == 2);
  mailimf_mailbox_list_free(mailbox_list);

  indx = 0;
  address = NULL;
  r = mailimf_address_parse(
      "A Group:Ed Jones <c@a.test>,joe@where.test,John <jdoe@one.test>;",
      strlen("A Group:Ed Jones <c@a.test>,joe@where.test,John <jdoe@one.test>;"),
      &indx, &address);
  assert_parse_consumes(r, indx,
      "A Group:Ed Jones <c@a.test>,joe@where.test,John <jdoe@one.test>;");
  assert(address->ad_type == MAILIMF_ADDRESS_GROUP);
  assert(strcmp(address->ad_data.ad_group->grp_display_name, "A Group") == 0);
  assert(clist_count(address->ad_data.ad_group->grp_mb_list->mb_list) == 3);
  mailimf_address_free(address);

  indx = 0;
  address_list = NULL;
  r = mailimf_address_list_parse(
      "Friends: a@example.com, b@example.net;, Undisclosed recipients:;",
      strlen("Friends: a@example.com, b@example.net;, Undisclosed recipients:;"),
      &indx, &address_list);
  assert_parse_consumes(r, indx,
      "Friends: a@example.com, b@example.net;, Undisclosed recipients:;");
  assert(clist_count(address_list->ad_list) == 2);
  mailimf_address_list_free(address_list);
}

static void check_identifier_fields(void)
{
  struct mailimf_references * references;
  char * message_id;
  size_t indx;
  int r;

  indx = 0;
  message_id = NULL;
  r = mailimf_msg_id_parse("<1234@local.machine.example>",
      strlen("<1234@local.machine.example>"), &indx, &message_id);
  assert_parse_consumes(r, indx, "<1234@local.machine.example>");
  assert(strcmp(message_id, "1234@local.machine.example") == 0);
  free(message_id);

  indx = 0;
  message_id = NULL;
  r = mailimf_msg_id_parse(
      "<NMID-105-12327863-31469233-domiagrp_prod-AA==@communication.aca\r\n\tdomia.fr>",
      strlen("<NMID-105-12327863-31469233-domiagrp_prod-AA==@communication.aca\r\n\tdomia.fr>"),
      &indx, &message_id);
  assert_parse_consumes(r, indx,
      "<NMID-105-12327863-31469233-domiagrp_prod-AA==@communication.aca\r\n\tdomia.fr>");
  assert(strcmp(message_id,
        "NMID-105-12327863-31469233-domiagrp_prod-AA==@communication.acadomia.fr") == 0);
  free(message_id);

  indx = 0;
  references = NULL;
  r = mailimf_references_parse("References: <a@example> <b@example>\r\n",
      strlen("References: <a@example> <b@example>\r\n"), &indx, &references);
  assert_parse_consumes(r, indx, "References: <a@example> <b@example>\r\n");
  assert(clist_count(references->mid_list) == 2);
  mailimf_references_free(references);
}

static void check_all_standard_fields(void)
{
  static const int expected_types[] = {
    MAILIMF_FIELD_RETURN_PATH,
    MAILIMF_FIELD_OPTIONAL_FIELD,
    MAILIMF_FIELD_RESENT_DATE,
    MAILIMF_FIELD_RESENT_FROM,
    MAILIMF_FIELD_OPTIONAL_FIELD,
    MAILIMF_FIELD_RESENT_TO,
    MAILIMF_FIELD_RESENT_CC,
    MAILIMF_FIELD_RESENT_BCC,
    MAILIMF_FIELD_RESENT_MSG_ID,
    MAILIMF_FIELD_ORIG_DATE,
    MAILIMF_FIELD_FROM,
    MAILIMF_FIELD_SENDER,
    MAILIMF_FIELD_REPLY_TO,
    MAILIMF_FIELD_TO,
    MAILIMF_FIELD_CC,
    MAILIMF_FIELD_BCC,
    MAILIMF_FIELD_MESSAGE_ID,
    MAILIMF_FIELD_IN_REPLY_TO,
    MAILIMF_FIELD_REFERENCES,
    MAILIMF_FIELD_SUBJECT,
    MAILIMF_FIELD_COMMENTS,
    MAILIMF_FIELD_KEYWORDS,
    MAILIMF_FIELD_OPTIONAL_FIELD
  };
  struct mailimf_fields * fields;
  struct mailimf_field * field;
  MMAPString * input;
  size_t indx = 0;
  size_t i;
  int r;

  input = read_fixture("data/fields/all-standard.imf");
  fields = NULL;
  r = mailimf_fields_parse(input->str, input->len, &indx, &fields);
  assert(r == MAILIMF_NO_ERROR);
  assert(indx == input->len);
  assert(clist_count(fields->fld_list) ==
      (int) (sizeof(expected_types) / sizeof(expected_types[0])));

  for (i = 0; i < sizeof(expected_types) / sizeof(expected_types[0]); i++) {
    int actual = field_at(fields, (int) i)->fld_type;

    if (actual != expected_types[i]) {
      fprintf(stderr, "field %lu has type %d, expected %d\n",
          (unsigned long) i, actual, expected_types[i]);
      assert(actual == expected_types[i]);
    }
  }

  field = field_at(fields, 0);
  assert(strcmp(field->fld_data.fld_return_path->ret_path->pt_addr_spec,
        "sender@example.org") == 0);

  field = field_at(fields, 1);
  assert(strcmp(field->fld_data.fld_optional_field->fld_name,
        "Received") == 0);

  field = field_at(fields, 14);
  assert(clist_count(field->fld_data.fld_cc->cc_addr_list->ad_list) == 1);

  field = field_at(fields, 15);
  assert(field->fld_data.fld_bcc->bcc_addr_list == NULL);

  field = field_at(fields, 16);
  assert(strcmp(field->fld_data.fld_message_id->mid_value,
        "5678.21-Nov-1997@example.com") == 0);

  field = field_at(fields, 19);
  assert(strcmp(field->fld_data.fld_subject->sbj_value, "Saying Hello") == 0);

  field = field_at(fields, 22);
  assert(strcmp(field->fld_data.fld_optional_field->fld_name, "X-Trace") == 0);
  assert(strcmp(field->fld_data.fld_optional_field->fld_value,
        "custom optional field") == 0);

  mailimf_fields_free(fields);
  mmap_string_free(input);
}

static void check_message_file(const char * path)
{
  struct mailimf_message * message = NULL;
  MMAPString * input;
  size_t indx = 0;
  int r;

  printf("testing %s\n", path);

  input = read_fixture(path);
  r = mailimf_message_parse(input->str, input->len, &indx, &message);
  assert(r == MAILIMF_NO_ERROR);
  assert(indx == input->len);
  assert(message != NULL);
  assert(message->msg_fields != NULL);
  assert(clist_count(message->msg_fields->fld_list) > 0);
  assert(message->msg_body != NULL);
  assert(message->msg_body->bd_size > 0);

  mailimf_message_free(message);
  mmap_string_free(input);
}

static void check_rfc822_style_messages(void)
{
  static const char * paths[] = {
    "data/messages/simple-rfc822.eml",
    "data/messages/folded-comments-rfc822.eml",
    "data/messages/resent-trace-rfc822.eml",
  };
  size_t i;

  for (i = 0; i < sizeof(paths) / sizeof(paths[0]); i++)
    check_message_file(paths[i]);
}

int imf_parser_test_run(void)
{
  check_date_time();
  check_lexical_tokens();
  check_address_forms();
  check_identifier_fields();
  check_all_standard_fields();
  check_rfc822_style_messages();

  puts("parser_test: ok");
  return 0;
}
