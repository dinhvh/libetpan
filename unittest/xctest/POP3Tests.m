#import <XCTest/XCTest.h>

#include "pop3_tests.h"

@interface POP3Tests : XCTestCase
@end

@implementation POP3Tests

static void recordPOP3Failure(const char *file, unsigned line,
    const char *expression, const char *message, void *context)
{
  POP3Tests *test = (__bridge POP3Tests *) context;
  NSString *description = [NSString stringWithFormat:@"%s: %s",
      expression, message];
  XCTSourceCodeLocation *location = [[XCTSourceCodeLocation alloc]
      initWithFilePath:[NSString stringWithUTF8String:file] lineNumber:line];
  XCTIssue *issue = [[XCTIssue alloc]
      initWithType:XCTIssueTypeAssertionFailure
      compactDescription:description
      detailedDescription:nil
      sourceCodeContext:[[XCTSourceCodeContext alloc] initWithLocation:location]
      associatedError:nil
      attachments:@[]];
  [test recordIssue:issue];
}

- (void)testLISTIgnoresZeroMessageNumber
{
  int result = pop3_test_run_case(recordPOP3Failure,
      (__bridge void *) self);
  XCTAssertEqual(result, 0);
}

@end
