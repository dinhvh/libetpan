#import <XCTest/XCTest.h>
#import "SplitTestSupport.h"

#define main jmap_json_standalone_main
#include "../jmap/jmap-json-test.c"
#undef main

@interface JMAPJSONTests : XCTestCase @end
@implementation JMAPJSONTests
- (void)testParseInvalidJSON { XCTAssertTrue(RunTestFromRepositoryRoot(test_parse_invalid_json)); }
- (void)testParseDuplicateKeys { XCTAssertTrue(RunTestFromRepositoryRoot(test_parse_duplicate_keys)); }
- (void)testParseTrailingData { XCTAssertTrue(RunTestFromRepositoryRoot(test_parse_trailing_data)); }
- (void)testParseAndLookup { XCTAssertTrue(RunTestFromRepositoryRoot(test_parse_and_lookup)); }
- (void)testBuildAndSerialize { XCTAssertTrue(RunTestFromRepositoryRoot(test_build_and_serialize)); }
- (void)testBuildScalarsAndSortedSerialize { XCTAssertTrue(RunTestFromRepositoryRoot(test_build_scalars_and_sorted_serialize)); }
- (void)testIntegerCreationBounds { XCTAssertTrue(RunTestFromRepositoryRoot(test_integer_creation_bounds)); }
@end
