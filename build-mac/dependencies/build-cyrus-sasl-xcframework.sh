#!/bin/bash

set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
source_dir="$script_dir/submodules/cyrus-sasl"
build_root="${BUILD_DIR:-$script_dir/build/cyrus-sasl}"
output="${OUTPUT:-$script_dir/build/CyrusSASL.xcframework}"
macos_deployment_target="${MACOSX_DEPLOYMENT_TARGET:-10.13}"
ios_deployment_target="${IPHONEOS_DEPLOYMENT_TARGET:-12.0}"
jobs="${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)}"

if [[ ! -f "$source_dir/configure.ac" ]]; then
  echo "Cyrus SASL sources are missing. Run: git submodule update --init --recursive" >&2
  exit 1
fi

for command in autoreconf make xcodebuild xcrun lipo rsync; do
  if ! command -v "$command" >/dev/null 2>&1; then
    echo "Required command not found: $command" >&2
    exit 1
  fi
done

work_dir="$(mktemp -d "${TMPDIR:-/tmp}/libetpan-cyrus-sasl.XXXXXX")"
trap 'rm -rf "$work_dir"' EXIT

prepared_source="$work_dir/source"
mkdir -p "$prepared_source"
rsync -a --exclude .git/ "$source_dir/" "$prepared_source/"
(
  cd "$prepared_source"
  autoreconf -fi
)

build_arch() {
  local name="$1"
  local sdk="$2"
  local arch="$3"
  local host="$4"
  local minimum_flag="$5"
  local arch_source="$work_dir/source-$name-$arch"
  local prefix="$build_root/$name-$arch"
  local sdk_path

  sdk_path="$(xcrun --sdk "$sdk" --show-sdk-path)"
  rm -rf "$arch_source" "$prefix"
  cp -R "$prepared_source" "$arch_source"

  echo "Building Cyrus SASL for $name ($arch)"
  (
    cd "$arch_source"
    CC="$(xcrun --sdk "$sdk" --find clang)" \
    AR="$(xcrun --sdk "$sdk" --find ar)" \
    RANLIB="$(xcrun --sdk "$sdk" --find ranlib)" \
    CC_FOR_BUILD="$(xcrun --sdk macosx --find clang)" \
    CFLAGS_FOR_BUILD="-O2" \
    CPPFLAGS="-arch $arch -isysroot $sdk_path $minimum_flag" \
    CFLAGS="-std=gnu99 -arch $arch -isysroot $sdk_path $minimum_flag -O2" \
    LDFLAGS="-arch $arch -isysroot $sdk_path $minimum_flag" \
      ./configure \
        --host="$host" \
        --prefix="$prefix" \
        --enable-static \
        --disable-shared \
        --disable-staticdlopen \
        --disable-macos-framework \
        --disable-sample \
        --disable-digest \
        --disable-scram \
        --disable-otp \
        --disable-gssapi \
        --enable-login \
        --with-dblib=none \
        --without-openssl \
        --without-pam \
        --without-saslauthd \
        --without-authdaemond \
        --without-pwcheck
    "$(xcrun --sdk macosx --find clang)" \
      -isysroot "$(xcrun --sdk macosx --show-sdk-path)" \
      -O2 include/makemd5.c \
      -o include/makemd5
    make -j "$jobs"
    make install
  )
}

make_universal() {
  local name="$1"
  local destination="$build_root/$name-universal"

  rm -rf "$destination"
  mkdir -p "$destination/lib"
  cp -R "$build_root/$name-arm64/include" "$destination/include"
  lipo -create \
    "$build_root/$name-arm64/lib/libsasl2.a" \
    "$build_root/$name-x86_64/lib/libsasl2.a" \
    -output "$destination/lib/libsasl2.a"
}

rm -rf "$build_root"
mkdir -p "$build_root"

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

rm -rf "$output"
xcodebuild -create-xcframework \
  -library "$build_root/macos-universal/lib/libsasl2.a" \
  -headers "$build_root/macos-universal/include" \
  -library "$build_root/ios-arm64/lib/libsasl2.a" \
  -headers "$build_root/ios-arm64/include" \
  -library "$build_root/ios-simulator-universal/lib/libsasl2.a" \
  -headers "$build_root/ios-simulator-universal/include" \
  -output "$output"

echo "Created $output"
