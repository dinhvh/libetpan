#import <XCTest/XCTest.h>
#import "SplitTestSupport.h"

#define main jmap_http_standalone_main
#include "../jmap/jmap-http-test.c"
#undef main

@interface JMAPHTTPTests : XCTestCase @end
@implementation JMAPHTTPTests
- (void)testRequestResponseHelpers { XCTAssertTrue(RunTestFromRepositoryRoot(test_request_response_helpers)); }
- (void)testFakeTransport { XCTAssertTrue(RunTestFromRepositoryRoot(test_fake_transport)); }
@end
