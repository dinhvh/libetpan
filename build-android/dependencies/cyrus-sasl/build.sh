#!/bin/sh

version=2.1.26
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
current_dir="`pwd`"
package_dir="$current_dir/../../../build-mac/dependencies/packages"

if [ ! -e "$package_dir/$ARCHIVE_NAME" ]; then
  echo "Missing archive $ARCHIVE"
  exit 1
fi

if test ! -f "$current_dir/../openssl/openssl-android-$openssl_build_version.zip" ; then
  echo Building OpenSSL first
  cd "$current_dir/../openssl"
  ./build.sh
fi

function build {
  rm -rf "$current_dir/src"
  
  mkdir -p "$current_dir/src"
  cd "$current_dir/src"
  tar xzf "$package_dir/$ARCHIVE_NAME"
  if [ $? != 0 ]; then
    echo "Unable to decompress $ARCHIVE_NAME"
    exit 1
  fi
  
  if test ! -f "$current_dir/$package_name-$build_version/include/sasl/sasl.h" ; then
    mkdir -p "$current_dir/$package_name-$build_version"
    mkdir -p "$current_dir/$package_name-$build_version/include/sasl"
    public_headers="hmac-md5.h md5.h md5global.h sasl.h saslplug.h saslutil.h prop.h"
    cd "$current_dir/src/$ARCHIVE/include"
    cp -R $public_headers "$current_dir/$package_name-$build_version/include/sasl"
  fi

  cd "$current_dir/src"
  unzip -q "$current_dir/../openssl/openssl-android-$openssl_build_version.zip"

  cp -R "$current_dir/build-android" "$current_dir/src/$ARCHIVE"
  cd "$current_dir/src/$ARCHIVE/build-android/jni"
  $ANDROID_NDK/ndk-build TARGET_PLATFORM=$ANDROID_PLATFORM TARGET_ARCH_ABI=$TARGET_ARCH_ABI \
    OPENSSL_PATH="$current_dir/src/openssl-android-$openssl_build_version"

  mkdir -p "$current_dir/$package_name-$build_version/libs/$TARGET_ARCH_ABI"
  cp "$current_dir/src/$ARCHIVE/build-android/obj/local/$TARGET_ARCH_ABI/libsasl2.a" "$current_dir/$package_name-$build_version/libs/$TARGET_ARCH_ABI"
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
zip -qry "$package_name-$build_version.zip" "$package_name-$build_version"
rm -rf "$package_name-$build_version"
