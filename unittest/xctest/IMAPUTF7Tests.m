#import <XCTest/XCTest.h>

#include "imap_utf7_tests.h"

@interface IMAPUTF7Tests : XCTestCase
@end

@implementation IMAPUTF7Tests

static void recordUTF7Failure(const char *file, unsigned line,
    const char *expression, const char *message, void *context)
{
  IMAPUTF7Tests *test = (__bridge IMAPUTF7Tests *) context;
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

- (void)runCase:(size_t)index
{
  int result = imap_utf7_test_run(index, recordUTF7Failure,
      (__bridge void *) self);
  XCTAssertEqual(result, 0);
}

- (void)testAmpersand { [self runCase:0]; }
- (void)testMailboxPath { [self runCase:1]; }
- (void)testRFC2152Equivalent { [self runCase:2]; }
- (void)testShiftedPunctuation { [self runCase:3]; }
- (void)testJapanese { [self runCase:4]; }
- (void)testPoundSign { [self runCase:5]; }

@end
