#import <XCTest/XCTest.h>
#import "SplitTestSupport.h"

#define main jmap_mailbox_standalone_main
#include "../jmap/jmap-mailbox-test.c"
#undef main

@interface JMAPMailboxTests : XCTestCase @end
@implementation JMAPMailboxTests
- (void)testMailboxGet { XCTAssertTrue(RunTestFromRepositoryRoot(test_mailbox_get)); }
- (void)testMailboxSet { XCTAssertTrue(RunTestFromRepositoryRoot(test_mailbox_set)); }
- (void)testMailboxSetFastmailUpdatedMap { XCTAssertTrue(RunTestFromRepositoryRoot(test_mailbox_set_fastmail_updated_map)); }
- (void)testMailboxGetMapsLimitMethodError { XCTAssertTrue(RunTestFromRepositoryRoot(test_mailbox_get_maps_limit_method_error)); }
- (void)testMailboxChangesAndQuery { XCTAssertTrue(RunTestFromRepositoryRoot(test_mailbox_changes_and_query)); }
@end
