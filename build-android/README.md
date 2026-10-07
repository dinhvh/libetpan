# Building libetpan for Android

This directory builds `libetpan` (and its dependencies) as Android static
libraries, and contains a small Kotlin/Compose demo app that consumes them.

## Prerequisites

- **Android NDK r23 or newer.** On Apple Silicon (arm64) Macs, NDK r22 and the
  legacy `ndk-bundle` do **not** work — they abort with
  `ERROR: Unknown host CPU architecture: arm64`. `build.sh` detects this and
  stops early with a message. Tested with **NDK 27.1.12297006, 26.3.11579264**.
- Android SDK (for the demo app) and a JDK 17+ (for Gradle).
- Populated dependency submodules under `dependencies/submodules/`.
  `build-android/dependencies/bootstrap.sh` initializes them automatically.
- Native C/C++ compilers, GNU Make, Python 3, Autoconf, Automake, Libtool, and
  pkg-config. ICU builds native host tools before cross-compiling its libraries.

Point `ANDROID_NDK` at a suitable NDK, e.g.:

```sh
export ANDROID_NDK=$HOME/Library/Android/sdk/ndk/27.1.12297006
```

## Building the native libraries

```sh
export ANDROID_NDK=$HOME/Library/Android/sdk/ndk/27.1.12297006
cd build-android
./build.sh
```

`build.sh` builds, for the ABIs **arm64-v8a, armeabi-v7a, x86, x86_64**
(min API level android-23):

1. **OpenSSL** (3.5.8) → `dependencies/build/openssl-android/`
2. **JSON-C** → `dependencies/build/json-c-android/`
3. **libcurl** → `dependencies/build/curl-android/`
4. **Cyrus SASL** (2.1.28) → `dependencies/build/cyrus-sasl-android/`
5. **ICU** (78.2) → `dependencies/build/icu-android/`
6. **libxml2** → `dependencies/build/libxml2-android/`
7. **RNP** → `dependencies/build/rnp-android/`
8. **libetpan** → `build/libetpan-android/`

Each dependency is only rebuilt if its output directory is missing, so re-runs
are fast. After changing NDK versions, rebuild the cached dependencies together
to keep their C++ runtime compatible. To force all dependency rebuilds, run:

```sh
./dependencies/bootstrap.sh --force
```

### Building ICU separately

The ICU dependency script builds the pinned ICU submodule for Android:

```sh
git submodule update --init dependencies/submodules/icu
ANDROID_NDK=/path/to/android-ndk build-android/dependencies/icu/build.sh
```

Run this command from the repository root. It defaults to API 23 and all four
ABIs listed above. Set `ANDROID_PLATFORM`, `ANDROID_ABIS` (space-separated),
and `JOBS` to override those defaults. Native host compilers are required;
`HOST_CC` and `HOST_CXX` can override `cc` and `c++`. The script also requires
GNU Make and Python 3, and builds ICU's host data-generation tools first.

Output is `build-android/dependencies/build/icu-android/include/unicode/` and
`libs/<abi>/libicuuc.a`, `libicudata.a`, with the upstream license. Both archives
are static and compiled for inclusion in a JNI shared library. Conversion data
and supporting Unicode data are embedded; locale, collation, and break-iterator
data are excluded. Link `icuuc` before `icudata`, using the NDK C++ runtime.
The extra reviewed converters from `icu-data` remain embedded in libetpan's
generated charset header.

The script rebuilds its output on each direct invocation. `bootstrap.sh` builds
and caches ICU automatically. The libetpan Android build and demo use ICU for
charset conversion, with iconv disabled. No GNU libiconv checkout or download
is needed.

### Output layout

Each output directory contains `libs/<abi>/*.a` and headers, e.g.
`build/libetpan-android/`:

```
libetpan-android/
  include/libetpan/libetpan-config.h
  libs/arm64-v8a/libetpan.a
  libs/armeabi-v7a/libetpan.a
  libs/x86/libetpan.a
  libs/x86_64/libetpan.a
```

`libetpan.a` is **not** self-contained: linking it also requires `librnp.a`,
`libcurl.a`, `libxml2.a`, `libjson-c.a`, `libsasl2.a`, `libssl.a`,
`libcrypto.a`, `libicuuc.a`, and `libicudata.a`, plus the NDK C++ runtime and
system libs `z` and `log`. Link
order matters (consumers before providers):

```
etpan  rnp  curl  xml2  json-c  sasl2  ssl  crypto  icuuc  icudata  z  log
```

> Note: `build/libetpan-android/` currently contains only the exported
> libetpan headers copied by the Android build. The demo app's
> `prepare-libs.sh` sources those headers from `build-android/include/libetpan/`.

## Demo app

`example/` is a Kotlin + Jetpack Compose app that uses the libraries above via a
small C JNI bridge:

- **IMAP** — connect over implicit TLS (IMAPS:993), log in, list the newest
  message envelopes.
- **SMTP** — compose and send a plain-text message over implicit TLS
  (SMTPS:465), reusing the logged-in identity.

### Build & install

```sh
# 1. Build the native libraries first (see above) so the artifacts exist.
export ANDROID_NDK=$HOME/Library/Android/sdk/ndk/27.1.12297006

cd build-android/example

# 2. Stage the prebuilt .a files + headers into app/src/main/cpp/prebuilt/
./prepare-libs.sh

# 3. Build / install the debug APK (needs a device or emulator for install)
./gradlew installDebug      # or: ./gradlew assembleDebug
```

`prepare-libs.sh` copies the per-ABI static libs and headers out of the
directories produced by `build.sh`; it must be re-run if you rebuild the native
libraries.
The Gradle build uses CMake (`app/src/main/cpp/CMakeLists.txt`) to link the JNI
shared library `libetpanjni.so` against the staged static libs.

## Link smoke test

After building the native libraries, run the Android link smoke test from the
repository root to verify that the dependency-backed low-level features link for
all supported ABIs:

```sh
ANDROID_NDK=/path/to/android-ndk tests/android-link-smoke-test.sh
```

Set the SDK location via `local.properties` (`sdk.dir=...`) or the
`ANDROID_HOME` environment variable. If Gradle complains about the JDK, run it
with `JAVA_HOME` pointing at a JDK 17.

### Using the app

1. Launch it, enter your IMAP host (e.g. `imap.gmail.com`), port `993`, email and
   password (an app password for Gmail), and tap **Connect** to list the inbox.
2. Tap **Compose**, enter the SMTP host (e.g. `smtp.gmail.com`), port `465`,
   recipient, subject and body, and tap **Send**.

Connection/authentication failures are shown as an on-screen banner with the
libetpan error code.

> Security note: the demo does not verify server TLS certificates — it is a
> sample, not a hardened mail client.
