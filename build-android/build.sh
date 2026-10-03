#!/bin/sh -x

set -e

package_name=libetpan-android

current_dir=$(cd "$(dirname "$0")" && pwd)
output_dir="$current_dir/build/$package_name"
openssl_path="$current_dir/dependencies/build/openssl-android"
cyrus_sasl_path="$current_dir/dependencies/build/cyrus-sasl-android"
iconv_path="$current_dir/dependencies/build/iconv-android"


if test "x$ANDROID_NDK" = x ; then
  echo should set ANDROID_NDK before running this script.
  exit 1
fi

# NDK r23 is the first release with arm64 (Apple Silicon) host support. Older
# NDKs (e.g. the legacy ndk-bundle, r22) fail with
# "ERROR: Unknown host CPU architecture: arm64" before building anything.
# Detect that combination early and fail with a clear, actionable message
# instead of producing empty/partial output.
if test "`uname -s`" = "Darwin" && test "`uname -m`" = "arm64" ; then
  ndk_revision=`sed -n 's/^Pkg.Revision *= *//p' "$ANDROID_NDK/source.properties" 2>/dev/null`
  ndk_major=`echo "$ndk_revision" | cut -d. -f1`
  if test -n "$ndk_major" && test "$ndk_major" -lt 23 ; then
    echo "ERROR: ANDROID_NDK points to NDK r$ndk_revision, which has no arm64 host"
    echo "       support and cannot run on this Apple Silicon Mac."
    echo "       Set ANDROID_NDK to NDK r23 or newer, e.g.:"
    echo "         export ANDROID_NDK=\$HOME/Library/Android/sdk/ndk/27.1.12297006"
    echo "       Currently: ANDROID_NDK=$ANDROID_NDK"
    exit 1
  fi
fi

if test ! -d "$openssl_path" ; then
  echo Building OpenSSL first
  cd "$current_dir/dependencies/openssl"
  ./build.sh
fi

if test ! -d "$cyrus_sasl_path" ; then
  echo Building Cyrus SASL first
  cd "$current_dir/dependencies/cyrus-sasl"
  ./build.sh
fi

if test ! -d "$iconv_path" ; then
  echo Building ICONV first
  cd "$current_dir/dependencies/iconv"
  ./build.sh
fi

build() {
  rm -rf "$current_dir/obj"

  cd "$current_dir/jni"
  $ANDROID_NDK/ndk-build APP_PLATFORM=$ANDROID_PLATFORM TARGET_ARCH_ABI=$TARGET_ARCH_ABI \
    OPENSSL_PATH="$openssl_path" \
    CYRUS_SASL_PATH="$cyrus_sasl_path" \
    ICONV_PATH="$iconv_path" \
    JSON_C_PATH="$JSON_C_PATH"

  mkdir -p "$output_dir/libs/$TARGET_ARCH_ABI"
  cp "$current_dir/obj/local/$TARGET_ARCH_ABI/libetpan.a" "$output_dir/libs/$TARGET_ARCH_ABI"
  rm -rf "$current_dir/obj"
}

cd "$current_dir/.."
tar xzf "$current_dir/../build-mac/autogen-result.tar.gz"
./configure
# `make prepare` creates the include/libetpan symlinks but does NOT build
# libetpan-config.h (it is a BUILT_SOURCES target, only made by a full `make`).
# Generate it explicitly so the include/libetpan/libetpan-config.h symlink
# resolves and the headers copy below succeeds.
make prepare libetpan-config.h

# Copy the exact public header set rather than flattening every src header.
python3 "$current_dir/../tools/public_headers.py" \
  --source-root "$current_dir/.." \
  --build-root "$current_dir/.." \
  export-android --destination "$current_dir/include/libetpan"
rm -rf "$output_dir"
mkdir -p "$output_dir/include"
cp -r "$current_dir/include/libetpan" \
  "$output_dir/include"

# Start building.
ANDROID_PLATFORM=android-23
archs="arm64-v8a"	archs="arm64-v8a armeabi-v7a x86 x86_64"
for arch in $archs ; do
  TARGET_ARCH_ABI=$arch
  build
done

cd "$current_dir"
echo "Created $output_dir"
