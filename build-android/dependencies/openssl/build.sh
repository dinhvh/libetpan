#!/bin/sh

build_version=3
version=1.1.1i
package_name=openssl-android
export MIN_SDK_VERSION=23
export HOST_TAG=darwin-x86_64

if test "x$ANDROID_NDK" = x ; then
  echo should set ANDROID_NDK before running this script.
  exit 1
fi

if test ! -f packages/openssl-$version.tar.gz; then
  mkdir -p packages
  cd packages
  curl -O https://ftp.openssl.org/source/openssl-$version.tar.gz
  cd ..
fi

rm -rf "./src"
mkdir -p "./src"
cd "./src"

tar xzf "../packages/openssl-$version.tar.gz"


export TOOLCHAIN=$ANDROID_NDK/toolchains/llvm/prebuilt/$HOST_TAG

# openssl refers to the host specific toolchain as "ANDROID_NDK_HOME"
export ANDROID_NDK_HOME=$TOOLCHAIN
PATH=$TOOLCHAIN/bin:$PATH

mkdir -p build/openssl
cd "./openssl-$version"

# arm64
export TARGET_HOST=aarch64-linux-android
export ANDROID_ARCH=arm64-v8a

# openssl does not handle api suffix well
ln -sfn $TOOLCHAIN/bin/$TARGET_HOST$MIN_SDK_VERSION-clang $TOOLCHAIN/bin/$TARGET_HOST-clang

./Configure android-arm64 no-shared \
 -D__ANDROID_API__=$MIN_SDK_VERSION \
 --prefix=$PWD/build/$ANDROID_ARCH

make -j5
make install_sw
make clean
mkdir -p ../build/openssl/$ANDROID_ARCH
cp -R $PWD/build/$ANDROID_ARCH ../build/openssl/

# arm
export TARGET_HOST=arm-linux-androideabi
export ANDROID_ARCH=armeabi-v7a

# for 32-bit ARM, the compiler is prefixed with armv7a-linux-androideabi, but the binutils tools are prefixed with arm-linux-androideabi
ln -sfn $TOOLCHAIN/bin/armv7a-linux-androideabi$MIN_SDK_VERSION-clang $TOOLCHAIN/bin/$TARGET_HOST-clang

./Configure android-arm no-shared \
 -D__ANDROID_API__=$MIN_SDK_VERSION \
 --prefix=$PWD/build/$ANDROID_ARCH

export arch_dir_name="arm64-v8a"
mkdir -p "./../$package_name-$build_version"
mkdir -p "./../$package_name-$build_version/libs"
mkdir -p "./../$package_name-$build_version/libs/$arch_dir_name"
cp -r "./../build/openssl/arm64-v8a/include" "./../$package_name-$build_version"
cp "./../build/openssl/arm64-v8a/lib/libssl.a" "./../$package_name-$build_version/libs/$arch_dir_name"
cp "./../build/openssl/arm64-v8a/lib/libcrypto.a" "./../$package_name-$build_version/libs/$arch_dir_name"

make -j5
make install_sw
make clean
mkdir -p ../build/openssl/$ANDROID_ARCH
cp -R $PWD/build/$ANDROID_ARCH ../build/openssl/

# x86
export TARGET_HOST=i686-linux-android
export ANDROID_ARCH=x86

ln -sfn $TOOLCHAIN/bin/$TARGET_HOST$MIN_SDK_VERSION-clang $TOOLCHAIN/bin/$TARGET_HOST-clang

./Configure android-x86 no-shared \
 -D__ANDROID_API__=$MIN_SDK_VERSION \
 --prefix=$PWD/build/$ANDROID_ARCH

export arch_dir_name="armeabi-v7a"
mkdir -p "./../$package_name-$build_version"
mkdir -p "./../$package_name-$build_version/libs"
mkdir -p "./../$package_name-$build_version/libs/$arch_dir_name"
cp "./../build/openssl/$arch_dir_name/lib/libssl.a" "./../$package_name-$build_version/libs/$arch_dir_name"
cp "./../build/openssl/$arch_dir_name/lib/libcrypto.a" "./../$package_name-$build_version/libs/$arch_dir_name"

make -j5
make install_sw
make clean
mkdir -p ../build/openssl/$ANDROID_ARCH
cp -R $PWD/build/$ANDROID_ARCH ../build/openssl/

# x64
export TARGET_HOST=x86_64-linux-android
export ANDROID_ARCH=x86_64

ln -sfn $TOOLCHAIN/bin/$TARGET_HOST$MIN_SDK_VERSION-clang $TOOLCHAIN/bin/$TARGET_HOST-clang

./Configure android-x86_64 no-shared \
 -D__ANDROID_API__=$MIN_SDK_VERSION \
 --prefix=$PWD/build/$ANDROID_ARCH

export arch_dir_name="x86"
mkdir -p "./../$package_name-$build_version"
mkdir -p "./../$package_name-$build_version/libs"
mkdir -p "./../$package_name-$build_version/libs/$arch_dir_name"
cp "./../build/openssl/$arch_dir_name/lib/libssl.a" "./../$package_name-$build_version/libs/$arch_dir_name"
cp "./../build/openssl/$arch_dir_name/lib/libcrypto.a" "./../$package_name-$build_version/libs/$arch_dir_name"

make -j5
make install_sw
make clean
mkdir -p ../build/openssl/$ANDROID_ARCH
cp -R $PWD/build/$ANDROID_ARCH ../build/openssl/

export arch_dir_name="x86_64"
mkdir -p "./../$package_name-$build_version"
mkdir -p "./../$package_name-$build_version/libs"
mkdir -p "./../$package_name-$build_version/libs/$arch_dir_name"
cp "./../build/openssl/$arch_dir_name/lib/libssl.a" "./../$package_name-$build_version/libs/$arch_dir_name"
cp "./../build/openssl/$arch_dir_name/lib/libcrypto.a" "./../$package_name-$build_version/libs/$arch_dir_name"

cd ".."
zip -qry "$package_name-$build_version.zip" "$package_name-$build_version"
cd ".."
cp "./src/$package_name-$build_version.zip" `pwd`
rm -rf "./src"
