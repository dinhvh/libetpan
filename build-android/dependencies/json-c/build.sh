#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=../android-common.sh
source "$script_dir/../android-common.sh"

build_version=1
package_name=json-c-android
source_dir="$android_repo_root/build-mac/dependencies/submodules/json-c"
build_root="$script_dir/build"
package_dir="$script_dir/$package_name-$build_version"
zip_name="$package_name-$build_version.zip"

android_require_ndk
android_require_command cmake
android_source_required "$source_dir" CMakeLists.txt "JSON-C"

rm -rf "$build_root" "$package_dir"
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

  android_copy_headers_once "$package_dir" "$prefix/include"
  json_c_library="$(android_find_static_library "$prefix" libjson-c.a)" ||
    android_fail "JSON-C did not install libjson-c.a for $abi"
  android_copy_library "$package_dir" "$abi" "$json_c_library" libjson-c.a
done

android_zip_package "$script_dir" "$package_dir" "$zip_name"
rm -rf "$build_root" "$package_dir"
echo "Created $script_dir/$zip_name"
