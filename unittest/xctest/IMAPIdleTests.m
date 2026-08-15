#import <XCTest/XCTest.h>

#include "idle_test.h"

@interface IMAPIdleTests : XCTestCase
@end


@implementation IMAPIdleTests

static void recordIdleFailure(const char *file, unsigned line,
    const char *expression, const char *message, void *context)
{
  IMAPIdleTests *test = (__bridge IMAPIdleTests *) context;
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
  int result = imap_idle_test_run_case(index, recordIdleFailure,
      (__bridge void *) self);
  XCTAssertEqual(result, 0);
}

- (void)testImmediateContinuation { [self runCase:0]; }
- (void)testUpdatesBeforeContinuation { [self runCase:1]; }
- (void)testFlagsBeforeContinuation { [self runCase:2]; }
- (void)testQueuedUpdatesBeforeTaggedOK { [self runCase:3]; }
- (void)testNORejection { [self runCase:4]; }
- (void)testBADRejection { [self runCase:5]; }

@end
