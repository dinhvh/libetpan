#import <XCTest/XCTest.h>
#import "SplitTestSupport.h"

#define main jmap_email_standalone_main
#include "../jmap/jmap-email-test.c"
#undef main

@interface JMAPEmailTests : XCTestCase @end
@implementation JMAPEmailTests
- (void)testEmailQueryAndChanges
{
  XCTAssertTrue(RunTestFromRepositoryRoot(test_email_query_and_changes));
}
@end
