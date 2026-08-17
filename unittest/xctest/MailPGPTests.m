#import <XCTest/XCTest.h>
#import "SplitTestSupport.h"

#define main mailpgp_standalone_main
#include "../mailpgp/mailpgp_low_level_test.c"
#undef main

@interface MailPGPTests : XCTestCase @end
@implementation MailPGPTests
- (void)testSignedMIMEDetection { XCTAssertTrue(RunTestFromRepositoryRoot(check_signed_mime_detection)); }
- (void)testEncryptedMIMEDetection { XCTAssertTrue(RunTestFromRepositoryRoot(check_encrypted_mime_detection)); }
- (void)testInlineSignedDetection { XCTAssertTrue(RunTestFromRepositoryRoot(check_inline_signed_detection)); }
- (void)testHeaderFingerprintExtraction { XCTAssertTrue(RunTestFromRepositoryRoot(check_header_fingerprint_extraction)); }
- (void)testPublicKeyExtraction { XCTAssertTrue(RunTestFromRepositoryRoot(check_public_key_extraction)); }
#ifdef USE_PGP_RNP
- (void)testRNPPublicKeyMetadata { XCTAssertTrue(RunTestFromRepositoryRoot(check_rnp_public_key_metadata)); }
- (void)testRNPCryptoRoundTrip { XCTAssertTrue(RunTestFromRepositoryRoot(check_rnp_crypto_roundtrip)); }
- (void)testStaticHiddenRecipientFixture { XCTAssertTrue(RunTestFromRepositoryRoot(check_static_hidden_recipient_fixture)); }
#endif
- (void)testCryptoNotImplemented { XCTAssertTrue(RunTestFromRepositoryRoot(check_crypto_not_implemented)); }
@end
