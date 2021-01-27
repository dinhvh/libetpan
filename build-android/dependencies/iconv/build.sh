#!/bin/sh

version=1.15
build_version=1
package_name=iconv-android
current_dir="`pwd`"

if test "x$ANDROID_NDK" = x ; then
  echo should set ANDROID_NDK before running this script.
  exit 1
fi

function build {
  cd "$current_dir/build-android" 
  $ANDROID_NDK/ndk-build TARGET_PLATFORM=$ANDROID_PLATFORM TARGET_ARCH_ABI=$TARGET_ARCH_ABI
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
  	sed -i '.original' 's/HAVE_LANGINFO_CODESET 1/HAVE_LANGINFO_CODESET 0/g' config.h

  	cd "$current_dir"
  fi  

  rm -rf "$current_dir/build-android/obj"
  mkdir -p "$current_dir/$package_name-$build_version/libs/$arch_dir_name"
  cp -r "$current_dir/build-android/libiconv/include" "$current_dir/$package_name-$build_version"

  mkdir -p "$current_dir/$package_name-$build_version"

  # Start building.
  ANDROID_PLATFORM=android-23
  ANDROID_PLATFORM=android-16	  archs="arm64-v8a armeabi-v7a x86 x86_64"
  for arch in $archs ; do
    TARGET_ARCH_ABI=$arch
    build
  done

  cd "$current_dir"
  zip -qry "$package_name-$build_version.zip" "$package_name-$build_version"
  rm -rf "$package_name-$build_version"
fi 

