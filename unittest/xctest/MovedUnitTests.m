#import <XCTest/XCTest.h>

#include <unistd.h>

extern int gmail_http_test_main(void);
extern int jmap_blob_test_main(void);
extern int jmap_call_test_main(void);
extern int jmap_email_test_main(void);
extern int jmap_http_test_main(void);
extern int jmap_json_test_main(void);
extern int jmap_mailbox_test_main(void);
extern int jmap_request_test_main(void);
extern int jmap_response_test_main(void);
extern int jmap_session_test_main(void);
extern int jmap_thread_test_main(void);
extern int mailpgp_low_level_test_main(void);
extern int smime_low_level_test_main(void);

static int runFromRepositoryRoot(int (*testMain)(void))
{
  char previousDirectory[PATH_MAX];
  NSString *sourcePath = [NSString stringWithUTF8String:__FILE__];
  NSString *root = [[sourcePath stringByDeletingLastPathComponent]
      stringByDeletingLastPathComponent];
  root = [root stringByDeletingLastPathComponent];

  if ((getcwd(previousDirectory, sizeof(previousDirectory)) == NULL) ||
      (chdir(root.fileSystemRepresentation) != 0))
    return -1;
  int result = testMain();
  if (chdir(previousDirectory) != 0)
    return -1;
  return result;
}

#define DEFINE_MOVED_TEST_CASE(CLASS_NAME, METHOD_NAME, ENTRY_POINT) \
  @interface CLASS_NAME : XCTestCase @end \
  @implementation CLASS_NAME \
  - (void)METHOD_NAME { XCTAssertEqual(runFromRepositoryRoot(ENTRY_POINT), 0); } \
  @end

DEFINE_MOVED_TEST_CASE(GmailHTTPTests, testGmailHTTP, gmail_http_test_main)
DEFINE_MOVED_TEST_CASE(JMAPBlobTests, testJMAPBlob, jmap_blob_test_main)
DEFINE_MOVED_TEST_CASE(JMAPCallTests, testJMAPCall, jmap_call_test_main)
DEFINE_MOVED_TEST_CASE(JMAPEmailTests, testJMAPEmail, jmap_email_test_main)
DEFINE_MOVED_TEST_CASE(JMAPHTTPTests, testJMAPHTTP, jmap_http_test_main)
DEFINE_MOVED_TEST_CASE(JMAPJSONTests, testJMAPJSON, jmap_json_test_main)
DEFINE_MOVED_TEST_CASE(JMAPMailboxTests, testJMAPMailbox, jmap_mailbox_test_main)
DEFINE_MOVED_TEST_CASE(JMAPRequestTests, testJMAPRequest, jmap_request_test_main)
DEFINE_MOVED_TEST_CASE(JMAPResponseTests, testJMAPResponse, jmap_response_test_main)
DEFINE_MOVED_TEST_CASE(JMAPSessionTests, testJMAPSession, jmap_session_test_main)
DEFINE_MOVED_TEST_CASE(JMAPThreadTests, testJMAPThread, jmap_thread_test_main)
DEFINE_MOVED_TEST_CASE(MailPGPLowLevelTests, testMailPGPLowLevel,
    mailpgp_low_level_test_main)
DEFINE_MOVED_TEST_CASE(SMIMELowLevelTests, testSMIMELowLevel,
    smime_low_level_test_main)
