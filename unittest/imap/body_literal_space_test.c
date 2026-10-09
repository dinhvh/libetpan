#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "body_literal_space_test.h"

#include "mailimap.h"
#include "mailimap_parser.h"
#include "mmapstring.h"

static int parse_response(const char * text,
    struct mailimap_response ** response)
{
  MMAPString * buffer;
  mailimap * session;
  struct mailimap_parser_context * parser_ctx;
  size_t indx;
  int r;

  * response = NULL;
  buffer = mmap_string_new("");
  assert(buffer != NULL);
  assert(mmap_string_append(buffer, text) != NULL);
  session = mailimap_new(0, NULL);
  assert(session != NULL);
  parser_ctx = mailimap_parser_context_new(session);
  assert(parser_ctx != NULL);
  indx = 0;
  r = mailimap_response_parse(NULL, buffer, parser_ctx, &indx,
      response, 0, NULL);
  if (r != MAILIMAP_NO_ERROR && * response != NULL) {
    mailimap_response_free(* response);
    * response = NULL;
  }
  mailimap_parser_context_free(parser_ctx);
  mailimap_free(session);
  mmap_string_free(buffer);
  return r;
}

static struct mailimap_msg_att_body_section *
body_section_from_response(struct mailimap_response * response)
{
  clistiter * cur;

  assert(response != NULL);
  assert(response->rsp_cont_req_or_resp_data_list != NULL);
  for (cur = clist_begin(response->rsp_cont_req_or_resp_data_list);
       cur != NULL; cur = clist_next(cur)) {
    struct mailimap_cont_req_or_resp_data * cont;
    struct mailimap_response_data * data;
    struct mailimap_message_data * msg;
    clistiter * att_cur;

    cont = clist_content(cur);
    if (cont->rsp_type != MAILIMAP_RESP_RESP_DATA)
      continue;
    data = cont->rsp_data.rsp_resp_data;
    if (data == NULL ||
        data->rsp_type != MAILIMAP_RESP_DATA_TYPE_MESSAGE_DATA)
      continue;
    msg = data->rsp_data.rsp_message_data;
    if (msg == NULL || msg->mdt_type != MAILIMAP_MESSAGE_DATA_FETCH ||
        msg->mdt_msg_att == NULL || msg->mdt_msg_att->att_list == NULL)
      continue;
    for (att_cur = clist_begin(msg->mdt_msg_att->att_list);
         att_cur != NULL; att_cur = clist_next(att_cur)) {
      struct mailimap_msg_att_item * item;
      struct mailimap_msg_att_static * st;

      item = clist_content(att_cur);
      if (item->att_type != MAILIMAP_MSG_ATT_ITEM_STATIC)
        continue;
      st = item->att_data.att_static;
      if (st != NULL && st->att_type == MAILIMAP_MSG_ATT_BODY_SECTION)
        return st->att_data.att_body_section;
    }
  }
  return NULL;
}

static void assert_literal(const char * text, uint32_t origin,
    const char * bytes, size_t length)
{
  struct mailimap_response * response;
  struct mailimap_msg_att_body_section * section;
  int r;

  r = parse_response(text, &response);
  assert(r == MAILIMAP_NO_ERROR);
  section = body_section_from_response(response);
  assert(section != NULL);
  assert(section->sec_origin_octet == origin);
  assert(section->sec_length == length);
  assert(section->sec_body_part != NULL);
  assert(memcmp(section->sec_body_part, bytes, length) == 0);
  mailimap_response_free(response);
}

static void test_missing_space_before_partial_literal(void)
{
  assert_literal(
      "* 1 FETCH (UID 1 BODY[1]<0>{4}\r\n"
      "abcd)\r\n"
      "t OK done\r\n",
      0, "abcd", 4);
}

static void test_spaced_body_literal(void)
{
  assert_literal(
      "* 1 FETCH (UID 1 BODY[1] {4}\r\n"
      "abcd)\r\n"
      "t OK done\r\n",
      0, "abcd", 4);
}

static void test_spaced_header_partial_literal(void)
{
  assert_literal(
      "* 1 FETCH (UID 1 BODY[HEADER]<0> {4}\r\n"
      "abcd)\r\n"
      "t OK done\r\n",
      0, "abcd", 4);
}

static void test_missing_space_before_literal_without_origin(void)
{
  assert_literal(
      "* 1 FETCH (UID 1 BODY[1]{4}\r\n"
      "abcd)\r\n"
      "t OK done\r\n",
      0, "abcd", 4);
}

static void test_missing_space_before_nil(void)
{
  struct mailimap_response * response;
  struct mailimap_msg_att_body_section * section;
  int r;

  r = parse_response(
      "* 1 FETCH (UID 1 BODY[1]<0>NIL)\r\n"
      "t OK done\r\n",
      &response);
  assert(r == MAILIMAP_NO_ERROR);
  section = body_section_from_response(response);
  assert(section != NULL);
  assert(section->sec_origin_octet == 0);
  assert(section->sec_body_part == NULL);
  assert(section->sec_length == 0);
  mailimap_response_free(response);
}

static void test_missing_space_before_uid_is_parse_error(void)
{
  struct mailimap_response * response;
  int r;

  r = parse_response(
      "* 1 FETCH (UID 1 BODY[1]<0>UID 1)\r\n"
      "t OK done\r\n",
      &response);
  assert(r == MAILIMAP_ERROR_PARSE);
  assert(response == NULL);
}

int imap_body_literal_space_test_run(void)
{
  test_missing_space_before_partial_literal();
  test_spaced_body_literal();
  test_spaced_header_partial_literal();
  test_missing_space_before_literal_without_origin();
  test_missing_space_before_nil();
  test_missing_space_before_uid_is_parse_error();

  puts("body_literal_space_test: ok");
  return 0;
}
