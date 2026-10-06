## Dependencies

This file lists build tools and optional feature dependencies by platform.

## Linux

- Required: a C compiler, `make`, pthreads, Autoconf, Automake, and Libtool.
- Optional: OpenSSL for TLS and OpenSSL-backed S/MIME.
- Optional: GnuTLS as an alternate TLS backend.
- Optional: Cyrus SASL for SASL authentication.
- Optional: iconv, or ICU before iconv, for charset conversion.
- Optional: Berkeley DB or LMDB for cache database support.
- Optional: libxml2 for RSS/Atom feed support.
- Optional: JSON-C and libcurl for JSON and JMAP support.
- Optional: zlib 1.2.0.4 or newer for compressed streams.
- Optional: RNP for OpenPGP support.
- Optional: liblockfile for mailbox locking on systems that provide it.

On Ubuntu, install the build tools with:

```sh
sudo apt update
sudo apt install build-essential autoconf automake libtool pkg-config
```

Install optional feature dependencies with:

```sh
sudo apt install \
  libssl-dev \
  libgnutls28-dev \
  libsasl2-dev \
  libicu-dev \
  libdb-dev \
  liblmdb-dev \
  libxml2-dev \
  libjson-c-dev \
  libcurl4-openssl-dev \
  zlib1g-dev \
  librnp-dev \
  liblockfile-dev
```

If a package is unavailable on your Ubuntu release, omit it or install that library from another source; `./configure` will disable the corresponding optional feature unless it was explicitly requested.

## macOS with configure and make

- Required: Xcode and Apple SDK, `make`, pthreads, Autoconf, Automake, and Libtool.
- Optional: OpenSSL for TLS and OpenSSL-backed S/MIME.
- Optional: GnuTLS as an alternate TLS backend.
- Optional: Cyrus SASL for SASL authentication.
- Optional: iconv, or ICU before iconv, for charset conversion.
- Optional: Berkeley DB or LMDB for cache database support.
- Optional: libxml2 for RSS/Atom feed support.
- Optional: JSON-C with libcurl or NSURLSession for JSON and JMAP support.
- Optional: zlib 1.2.0.4 or newer for compressed streams.
- Optional: RNP for OpenPGP support.
- Optional: liblockfile for mailbox locking on systems that provide it.

Using Homebrew is preferred for the Unix-style macOS build. Install the build tools with:

```sh
xcode-select --install
brew install autoconf automake libtool pkg-config
```

Install optional feature dependencies with:

```sh
brew install \
  openssl@3 \
  gnutls \
  cyrus-sasl \
  libiconv \
  icu4c \
  berkeley-db \
  lmdb \
  libxml2 \
  json-c \
  curl \
  zlib \
  rnp
```

If `./configure` does not find a Homebrew library automatically, pass the relevant prefix, for example `./configure --with-openssl=$(brew --prefix openssl@3)`.

## iOS with configure and make

Install Xcode, CMake, Autoconf, Automake, Libtool, and pkg-config:

```sh
brew install cmake autoconf automake libtool pkg-config
NOCONFIGURE=1 ./autogen.sh
build-mac/dependencies/bootstrap.sh
bash build-mac/build-unix-ios.sh
```

Dependencies are built from submodules: OpenSSL, Cyrus SASL, JSON-C, and RNP.
Apple SDK libraries provide iconv, libxml2, zlib, and NSURLSession.

## macOS and iOS with Xcode

- Required: Xcode and Apple SDK.

If dependency XCFrameworks are already present in `build-mac/dependencies/build`, use:

```sh
cd build-mac
./bootstrap.sh --skip-dependencies
```

The other Xcode dependency libraries are provided as source submodules:

- JSON-C
- Cyrus SASL
- OpenSSL
- RNP
- tidy-html5, for tests

`build-mac/bootstrap.sh` builds those submodules by default. Install these tools only when rebuilding dependency XCFrameworks:

```sh
xcode-select --install
brew install cmake autoconf automake libtool
```

`build-mac/bootstrap.sh` initializes the submodules automatically. To do it manually, run:

```sh
git submodule update --init --recursive -- dependencies/submodules
```

## Android

- Required: Android NDK r23 or newer.
- Required for the sample app: Android SDK, Gradle, and JDK 17 or newer.
- Required on the first native build: network access to download OpenSSL, Cyrus SASL, and libiconv sources.
- Required: OpenSSL for TLS.
- Required: Cyrus SASL for SASL authentication.
- Required: libiconv for charset conversion.
- Required: JSON-C through `JSON_C_PATH` for JSON and JMAP support.
- Required: Android system `z` and `log` libraries.

## Windows

- Required: Visual Studio 2013, 2015, or 2017.
- Required: zlib for compressed streams.
- Required: OpenSSL for TLS.
- Required: Cyrus SASL for SASL authentication.
- Required: JSON-C for JSON support.
- Required: Winsock from the Windows SDK.
