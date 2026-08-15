#import <XCTest/XCTest.h>

#include "command_parameter_sender_test.h"

@interface IMAPCommandParameterSenderTests : XCTestCase
@end


@implementation IMAPCommandParameterSenderTests

static void recordCommandParameterFailure(const char *file, unsigned line,
    const char *expression, const char *message, void *context)
{
  IMAPCommandParameterSenderTests *test =
      (__bridge IMAPCommandParameterSenderTests *) context;
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
      pathForResource:@"command-parameters" ofType:nil];
  XCTAssertNotNil(root, @"The command-parameter fixtures are missing");
  if (root == nil) return;
  int result = imap_command_parameter_sender_test_run_case(index,
      root.fileSystemRepresentation, recordCommandParameterFailure,
      (__bridge void *) self);
  XCTAssertEqual(result, 0);
}

#define PARAMETER_TEST(NAME, INDEX) - (void)test##NAME { [self runCase:INDEX]; }
PARAMETER_TEST(SelectQuotedMailbox, 0)
PARAMETER_TEST(LoginQuotedCredentials, 1)
PARAMETER_TEST(AppendMinimal, 2)
PARAMETER_TEST(CopyWildcardSet, 3)
PARAMETER_TEST(UIDCopyOpenRange, 4)
PARAMETER_TEST(FetchSections, 5)
PARAMETER_TEST(FetchStaticAttributes, 6)
PARAMETER_TEST(StoreAllFlags, 7)
PARAMETER_TEST(StatusAllAttributes, 8)
PARAMETER_TEST(SearchStrings, 9)
PARAMETER_TEST(SearchDatesAndSizes, 10)
PARAMETER_TEST(SearchBooleanAndSets, 11)
PARAMETER_TEST(SearchAllStringKeys, 12)
PARAMETER_TEST(SearchKeywordHeaderDates, 13)
PARAMETER_TEST(SearchAllFlagKeys, 14)
#undef PARAMETER_TEST

@end
