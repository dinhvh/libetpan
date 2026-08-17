#import <XCTest/XCTest.h>
#import "SplitTestSupport.h"

#define main jmap_response_standalone_main
#include "../jmap/jmap-response-test.c"
#undef main

@interface JMAPResponseTests : XCTestCase @end
@implementation JMAPResponseTests
- (void)testParseSuccessAndError { XCTAssertTrue(RunTestFromRepositoryRoot(test_response_parse_success_and_error)); }
- (void)testProblemParse { XCTAssertTrue(RunTestFromRepositoryRoot(test_problem_parse)); }
- (void)testMalformedResponse { XCTAssertTrue(RunTestFromRepositoryRoot(test_malformed_response)); }
- (void)testOverlongMethodResponseTuple { XCTAssertTrue(RunTestFromRepositoryRoot(test_overlong_method_response_tuple)); }
- (void)testMissingMethodResponses { XCTAssertTrue(RunTestFromRepositoryRoot(test_missing_method_responses)); }
- (void)testMissingSessionState { XCTAssertTrue(RunTestFromRepositoryRoot(test_missing_session_state)); }
@end
