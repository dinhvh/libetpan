# Building libetpan for Android

This directory builds `libetpan` (and its dependencies) as Android static
libraries, and contains a small Kotlin/Compose demo app that consumes them.

## Prerequisites

- **Android NDK r23 or newer.** On Apple Silicon (arm64) Macs, NDK r22 and the
  legacy `ndk-bundle` do **not** work — they abort with
  `ERROR: Unknown host CPU architecture: arm64`. `build.sh` detects this and
  stops early with a message. Tested with **NDK 27.1.12297006, 26.3.11579264**.
- Android SDK (for the demo app) and a JDK 17+ (for Gradle).
- Network access on the first run — the dependency sources (OpenSSL, Cyrus SASL,
  libiconv) are downloaded automatically.

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

1. **OpenSSL** (1.1.1w) → `dependencies/openssl/openssl-android-3.zip`
2. **Cyrus SASL** (2.1.28) → `dependencies/cyrus-sasl/cyrus-sasl-android-4.zip`
3. **libiconv** (1.15) → `dependencies/iconv/iconv-android-1.zip`
4. **libetpan** → `libetpan-android-7.zip`

Each dependency is only rebuilt if its `*.zip` is missing, so re-runs are fast.
To force a dependency rebuild, delete its zip first.

### Output layout

Each zip contains `libs/<abi>/*.a` and headers, e.g. `libetpan-android-7.zip`:

```
libetpan-android-7/
  include/libetpan/libetpan-config.h
  libs/arm64-v8a/libetpan.a
  libs/armeabi-v7a/libetpan.a
  libs/x86/libetpan.a
  libs/x86_64/libetpan.a
```

`libetpan.a` is **not** self-contained: linking it also requires
`libssl.a`, `libcrypto.a` (OpenSSL), `libsasl2.a` (Cyrus SASL) and
`libiconv.a`, plus the NDK system libs `z` and `log`. Link order matters
(consumers before providers):

```
etpan  sasl2  ssl  crypto  iconv  z  log
```

> Note: `libetpan-android-7.zip` currently ships only `libetpan-config.h`. The
> full public API headers (`mailimap.h`, `mailsmtp.h`, …) are written to
> `build-android/include/libetpan/` during the build; the demo app's
> `prepare-libs.sh` sources them from there.

## Demo app

`demo-app/` is a Kotlin + Jetpack Compose app that uses the libraries above via a
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

`prepare-libs.sh` copies the per-ABI static libs and headers out of the zips
produced by `build.sh`; it must be re-run if you rebuild the native libraries.
The Gradle build uses CMake (`app/src/main/cpp/CMakeLists.txt`) to link the JNI
shared library `libetpanjni.so` against the five static libs.

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
