#import <XCTest/XCTest.h>

#include "mime_tests.h"

@interface MIMEParserTests : XCTestCase
@end

@implementation MIMEParserTests

static void recordMIMEFailure(const char *file, unsigned line,
    const char *expression, const char *message, void *context)
{
  MIMEParserTests *test = (__bridge MIMEParserTests *) context;
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
      pathForResource:@"mime-data" ofType:nil];
  XCTAssertNotNil(root, @"The MIME fixtures are missing from the test bundle");
  if (root == nil)
    return;
  int result = mime_parser_test_run_case(index, root.fileSystemRepresentation,
      recordMIMEFailure, (__bridge void *) self);
  XCTAssertEqual(result, 0);
}

- (void)testContentTypeGrammar { [self runCase:0]; }
- (void)testEncodingGrammar { [self runCase:1]; }
- (void)testFieldGrammar { [self runCase:2]; }
- (void)testEncodedStructuredFieldRecovery { [self runCase:3]; }
- (void)testRFC2231ContentTypeParameters { [self runCase:4]; }
- (void)testRFC2231DispositionFilename { [self runCase:5]; }
- (void)testTransferDecoders { [self runCase:6]; }
- (void)testRFC2047EncodedWords { [self runCase:7]; }
- (void)testRFC822MultipartFile { [self runCase:8]; }
- (void)testRFC822AlternativeFile { [self runCase:9]; }
- (void)testFullRFC822Multipart { [self runCase:10]; }
- (void)testQuotedPairBoundaryQuote { [self runCase:11]; }

@end
