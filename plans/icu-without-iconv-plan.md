# ICU without iconv on Linux and Android

## Goal and answers

Allow a build with usable ICU charset converters to omit iconv while preserving general email charset conversion. This document is an implementation plan; no build or runtime changes have been made.

- **Linux:** a separate GNU libiconv library is generally unnecessary on glibc systems because libc provides iconv. The existing configure logic first tries linking without `-liconv`, then tries the separate library. Other libc environments should be checked rather than treated as identical to glibc. [GNU gettext documentation](https://www.gnu.org/software/gettext/manual/html_node/AM_005fICONV.html).
- **Android today:** this repository's build requires bundled GNU libiconv. It targets API 23, requires `ICONV_PATH`, defines `HAVE_ICONV`, and packages `libiconv.a`. Android's own iconv starts at API 28 and supports a limited Unicode/ASCII encoding set, so it does not replace broad email charset support. [Bionic iconv header](https://android.googlesource.com/platform/bionic/+/main/libc/include/iconv.h).
- **With ICU:** ICU can provide the replacement backend, but simply enabling ICU and disabling iconv in the current code loses general charset conversion. ICU is currently attempted only for selected Japanese source encodings. Expand that dispatch and validate conversion behavior before skipping iconv.
- **Android ICU availability:** device-internal ICU is insufficient. The public native ICU subset starts at API 31 and uses `libicu.so`; it is not the desktop `icu-uc` package. Preserve API 23 by accepting an application-supplied, cross-compiled ICU4C build with converter data for each ABI. A platform ICU backend would be a separate future option requiring verification of every needed converter API. [Android ICU documentation](https://developer.android.com/guide/topics/resources/internationalization).

## Current repository behavior

- `configure.ac:361–441` detects iconv independently of ICU. `--disable-iconv` skips detection. The `AC_ARG_ENABLE` action currently sets `enable_iconv=no` for any supplied iconv option, including `--enable-iconv`; fix this parsing as part of the build work.
- `configure.ac:477–513` automatically detects `icu-uc`, permits `--with-icu=no`, and fails if explicitly requested ICU is missing. Finding ICU does not disable iconv.
- `src/data-types/charconv.c` implements ICU through `ucnv_convert()`. Both `charconv()` and `charconv_buffer()` restrict it through `icu_should_try_charset()`. Other source encodings fall through to iconv or return `MAIL_CHARCONV_ERROR_UNKNOWN_CHARSET` when iconv is absent.
- Existing alias normalization in `get_valid_charset()` applies before backend selection. Preserve it, including the GB2312-to-GBK and ISO-2022-JP-to-ISO-2022-JP-2 mappings.
- `src/Makefile.am` links `@ICULIB@`, but `libetpan.pc.in` does not include it. Static consumers need the selected backend's complete link dependencies.
- Android requires iconv in `build-android/dependencies/bootstrap.sh`, `build-android/build.sh`, and `build-android/jni/Android.mk`; the example preparation/CMake files and `tests/android-link-smoke-test.sh` also require its archive. There is no Android ICU build path today.
- Android's libxml2 recipe already sets `LIBXML2_WITH_ICONV=OFF`. Audit the other enabled dependencies before promising that the final application has no transitive iconv dependency.

## Proposed behavior

Keep existing defaults for compatibility. Make ICU-only an explicit supported configuration first:

| Configuration | Conversion behavior |
| --- | --- |
| ICU + iconv | Preserve current Japanese ICU preference and iconv behavior for other encodings initially. |
| ICU without iconv | Attempt ICU for every source/destination charset pair. Unsupported converters return the existing unknown-charset error. |
| iconv without ICU | Preserve existing iconv behavior. On glibc Linux, no separate libiconv is normally linked. |
| Neither backend | Preserve the reduced build and extension callback; document the loss of built-in general conversion. |

For Autotools, retain `--with-icu=yes --disable-iconv` as the explicit ICU-only request. Do not silently disable iconv merely because ICU is installed. Any later change to auto-selection should follow compatibility evidence from the tests below.

## Implementation steps

1. **Expand ICU-only dispatch.** In both conversion entry points, attempt ICU unconditionally when `HAVE_ICU` is defined and `HAVE_ICONV` is absent. Retain the Japanese allowlist in dual-backend builds. Preserve extension callback precedence, allocation ownership, NUL termination, explicit lengths, and error propagation. Add a regression for the existing difference in extension fallback: `charconv_buffer()` currently returns an unknown-charset callback result instead of continuing to the built-in backend; align it with `charconv()` when touching dispatch.
2. **Define conversion compatibility.** Characterize invalid sequences, truncated input, unmappable output, charset aliases, and stateful encodings before changing ICU error handling. The iconv wrapper substitutes `?` for some errors; ICU has its own substitution defaults, so successful output is not automatically equivalent. Use explicit ICU converters/callbacks if necessary to implement the chosen behavior. Distinguish unsupported charset names from corrupt or missing packaged converter data. Handle output expansion and ICU's 32-bit length limits without overflow or truncated success. [ICU converter documentation](https://unicode-org.github.io/icu/userguide/conversion/converters.html).
3. **Finish Autotools integration.** Correct iconv option parsing, update ICU help text for ICU-only support, and report selected backends in configure output. Ensure disabled iconv contributes no include paths, defines, or link flags. Add ICU link requirements to pkg-config metadata in the appropriate public/private fields and verify static consumers, including ICU data and C++ runtime requirements where applicable. Regenerate tracked generated files using repository conventions.
4. **Add Android backend selection.** Introduce `CHARCONV_BACKEND=iconv|icu`, defaulting to `iconv`, consistently across bootstrap, library build, example packaging, and smoke tests. For `icu`, accept a supplied `ICU_PATH` containing headers and per-ABI libraries/data, validate all requested ABIs, skip the iconv build/checks, define `HAVE_ICU`, and remove `HAVE_ICONV` and iconv include/link requirements. Validate target ICU independently of the host `./configure` run. Record the selected backend and its consumer link requirements with the output artifacts so examples cannot accidentally mix cached variants.
5. **Verify dependency closure and document usage.** Audit static and shared dependencies for remaining iconv references; disable optional dependency iconv support where appropriate or report it as a remaining requirement. Update `build-android/README.md` with the ICU input layout, data packaging, backend selector, and link order. Keep the libiconv source/submodule available for the default backend and other platforms.

## Validation

Use the existing unit suites as the primary regression gate: establish an iconv-enabled baseline, rebuild the library and test executables with `--with-icu=yes --disable-iconv`, and run `make -C unittest check`. Compare existing expected results without regenerating them to accommodate backend differences. In particular, `unittest/charconv/main.c` already checks exact Japanese conversion bytes, and the plaintext-rendering code exercises `charconv_buffer()` for message bodies and charset fallback. MIME and IMAP modified UTF-7 suites provide surrounding behavior coverage. The charset-detection suite currently uses local detection logic and fixture filenames rather than calling the conversion backend, so its success alone does not demonstrate ICU conversion coverage.

Review coverage after running those suites and add only missing cases below. Verify `HAVE_ICONV` is absent from the compiled library configuration and inspect symbol/link dependencies so a passing run cannot accidentally rely on iconv or stale binaries. Keep existing expected outputs authoritative; investigate differences before approving any intentional behavior change.

Extend `unittest/charconv/main.c` and preserve `unittest/Makefile` conventions. Exercise both public conversion APIs with fixed byte fixtures for ASCII, UTF-8, Latin-1, Windows-1252, Cyrillic, GBK/GB2312, Big5, Korean, and the existing Japanese cases. Include conversions from UTF-8 into legacy encodings, embedded NULs for the buffer API, empty input, unknown source/destination names, malformed/truncated input, output expansion, and extension callback success/unknown/error paths. Assert exact bytes, lengths, error codes, and ownership behavior. Unsupported mappings must have deliberate expectations for each backend.

Build in clean configurations to avoid stale `config.h`, objects, or cached dependency artifacts:

- Linux: ICU-only (`--with-icu=yes --disable-iconv`), libc-iconv-only (`--with-icu=no`), both enabled, and both disabled. Verify explicit ICU failure when unavailable and correct `--enable-iconv` parsing. Run focused charset tests and relevant MIME decoding tests for supported configurations.
- Link an installed consumer with `pkg-config --static --libs libetpan` in ICU-only mode. Inspect objects/final links for iconv/libiconv references, not just archive filenames. Audit transitive dependencies separately.
- Android: build both backend variants for arm64-v8a, armeabi-v7a, x86, and x86_64 at API 23; link the example and smoke consumer with the selected dependencies. Run ICU fixture conversions on a device/emulator to validate runtime converter data. Keep device/emulator tests and manual build/link diagnostics in `tests/`; deterministic local fixture and mocked build-selection tests belong in `unittest/`.

## Completion criteria

An ICU-only Linux build and API-23 Android build decode the agreed email charset fixtures through both APIs; libetpan itself has no iconv symbol references or link requirement; any transitive iconv requirements are identified; static consumers link with documented ICU dependencies; and existing iconv defaults and Japanese conversion tests continue to pass.
