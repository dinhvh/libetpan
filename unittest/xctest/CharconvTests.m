#import <XCTest/XCTest.h>

#include "../charconv/charset_cases.h"

@interface CharconvTests : XCTestCase
@end

@implementation CharconvTests

- (void)testCharsetInventory
{
  NSString *inventory = [[NSBundle bundleForClass:self.class]
      pathForResource:@"iconv-glibc-charsets" ofType:@"txt"];
  XCTAssertNotNil(inventory, @"The charset inventory is missing from the test bundle");
  if (inventory == nil)
    return;
  XCTAssertEqual(charset_cases_test_with_inventory(inventory.fileSystemRepresentation), 0);
}

@end
