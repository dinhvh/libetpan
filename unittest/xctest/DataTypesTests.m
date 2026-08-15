#import <XCTest/XCTest.h>

#include "data_types_tests.h"

@interface DataTypesTests : XCTestCase
@end

@implementation DataTypesTests

static void recordDataTypesFailure(const char *file, unsigned line,
    const char *expression, const char *message, void *context)
{
  DataTypesTests *test = (__bridge DataTypesTests *) context;
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
  int result = data_types_test_run_case(index, recordDataTypesFailure,
      (__bridge void *) self);
  XCTAssertEqual(result, 0);
}

- (void)testUINT4Size { [self runCase:0]; }
- (void)testMD5Vectors { [self runCase:1]; }
- (void)testMD5ChunkedUpdates { [self runCase:2]; }
- (void)testHMACMD5Vectors { [self runCase:3]; }
- (void)testHMACMD5Streaming { [self runCase:4]; }
- (void)testBase64Codec { [self runCase:5]; }

@end
