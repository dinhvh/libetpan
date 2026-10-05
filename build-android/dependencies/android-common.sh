#!/usr/bin/env bash

android_common_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
android_dependencies_dir="$android_common_dir"
android_build_android_dir="$(cd "$android_dependencies_dir/.." && pwd)"
android_repo_root="$(cd "$android_build_android_dir/.." && pwd)"
android_submodules_dir="$android_repo_root/dependencies/submodules"

android_default_abis="arm64-v8a armeabi-v7a x86 x86_64"

android_fail() {
  echo "ERROR: $*" >&2
  exit 1
}

android_require_command() {
  command -v "$1" >/dev/null 2>&1 || android_fail "Required command not found: $1"
}

android_require_ndk() {
  if [[ -z "${ANDROID_NDK:-}" ]]; then
    android_fail "ANDROID_NDK must be set before running this script."
  fi
  [[ -d "$ANDROID_NDK" ]] || android_fail "ANDROID_NDK does not exist: $ANDROID_NDK"
  [[ -f "$ANDROID_NDK/build/cmake/android.toolchain.cmake" ]] ||
    android_fail "ANDROID_NDK does not contain the CMake Android toolchain: $ANDROID_NDK"
}

android_host_tag() {
  local system machine base tag candidate

  system="$(uname -s)"
  machine="$(uname -m)"
  case "$system" in
    Darwin)
      base="$ANDROID_NDK/toolchains/llvm/prebuilt"
      if [[ "$machine" == "arm64" && -d "$base/darwin-arm64" ]]; then
        tag="darwin-arm64"
      else
        tag="darwin-x86_64"
      fi
      ;;
    Linux)
      tag="linux-x86_64"
      ;;
    *)
      android_fail "Unsupported host for Android NDK prebuilt toolchain: $system $machine"
      ;;
  esac

  candidate="$ANDROID_NDK/toolchains/llvm/prebuilt/$tag"
  [[ -d "$candidate" ]] || android_fail "NDK toolchain directory not found: $candidate"
  echo "$tag"
}

android_toolchain_bin() {
  local tag
  tag="$(android_host_tag)"
  echo "$ANDROID_NDK/toolchains/llvm/prebuilt/$tag/bin"
}

android_jobs() {
  if [[ -n "${JOBS:-}" ]]; then
    echo "$JOBS"
  elif command -v getconf >/dev/null 2>&1; then
    getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4
  else
    echo 4
  fi
}

android_api_level() {
  local platform="${ANDROID_PLATFORM:-android-23}"
  echo "${platform#android-}"
}

android_abis() {
  echo "${ANDROID_ABIS:-$android_default_abis}"
}

android_source_required() {
  local source_dir="$1"
  local marker="$2"
  local hint="$3"

  [[ -e "$source_dir/$marker" ]] || android_fail "$hint sources are missing at $source_dir. Run: git submodule update --init --recursive"
}

android_cmake_configure() {
  local source_dir="$1"
  local build_dir="$2"
  local prefix="$3"
  local abi="$4"
  shift 4

  cmake -S "$source_dir" -B "$build_dir" \
    -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI="$abi" \
    -DANDROID_PLATFORM="${ANDROID_PLATFORM:-android-23}" \
    -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}" \
    -DCMAKE_INSTALL_PREFIX="$prefix" \
    -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
    "$@"
}

android_find_static_library() {
  local prefix="$1"
  local library="$2"

  if [[ -f "$prefix/lib/$library" ]]; then
    echo "$prefix/lib/$library"
  elif [[ -f "$prefix/lib64/$library" ]]; then
    echo "$prefix/lib64/$library"
  else
    return 1
  fi
}

android_copy_headers_once() {
  local package_dir="$1"
  local include_dir="$2"

  if [[ ! -d "$package_dir/include" ]]; then
    [[ -d "$include_dir" ]] || android_fail "Missing include directory: $include_dir"
    mkdir -p "$package_dir"
    cp -R "$include_dir" "$package_dir/include"
  fi
}

android_copy_library() {
  local package_dir="$1"
  local abi="$2"
  local source_library="$3"
  local output_name="$4"

  [[ -f "$source_library" ]] || android_fail "Missing static library: $source_library"
  mkdir -p "$package_dir/libs/$abi"
  cp "$source_library" "$package_dir/libs/$abi/$output_name"
}

android_dependency_output_dir() {
  local package_name="$1"

  echo "$android_dependencies_dir/build/$package_name"
}

android_require_dependency_output() {
  local output_dir="$1"
  local hint="$2"

  [[ -d "$output_dir/include" ]] || android_fail "$hint headers are missing at $output_dir. Run its Android dependency build script first."
}

android_combine_static_libraries() {
  local output="$1"
  shift

  local tool_bin work_dir script
  tool_bin="$(android_toolchain_bin)"
  work_dir="$(mktemp -d "${TMPDIR:-/tmp}/libetpan-android-ar.XXXXXX")"
  script="$work_dir/combine.mri"

  {
    echo "CREATE $output"
    for library in "$@"; do
      [[ -f "$library" ]] || android_fail "Missing static library to combine: $library"
      echo "ADDLIB $library"
    done
    echo "SAVE"
    echo "END"
  } > "$script"

  rm -f "$output"
  "$tool_bin/llvm-ar" -M < "$script"
  "$tool_bin/llvm-ranlib" "$output"
  rm -rf "$work_dir"
}
