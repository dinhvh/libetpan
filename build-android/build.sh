#!/bin/sh

build_version=1
ANDROID_PLATFORM=android-21
openssl_build_version=1
cyrus_sasl_build_version=1
archs="armeabi armeabi-v7a x86 x86_64"
package_name=libetpan-android

current_dir="`pwd`"

if test "x$ANDROID_NDK" = x ; then
  echo should set ANDROID_NDK before running this script.
  exit 1
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

function build {
  rm -rf "$current_dir/obj"
  
  cd "$current_dir/jni"
  $ANDROID_NDK/ndk-build TARGET_PLATFORM=$ANDROID_PLATFORM TARGET_ARCH_ABI=$TARGET_ARCH_ABI \
    OPENSSL_PATH="$current_dir/third-party/openssl-android-$openssl_build_version" \
    CYRUS_SASL_PATH="$current_dir/third-party/cyrus-sasl-android-$cyrus_sasl_build_version"

  mkdir -p "$current_dir/$package_name-$build_version/libs/$TARGET_ARCH_ABI"
  cp "$current_dir/obj/local/$TARGET_ARCH_ABI/libetpan.a" "$current_dir/$package_name-$build_version/libs/$TARGET_ARCH_ABI"
  rm -rf "$current_dir/obj"
}

mkdir -p "$current_dir/third-party"
cd "$current_dir/third-party"
unzip -qo "$current_dir/dependencies/openssl/openssl-android-$openssl_build_version.zip"
unzip -qo "$current_dir/dependencies/cyrus-sasl/cyrus-sasl-android-$cyrus_sasl_build_version.zip"

cd "$current_dir/.."
tar xzf "$current_dir/../build-mac/autogen-result.tar.gz"
./configure
make stamp-prepare

# Copy public headers to include
cp -rL include/libetpan "$current_dir/include"
mkdir -p "$current_dir/$package_name-$build_version/include"
cp -rL include/libetpan "$current_dir/$package_name-$build_version/include"

# Start building.
for arch in $archs ; do
  TARGET_ARCH_ABI=$arch
  build
done

rm -rf "$current_dir/third-party"
cd "$current_dir"
zip -qry "$package_name-$build_version.zip" "$package_name-$build_version"
rm -rf "$package_name-$build_version"
