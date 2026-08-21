#import <XCTest/XCTest.h>

#import <TargetConditionals.h>

#include <libetpan/mailsmime.h>

#include <limits.h>
#include <unistd.h>

extern int smime_test_signed_fixture(const char *filename);
extern int smime_test_encrypted_fixture(const char *filename);
extern int smime_test_crypto_round_trip(void);
extern int smime_test_passphrase_callback(void);
extern int smime_test_crypto_round_trip_with_backend(
    enum mailsmime_backend backend);
extern int smime_test_passphrase_callback_with_backend(
    enum mailsmime_backend backend);
extern void smime_test_set_fixture_directory(const char *directory);

@interface SMIMETests : XCTestCase
@end

static int runFixtureFromRepositoryRoot(
    int (*testFixture)(const char *), const char *fixture)
{
  char previousDirectory[PATH_MAX];
  NSString *sourcePath = [NSString stringWithUTF8String:__FILE__];
  NSString *root = [[[sourcePath stringByDeletingLastPathComponent]
      stringByDeletingLastPathComponent] stringByDeletingLastPathComponent];

  if ((getcwd(previousDirectory, sizeof(previousDirectory)) == NULL) ||
      (chdir(root.fileSystemRepresentation) != 0))
    return 0;
  int result = testFixture(fixture);
  if (chdir(previousDirectory) != 0)
    return 0;
  return result;
}

static int runBackendTestFromRepositoryRoot(
    int (*testBackend)(enum mailsmime_backend), enum mailsmime_backend backend)
{
#if TARGET_OS_IPHONE
  return testBackend(backend);
#else
  char previousDirectory[PATH_MAX];
  NSString *sourcePath = [NSString stringWithUTF8String:__FILE__];
  NSString *root = [[[sourcePath stringByDeletingLastPathComponent]
      stringByDeletingLastPathComponent] stringByDeletingLastPathComponent];

  if ((getcwd(previousDirectory, sizeof(previousDirectory)) == NULL) ||
      (chdir(root.fileSystemRepresentation) != 0))
    return 0;
  int result = testBackend(backend);
  if (chdir(previousDirectory) != 0)
    return 0;
  return result;
#endif
}

@implementation SMIMETests

- (void)setUp
{
  [super setUp];
#if TARGET_OS_IPHONE
  smime_test_set_fixture_directory(
      [NSBundle bundleForClass:self.class].resourcePath.fileSystemRepresentation);
#else
  smime_test_set_fixture_directory(NULL);
#endif
}

#if !TARGET_OS_IPHONE
- (void)testSignedFixture118
{
  XCTAssertTrue(runFixtureFromRepositoryRoot(smime_test_signed_fixture,
      "unittest/mime-parser/data/input/mbox/jwz/118"));
}

- (void)testSignedFixture128
{
  XCTAssertTrue(runFixtureFromRepositoryRoot(smime_test_signed_fixture,
      "unittest/mime-parser/data/input/mbox/jwz/128"));
}

- (void)testEncryptedFixture105
{
  XCTAssertTrue(runFixtureFromRepositoryRoot(smime_test_encrypted_fixture,
      "unittest/mime-parser/data/input/mbox/jwz/105"));
}

- (void)testEncryptedFixture121
{
  XCTAssertTrue(runFixtureFromRepositoryRoot(smime_test_encrypted_fixture,
      "unittest/mime-parser/data/input/mbox/jwz/121"));
}
#endif

#ifdef USE_SMIME_APPLE
- (void)testAppleCryptoRoundTrip
{
  XCTAssertTrue(runBackendTestFromRepositoryRoot(
      smime_test_crypto_round_trip_with_backend,
      MAILSMIME_BACKEND_APPLE));
}

- (void)testApplePassphraseCallback
{
  XCTAssertTrue(runBackendTestFromRepositoryRoot(
      smime_test_passphrase_callback_with_backend,
      MAILSMIME_BACKEND_APPLE));
}
#endif

#ifdef USE_SMIME_OPENSSL
- (void)testOpenSSLCryptoRoundTrip
{
  XCTAssertTrue(runBackendTestFromRepositoryRoot(
      smime_test_crypto_round_trip_with_backend,
      MAILSMIME_BACKEND_OPENSSL));
}

- (void)testOpenSSLPassphraseCallback
{
  XCTAssertTrue(runBackendTestFromRepositoryRoot(
      smime_test_passphrase_callback_with_backend,
      MAILSMIME_BACKEND_OPENSSL));
}

- (void)testOpenSSLIsDefaultBackend
{
  XCTAssertTrue(runBackendTestFromRepositoryRoot(
      smime_test_crypto_round_trip_with_backend,
      MAILSMIME_BACKEND_DEFAULT));
}
#endif

@end
