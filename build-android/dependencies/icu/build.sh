#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=../android-common.sh
source "$script_dir/../android-common.sh"

source_dir="$android_submodules_dir/icu/icu4c/source"
build_root="$android_dependencies_dir/build/icu"
output_dir="$(android_dependency_output_dir icu-android)"
host_build="$build_root/host"

android_require_ndk
android_require_command make
android_require_command python3
android_require_command "${HOST_CC:-cc}"
android_require_command "${HOST_CXX:-c++}"
android_source_required "$source_dir" configure "ICU"
tool_bin="$(android_toolchain_bin)"
api="$(android_api_level)"
abis="$(android_abis)"
jobs="$(android_jobs)"
[[ "$api" =~ ^[0-9]+$ && "$api" -ge 21 ]] ||
  android_fail "ICU requires an Android API level of at least 21."
[[ "$jobs" =~ ^[0-9]+$ && "$jobs" -gt 0 ]] || android_fail "JOBS must be a positive integer."
[[ -n "$abis" ]] || android_fail "ANDROID_ABIS must contain at least one ABI."
for abi in $abis; do
  case "$abi" in
    arm64-v8a) compiler_target=aarch64-linux-android ;;
    armeabi-v7a) compiler_target=armv7a-linux-androideabi ;;
    x86) compiler_target=i686-linux-android ;;
    x86_64) compiler_target=x86_64-linux-android ;;
    *) android_fail "Unsupported Android ABI: $abi" ;;
  esac
  [[ -x "$tool_bin/$compiler_target$api-clang" &&
     -x "$tool_bin/$compiler_target$api-clang++" ]] ||
    android_fail "NDK compilers are unavailable for $abi at API $api."
done

rm -rf "$build_root" "$output_dir"
mkdir -p "$host_build"

# ICU's cross build runs host-native tools to compile and package target data.
# Build those tools from the same source revision as the Android libraries.
echo "Building ICU host tools"
(
  cd "$host_build"
  CC="${HOST_CC:-cc}" CXX="${HOST_CXX:-c++}" \
    CFLAGS="-O2" CXXFLAGS="-O2" CPPFLAGS="" LDFLAGS="" \
    "$source_dir/configure" --disable-shared --enable-static \
      --disable-tests --disable-samples --disable-extras --disable-icuio
  make -j "$jobs" LOCAL_SUBDIRS="stubdata common i18n tools"
)

for abi in $abis; do
  case "$abi" in
    arm64-v8a) target=aarch64-linux-android; compiler_target="$target"; abi_flags="" ;;
    armeabi-v7a) target=arm-linux-androideabi; compiler_target=armv7a-linux-androideabi; abi_flags="-march=armv7-a -mfloat-abi=softfp -mfpu=neon" ;;
    x86) target=i686-linux-android; compiler_target="$target"; abi_flags="" ;;
    x86_64) target=x86_64-linux-android; compiler_target="$target"; abi_flags="" ;;
  esac
  build_dir="$build_root/$abi/build"
  prefix="$build_root/$abi/install"
  mkdir -p "$build_dir"
  echo "Building ICU for $abi (API $api)"
  (
    cd "$build_dir"
    ICU_DATA_FILTER_FILE="$script_dir/data-filter.json" \
      CC="$tool_bin/$compiler_target$api-clang" \
      CXX="$tool_bin/$compiler_target$api-clang++" \
      AR="$tool_bin/llvm-ar" RANLIB="$tool_bin/llvm-ranlib" \
      CFLAGS="-O2 -fPIC $abi_flags" CXXFLAGS="-O2 -fPIC $abi_flags" \
      CPPFLAGS="" LDFLAGS="" \
      "$source_dir/configure" --host="$target" --prefix="$prefix" \
        --with-cross-build="$host_build" --with-data-packaging=static \
        --disable-shared --enable-static --disable-tests --disable-samples \
        --disable-extras --disable-icuio --disable-tools
    # libetpan needs the common/conversion library, not ICU's i18n library.
    make -j "$jobs" LOCAL_SUBDIRS="stubdata common data"
    make -C common install
    make -C data install
  )
  android_copy_headers_once "$output_dir" "$prefix/include"
  for library in libicuuc.a libicudata.a; do
    # The common static library has a 'libs' prefix on some host configurations.
    # Expose conventional names in the Android dependency package.
    archive="$(android_find_static_library "$prefix" "$library" ||
      android_find_static_library "$prefix" "libs${library#lib}")" ||
      android_fail "ICU did not install $library for $abi"
    android_copy_library "$output_dir" "$abi" "$archive" "$library"
  done
done

cp "$android_submodules_dir/icu/LICENSE" "$output_dir/LICENSE"
rm -rf "$build_root"
echo "Created $output_dir"
