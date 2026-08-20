#!/bin/bash

set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
source_dir="$script_dir/submodules/openssl"
build_root="${BUILD_DIR:-$script_dir/build/openssl}"
crypto_output="${CRYPTO_OUTPUT:-$script_dir/build/OpenSSL-Crypto.xcframework}"
ssl_output="${SSL_OUTPUT:-$script_dir/build/OpenSSL-SSL.xcframework}"
macos_deployment_target="${MACOSX_DEPLOYMENT_TARGET:-10.13}"
ios_deployment_target="${IPHONEOS_DEPLOYMENT_TARGET:-12.0}"
jobs="${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)}"

if [[ ! -f "$source_dir/Configure" ]]; then
  echo "OpenSSL sources are missing. Run: git submodule update --init --recursive" >&2
  exit 1
fi

for command in make perl xcodebuild xcrun lipo rsync; do
  if ! command -v "$command" >/dev/null 2>&1; then
    echo "Required command not found: $command" >&2
    exit 1
  fi
done

work_dir="$(mktemp -d "${TMPDIR:-/tmp}/libetpan-openssl.XXXXXX")"
trap 'rm -rf "$work_dir"' EXIT

prepared_source="$work_dir/source"
mkdir -p "$prepared_source"
rsync -a --exclude .git/ "$source_dir/" "$prepared_source/"

build_arch() {
  local name="$1"
  local sdk="$2"
  local arch="$3"
  local target="$4"
  local minimum_flag="$5"
  local arch_source="$work_dir/source-$name-$arch"
  local prefix="$build_root/$name-$arch/install"
  local sdk_path

  sdk_path="$(xcrun --sdk "$sdk" --show-sdk-path)"
  rm -rf "$arch_source" "$prefix"
  cp -R "$prepared_source" "$arch_source"

  echo "Building OpenSSL for $name ($arch)"
  (
    cd "$arch_source"
    CC="$(xcrun --sdk "$sdk" --find clang)" \
    AR="$(xcrun --sdk "$sdk" --find ar)" \
    RANLIB="$(xcrun --sdk "$sdk" --find ranlib)" \
      perl ./Configure "$target" \
        --prefix="$prefix" \
        --libdir=lib \
        no-apps \
        no-docs \
        no-shared \
        no-tests \
        "-isysroot" "$sdk_path" \
        "$minimum_flag"
    make -j "$jobs" build_libs
    make install_dev
  )
}

make_universal() {
  local name="$1"
  local destination="$build_root/$name-universal"

  rm -rf "$destination"
  mkdir -p "$destination/lib"
  cp -R "$build_root/$name-arm64/install/include" "$destination/include"
  for library in libcrypto.a libssl.a; do
    lipo -create \
      "$build_root/$name-arm64/install/lib/$library" \
      "$build_root/$name-x86_64/install/lib/$library" \
      -output "$destination/lib/$library"
  done
}

rm -rf "$build_root"
mkdir -p "$build_root"

build_arch macos macosx arm64 darwin64-arm64 \
  "-mmacosx-version-min=$macos_deployment_target"
build_arch macos macosx x86_64 darwin64-x86_64 \
  "-mmacosx-version-min=$macos_deployment_target"
make_universal macos

build_arch ios iphoneos arm64 ios64-xcrun \
  "-miphoneos-version-min=$ios_deployment_target"

build_arch ios-simulator iphonesimulator arm64 iossimulator-arm64-xcrun \
  "-mios-simulator-version-min=$ios_deployment_target"
build_arch ios-simulator iphonesimulator x86_64 iossimulator-x86_64-xcrun \
  "-mios-simulator-version-min=$ios_deployment_target"
make_universal ios-simulator

rm -rf "$crypto_output" "$ssl_output"
xcodebuild -create-xcframework \
  -library "$build_root/macos-universal/lib/libcrypto.a" \
  -headers "$build_root/macos-universal/include" \
  -library "$build_root/ios-arm64/install/lib/libcrypto.a" \
  -headers "$build_root/ios-arm64/install/include" \
  -library "$build_root/ios-simulator-universal/lib/libcrypto.a" \
  -headers "$build_root/ios-simulator-universal/include" \
  -output "$crypto_output"

xcodebuild -create-xcframework \
  -library "$build_root/macos-universal/lib/libssl.a" \
  -headers "$build_root/macos-universal/include" \
  -library "$build_root/ios-arm64/install/lib/libssl.a" \
  -headers "$build_root/ios-arm64/install/include" \
  -library "$build_root/ios-simulator-universal/lib/libssl.a" \
  -headers "$build_root/ios-simulator-universal/include" \
  -output "$ssl_output"

echo "Created $crypto_output"
echo "Created $ssl_output"
