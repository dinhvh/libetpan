#!/usr/bin/env bash

set -euo pipefail

version=1.15
build_version=1
package_name=iconv-android
current_dir="$(cd "$(dirname "$0")" && pwd)"
cd "$current_dir"

create_zip() {
  local zip_path="$1"
  local entry_name="$2"

  rm -f "$zip_path"
  if command -v zip >/dev/null 2>&1; then
    zip -qry "$zip_path" "$entry_name"
  elif command -v python3 >/dev/null 2>&1; then
    python3 - "$zip_path" "$entry_name" <<'PY'
import os
import sys
import zipfile

zip_path, entry_name = sys.argv[1], sys.argv[2]
with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as archive:
    for root, _, files in os.walk(entry_name):
        for name in files:
            path = os.path.join(root, name)
            archive.write(path, path)
PY
  else
    echo "Required command not found: zip or python3" >&2
    exit 1
  fi
}

if test "x$ANDROID_NDK" = x ; then
  echo should set ANDROID_NDK before running this script.
  exit 1
fi

function build {
  cd "$current_dir/build-android" 
  $ANDROID_NDK/ndk-build APP_PLATFORM=$ANDROID_PLATFORM TARGET_ARCH_ABI=$TARGET_ARCH_ABI
  mkdir -p "$current_dir/$package_name-$build_version/libs/$TARGET_ARCH_ABI"
  cp "$current_dir/build-android/obj/local/$TARGET_ARCH_ABI/libiconv.a" "$current_dir/$package_name-$build_version/libs/$TARGET_ARCH_ABI"
}

if test ! -f $current_dir/$package_name-$build_version.zip; then
  if test ! -f $current_dir/build-android/libiconv-$version.tar.gz; then
    cd "$current_dir/build-android"
    curl -O http://ftp.gnu.org/gnu/libiconv/libiconv-$version.tar.gz
    cd ..
  fi
  
  #rm -rf "$current_dir/build-android/libiconv"
  if test ! -d $current_dir/build-android/libiconv; then
    cd "$current_dir/build-android"
  	tar xzf "$current_dir/build-android/libiconv-$version.tar.gz"
  	mv -v "$current_dir/build-android/libiconv-$version" "$current_dir/build-android/libiconv"

  	cd "$current_dir/build-android/libiconv"
  	./configure

  	# Disable HAVE_LANGINFO_CODESET
  	cd "$current_dir/build-android/libiconv/libcharset"
  	sed -i.original 's/HAVE_LANGINFO_CODESET 1/HAVE_LANGINFO_CODESET 0/g' config.h

  	cd "$current_dir"
  fi  

  rm -rf "$current_dir/build-android/obj"
  mkdir -p "$current_dir/$package_name-$build_version/libs"
  cp -r "$current_dir/build-android/libiconv/include" "$current_dir/$package_name-$build_version"

  mkdir -p "$current_dir/$package_name-$build_version"

  # Start building.
  ANDROID_PLATFORM=android-23
  archs="arm64-v8a armeabi-v7a x86 x86_64"
  for arch in $archs ; do
    TARGET_ARCH_ABI=$arch
    build
  done

  cd "$current_dir"
  create_zip "$package_name-$build_version.zip" "$package_name-$build_version"
  rm -rf "$package_name-$build_version"
fi 
