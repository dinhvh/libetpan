/*
 * libEtPan! -- a mail stuff library
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include <libetpan/mailgmail.h>

#include "../src/low-level/gmail/mailgmail_parser.h"
#include "../src/low-level/gmail/mailgmail_url.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int check(int condition, const char * message)
{
  if (!condition) {
    fprintf(stderr, "%s\n", message);
    return 0;
  }

  return 1;
}

static int str_equal(const char * left, const char * right)
{
  return (left != NULL) && (right != NULL) && (strcmp(left, right) == 0);
}

static int test_list_messages_url(void)
{
  mailgmail * session;
  struct mailgmail_message_list_request * request;
  char * url;
  int r;
  int ok;

  session = mailgmail_new();
  request = mailgmail_message_list_request_new();
  url = NULL;
  ok = 0;

  if (!check((session != NULL) && (request != NULL), "allocation failed"))
    goto cleanup;

  request->max_results = 25;
  request->include_spam_trash = 1;
  r = mailgmail_message_list_request_set_query(request, "from:a@example.com");
  if (!check(r == MAILGMAIL_NO_ERROR, "query setter failed"))
    goto cleanup;
  r = mailgmail_message_list_request_set_page_token(request, "page token");
  if (!check(r == MAILGMAIL_NO_ERROR, "page token setter failed"))
    goto cleanup;
  r = mailgmail_message_list_request_add_label_id(request, "INBOX");
  if (!check(r == MAILGMAIL_NO_ERROR, "label setter failed"))
    goto cleanup;

  r = mailgmail_url_messages_list(session, request, &url);
  if (!check(r == MAILGMAIL_NO_ERROR, "url builder failed"))
    goto cleanup;

  ok = check(str_equal(url,
      "https://gmail.googleapis.com/gmail/v1/users/me/messages"
      "?maxResults=25&pageToken=page%20token&q=from%3Aa%40example.com"
      "&labelIds=INBOX&includeSpamTrash=true"),
      "messages.list URL mismatch");

 cleanup:
  free(url);
  mailgmail_message_list_request_free(request);
  mailgmail_free(session);
  return ok;
}

static int test_message_list_parse(void)
{
  static const char json[] =
      "{"
      "\"messages\":["
      "{\"id\":\"m1\",\"threadId\":\"t1\"},"
      "{\"id\":\"m2\",\"threadId\":\"t2\"}"
      "],"
      "\"nextPageToken\":\"next\","
      "\"resultSizeEstimate\":2"
      "}";
  struct mailgmail_message_list * list;
  struct mailgmail_message_summary * first;
  int r;
  int ok;

  list = NULL;
  r = mailgmail_parser_parse_message_list(json, strlen(json), &list);
  if (!check(r == MAILGMAIL_NO_ERROR, "message list parse failed"))
    return 0;

  first = clist_content(clist_begin(list->messages));
  ok = check(clist_count(list->messages) == 2, "message count mismatch") &&
      check(str_equal(list->next_page_token, "next"),
          "next page token mismatch") &&
      check(list->result_size_estimate == 2, "size estimate mismatch") &&
      check(str_equal(first->id, "m1"), "first message id mismatch") &&
      check(str_equal(first->thread_id, "t1"), "first thread id mismatch");

  mailgmail_message_list_free(list);
  return ok;
}

static int test_list_messages_max_results_limit(void)
{
  mailgmail * session;
  struct mailgmail_message_list_request * request;
  char * url;
  int r;
  int ok;

  session = mailgmail_new();
  request = mailgmail_message_list_request_new();
  url = NULL;
  ok = 0;

  if (!check((session != NULL) && (request != NULL), "allocation failed"))
    goto cleanup;

  request->max_results = 501;
  r = mailgmail_url_messages_list(session, request, &url);
  ok = check(r == MAILGMAIL_ERROR_BAD_STATE,
      "maxResults limit was not enforced");

 cleanup:
  free(url);
  mailgmail_message_list_request_free(request);
  mailgmail_free(session);
  return ok;
}

static int test_get_message_metadata_url(void)
{
  mailgmail * session;
  struct mailgmail_message_get_request * request;
  char * url;
  int r;
  int ok;

  session = mailgmail_new();
  request = mailgmail_message_get_request_new(MAILGMAIL_MESSAGE_FORMAT_METADATA);
  url = NULL;
  ok = 0;

  if (!check((session != NULL) && (request != NULL), "allocation failed"))
    goto cleanup;

  r = mailgmail_message_get_request_add_metadata_header(request, "Subject");
  if (!check(r == MAILGMAIL_NO_ERROR, "metadata header add failed"))
    goto cleanup;
  r = mailgmail_message_get_request_add_metadata_header(request, "X Custom");
  if (!check(r == MAILGMAIL_NO_ERROR, "metadata header add failed"))
    goto cleanup;

  r = mailgmail_url_message_get(session, "msg/id", request, &url);
  if (!check(r == MAILGMAIL_NO_ERROR, "get message URL failed"))
    goto cleanup;

  ok = check(str_equal(url,
      "https://gmail.googleapis.com/gmail/v1/users/me/messages/msg%2Fid"
      "?format=metadata&metadataHeaders=Subject&metadataHeaders=X%20Custom"),
      "messages.get metadata URL mismatch");

 cleanup:
  free(url);
  mailgmail_message_get_request_free(request);
  mailgmail_free(session);
  return ok;
}

static int test_profile_parse(void)
{
  static const char json[] =
      "{"
      "\"emailAddress\":\"user@example.com\","
      "\"messagesTotal\":10,"
      "\"threadsTotal\":3,"
      "\"historyId\":\"123456\""
      "}";
  struct mailgmail_profile * profile;
  int r;
  int ok;

  profile = NULL;
  r = mailgmail_parser_parse_profile(json, strlen(json), &profile);
  if (!check(r == MAILGMAIL_NO_ERROR, "profile parse failed"))
    return 0;

  ok = check(str_equal(profile->email_address, "user@example.com"),
      "profile email mismatch") &&
      check(profile->messages_total == 10, "profile messages mismatch") &&
      check(profile->threads_total == 3, "profile threads mismatch") &&
      check(str_equal(profile->history_id, "123456"),
          "profile history id mismatch");

  mailgmail_profile_free(profile);
  return ok;
}

static int test_label_list_parse(void)
{
  static const char json[] =
      "{"
      "\"labels\":["
      "{"
      "\"id\":\"INBOX\","
      "\"name\":\"INBOX\","
      "\"messageListVisibility\":\"show\","
      "\"labelListVisibility\":\"labelShow\","
      "\"type\":\"system\""
      "},"
      "{"
      "\"id\":\"Label_1\","
      "\"name\":\"Project\","
      "\"messageListVisibility\":\"hide\","
      "\"labelListVisibility\":\"labelShowIfUnread\","
      "\"type\":\"user\","
      "\"messagesTotal\":5,"
      "\"messagesUnread\":2,"
      "\"threadsTotal\":4,"
      "\"threadsUnread\":1"
      "}"
      "]"
      "}";
  struct mailgmail_label_list * label_list;
  struct mailgmail_label * second;
  int r;
  int ok;

  label_list = NULL;
  r = mailgmail_parser_parse_label_list(json, strlen(json), &label_list);
  if (!check(r == MAILGMAIL_NO_ERROR, "label list parse failed"))
    return 0;

  second = clist_content(clist_next(clist_begin(label_list->labels)));
  ok = check(clist_count(label_list->labels) == 2, "label count mismatch") &&
      check(str_equal(second->id, "Label_1"), "label id mismatch") &&
      check(str_equal(second->name, "Project"), "label name mismatch") &&
      check(str_equal(second->type, "user"), "label type mismatch") &&
      check(second->messages_total == 5, "label total mismatch") &&
      check(second->messages_unread == 2, "label unread mismatch") &&
      check(second->threads_total == 4, "label thread total mismatch") &&
      check(second->threads_unread == 1, "label thread unread mismatch");

  mailgmail_label_list_free(label_list);
  return ok;
}

static int test_attachment_parse(void)
{
  static const char json[] =
      "{"
      "\"attachmentId\":\"att1\","
      "\"size\":12,"
      "\"data\":\"YWJjZA\""
      "}";
  struct mailgmail_attachment * attachment;
  int r;
  int ok;

  attachment = NULL;
  r = mailgmail_parser_parse_attachment(json, strlen(json), &attachment);
  if (!check(r == MAILGMAIL_NO_ERROR, "attachment parse failed"))
    return 0;

  ok = check(str_equal(attachment->attachment_id, "att1"),
      "attachment id mismatch") &&
      check(attachment->size == 12, "attachment size mismatch") &&
      check(str_equal(attachment->data, "YWJjZA"),
          "attachment data mismatch");

  mailgmail_attachment_free(attachment);
  return ok;
}

static int test_message_parse(void)
{
  static const char json[] =
      "{"
      "\"id\":\"m1\","
      "\"threadId\":\"t1\","
      "\"labelIds\":[\"INBOX\",\"UNREAD\"],"
      "\"snippet\":\"hello\","
      "\"historyId\":\"42\","
      "\"internalDate\":\"12345\","
      "\"sizeEstimate\":99,"
      "\"payload\":{"
      "\"partId\":\"\","
      "\"mimeType\":\"text/plain\","
      "\"filename\":\"\","
      "\"headers\":[{\"name\":\"Subject\",\"value\":\"Hi\"}],"
      "\"body\":{\"size\":5,\"data\":\"aGVsbG8\"}"
      "}"
      "}";
  struct mailgmail_message * message;
  struct mailgmail_message_header * header;
  int r;
  int ok;

  message = NULL;
  r = mailgmail_parser_parse_message(json, strlen(json), &message);
  if (!check(r == MAILGMAIL_NO_ERROR, "message parse failed"))
    return 0;

  header = clist_content(clist_begin(message->payload->headers));
  ok = check(str_equal(message->id, "m1"), "message id mismatch") &&
      check(str_equal(message->thread_id, "t1"), "thread id mismatch") &&
      check(clist_count(message->label_ids) == 2, "label count mismatch") &&
      check(str_equal(message->payload->mime_type, "text/plain"),
          "payload mime type mismatch") &&
      check(str_equal(header->name, "Subject"), "header name mismatch") &&
      check(str_equal(header->value, "Hi"), "header value mismatch") &&
      check(str_equal(message->payload->body->data, "aGVsbG8"),
          "body data mismatch");

  mailgmail_message_free(message);
  return ok;
}

int main(void)
{
  if (!test_list_messages_url())
    return 1;
  if (!test_list_messages_max_results_limit())
    return 1;
  if (!test_get_message_metadata_url())
    return 1;
  if (!test_profile_parse())
    return 1;
  if (!test_label_list_parse())
    return 1;
  if (!test_attachment_parse())
    return 1;
  if (!test_message_list_parse())
    return 1;
  if (!test_message_parse())
    return 1;

  printf("gmail-http-test: ok\n");
  return 0;
}
