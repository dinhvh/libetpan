#import <XCTest/XCTest.h>

#include "response_data_test.h"

@interface IMAPResponseDataTests : XCTestCase
@end


@implementation IMAPResponseDataTests

static void recordResponseDataFailure(const char *file, unsigned line,
    const char *expression, const char *message, void *context)
{
  IMAPResponseDataTests *test = (__bridge IMAPResponseDataTests *) context;
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

- (void)runCase:(size_t)index compressed:(BOOL)compressed
{
  NSString *root = [[NSBundle bundleForClass:self.class]
      pathForResource:@"response-data" ofType:nil];
  XCTAssertNotNil(root, @"The IMAP response-data fixtures are missing");
  if (root == nil) return;
  int result = imap_response_data_test_run_case(index, compressed,
      root.fileSystemRepresentation, recordResponseDataFailure,
      (__bridge void *) self);
  XCTAssertEqual(result, 0);
}

#define RESPONSE_DATA_TEST(NAME, INDEX) \
- (void)test##NAME { [self runCase:INDEX compressed:NO]; } \
- (void)test##NAME##Compressed { [self runCase:INDEX compressed:YES]; }

RESPONSE_DATA_TEST(ConditionOK, 0)
RESPONSE_DATA_TEST(ConditionOKAlert, 1)
RESPONSE_DATA_TEST(ConditionOKBadCharset, 2)
RESPONSE_DATA_TEST(ConditionOKCapabilityCode, 3)
RESPONSE_DATA_TEST(ConditionOKReadOnly, 4)
RESPONSE_DATA_TEST(ConditionOKUIDNext, 5)
RESPONSE_DATA_TEST(ConditionOKUIDValidity, 6)
RESPONSE_DATA_TEST(ConditionOKUnknownCode, 7)
RESPONSE_DATA_TEST(ConditionNO, 8)
RESPONSE_DATA_TEST(ConditionBAD, 9)
RESPONSE_DATA_TEST(ConditionBYE, 10)
RESPONSE_DATA_TEST(Capability, 11)
RESPONSE_DATA_TEST(Flags, 12)
RESPONSE_DATA_TEST(EmptyFlags, 13)
RESPONSE_DATA_TEST(List, 14)
RESPONSE_DATA_TEST(ListEmptyFlagExtension, 15)
RESPONSE_DATA_TEST(ListNilDelimiter, 16)
RESPONSE_DATA_TEST(LSub, 17)
RESPONSE_DATA_TEST(Status, 18)
RESPONSE_DATA_TEST(Exists, 19)
RESPONSE_DATA_TEST(ObsoleteSearch, 20)
RESPONSE_DATA_TEST(ObsoleteSearchEmpty, 21)
RESPONSE_DATA_TEST(ObsoleteRecent, 22)
RESPONSE_DATA_TEST(Expunge, 23)
RESPONSE_DATA_TEST(FetchFlags, 24)
RESPONSE_DATA_TEST(FetchLiteral, 25)
RESPONSE_DATA_TEST(FetchBodyStructure, 26)
RESPONSE_DATA_TEST(FetchEnvelope, 27)
RESPONSE_DATA_TEST(FetchEnvelopeICloudMessageID, 28)
RESPONSE_DATA_TEST(FetchRFC822Text, 29)
RESPONSE_DATA_TEST(Enabled, 30)
RESPONSE_DATA_TEST(Namespace, 31)
RESPONSE_DATA_TEST(XListEmptyFlagExtension, 32)
RESPONSE_DATA_TEST(NestedInvalidFlags, 33)
RESPONSE_DATA_TEST(NestedInvalidPermanentFlags, 34)
RESPONSE_DATA_TEST(NestedInvalidFetchFlags, 35)
RESPONSE_DATA_TEST(ICloudMessageID, 36)
RESPONSE_DATA_TEST(NumberOverflow, 37)

#undef RESPONSE_DATA_TEST

@end
