#import <XCTest/XCTest.h>
#import "SplitTestSupport.h"

#define main jmap_blob_standalone_main
#include "../jmap/jmap-blob-test.c"
#undef main

@interface JMAPBlobTests : XCTestCase @end
@implementation JMAPBlobTests
- (void)testUploadAndDownload
{
  XCTAssertTrue(RunTestFromRepositoryRoot(test_upload_and_download));
}
@end
