#!/bin/sh -x

build_version=7
openssl_build_version=3
cyrus_sasl_build_version=4
iconv_build_version=1
package_name=libetpan-android

current_dir="`pwd`"
#find ../src -name *.h | xargs -0 cp --target-directory=./include/libetpan

mkdir -p ./include/libetpan
find ../src -name "*.h" -type file -exec cp {} ./include/libetpan \;


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

if test ! -f "$current_dir/dependencies/openssl/openssl-android-$openssl_build_version.zip" ; then
  echo Building OpenSSL first
  cd "$current_dir/dependencies/openssl"
  ./build.sh
fi

if test ! -f "$current_dir/dependencies/cyrus-sasl/cyrus-sasl-android-$cyrus_sasl_build_version.zip" ; then
  echo Building Cyrus SASL first
  cd "$current_dir/dependencies/cyrus-sasl"
  ./build.sh
fi

if test ! -f "$current_dir/dependencies/iconv/iconv-android-$iconv_build_version.zip" ; then
  echo Building ICONV first
  cd "$current_dir/dependencies/iconv"
  ./build.sh
fi

function build {
  rm -rf "$current_dir/obj"

  cd "$current_dir/jni"
  $ANDROID_NDK/ndk-build TARGET_PLATFORM=$ANDROID_PLATFORM TARGET_ARCH_ABI=$TARGET_ARCH_ABI \
    OPENSSL_PATH="$current_dir/third-party/openssl-android-$openssl_build_version" \
    CYRUS_SASL_PATH="$current_dir/third-party/cyrus-sasl-android-$cyrus_sasl_build_version" \
    ICONV_PATH="$current_dir/third-party/iconv-android-$iconv_build_version"

  mkdir -p "$current_dir/$package_name-$build_version/libs/$TARGET_ARCH_ABI"
  cp "$current_dir/obj/local/$TARGET_ARCH_ABI/libetpan.a" "$current_dir/$package_name-$build_version/libs/$TARGET_ARCH_ABI"
  rm -rf "$current_dir/obj"
}

mkdir -p "$current_dir/third-party"
cd "$current_dir/third-party"
unzip -qo "$current_dir/dependencies/openssl/openssl-android-$openssl_build_version.zip"
unzip -qo "$current_dir/dependencies/cyrus-sasl/cyrus-sasl-android-$cyrus_sasl_build_version.zip"
unzip -qo "$current_dir/dependencies/iconv/iconv-android-$iconv_build_version.zip"

cd "$current_dir/.."
tar xzf "$current_dir/../build-mac/autogen-result.tar.gz"
./configure
# `make prepare` creates the include/libetpan symlinks but does NOT build
# libetpan-config.h (it is a BUILT_SOURCES target, only made by a full `make`).
# Generate it explicitly so the include/libetpan/libetpan-config.h symlink
# resolves and the headers copy below succeeds.
make prepare libetpan-config.h

# Copy public headers to include
cp -r include/libetpan "$current_dir/include"
mkdir -p "$current_dir/$package_name-$build_version/include"
cp -r include/libetpan "$current_dir/$package_name-$build_version/include"

# Start building.
ANDROID_PLATFORM=android-23
archs="arm64-v8a"	archs="arm64-v8a armeabi-v7a x86 x86_64"
for arch in $archs ; do
  TARGET_ARCH_ABI=$arch
  build
done

rm -rf "$current_dir/third-party"
cd "$current_dir"
zip -qry "$package_name-$build_version.zip" "$package_name-$build_version"
rm -rf "$package_name-$build_version"
