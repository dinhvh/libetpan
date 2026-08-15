#import <XCTest/XCTest.h>

#include "imf_tests.h"

@interface IMFParserTests : XCTestCase
@end

@implementation IMFParserTests

static void recordIMFFailure(const char *file, unsigned line,
    const char *expression, const char *message, void *context)
{
  IMFParserTests *test = (__bridge IMFParserTests *) context;
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
  NSString *root = [[NSBundle bundleForClass:self.class]
      pathForResource:@"data" ofType:nil];
  XCTAssertNotNil(root, @"The IMF fixtures are missing from the test bundle");
  if (root == nil)
    return;
  int result = imf_parser_test_run_case(index, root.fileSystemRepresentation,
      recordIMFFailure, (__bridge void *) self);
  XCTAssertEqual(result, 0);
}

- (void)testDateTime { [self runCase:0]; }
- (void)testLexicalTokens { [self runCase:1]; }
- (void)testAddressForms { [self runCase:2]; }
- (void)testIdentifierFields { [self runCase:3]; }
- (void)testAllStandardFields { [self runCase:4]; }
- (void)testSimpleRFC822Message { [self runCase:5]; }
- (void)testFoldedCommentsRFC822Message { [self runCase:6]; }
- (void)testResentTraceRFC822Message { [self runCase:7]; }

@end
