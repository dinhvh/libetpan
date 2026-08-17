#import <XCTest/XCTest.h>

#include <libetpan/mailgmail.h>

#include "../../src/low-level/gmail/mailgmail_parser.h"
#include "../../src/low-level/gmail/mailgmail_url.h"

#include <stdlib.h>
#include <string.h>

@interface GmailTests : XCTestCase
@end

static NSString *StringFromCString(const char *value)
{
  return value == NULL ? nil : [NSString stringWithUTF8String:value];
}

@implementation GmailTests

- (void)testListMessagesURL
{
  mailgmail *session = mailgmail_new();
  struct mailgmail_message_list_request *request =
      mailgmail_message_list_request_new();
  char *url = NULL;

  XCTAssertNotEqual(session, NULL);
  XCTAssertNotEqual(request, NULL);
  if ((session == NULL) || (request == NULL))
    goto cleanup;

  request->max_results = 25;
  request->include_spam_trash = 1;
  XCTAssertEqual(mailgmail_message_list_request_set_query(request,
      "from:a@example.com"), MAILGMAIL_NO_ERROR);
  XCTAssertEqual(mailgmail_message_list_request_set_page_token(request,
      "page token"), MAILGMAIL_NO_ERROR);
  XCTAssertEqual(mailgmail_message_list_request_add_label_id(request, "INBOX"),
      MAILGMAIL_NO_ERROR);
  XCTAssertEqual(mailgmail_url_messages_list(session, request, &url),
      MAILGMAIL_NO_ERROR);
  XCTAssertEqualObjects(StringFromCString(url),
      @"https://gmail.googleapis.com/gmail/v1/users/me/messages"
       "?maxResults=25&pageToken=page%20token&q=from%3Aa%40example.com"
       "&labelIds=INBOX&includeSpamTrash=true");

cleanup:
  free(url);
  mailgmail_message_list_request_free(request);
  mailgmail_free(session);
}

- (void)testListMessagesMaxResultsLimit
{
  mailgmail *session = mailgmail_new();
  struct mailgmail_message_list_request *request =
      mailgmail_message_list_request_new();
  char *url = NULL;

  XCTAssertNotEqual(session, NULL);
  XCTAssertNotEqual(request, NULL);
  if ((session != NULL) && (request != NULL)) {
    request->max_results = 501;
    XCTAssertEqual(mailgmail_url_messages_list(session, request, &url),
        MAILGMAIL_ERROR_BAD_STATE);
  }

  free(url);
  mailgmail_message_list_request_free(request);
  mailgmail_free(session);
}

- (void)testGetMessageMetadataURL
{
  mailgmail *session = mailgmail_new();
  struct mailgmail_message_get_request *request =
      mailgmail_message_get_request_new(MAILGMAIL_MESSAGE_FORMAT_METADATA);
  char *url = NULL;

  XCTAssertNotEqual(session, NULL);
  XCTAssertNotEqual(request, NULL);
  if ((session == NULL) || (request == NULL))
    goto cleanup;

  XCTAssertEqual(mailgmail_message_get_request_add_metadata_header(request,
      "Subject"), MAILGMAIL_NO_ERROR);
  XCTAssertEqual(mailgmail_message_get_request_add_metadata_header(request,
      "X Custom"), MAILGMAIL_NO_ERROR);
  XCTAssertEqual(mailgmail_url_message_get(session, "msg/id", request, &url),
      MAILGMAIL_NO_ERROR);
  XCTAssertEqualObjects(StringFromCString(url),
      @"https://gmail.googleapis.com/gmail/v1/users/me/messages/msg%2Fid"
       "?format=metadata&metadataHeaders=Subject&metadataHeaders=X%20Custom");

cleanup:
  free(url);
  mailgmail_message_get_request_free(request);
  mailgmail_free(session);
}

- (void)testMessageListParsing
{
  static const char json[] =
      "{\"messages\":[{\"id\":\"m1\",\"threadId\":\"t1\"},"
      "{\"id\":\"m2\",\"threadId\":\"t2\"}],"
      "\"nextPageToken\":\"next\",\"resultSizeEstimate\":2}";
  struct mailgmail_message_list *list = NULL;

  XCTAssertEqual(mailgmail_parser_parse_message_list(json, strlen(json), &list),
      MAILGMAIL_NO_ERROR);
  XCTAssertNotEqual(list, NULL);
  if (list == NULL)
    return;

  XCTAssertEqual(clist_count(list->messages), 2U);
  XCTAssertEqualObjects(StringFromCString(list->next_page_token), @"next");
  XCTAssertEqual(list->result_size_estimate, 2U);
  struct mailgmail_message_summary *first =
      clist_content(clist_begin(list->messages));
  XCTAssertEqualObjects(StringFromCString(first->id), @"m1");
  XCTAssertEqualObjects(StringFromCString(first->thread_id), @"t1");
  mailgmail_message_list_free(list);
}

- (void)testProfileParsing
{
  static const char json[] =
      "{\"emailAddress\":\"user@example.com\",\"messagesTotal\":10,"
      "\"threadsTotal\":3,\"historyId\":\"123456\"}";
  struct mailgmail_profile *profile = NULL;

  XCTAssertEqual(mailgmail_parser_parse_profile(json, strlen(json), &profile),
      MAILGMAIL_NO_ERROR);
  XCTAssertNotEqual(profile, NULL);
  if (profile == NULL)
    return;

  XCTAssertEqualObjects(StringFromCString(profile->email_address),
      @"user@example.com");
  XCTAssertEqual(profile->messages_total, 10U);
  XCTAssertEqual(profile->threads_total, 3U);
  XCTAssertEqualObjects(StringFromCString(profile->history_id), @"123456");
  mailgmail_profile_free(profile);
}

- (void)testLabelListParsing
{
  static const char json[] =
      "{\"labels\":[{\"id\":\"INBOX\",\"name\":\"INBOX\","
      "\"messageListVisibility\":\"show\","
      "\"labelListVisibility\":\"labelShow\",\"type\":\"system\"},"
      "{\"id\":\"Label_1\",\"name\":\"Project\","
      "\"messageListVisibility\":\"hide\","
      "\"labelListVisibility\":\"labelShowIfUnread\",\"type\":\"user\","
      "\"messagesTotal\":5,\"messagesUnread\":2,\"threadsTotal\":4,"
      "\"threadsUnread\":1}]}";
  struct mailgmail_label_list *labelList = NULL;

  XCTAssertEqual(mailgmail_parser_parse_label_list(json, strlen(json),
      &labelList), MAILGMAIL_NO_ERROR);
  XCTAssertNotEqual(labelList, NULL);
  if (labelList == NULL)
    return;

  XCTAssertEqual(clist_count(labelList->labels), 2U);
  struct mailgmail_label *label =
      clist_content(clist_next(clist_begin(labelList->labels)));
  XCTAssertEqualObjects(StringFromCString(label->id), @"Label_1");
  XCTAssertEqualObjects(StringFromCString(label->name), @"Project");
  XCTAssertEqualObjects(StringFromCString(label->type), @"user");
  XCTAssertEqual(label->messages_total, 5U);
  XCTAssertEqual(label->messages_unread, 2U);
  XCTAssertEqual(label->threads_total, 4U);
  XCTAssertEqual(label->threads_unread, 1U);
  mailgmail_label_list_free(labelList);
}

- (void)testAttachmentParsing
{
  static const char json[] =
      "{\"attachmentId\":\"att1\",\"size\":12,\"data\":\"YWJjZA\"}";
  struct mailgmail_attachment *attachment = NULL;

  XCTAssertEqual(mailgmail_parser_parse_attachment(json, strlen(json),
      &attachment), MAILGMAIL_NO_ERROR);
  XCTAssertNotEqual(attachment, NULL);
  if (attachment == NULL)
    return;

  XCTAssertEqualObjects(StringFromCString(attachment->attachment_id), @"att1");
  XCTAssertEqual(attachment->size, 12U);
  XCTAssertEqualObjects(StringFromCString(attachment->data), @"YWJjZA");
  mailgmail_attachment_free(attachment);
}

- (void)testMessageParsing
{
  static const char json[] =
      "{\"id\":\"m1\",\"threadId\":\"t1\","
      "\"labelIds\":[\"INBOX\",\"UNREAD\"],\"snippet\":\"hello\","
      "\"historyId\":\"42\",\"internalDate\":\"12345\","
      "\"sizeEstimate\":99,\"payload\":{\"partId\":\"\","
      "\"mimeType\":\"text/plain\",\"filename\":\"\","
      "\"headers\":[{\"name\":\"Subject\",\"value\":\"Hi\"}],"
      "\"body\":{\"size\":5,\"data\":\"aGVsbG8\"}}}";
  struct mailgmail_message *message = NULL;

  XCTAssertEqual(mailgmail_parser_parse_message(json, strlen(json), &message),
      MAILGMAIL_NO_ERROR);
  XCTAssertNotEqual(message, NULL);
  if (message == NULL)
    return;

  XCTAssertEqualObjects(StringFromCString(message->id), @"m1");
  XCTAssertEqualObjects(StringFromCString(message->thread_id), @"t1");
  XCTAssertEqual(clist_count(message->label_ids), 2U);
  XCTAssertEqualObjects(StringFromCString(message->payload->mime_type),
      @"text/plain");
  struct mailgmail_message_header *header =
      clist_content(clist_begin(message->payload->headers));
  XCTAssertEqualObjects(StringFromCString(header->name), @"Subject");
  XCTAssertEqualObjects(StringFromCString(header->value), @"Hi");
  XCTAssertEqualObjects(StringFromCString(message->payload->body->data),
      @"aGVsbG8");
  mailgmail_message_free(message);
}

@end
