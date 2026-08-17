#import <XCTest/XCTest.h>
#import "SplitTestSupport.h"

#define main jmap_call_standalone_main
#include "../jmap/jmap-call-test.c"
#undef main

@interface JMAPCallTests : XCTestCase @end
@implementation JMAPCallTests
- (void)testPostsToAPIURL { XCTAssertTrue(RunTestFromRepositoryRoot(test_call_posts_to_api_url)); }
- (void)testRecordsMethodErrorDiagnostics { XCTAssertTrue(RunTestFromRepositoryRoot(test_call_records_method_error_diagnostics)); }
- (void)testRecordsProblemDiagnostics { XCTAssertTrue(RunTestFromRepositoryRoot(test_call_records_problem_diagnostics)); }
- (void)testRecordsHTTPProblemDiagnostics { XCTAssertTrue(RunTestFromRepositoryRoot(test_call_records_http_problem_diagnostics)); }
- (void)testMapsUnknownCapabilityProblem { XCTAssertTrue(RunTestFromRepositoryRoot(test_call_maps_unknown_capability_problem)); }
@end
