#!/usr/bin/env bash

set -euo pipefail

version=1.15
package_name=iconv-android
current_dir="$(cd "$(dirname "$0")" && pwd)"
repo_root="$(cd "$current_dir/../../.." && pwd)"
submodule_dir="$repo_root/build-mac/dependencies/submodules/libiconv"
output_dir="$current_dir/../build/$package_name"
cd "$current_dir"

if test "x$ANDROID_NDK" = x ; then
  echo should set ANDROID_NDK before running this script.
  exit 1
fi

function build {
  cd "$current_dir/build-android" 
  $ANDROID_NDK/ndk-build APP_PLATFORM=$ANDROID_PLATFORM TARGET_ARCH_ABI=$TARGET_ARCH_ABI
  mkdir -p "$output_dir/libs/$TARGET_ARCH_ABI"
  cp "$current_dir/build-android/obj/local/$TARGET_ARCH_ABI/libiconv.a" "$output_dir/libs/$TARGET_ARCH_ABI"
}

if test ! -d "$output_dir"; then
  if test ! -f "$submodule_dir/configure" && test ! -f $current_dir/build-android/libiconv-$version.tar.gz; then
    cd "$current_dir/build-android"
    curl -O http://ftp.gnu.org/gnu/libiconv/libiconv-$version.tar.gz
    cd ..
  fi
  
  #rm -rf "$current_dir/build-android/libiconv"
  if test ! -d $current_dir/build-android/libiconv; then
    cd "$current_dir/build-android"
    if test -f "$submodule_dir/configure" ; then
      mkdir -p "$current_dir/build-android/libiconv"
      (
        cd "$submodule_dir"
        tar --exclude=.git -cf - .
      ) | (
        cd "$current_dir/build-android/libiconv"
        tar -xf -
      )
    else
      tar xzf "$current_dir/build-android/libiconv-$version.tar.gz"
      mv -v "$current_dir/build-android/libiconv-$version" "$current_dir/build-android/libiconv"
    fi

  	cd "$current_dir/build-android/libiconv"
  	./configure

  	# Disable HAVE_LANGINFO_CODESET
  	cd "$current_dir/build-android/libiconv/libcharset"
  	sed -i.original 's/HAVE_LANGINFO_CODESET 1/HAVE_LANGINFO_CODESET 0/g' config.h

  	cd "$current_dir"
  fi  

  rm -rf "$current_dir/build-android/obj"
  mkdir -p "$output_dir/libs"
  cp -r "$current_dir/build-android/libiconv/include" "$output_dir"

  mkdir -p "$output_dir"

  # Start building.
  ANDROID_PLATFORM=android-23
  archs="arm64-v8a armeabi-v7a x86 x86_64"
  for arch in $archs ; do
    TARGET_ARCH_ABI=$arch
    build
  done

  cd "$current_dir"
  echo "Created $output_dir"
fi 
