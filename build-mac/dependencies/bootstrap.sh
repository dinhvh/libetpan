#!/bin/bash

set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
repository_root="$(cd "$script_dir/../.." && pwd)"
submodules_dir="build-mac/dependencies/submodules"
logfile="$(mktemp "${TMPDIR:-/tmp}/libetpan-dependencies-bootstrap.XXXXXX")"

trap 'rm -f "$logfile"' EXIT

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

if ! command -v git >/dev/null 2>&1; then
  echo "Required command not found: git" >&2
  exit 1
fi

run_step "Initializing macOS/iOS dependency submodules" \
  git -C "$repository_root" submodule update --init --recursive -- "$submodules_dir"

run_step "Building JSON-C XCFramework" \
  "$script_dir/build-json-c-xcframework.sh"
run_step "Building Cyrus SASL XCFramework" \
  "$script_dir/build-cyrus-sasl-xcframework.sh"
run_step "Building OpenSSL libcrypto XCFramework" \
  "$script_dir/build-openssl-xcframework.sh"
run_step "Building tidy-html5 XCFramework" \
  "$script_dir/build-tidy-html5-xcframework.sh"
run_step "Building RNP XCFramework" \
  "$script_dir/build-rnp-xcframework.sh"

echo "Dependencies are ready in $script_dir/build"
