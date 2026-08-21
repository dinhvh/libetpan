#!/bin/bash

set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
source_dir="$script_dir/submodules/rnp"
build_root="${BUILD_DIR:-$script_dir/build/rnp}"
output="${OUTPUT:-$script_dir/build/RNP.xcframework}"
openssl_root="${OPENSSL_BUILD_DIR:-$script_dir/build/openssl}"
json_c_root="${JSON_C_BUILD_DIR:-$script_dir/build/json-c}"
macos_deployment_target="${MACOSX_DEPLOYMENT_TARGET:-10.13}"
ios_deployment_target="${IPHONEOS_DEPLOYMENT_TARGET:-12.0}"

if [[ ! -f "$source_dir/CMakeLists.txt" || \
      ! -f "$source_dir/src/libsexpp/CMakeLists.txt" ]]; then
  echo "RNP sources are missing. Run: git submodule update --init --recursive" >&2
  exit 1
fi

for command in cmake libtool xcodebuild xcrun; do
  if ! command -v "$command" >/dev/null 2>&1; then
    echo "Required command not found: $command" >&2
    exit 1
  fi
done

build_variant() {
  local name="$1"
  local sdk="$2"
  local architectures="$3"
  local deployment_target="$4"
  local system_name="$5"
  local openssl_prefix="$6"
  local variant_root="$build_root/$name"
  local json_c_prefix="$json_c_root/$name/install"
  local rnp_build="$variant_root/rnp-build"
  local combined="$variant_root/librnp.a"
  local openssl_configure_prefix="$openssl_root/macos-universal"
  local sdk_path

  if [[ ! -f "$openssl_prefix/lib/libcrypto.a" ]]; then
    echo "OpenSSL build is missing for $name: $openssl_prefix/lib/libcrypto.a" >&2
    echo "Run build-openssl-xcframework.sh first." >&2
    exit 1
  fi

  if [[ ! -f "$json_c_prefix/lib/libjson-c.a" ]]; then
    echo "JSON-C build is missing for $name: $json_c_prefix/lib/libjson-c.a" >&2
    echo "Run build-json-c-xcframework.sh first." >&2
    exit 1
  fi

  sdk_path="$(xcrun --sdk "$sdk" --show-sdk-path)"
  echo "Building RNP for $name ($architectures)"

  cmake -S "$source_dir" -B "$rnp_build" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_SYSTEM_NAME="$system_name" \
    -DCMAKE_C_COMPILER="$(xcrun --sdk "$sdk" --find clang)" \
    -DCMAKE_CXX_COMPILER="$(xcrun --sdk "$sdk" --find clang++)" \
    -DCMAKE_OSX_SYSROOT="$sdk_path" \
    -DCMAKE_OSX_ARCHITECTURES="$architectures" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="$deployment_target" \
    -DCMAKE_MACOSX_BUNDLE=OFF \
    -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_TESTING=OFF \
    -DDOWNLOAD_GTEST=OFF \
    -DENABLE_DOC=OFF \
    -DENABLE_BZIP2=OFF \
    -DENABLE_CRYPTO_REFRESH=OFF \
    -DENABLE_PQC=OFF \
    -DCRYPTO_BACKEND=openssl \
    -DOPENSSL_ROOT_DIR="$openssl_configure_prefix" \
    -DOPENSSL_INCLUDE_DIR="$openssl_configure_prefix/include" \
    -DOPENSSL_CRYPTO_LIBRARY="$openssl_configure_prefix/lib/libcrypto.a" \
    -DOPENSSL_USE_STATIC_LIBS=TRUE \
    -DJSON-C_INCLUDE_DIR="$json_c_prefix/include/json-c" \
    -DJSON-C_LIBRARY="$json_c_prefix/lib/libjson-c.a"
  cmake --build "$rnp_build" --target librnp sexpp --parallel

  # RNP's static target leaves sexpp for consumers to link. JSON-C remains an
  # explicit dependency and is not copied into RNP.xcframework.
  libtool -static -o "$combined" \
    "$rnp_build/src/lib/librnp.a" \
    "$rnp_build/src/libsexpp/libsexpp.a"

  rm -rf "$variant_root/include"
  mkdir -p "$variant_root/include/rnp"
  cp "$source_dir/include/rnp/rnp.h" "$variant_root/include/rnp/"
  cp "$source_dir/include/rnp/rnp_err.h" "$variant_root/include/rnp/"
  cp "$rnp_build/src/lib/rnp/rnp_export.h" "$variant_root/include/rnp/"
  cp "$rnp_build/src/lib/version.h" "$variant_root/include/rnp/rnp_ver.h"
}

rm -rf "$build_root"
mkdir -p "$build_root"

build_variant macos macosx "arm64;x86_64" "$macos_deployment_target" Darwin \
  "$openssl_root/macos-universal"
build_variant ios iphoneos arm64 "$ios_deployment_target" iOS \
  "$openssl_root/ios-arm64/install"
build_variant ios-simulator iphonesimulator "arm64;x86_64" \
  "$ios_deployment_target" iOS \
  "$openssl_root/ios-simulator-universal"

rm -rf "$output"
xcodebuild -create-xcframework \
  -library "$build_root/macos/librnp.a" \
  -headers "$build_root/macos/include" \
  -library "$build_root/ios/librnp.a" \
  -headers "$build_root/ios/include" \
  -library "$build_root/ios-simulator/librnp.a" \
  -headers "$build_root/ios-simulator/include" \
  -output "$output"

echo "Created $output"
