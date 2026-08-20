#!/bin/bash

set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
project="$script_dir/libetpan.xcodeproj"
build_root="${LIBETPAN_XCFRAMEWORK_BUILD_DIR:-$script_dir/build/xcframework}"
output="${LIBETPAN_XCFRAMEWORK_OUTPUT:-$script_dir/build/LibEtPan.xcframework}"
configuration="${CONFIGURATION:-Release}"
macos_deployment_target="${LIBETPAN_MACOSX_DEPLOYMENT_TARGET:-12.0}"

for command in xcodebuild xcrun; do
  if ! command -v "$command" >/dev/null 2>&1; then
    echo "Required command not found: $command" >&2
    exit 1
  fi
done

for dependency in JsonC CyrusSASL; do
  if [[ ! -d "$script_dir/dependencies/build/$dependency.xcframework" ]]; then
    echo "$dependency.xcframework is missing." >&2
    echo "Run: $script_dir/dependencies/bootstrap.sh" >&2
    exit 1
  fi
done

build_variant() {
  local name="$1"
  local target="$2"
  local sdk="$3"
  local architectures="$4"
  local products_dir="$build_root/$name/products"

  echo "Building libetpan for $name ($architectures)"
  xcodebuild \
    -project "$project" \
    -target "$target" \
    -configuration "$configuration" \
    -sdk "$sdk" \
    ARCHS="$architectures" \
    ONLY_ACTIVE_ARCH=NO \
    CODE_SIGNING_ALLOWED=NO \
    MACOSX_DEPLOYMENT_TARGET="$macos_deployment_target" \
    CONFIGURATION_BUILD_DIR="$products_dir" \
    OBJROOT="$build_root/$name/objects" \
    build
}

rm -rf "$build_root" "$output"
mkdir -p "$build_root" "$(dirname "$output")"

build_variant macos "static libetpan" macosx "arm64 x86_64"
build_variant ios "libetpan ios" iphoneos arm64
build_variant ios-simulator "libetpan ios" iphonesimulator "arm64 x86_64"

xcodebuild -create-xcframework \
  -library "$build_root/macos/products/libetpan.a" \
  -headers "$build_root/macos/products/include" \
  -library "$build_root/ios/products/libetpan-ios.a" \
  -headers "$build_root/ios/products/include" \
  -library "$build_root/ios-simulator/products/libetpan-ios.a" \
  -headers "$build_root/ios-simulator/products/include" \
  -output "$output"

echo "Created $output"
