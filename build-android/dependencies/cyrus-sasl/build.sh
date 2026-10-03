#!/usr/bin/env bash

set -euo pipefail

# 2.1.28 (vs the old 2.1.26) ships upstream support for the OpenSSL 1.1.x
# opaque HMAC_CTX/EVP_MD_CTX/EVP_CIPHER_CTX API. 2.1.26 predates it and fails
# to compile against the OpenSSL 1.1.1 we build below. This matches the version
# already used by the iOS build.
version=2.1.28
build_version=4
ARCHIVE=cyrus-sasl-$version
openssl_build_version=3
package_name=cyrus-sasl-android

if test "x$ANDROID_NDK" = x ; then
  echo should set ANDROID_NDK before running this script.
  exit 1
fi

ARCHIVE_NAME=$ARCHIVE.tar.gz
ARCHIVE_PATCH=$ARCHIVE.patch
current_dir="$(cd "$(dirname "$0")" && pwd)"
cd "$current_dir"
package_dir="$current_dir/../../../build-mac/dependencies/packages"
output_dir="$current_dir/../build/$package_name-$build_version"
openssl_root="$current_dir/../build/openssl-android-$openssl_build_version"

if [ ! -e "$package_dir/$ARCHIVE_NAME" ]; then
  echo "Downloading $ARCHIVE_NAME"
  mkdir -p "$package_dir"
  curl -L -o "$package_dir/$ARCHIVE_NAME" \
    "https://github.com/cyrusimap/cyrus-sasl/releases/download/$ARCHIVE/$ARCHIVE_NAME"
fi

if [ ! -e "$package_dir/$ARCHIVE_NAME" ]; then
  echo "Missing archive $ARCHIVE"
  exit 1
fi

if test ! -d "$openssl_root" ; then
  echo Building OpenSSL first
  cd "$current_dir/../openssl"
  ./build.sh
fi
if test ! -d "$openssl_root/include" ; then
  echo "Missing OpenSSL build at $openssl_root"
  exit 1
fi

rm -rf "$output_dir"

function build {
  rm -rf "$current_dir/src"
  
  mkdir -p "$current_dir/src"
  cd "$current_dir/src"
  tar xzf "$package_dir/$ARCHIVE_NAME"
  if [ $? != 0 ]; then
    echo "Unable to decompress $ARCHIVE_NAME"
    exit 1
  fi

  if test ! -f "$output_dir/include/sasl/sasl.h" ; then
    mkdir -p "$output_dir"
    mkdir -p "$output_dir/include/sasl"
    # 2.1.28 no longer ships md5.h / md5global.h under include/; libetpan only
    # needs sasl.h and saslutil.h, so copy the headers that actually exist.
    public_headers="hmac-md5.h sasl.h saslplug.h saslutil.h prop.h"
    cd "$current_dir/src/$ARCHIVE/include"
    cp -R $public_headers "$output_dir/include/sasl"
  fi

  cp -R "$current_dir/build-android" "$current_dir/src/$ARCHIVE"
  cd "$current_dir/src/$ARCHIVE/build-android/jni"
  $ANDROID_NDK/ndk-build APP_PLATFORM=$ANDROID_PLATFORM TARGET_ARCH_ABI=$TARGET_ARCH_ABI \
    OPENSSL_PATH="$openssl_root"

  mkdir -p "$output_dir/libs/$TARGET_ARCH_ABI"
  cp "$current_dir/src/$ARCHIVE/build-android/obj/local/$TARGET_ARCH_ABI/libsasl2.a" "$output_dir/libs/$TARGET_ARCH_ABI"
  rm -rf "$current_dir/src"
}

# Start building.
	ANDROID_PLATFORM=android-23
archs="arm64-v8a"	archs="arm64-v8a armeabi-v7a x86 x86_64"
for arch in $archs ; do
  TARGET_ARCH_ABI=$arch
  build
done

cd "$current_dir"
echo "Created $output_dir"
