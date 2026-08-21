#import <XCTest/XCTest.h>

extern int mailstream_tls_backend_roundtrip_test(void);

@interface MailstreamTLSTests : XCTestCase
@end

@implementation MailstreamTLSTests

- (void)testBackendRoundTripAndSTARTTLS
{
  XCTestExpectation *finished = [self expectationWithDescription:@"TLS fixture"];
  __block int result = -1;

  dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
    result = mailstream_tls_backend_roundtrip_test();
    [finished fulfill];
  });

  [self waitForExpectations:@[finished] timeout:30.0];
  XCTAssertEqual(result, 0);
}

@end
