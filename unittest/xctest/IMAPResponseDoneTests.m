#import <XCTest/XCTest.h>

#include "mailimap_types.h"
#include "response_done_test.h"

@interface IMAPResponseDoneTests : XCTestCase
@end

@implementation IMAPResponseDoneTests

- (NSString *)fixtureRoot
{
  NSString *path = [[NSBundle bundleForClass:self.class]
      pathForResource:@"response-done" ofType:nil];
  XCTAssertNotNil(path, @"The response-done fixtures are missing from the test bundle");
  return path;
}

static void recordFailure(const char *file, unsigned line,
    const char *expression, const char *message, void *context)
{
  IMAPResponseDoneTests *test = (__bridge IMAPResponseDoneTests *) context;
  NSString *description = [NSString stringWithFormat:@"%s: %s",
      expression, message];
  NSString *filePath = [NSString stringWithUTF8String:file];
  XCTSourceCodeLocation *location = [[XCTSourceCodeLocation alloc]
      initWithFilePath:filePath lineNumber:line];
  XCTSourceCodeContext *sourceContext = [[XCTSourceCodeContext alloc]
      initWithLocation:location];
  XCTIssue *issue = [[XCTIssue alloc]
      initWithType:XCTIssueTypeAssertionFailure
      compactDescription:description
      detailedDescription:nil
      sourceCodeContext:sourceContext
      associatedError:nil
      attachments:@[]];
  [test recordIssue:issue];
}

- (void)runFixture:(NSString *)fixture compressed:(BOOL)compressed
    condition:(int)condition elements:(unsigned)elements
{
  NSString *root = [self fixtureRoot];
  if (root == nil)
    return;

  int result = imap_response_done_test_case(root.fileSystemRepresentation,
      fixture.fileSystemRepresentation, compressed, condition, elements,
      recordFailure, (__bridge void *) self);
  XCTAssertEqual(result, 0);
}

- (void)testTaggedOK
{
  [self runFixture:@"tagged-ok.imap" compressed:NO
      condition:MAILIMAP_RESP_COND_STATE_OK elements:0];
}

- (void)testTaggedOKCompressed
{
  [self runFixture:@"tagged-ok.imap" compressed:YES
      condition:MAILIMAP_RESP_COND_STATE_OK elements:0];
}

- (void)testTaggedNO
{
  [self runFixture:@"tagged-no.imap" compressed:NO
      condition:MAILIMAP_RESP_COND_STATE_NO elements:0];
}

- (void)testTaggedNOCompressed
{
  [self runFixture:@"tagged-no.imap" compressed:YES
      condition:MAILIMAP_RESP_COND_STATE_NO elements:0];
}

- (void)testTaggedBAD
{
  [self runFixture:@"tagged-bad.imap" compressed:NO
      condition:MAILIMAP_RESP_COND_STATE_BAD elements:0];
}

- (void)testTaggedBADCompressed
{
  [self runFixture:@"tagged-bad.imap" compressed:YES
      condition:MAILIMAP_RESP_COND_STATE_BAD elements:0];
}

- (void)testContinueThenOK
{
  [self runFixture:@"continue-then-ok.imap" compressed:NO
      condition:MAILIMAP_RESP_COND_STATE_OK elements:1];
}

- (void)testContinueThenOKCompressed
{
  [self runFixture:@"continue-then-ok.imap" compressed:YES
      condition:MAILIMAP_RESP_COND_STATE_OK elements:1];
}

- (void)testMultiResponse
{
  [self runFixture:@"multi-response.imap" compressed:NO
      condition:MAILIMAP_RESP_COND_STATE_OK elements:5];
}

- (void)testMultiResponseCompressed
{
  [self runFixture:@"multi-response.imap" compressed:YES
      condition:MAILIMAP_RESP_COND_STATE_OK elements:5];
}

@end
