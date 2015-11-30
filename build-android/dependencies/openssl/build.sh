#!/bin/sh

build_version=2
version=1.0.2d
android_platform=android-16
package_name=openssl-android

if test "x$ANDROID_NDK" = x ; then
  echo should set ANDROID_NDK before running this script.
  exit 1
fi

if test ! -f packages/openssl-$version.tar.gz; then
  mkdir -p packages
  cd packages
  curl -O https://www.openssl.org/source/openssl-$version.tar.gz
  cd ..
fi

function build_x86_64 {
  toolchain=x86_64-4.9
  toolchain_name=x86_64-linux-android
  arch_cflags=""
  arch_ldflags=""
  arch_dir_name="x86_64"
  openssl_configure_mode="linux-x86_64"
  
  build
}

function build_armeabi {
  toolchain=arm-linux-androideabi-4.9
  toolchain_name=arm-linux-androideabi
  arch_cflags="-mthumb"
  arch_ldflags=""
  arch_dir_name="armeabi"
  openssl_configure_mode="android"
  
  build
}

function build_x86 {
  toolchain=x86-4.9
  toolchain_name=i686-linux-android
  arch_cflags="-march=i686 -msse3 -mstackrealign -mfpmath=sse"
  arch_ldflags=""
  arch_dir_name="x86"
  openssl_configure_mode="android-x86"
  
  build
}

function build_armeabi_v7a {
  toolchain=arm-linux-androideabi-4.9
  toolchain_name=arm-linux-androideabi
  arch_cflags="-march=armv7-a -mfloat-abi=softfp -mfpu=vfpv3-d16"
  arch_ldflags="-march=armv7-a -Wl,--fix-cortex-a8"
  arch_dir_name="armeabi-v7a"
  openssl_configure_mode="android-armv7"
  
  build
}

function build {
  rm -rf "$current_dir/src"
  mkdir -p "$current_dir/src"
  cd "$current_dir/src"
  
  tar xzf "$current_dir/packages/openssl-$version.tar.gz"
  cd openssl-$version

  toolchain_path="`pwd`/$toolchain_name"
  $ANDROID_NDK/build/tools/make-standalone-toolchain.sh --platform=$android_platform --toolchain=$toolchain --install-dir="$toolchain_path"
  toolchain_bin_path="$toolchain_path/bin"
  export TOOL=arm-linux-androideabi
  export ndk_toolchain_base_name="$toolchain_bin_path/$toolchain_name"
  export CC=$ndk_toolchain_base_name-gcc
  export CXX=$ndk_toolchain_base_name-g++
  export LINK=${CXX}
  export LD=$ndk_toolchain_base_name-ld
  export AR=$ndk_toolchain_base_name-ar
  export RANLIB=$ndk_toolchain_base_name-ranlib
  export STRIP=$ndk_toolchain_base_name-strip
  export CPPFLAGS="$arch_cflags -fpic -ffunction-sections -funwind-tables -fstack-protector -fno-strict-aliasing -finline-limit=64 "
  export CXXFLAGS="$arch_cflags -fpic -ffunction-sections -funwind-tables -fstack-protector -fno-strict-aliasing -finline-limit=64 -frtti -fexceptions "
  export CFLAGS="$arch_cflags -fpic -ffunction-sections -funwind-tables -fstack-protector -fno-strict-aliasing -finline-limit=64 "
  export LDFLAGS="$arch_ldflags"
  ./Configure "$openssl_configure_mode"
  make
  
  mkdir -p "$current_dir/$package_name-$build_version"
  mkdir -p "$current_dir/$package_name-$build_version/libs/$arch_dir_name"
  cp -r include "$current_dir/$package_name-$build_version"
  cp libcrypto.a libssl.a "$current_dir/$package_name-$build_version/libs/$arch_dir_name"
  cd "$current_dir"
  rm -rf src
}

# start building.
current_dir="`pwd`"
build_armeabi
build_armeabi_v7a
build_x86

cd "$current_dir"
zip -qry "$package_name-$build_version.zip" "$package_name-$build_version"
rm -rf "$package_name-$build_version"
