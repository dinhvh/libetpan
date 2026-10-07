#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
repo_root="$(cd "$script_dir/.." && pwd)"
build_android="$repo_root/build-android"
output_root="${TMPDIR:-/tmp}/libetpan-android-link-smoke"
android_platform="${ANDROID_PLATFORM:-android-23}"
api_level="${android_platform#android-}"
abis="${ANDROID_ABIS:-arm64-v8a armeabi-v7a x86 x86_64}"

fail() {
  echo "ERROR: $*" >&2
  exit 1
}

host_tag() {
  case "$(uname -s)" in
    Darwin)
      if [[ "$(uname -m)" == arm64 &&
          -d "$ANDROID_NDK/toolchains/llvm/prebuilt/darwin-arm64" ]]; then
        echo darwin-arm64
      else
        echo darwin-x86_64
      fi
      ;;
    Linux)
      echo linux-x86_64
      ;;
    *)
      fail "Unsupported Android NDK host: $(uname -s) $(uname -m)"
      ;;
  esac
}

target_for_abi() {
  case "$1" in
    arm64-v8a) echo "aarch64-linux-android$api_level" ;;
    armeabi-v7a) echo "armv7a-linux-androideabi$api_level" ;;
    x86) echo "i686-linux-android$api_level" ;;
    x86_64) echo "x86_64-linux-android$api_level" ;;
    *) fail "Unsupported ABI: $1" ;;
  esac
}

require_dir() {
  [[ -d "$1" ]] || fail "Missing directory: $1"
}

require_file() {
  [[ -f "$1" ]] || fail "Missing file: $1"
}

[[ -n "${ANDROID_NDK:-}" ]] || fail "ANDROID_NDK must be set."
require_dir "$ANDROID_NDK"

toolchain="$ANDROID_NDK/toolchains/llvm/prebuilt/$(host_tag)"
clangxx="$toolchain/bin/clang++"
require_file "$clangxx"

etpan_dir="$build_android/build/libetpan-android"
openssl_dir="$build_android/dependencies/build/openssl-android"
sasl_dir="$build_android/dependencies/build/cyrus-sasl-android"
icu_dir="$build_android/dependencies/build/icu-android"
json_c_dir="$build_android/dependencies/build/json-c-android"
curl_dir="$build_android/dependencies/build/curl-android"
libxml2_dir="$build_android/dependencies/build/libxml2-android"
rnp_dir="$build_android/dependencies/build/rnp-android"

for artifact_dir in "$etpan_dir" "$openssl_dir" "$sasl_dir" "$icu_dir" \
    "$json_c_dir" "$curl_dir" "$libxml2_dir" "$rnp_dir"; do
  require_dir "$artifact_dir"
done

rm -rf "$output_root"
mkdir -p "$output_root"

for abi in $abis; do
  target="$(target_for_abi "$abi")"
  output="$output_root/$abi/libetpan-android-link-smoke.so"
  require_file "$etpan_dir/libs/$abi/libetpan.a"
  require_file "$rnp_dir/libs/$abi/librnp.a"
  require_file "$curl_dir/libs/$abi/libcurl.a"
  require_file "$libxml2_dir/libs/$abi/libxml2.a"
  require_file "$json_c_dir/libs/$abi/libjson-c.a"
  require_file "$sasl_dir/libs/$abi/libsasl2.a"
  require_file "$openssl_dir/libs/$abi/libssl.a"
  require_file "$openssl_dir/libs/$abi/libcrypto.a"
  require_file "$icu_dir/libs/$abi/libicuuc.a"
  require_file "$icu_dir/libs/$abi/libicudata.a"
  mkdir -p "$(dirname "$output")"
  echo "Linking Android smoke test for $abi"
  "$clangxx" \
    --target="$target" \
    -fPIC \
    -shared \
    -Wl,--no-undefined \
    -Wl,--fatal-warnings \
    -static-libstdc++ \
    -I"$build_android/include" \
    -I"$repo_root/src/low-level/feed" \
    -I"$sasl_dir/include" \
    -I"$icu_dir/include" \
    -I"$json_c_dir/include" \
    -I"$curl_dir/include" \
    -I"$libxml2_dir/include/libxml2" \
    -I"$rnp_dir/include" \
    -x c \
    "$script_dir/android-link-smoke-test.c" \
    -x none \
    "$etpan_dir/libs/$abi/libetpan.a" \
    "$rnp_dir/libs/$abi/librnp.a" \
    "$curl_dir/libs/$abi/libcurl.a" \
    "$libxml2_dir/libs/$abi/libxml2.a" \
    "$json_c_dir/libs/$abi/libjson-c.a" \
    "$sasl_dir/libs/$abi/libsasl2.a" \
    "$openssl_dir/libs/$abi/libssl.a" \
    "$openssl_dir/libs/$abi/libcrypto.a" \
    "$icu_dir/libs/$abi/libicuuc.a" \
    "$icu_dir/libs/$abi/libicudata.a" \
    -lz \
    -llog \
    -latomic \
    -lm \
    -o "$output"
done

echo "Android link smoke outputs are in $output_root"
