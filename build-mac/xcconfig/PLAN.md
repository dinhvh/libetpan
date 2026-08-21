# Xcode configuration cleanup plan

## Intended feature matrix

| Capability | iOS | macOS |
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

Disabled feature macros must be absent from the effective preprocessor definitions.

## Configuration structure

1. Keep settings shared by all product targets in `libetpan-project.xcconfig`:
   - `HAVE_CFNETWORK=1`
   - `HAVE_CONFIG_H=1`
   - `HAVE_COREFOUNDATION_CHARCONV=1`
   - `HAVE_JMAP=1`
   - `HAVE_ZLIB=1`
   - `USE_PGP_RNP=1`
   - `USE_SASL=1`
   - Common language, warning, header-search, and Debug settings.

2. Add `libetpan-macos.xcconfig` for settings shared by the macOS framework and
   static-library targets:
   - `USE_SMIME_APPLE=1`
   - The common macOS deployment target and SDK settings.
   - Any other settings that are genuinely common to both macOS products.

3. Add `libetpan-macos.xcconfig` to the Xcode project's xcconfig group and include
   it from both `libetpan-framework.xcconfig` and
   `libetpan-static-macos.xcconfig`.

4. Keep only the iOS-specific feature definitions in
   `libetpan-static-ios.xcconfig`:
   - `HAVE_OPENSSL=1`
   - `USE_SMIME_OPENSSL=1`

5. Keep product-specific settings in the leaf configurations:
   - Framework versioning, wrapper, install path, product name, and framework
     linker settings in `libetpan-framework.xcconfig`.
   - Static-library product name and install behavior in the static macOS and
     iOS configurations.
   - XCFramework aggregate settings in `libetpan-xcframework.xcconfig`.

All target-level `GCC_PREPROCESSOR_DEFINITIONS` values must begin with
`$(inherited)` so project-level definitions remain effective.

## Dependency linkage

1. Link static dependency XCFrameworks only into the dynamic `libetpan.framework`
   target. For the requested feature matrix, that means RNP, CyrusSASL, JsonC,
   and OpenSSL Crypto as RNP's transitive cryptographic dependency. Do not link
   OpenSSL SSL, and do not define libetpan's OpenSSL feature macros on macOS.

2. Remove OpenSSL, RNP, CyrusSASL, and JsonC from the Frameworks build phases of
   the iOS and macOS static-library targets. A static archive does not absorb or
   re-export these dependencies; the final consumer is responsible for linking
   the dependency libraries required by the selected libetpan features.

3. Preserve explicit compile-time header visibility for the static targets after
   removing the Frameworks-phase entries. In particular:
   - iOS needs OpenSSL headers for OpenSSL S/MIME.
   - Both platforms need RNP, CyrusSASL, and JsonC headers.
   Prefer shared xcconfig search-path definitions over target-local project-file
   settings.

4. Keep the Apple Security, CoreFoundation, Foundation, and other required system
   frameworks on the dynamic macOS framework target. Document the corresponding
   system-library requirements for consumers of the static archives.

5. Keep `-lz` on the dynamic framework link. Document that static-library
   consumers must link zlib themselves.

## Source membership

- Leave `mailhttp_curl.c` in the source tree and in the existing target source
  phases. With `HAVE_CURL` undefined, its curl implementation is compiled out.

## Remove deprecated, obsolete, or redundant settings

- Remove `ALWAYS_SEARCH_USER_PATHS` from all xcconfigs.
- Remove `ENABLE_BITCODE`.
- Remove `GCC_MODEL_TUNING = G5`.
- Remove `ZERO_LINK`.
- Remove duplicated target-level `GCC_OPTIMIZATION_LEVEL[config=Debug]` and keep
  the project-level definition.
- Remove `COMBINE_HIDPI_IMAGES` from these library/framework targets.
- Verify and remove redundant `COPY_PHASE_STRIP[config=Debug]` and
  `GCC_DYNAMIC_NO_PIC[config=Debug]` settings when Xcode's effective defaults are
  equivalent.
- Review remaining settings with `xcodebuild -showBuildSettings` before deleting
  any whose effect is uncertain.

## Verification

1. Run `xcodebuild -showBuildSettings` for each product target in Debug and
   Release and verify the complete feature matrix above.
2. Confirm that `HAVE_OPENSSL`, `USE_SMIME_OPENSSL`, `HAVE_GNUTLS`, and
   `HAVE_CURL` are absent from macOS settings as applicable, rather than defined
   as zero.
3. Confirm that the static targets retain all dependency header search paths but
   no longer attempt to link dependency XCFrameworks.
4. Build the macOS framework, macOS static library, iOS static library, and the
   XCFramework aggregate.
5. Link small consumer fixtures against each static archive plus its documented
   transitive dependencies. Building an archive alone cannot detect missing
   downstream linkage.
6. Run the deterministic unit tests covering S/MIME, JMAP, compression, and
   authentication.
