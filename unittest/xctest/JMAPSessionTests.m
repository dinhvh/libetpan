#import <XCTest/XCTest.h>
#import "SplitTestSupport.h"

#define main jmap_session_standalone_main
#include "../jmap/jmap-session-test.c"
#undef main

@interface JMAPSessionTests : XCTestCase @end
@implementation JMAPSessionTests
- (void)testDiscoverSuccess { XCTAssertTrue(RunTestFromRepositoryRoot(test_discover_success)); }
- (void)testDiscoverNotFound { XCTAssertTrue(RunTestFromRepositoryRoot(test_discover_not_found)); }
- (void)testGetSessionSuccess { XCTAssertTrue(RunTestFromRepositoryRoot(test_get_session_success)); }
- (void)testMinimalSessionFixture { XCTAssertTrue(RunTestFromRepositoryRoot(test_minimal_session_fixture)); }
- (void)testFastmailStyleSessionFixture { XCTAssertTrue(RunTestFromRepositoryRoot(test_fastmail_style_session_fixture)); }
- (void)testGetSessionAuthFailure { XCTAssertTrue(RunTestFromRepositoryRoot(test_get_session_auth_failure)); }
@end
