#import <XCTest/XCTest.h>

#include "activesync_tests.h"

@interface ActiveSyncWBXMLTests : XCTestCase
@end

@implementation ActiveSyncWBXMLTests

static void recordWBXMLFailure(const char *file, unsigned line,
    const char *expression, const char *message, void *context)
{
  ActiveSyncWBXMLTests *test = (__bridge ActiveSyncWBXMLTests *) context;
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
  int result = activesync_wbxml_test_run_case(index, recordWBXMLFailure,
      (__bridge void *) self);
  XCTAssertEqual(result, 0);
}

- (void)testTagLookup { [self runCase:0]; }
- (void)testFolderSyncGolden { [self runCase:1]; }
- (void)testOpaqueAndMalformed { [self runCase:2]; }
- (void)testSettingsDeviceInformationGolden { [self runCase:3]; }

@end
