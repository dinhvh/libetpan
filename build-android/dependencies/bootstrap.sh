#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
repository_root="$(cd "$script_dir/../.." && pwd)"
submodules_dir="dependencies/submodules"
logfile="$(mktemp "${TMPDIR:-/tmp}/libetpan-android-dependencies-bootstrap.XXXXXX")"
force=false

trap 'rm -f "$logfile"' EXIT

fail() {
  echo "ERROR: $*" >&2
  exit 1
}

run_step() {
  local description="$1"
  shift

  echo "$description"
  if "$@" >"$logfile" 2>&1; then
    return
  fi

  cat "$logfile" >&2
  return 1
}

build_step() {
  local description="$1"
  local output_dir="$2"
  shift 2

  if ! "$force" && [[ -d "$output_dir" ]]; then
    echo "Using cached $description at $output_dir"
    return
  fi

  run_step "Building $description" "$@"
}

require_source() {
  local marker="$1"
  local name="$2"

  [[ -e "$repository_root/$marker" ]] ||
    fail "$name sources are missing at $marker. Run this script again after submodules are available."
}

for argument in "$@"; do
  case "$argument" in
    --force)
      force=true
      ;;
    -h|--help)
      echo "Usage: $0 [--force]"
      exit 0
      ;;
    *)
      fail "Unknown argument: $argument"
      ;;
  esac
done

[[ -n "${ANDROID_NDK:-}" ]] || fail "ANDROID_NDK must be set before running this script."
command -v git >/dev/null 2>&1 || fail "Required command not found: git"

run_step "Initializing dependency submodules" \
  git -C "$repository_root" submodule update --init --recursive -- "$submodules_dir"

require_source "$submodules_dir/openssl/Configure" "OpenSSL"
require_source "$submodules_dir/cyrus-sasl/configure.ac" "Cyrus SASL"
require_source "$submodules_dir/json-c/CMakeLists.txt" "JSON-C"
require_source "$submodules_dir/curl/CMakeLists.txt" "curl"
require_source "$submodules_dir/libxml2/CMakeLists.txt" "libxml2"
require_source "$submodules_dir/libiconv/configure.ac" "libiconv"
require_source "$submodules_dir/rnp/CMakeLists.txt" "RNP"

build_step "Android OpenSSL" "$script_dir/build/openssl-android" "$script_dir/openssl/build.sh"
build_step "Android JSON-C" "$script_dir/build/json-c-android" "$script_dir/json-c/build.sh"
build_step "Android curl" "$script_dir/build/curl-android" "$script_dir/curl/build.sh"
build_step "Android Cyrus SASL" "$script_dir/build/cyrus-sasl-android" "$script_dir/cyrus-sasl/build.sh"
build_step "Android libiconv" "$script_dir/build/iconv-android" "$script_dir/iconv/build.sh"
build_step "Android libxml2" "$script_dir/build/libxml2-android" "$script_dir/libxml2/build.sh"
build_step "Android RNP" "$script_dir/build/rnp-android" "$script_dir/rnp/build.sh"

echo "Android dependencies are ready in $script_dir/build"
