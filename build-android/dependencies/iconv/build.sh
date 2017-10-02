#!/bin/sh

version=1.15
build_version=1
build_version=3
package_name=iconv-android

current_dir="`pwd`"

if test "x$ANDROID_NDK" = x ; then
  echo should set ANDROID_NDK before running this script.
  exit 1
fi

if test ! -f $current_dir/build-android/$libiconv-$version.tar.gz; then
  cd build-android
  curl -O http://ftp.gnu.org/gnu/libiconv/libiconv-$version.tar.gz
  cd ..
fi

function build {
  rm -rf "$current_dir/build-android/obj"
  rm -rf "$current_dir/build-andnroid/libiconv-$version"
  mkdir -p "$current_dir/build-android/libiconv-$version"
  cd "$current_dir/build-android/libiconv-$version"

  tar xzf "$current_dir/build-android/libiconv-$version.tar.gz"

  cd "$current_dir/build-android" $ANDROID_NDK/ndk-build TARGET_PLATFORM=$ANDROID_PLATFORM TARGET_ARCH_ABI=$TARGET_ARCH_ABI
}

# Start building.
ANDROID_PLATFORM=android-16
archs="armeabi armeabi-v7a x86"
for arch in $archs ; do
  TARGET_ARCH_ABI=$arch
  build
done
ANDROID_PLATFORM=android-21
archs="arm64-v8a"
for arch in $archs ; do
  TARGET_ARCH_ABI=$arch
  build
done

cd "$current_dir/build-android/"
zip -qry "$package_name-$build_version.zip" "$current_dir/build-android/obj/local"
rm -rf "$package_name-$build_version"
