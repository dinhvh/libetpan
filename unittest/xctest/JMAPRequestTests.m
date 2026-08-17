#import <XCTest/XCTest.h>
#import "SplitTestSupport.h"

#define main jmap_request_standalone_main
#include "../jmap/jmap-request-test.c"
#undef main

@interface JMAPRequestTests : XCTestCase @end
@implementation JMAPRequestTests
- (void)testConvenienceHelpers { XCTAssertTrue(RunTestFromRepositoryRoot(test_request_convenience_helpers)); }
- (void)testSerialize { XCTAssertTrue(RunTestFromRepositoryRoot(test_request_serialize)); }
- (void)testResultReference { XCTAssertTrue(RunTestFromRepositoryRoot(test_request_result_reference)); }
@end
