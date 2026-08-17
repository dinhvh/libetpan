#import <XCTest/XCTest.h>
#import "SplitTestSupport.h"

#define main jmap_thread_standalone_main
#include "../jmap/jmap-thread-test.c"
#undef main

@interface JMAPThreadTests : XCTestCase @end
@implementation JMAPThreadTests
- (void)testThreadGetAndChanges
{
  XCTAssertTrue(RunTestFromRepositoryRoot(test_thread_get_and_changes));
}
@end
