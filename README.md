## LibEtPan

LibEtPan is a portable C mail library for accessing, parsing, and composing email and news messages.

## Features

- Mail protocols and services: IMAP, SMTP, POP3, NNTP, JMAP, ActiveSync, RSS/Atom feeds, and Gmail helpers
- Message and content handling: RFC 822/IMF, MIME, and message composition
- Local mailbox storage: Maildir, mbox, MH, and cache database support
- Security and authentication: TLS, SASL, OpenPGP, and S/MIME
- Platform support for Linux, macOS, iOS, Android, and Windows

## Build instructions

The primary maintained build paths are `configure` and `make` on Linux and macOS, and Xcode on macOS and iOS.
Android and Windows build files are also included.

### Linux

Install Autoconf, Automake, and Libtool, then build:

```sh
./autogen.sh
./configure
make
```

On glibc Linux, iconv is provided by libc; a separate GNU libiconv library is
normally unnecessary. To use ICU for all charset conversion and omit iconv,
install ICU development headers, libraries, and converter data, then build:

```sh
./configure --with-icu=yes --disable-iconv
make
make -C unittest check
```

Run `make clean` before rebuilding an existing checkout with different backend
options. ICU-only conversion uses ICU's charset aliases and substitution rules;
malformed input can produce replacement characters rather than the iconv
wrapper's `?`. Missing converters return an unknown-charset error. The default
build continues to use iconv, with ICU preferred for selected Japanese encodings
when available. Use `--with-icu=no` for an iconv-only build.

### macOS with configure and make

Install Autoconf, Automake, and Libtool, then build:

```sh
./autogen.sh
./configure
make
```

### macOS and iOS with Xcode

Install Xcode and CMake, then prepare the workspace and dependencies:

```sh
cd build-mac
./bootstrap.sh
open libetpan.xcworkspace
```

### Android

Install Android NDK r23 or newer, set `ANDROID_NDK`, and build from `build-android`:

```sh
export ANDROID_NDK=$HOME/Library/Android/sdk/ndk/27.1.12297006
cd build-android
./build.sh
```

See `build-android/README.md` for dependency details, supported ABIs, and the sample app.

### Windows

Open `build-windows/libetpan.sln` in Visual Studio and build the solution.

See `build-windows/README.md` for supported Visual Studio versions and required third-party binaries.

## More information

See [DEPENDENCIES.md](DEPENDENCIES.md) for required and optional dependencies by platform.

See http://etpan.org/libetpan.html for more information and examples.
