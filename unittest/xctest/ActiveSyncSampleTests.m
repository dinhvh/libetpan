#import <XCTest/XCTest.h>

#include "activesync_tests.h"

@interface ActiveSyncSampleTests : XCTestCase
@end

@implementation ActiveSyncSampleTests

static void recordSampleFailure(const char *file, unsigned line,
    const char *expression, const char *message, void *context)
{
  ActiveSyncSampleTests *test = (__bridge ActiveSyncSampleTests *) context;
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
  int result = activesync_sample_test_run_case(index, recordSampleFailure,
      (__bridge void *) self);
  XCTAssertEqual(result, 0);
}

- (void)testMoreAvailableLoop { [self runCase:0]; }
- (void)testSyncStateInvalidation { [self runCase:1]; }
- (void)testProvisionPersistsPolicyKey { [self runCase:2]; }
- (void)testSettingsAndGetItemEstimate { [self runCase:3]; }

@end
