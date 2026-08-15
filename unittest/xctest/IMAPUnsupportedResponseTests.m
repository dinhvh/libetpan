#import <XCTest/XCTest.h>

#include "unsupported_response_test.h"

@interface IMAPUnsupportedResponseTests : XCTestCase
@end


@implementation IMAPUnsupportedResponseTests

static void recordUnsupportedFailure(const char *file, unsigned line,
    const char *expression, const char *message, void *context)
{
  IMAPUnsupportedResponseTests *test =
      (__bridge IMAPUnsupportedResponseTests *) context;
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

- (void)runCase:(size_t)index compressed:(BOOL)compressed
{
  NSString *root = [[NSBundle bundleForClass:self.class]
      pathForResource:@"unsupported" ofType:nil];
  XCTAssertNotNil(root, @"The unsupported-response fixtures are missing");
  if (root == nil) return;
  int result = imap_unsupported_response_test_run_case(index, compressed,
      root.fileSystemRepresentation, recordUnsupportedFailure,
      (__bridge void *) self);
  XCTAssertEqual(result, 0);
}

#define UNSUPPORTED_TEST(NAME, INDEX) \
- (void)test##NAME { [self runCase:INDEX compressed:NO]; } \
- (void)test##NAME##Compressed { [self runCase:INDEX compressed:YES]; }

UNSUPPORTED_TEST(ESearch, 0)
UNSUPPORTED_TEST(ListExtendedOldName, 1)
UNSUPPORTED_TEST(FetchBinary, 2)
UNSUPPORTED_TEST(FetchBinarySize, 3)
UNSUPPORTED_TEST(FetchLiteral8, 4)
UNSUPPORTED_TEST(StatusRev2DeletedSize, 5)
UNSUPPORTED_TEST(FatalBye, 6)

#undef UNSUPPORTED_TEST

@end
