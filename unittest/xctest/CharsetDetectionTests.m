#import <XCTest/XCTest.h>

#include "charset_detection_tests.h"

@interface CharsetDetectionTests : XCTestCase
@end

@implementation CharsetDetectionTests

static void recordCharsetFailure(const char *file, unsigned line,
    const char *expression, const char *message, void *context)
{
  CharsetDetectionTests *test = (__bridge CharsetDetectionTests *) context;
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
      pathForResource:@"charset-data" ofType:nil];
  XCTAssertNotNil(root, @"The charset fixtures are missing from the test bundle");
  if (root == nil) return;
  int result = charset_detection_test_run_case(index,
      root.fileSystemRepresentation, recordCharsetFailure,
      (__bridge void *) self);
  XCTAssertEqual(result, 0);
}

- (void)testBig5 { [self runCase:0]; }
- (void)testGB18030 { [self runCase:1]; }
- (void)testShiftJIS { [self runCase:2]; }
- (void)testUTF8 { [self runCase:3]; }

@end
