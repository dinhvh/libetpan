#!/bin/bash

set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
source_dir="$script_dir/submodules/icu/icu4c/source"
build_root="${BUILD_DIR:-$script_dir/build/icu}"
output_dir="${OUTPUT_DIR:-$script_dir/build}"
macos_deployment_target="${MACOSX_DEPLOYMENT_TARGET:-10.13}"
ios_deployment_target="${IPHONEOS_DEPLOYMENT_TARGET:-12.0}"
jobs="${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)}"

if [[ ! -x "$source_dir/configure" ]]; then
  echo "ICU sources are missing. Run: git submodule update --init --recursive" >&2
  exit 1
fi

for command in make xcodebuild xcrun lipo; do
  if ! command -v "$command" >/dev/null 2>&1; then
    echo "Required command not found: $command" >&2
    exit 1
  fi
done

configure_options=(
  --disable-shared
  --enable-static
  --disable-tests
  --disable-samples
  --with-data-packaging=static
)

build_native_tools() {
  local build_dir="$build_root/native"

  mkdir -p "$build_dir"
  echo "Building native ICU tools"
  (
    cd "$build_dir"
    "$source_dir/runConfigureICU" MacOSX \
      "${configure_options[@]}" \
      --prefix="$build_dir/install"
    make -j "$jobs"
  )
}

build_arch() {
  local name="$1"
  local sdk="$2"
  local arch="$3"
  local host="$4"
  local minimum_flag="$5"
  local build_dir="$build_root/$name-$arch/build"
  local prefix="$build_root/$name-$arch/install"
  local sdk_path

  sdk_path="$(xcrun --sdk "$sdk" --show-sdk-path)"
  mkdir -p "$build_dir"

  echo "Building ICU for $name ($arch)"
  (
    cd "$build_dir"
    CC="$(xcrun --sdk "$sdk" --find clang)" \
    CXX="$(xcrun --sdk "$sdk" --find clang++)" \
    AR="$(xcrun --sdk "$sdk" --find ar)" \
    RANLIB="$(xcrun --sdk "$sdk" --find ranlib)" \
    CPPFLAGS="-arch $arch -isysroot $sdk_path $minimum_flag" \
    CFLAGS="-arch $arch -isysroot $sdk_path $minimum_flag -O2" \
    CXXFLAGS="-arch $arch -isysroot $sdk_path $minimum_flag -O2" \
    LDFLAGS="-arch $arch -isysroot $sdk_path $minimum_flag" \
      "$source_dir/configure" \
        --host="$host" \
        --prefix="$prefix" \
        --with-cross-build="$build_root/native" \
        "${configure_options[@]}"
    make -j "$jobs"
    make install
  )
}

make_universal() {
  local name="$1"
  local destination="$build_root/$name-universal"
  local library

  rm -rf "$destination"
  mkdir -p "$destination/lib"
  cp -R "$build_root/$name-arm64/install/include" "$destination/include"
  for library in icuuc icui18n icudata; do
    lipo -create \
      "$build_root/$name-arm64/install/lib/lib$library.a" \
      "$build_root/$name-x86_64/install/lib/lib$library.a" \
      -output "$destination/lib/lib$library.a"
  done
}

create_xcframework() {
  local library="$1"
  local product_name="$2"
  local output="$output_dir/$product_name.xcframework"

  rm -rf "$output"
  xcodebuild -create-xcframework \
    -library "$build_root/macos-universal/lib/lib$library.a" \
    -headers "$build_root/macos-universal/include" \
    -library "$build_root/ios-arm64/install/lib/lib$library.a" \
    -headers "$build_root/ios-arm64/install/include" \
    -library "$build_root/ios-simulator-universal/lib/lib$library.a" \
    -headers "$build_root/ios-simulator-universal/include" \
    -output "$output"
  echo "Created $output"
}

rm -rf "$build_root"
mkdir -p "$build_root" "$output_dir"

build_native_tools

build_arch macos macosx arm64 arm-apple-darwin \
  "-mmacosx-version-min=$macos_deployment_target"
build_arch macos macosx x86_64 x86_64-apple-darwin \
  "-mmacosx-version-min=$macos_deployment_target"
make_universal macos

build_arch ios iphoneos arm64 arm-apple-darwin \
  "-miphoneos-version-min=$ios_deployment_target"

build_arch ios-simulator iphonesimulator arm64 arm-apple-darwin \
  "-mios-simulator-version-min=$ios_deployment_target"
build_arch ios-simulator iphonesimulator x86_64 x86_64-apple-darwin \
  "-mios-simulator-version-min=$ios_deployment_target"
make_universal ios-simulator

create_xcframework icuuc ICUUC
create_xcframework icui18n ICUI18N
create_xcframework icudata ICUData
