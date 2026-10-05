#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=../android-common.sh
source "$script_dir/../android-common.sh"

package_name=json-c-android
source_dir="$android_submodules_dir/json-c"
build_root="$android_dependencies_dir/build/json-c"
output_dir="$(android_dependency_output_dir "$package_name")"

android_require_ndk
android_require_command cmake
android_source_required "$source_dir" CMakeLists.txt "JSON-C"

rm -rf "$build_root" "$output_dir"
mkdir -p "$build_root"

for abi in $(android_abis); do
  echo "Building JSON-C for $abi"
  variant_root="$build_root/$abi"
  build_dir="$variant_root/build"
  prefix="$variant_root/install"

  android_cmake_configure "$source_dir" "$build_dir" "$prefix" "$abi" \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_STATIC_LIBS=ON \
    -DBUILD_APPS=OFF \
    -DBUILD_TESTING=OFF \
    -DDISABLE_WERROR=ON
  cmake --build "$build_dir" --target json-c --parallel "$(android_jobs)"
  cmake --install "$build_dir"

  android_copy_headers_once "$output_dir" "$prefix/include"
  json_c_library="$(android_find_static_library "$prefix" libjson-c.a)" ||
    android_fail "JSON-C did not install libjson-c.a for $abi"
  android_copy_library "$output_dir" "$abi" "$json_c_library" libjson-c.a
done

rm -rf "$build_root"
echo "Created $output_dir"
