#!/bin/bash

set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
source_dir="$script_dir/submodules/tidy-html5"
build_root="${BUILD_DIR:-$script_dir/build/tidy-html5}"
output="${OUTPUT:-$script_dir/build/Tidy.xcframework}"
macos_deployment_target="${MACOSX_DEPLOYMENT_TARGET:-10.13}"
ios_deployment_target="${IPHONEOS_DEPLOYMENT_TARGET:-12.0}"

if [[ ! -f "$source_dir/CMakeLists.txt" ]]; then
  echo "tidy-html5 sources are missing. Run: git submodule update --init --recursive" >&2
  exit 1
fi

for command in cmake xcodebuild xcrun; do
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
  local variant_build="$build_root/$name/build"
  local prefix="$build_root/$name/install"
  local sdk_path

  echo "Building tidy-html5 for $name ($architectures)"
  sdk_path="$(xcrun --sdk "$sdk" --show-sdk-path)"
  cmake -S "$source_dir" -B "$variant_build" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DCMAKE_SYSTEM_NAME="$system_name" \
    -DCMAKE_C_COMPILER="$(xcrun --sdk "$sdk" --find clang)" \
    -DCMAKE_OSX_SYSROOT="$sdk_path" \
    -DCMAKE_OSX_ARCHITECTURES="$architectures" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="$deployment_target" \
    -DCMAKE_INSTALL_PREFIX="$prefix" \
    -DBUILD_SHARED_LIB=OFF \
    -DBUILD_TAB2SPACE=OFF \
    -DBUILD_SAMPLE_CODE=OFF \
    -DSUPPORT_CONSOLE_APP=OFF
  cmake --build "$variant_build" --target tidy-static --parallel
  cmake --install "$variant_build"
}

rm -rf "$build_root"
mkdir -p "$build_root"

build_variant macos macosx "arm64;x86_64" "$macos_deployment_target" Darwin
build_variant ios iphoneos arm64 "$ios_deployment_target" iOS
build_variant ios-simulator iphonesimulator "arm64;x86_64" \
  "$ios_deployment_target" iOS

rm -rf "$output"
xcodebuild -create-xcframework \
  -library "$build_root/macos/install/lib/libtidy.a" \
  -headers "$build_root/macos/install/include" \
  -library "$build_root/ios/install/lib/libtidy.a" \
  -headers "$build_root/ios/install/include" \
  -library "$build_root/ios-simulator/install/lib/libtidy.a" \
  -headers "$build_root/ios-simulator/install/include" \
  -output "$output"

echo "Created $output"
