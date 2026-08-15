#import <XCTest/XCTest.h>

#include "command_sender_test.h"

@interface IMAPCommandSenderTests : XCTestCase
@end


@implementation IMAPCommandSenderTests

static void recordCommandSenderFailure(const char *file, unsigned line,
    const char *expression, const char *message, void *context)
{
  IMAPCommandSenderTests *test = (__bridge IMAPCommandSenderTests *) context;
  XCTSourceCodeLocation *location = [[XCTSourceCodeLocation alloc]
      initWithFilePath:[NSString stringWithUTF8String:file] lineNumber:line];
  XCTIssue *issue = [[XCTIssue alloc]
      initWithType:XCTIssueTypeAssertionFailure
      compactDescription:[NSString stringWithFormat:@"%s: %s", expression, message]
      detailedDescription:nil
      sourceCodeContext:[[XCTSourceCodeContext alloc] initWithLocation:location]
      associatedError:nil attachments:@[]];
  [test recordIssue:issue];
}

- (void)runCase:(size_t)index
{
  NSString *root = [[NSBundle bundleForClass:self.class]
      pathForResource:@"command-sender" ofType:nil];
  XCTAssertNotNil(root, @"The command-sender fixtures are missing");
  if (root == nil) return;
  int result = imap_command_sender_test_run_case(index,
      root.fileSystemRepresentation, recordCommandSenderFailure,
      (__bridge void *) self);
  XCTAssertEqual(result, 0);
}

#define COMMAND_TEST(NAME, INDEX) - (void)test##NAME { [self runCase:INDEX]; }
COMMAND_TEST(Capability, 0)
COMMAND_TEST(Noop, 1)
COMMAND_TEST(Logout, 2)
COMMAND_TEST(StartTLS, 3)
COMMAND_TEST(Authenticate, 4)
COMMAND_TEST(Login, 5)
COMMAND_TEST(Select, 6)
COMMAND_TEST(SelectCondstore, 7)
COMMAND_TEST(Examine, 8)
COMMAND_TEST(Create, 9)
COMMAND_TEST(Delete, 10)
COMMAND_TEST(Rename, 11)
COMMAND_TEST(Subscribe, 12)
COMMAND_TEST(Unsubscribe, 13)
COMMAND_TEST(List, 14)
COMMAND_TEST(LSub, 15)
COMMAND_TEST(Namespace, 16)
COMMAND_TEST(Status, 17)
COMMAND_TEST(Append, 18)
COMMAND_TEST(Check, 19)
COMMAND_TEST(Close, 20)
COMMAND_TEST(Expunge, 21)
COMMAND_TEST(Search, 22)
COMMAND_TEST(UIDSearch, 23)
COMMAND_TEST(Fetch, 24)
COMMAND_TEST(UIDFetch, 25)
COMMAND_TEST(Store, 26)
COMMAND_TEST(UIDStore, 27)
COMMAND_TEST(Copy, 28)
COMMAND_TEST(UIDCopy, 29)
COMMAND_TEST(Move, 30)
COMMAND_TEST(UIDMove, 31)
#undef COMMAND_TEST

@end
