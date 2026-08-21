# Apple build and S/MIME test plan

## Goal

Keep the Apple build feature split explicit and add deterministic iOS Simulator
coverage for the OpenSSL S/MIME implementation. The test must validate the
same static-library linkage that an iOS consumer uses, not only compile the
archive.

## Status

Completed on 2026-08-20.

- Added the shared `libetpan-ios-smime-tests` scheme and iOS Simulator XCTest
  product.
- Added an explicit dependency on the real `libetpan ios` static-library target.
- Linked the OpenSSL Crypto simulator slice as a static-consumer dependency.
- Bundled the deterministic PEM fixtures and made the reusable C test helpers
  accept a fixture directory.
- Restricted the iOS suite to OpenSSL tests and verified that the default backend
  executes the OpenSSL round trip.
- Ran the iOS suite successfully: 3 tests, 0 failures.
- Ran the macOS regression suite successfully: 509 tests, 0 failures, including
  all 6 Apple S/MIME tests.

## Current state (2026-08-20)

### Product feature matrix

| Capability | iOS static library | macOS static library/framework |
| --- | --- | --- |
| OpenSSL transport | Enabled | Disabled |
| S/MIME OpenSSL | Enabled | Disabled |
| S/MIME Apple | Disabled | Enabled |
| GnuTLS | Disabled | Disabled |
| RNP | Enabled | Enabled |
| SASL | Enabled | Enabled |
| JMAP | Enabled | Enabled |
| curl | Disabled | Disabled |
| zlib | Enabled | Enabled |

The feature split is implemented in the xcconfigs:

- `libetpan-static-ios.xcconfig` defines `HAVE_OPENSSL=1` and
  `USE_SMIME_OPENSSL=1`, with device and simulator header paths for the
  dependency XCFrameworks.
- `libetpan-macos.xcconfig` defines `USE_SMIME_APPLE=1` and is included by the
  macOS framework and static-library configurations.
- Common features remain in `libetpan-project.xcconfig`.
- Static-library targets do not link their transitive dependency XCFrameworks;
  the final consumer supplies them.

### Existing S/MIME tests

- `unittest/smime/smime_low_level_test.c` contains deterministic fixture,
  sign/verify, tamper, encrypt/decrypt, multiple-recipient, signer-certificate,
  wrong-key, and encrypted-private-key/passphrase-callback coverage.
- The reusable crypto functions accept an explicit `enum mailsmime_backend`.
- `unittest/xctest/SMIMETests.m` exposes Apple and conditionally compiled
  OpenSSL XCTest methods.
- The only XCTest product is currently `libetpan-unit-tests`, configured by
  `tests-unit.xcconfig` for macOS only (`SDKROOT=macosx`,
  `SUPPORTED_PLATFORMS=macosx`, and `USE_SMIME_APPLE=1`). Therefore its OpenSSL
  XCTest methods are not compiled or run.
- The macOS XCTest target builds against the nested `libetpan.xcodeproj`; the
  shared scheme was repaired in commit `383b6a6` (`Fix Xcode unit test build`).
- The Autotools test suite can exercise the OpenSSL backend on a host, including
  command-line OpenSSL interoperability, but that does not validate iOS SDK
  compilation, simulator linkage, or execution.

### Remaining coverage gap

The `libetpan ios` target can compile for `iphonesimulator`, but no iOS test
consumer links `libetpan-ios.a` with its required dependencies and executes the
OpenSSL S/MIME backend. An archive-only build cannot detect missing symbols,
incorrect XCFramework slice selection, resource access failures, or an
incorrect default backend at runtime.

## Implemented plan

### 1. Make deterministic fixtures bundle-aware

- Add a fixture-root or fixture-path injection mechanism to the reusable S/MIME
  test helpers. Preserve the existing repository-relative fallback for the C
  command-line and macOS test paths.
- Add the PEM certificates and private keys required by the OpenSSL tests to the
  iOS test bundle's Resources phase.
- Resolve bundled resources with `NSBundle` in the XCTest adapter and pass
  absolute paths to the C helpers. Do not depend on `__FILE__`, the repository
  checkout, or changing the process working directory on iOS.
- Keep these deterministic tests and fixtures under `unittest/`.

### 2. Add an iOS Simulator XCTest target

- Add a separate `libetpan-ios-smime-tests` unit-test bundle to
  `build-mac/libetpan Tests.xcodeproj` with its own shared scheme and leaf
  xcconfig.
- Configure it for `iphonesimulator`, define `HAVE_OPENSSL=1` and
  `USE_SMIME_OPENSSL=1`, and ensure `USE_SMIME_APPLE` is absent.
- Compile `SMIMETests.m` and `smime_low_level_test.c` with
  `SMIME_LOW_LEVEL_NO_MAIN`.
- Add a target dependency on `libetpan ios` from the nested library project.
- Link the produced `libetpan-ios.a` plus the simulator slices of all required
  static dependencies: OpenSSL Crypto, RNP, CyrusSASL, JsonC, Tidy where needed,
  zlib, and required Apple system frameworks. Do not link OpenSSL SSL unless an
  unresolved transport symbol proves it is required by this consumer.
- Give the test bundle a unique product name and bundle identifier so it cannot
  collide with the macOS XCTest product.

### 3. Exercise the iOS OpenSSL contract

- Run the existing signed/encrypted MIME recognition tests using bundled
  fixtures where they remain useful.
- Run `smime_test_crypto_round_trip_with_backend(MAILSMIME_BACKEND_OPENSSL)`.
- Run
  `smime_test_passphrase_callback_with_backend(MAILSMIME_BACKEND_OPENSSL)`.
- Add a small assertion that `MAILSMIME_BACKEND_DEFAULT` selects OpenSSL in the
  iOS configuration. Prefer exposing a backend query if one already exists;
  otherwise verify it by successfully running the default-backend round trip.
- Keep Apple-backend tests out of this target so an accidental feature-matrix
  change fails at compile or link time instead of silently testing the wrong
  implementation.

### 4. Verify build settings, linkage, and execution

- Use `xcodebuild -showBuildSettings` for the iOS test target in Debug and
  Release and confirm:
  - `SDKROOT` resolves to `iphonesimulator`;
  - `SUPPORTED_PLATFORMS` contains only `iphonesimulator`;
  - `HAVE_OPENSSL=1` and `USE_SMIME_OPENSSL=1` are present;
  - `USE_SMIME_APPLE`, `HAVE_GNUTLS`, and `HAVE_CURL` are absent;
  - simulator dependency header and library slices are selected.
- Build the iOS test target for a generic simulator destination to catch compile
  and link errors without requiring a booted simulator.
- Run the shared scheme on an available iOS Simulator and require all OpenSSL
  S/MIME tests to pass.
- Re-run the macOS `libetpan-unit-tests` scheme to ensure its Apple S/MIME tests
  still pass and its feature settings remain unchanged.
- Build the iOS static library and XCFramework aggregate after the test-project
  changes.

## Acceptance criteria

- A shared iOS Simulator test scheme is present and runnable from both Xcode and
  `xcodebuild`.
- The test target links the real `libetpan-ios.a` and simulator dependency
  slices with no unresolved or duplicate crypto symbols.
- OpenSSL sign/verify, encrypt/decrypt, trust status, tamper detection,
  multi-recipient encryption, wrong-key handling, certificate export, and
  encrypted-key passphrase callback execute successfully on the simulator.
- The default S/MIME backend is proven to be OpenSSL in the iOS build.
- Tests use bundled deterministic fixtures and require no network, credentials,
  keychain state, or repository-relative runtime paths.
- Existing macOS Apple-backend XCTest coverage remains green.

## Optional follow-up

Add a minimal device-hosted smoke test only if CI or release validation needs to
prove the `iphoneos` slice links and starts on physical hardware. It should
reuse the same test helpers; full crypto behavior belongs in the deterministic
simulator suite.

## Verification results

- Debug effective settings resolve `SDKROOT` to the iPhone Simulator SDK and
  `SUPPORTED_PLATFORMS` to `iphonesimulator` only.
- The iOS test compile definitions contain `HAVE_OPENSSL=1` and
  `USE_SMIME_OPENSSL=1`; `USE_SMIME_APPLE`, `HAVE_GNUTLS`, and `HAVE_CURL` are
  absent.
- A generic iOS Simulator build successfully compiled `libetpan-ios.a`, selected
  the simulator OpenSSL Crypto archive, and linked the XCTest bundle for arm64
  and x86_64.
- The shared scheme passed on an iPhone 17 Pro simulator running iOS 27.0: 3
  tests, 0 failures.
- The existing macOS shared scheme passed: 509 tests, 0 failures.
