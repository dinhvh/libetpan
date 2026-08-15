#import <XCTest/XCTest.h>

#include "mime_builder_tests.h"

@interface MIMEBuilderTests : XCTestCase
@end


@implementation MIMEBuilderTests

static void recordBuilderFailure(const char *file, unsigned line,
    const char *expression, const char *message, void *context)
{
  MIMEBuilderTests *test = (__bridge MIMEBuilderTests *) context;
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
      pathForResource:@"mime-builder-data" ofType:nil];
  XCTAssertNotNil(root, @"The MIME builder fixtures are missing");
  if (root == nil) return;
  int result = mime_builder_test_run_case(index, root.fileSystemRepresentation,
      recordBuilderFailure, (__bridge void *) self);
  XCTAssertEqual(result, 0);
}

- (void)testMessageBuilder1 { [self runCase:0]; }
- (void)testMessageBuilder2 { [self runCase:1]; }
- (void)testMessageBuilder3 { [self runCase:2]; }

@end
