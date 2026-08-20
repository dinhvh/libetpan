#!/bin/bash

set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
repository_root="$(cd "$script_dir/../.." && pwd)"
submodules_dir="build-mac/dependencies/submodules"

if ! command -v git >/dev/null 2>&1; then
  echo "Required command not found: git" >&2
  exit 1
fi

echo "Initializing macOS/iOS dependency submodules"
git -C "$repository_root" submodule update --init --recursive -- "$submodules_dir"

echo "Building JSON-C XCFramework"
"$script_dir/build-json-c-xcframework.sh"

echo "Building Cyrus SASL XCFramework"
"$script_dir/build-cyrus-sasl-xcframework.sh"

echo "Building OpenSSL libcrypto XCFramework"
"$script_dir/build-openssl-xcframework.sh"

echo "Building tidy-html5 XCFramework"
"$script_dir/build-tidy-html5-xcframework.sh"

echo "Building RNP XCFramework"
"$script_dir/build-rnp-xcframework.sh"

echo "Building ICU XCFrameworks"
"$script_dir/build-icu-xcframework.sh"

echo "Dependencies are ready in $script_dir/build"
