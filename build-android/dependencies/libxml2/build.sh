#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=../android-common.sh
source "$script_dir/../android-common.sh"

build_version=1
package_name=libxml2-android
source_dir="$android_repo_root/build-mac/dependencies/submodules/libxml2"
build_root="$android_dependencies_dir/build/libxml2"
output_dir="$(android_dependency_output_dir "$package_name" "$build_version")"

android_require_ndk
android_require_command cmake
android_source_required "$source_dir" CMakeLists.txt "libxml2"

rm -rf "$build_root" "$output_dir"
mkdir -p "$build_root"

for abi in $(android_abis); do
  echo "Building libxml2 for $abi"
  variant_root="$build_root/$abi"
  build_dir="$variant_root/build"
  prefix="$variant_root/install"

  android_cmake_configure "$source_dir" "$build_dir" "$prefix" "$abi" \
    -DBUILD_SHARED_LIBS=OFF \
    -DLIBXML2_WITH_PROGRAMS=OFF \
    -DLIBXML2_WITH_TESTS=OFF \
    -DLIBXML2_WITH_PYTHON=OFF \
    -DLIBXML2_WITH_LZMA=OFF \
    -DLIBXML2_WITH_ICONV=OFF \
    -DLIBXML2_WITH_ZLIB=ON
  cmake --build "$build_dir" --parallel "$(android_jobs)"
  cmake --install "$build_dir"

  android_copy_headers_once "$output_dir" "$prefix/include"
  xml2_library="$(android_find_static_library "$prefix" libxml2.a)" ||
    android_fail "libxml2 did not install libxml2.a for $abi"
  android_copy_library "$output_dir" "$abi" "$xml2_library" libxml2.a
done

rm -rf "$build_root"
echo "Created $output_dir"
