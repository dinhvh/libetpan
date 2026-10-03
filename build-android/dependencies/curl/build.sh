#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=../android-common.sh
source "$script_dir/../android-common.sh"

package_name=curl-android
source_dir="$android_repo_root/build-mac/dependencies/submodules/curl"
build_root="$android_dependencies_dir/build/curl"
output_dir="$(android_dependency_output_dir "$package_name")"
openssl_root="$(android_dependency_output_dir openssl-android)"

android_require_ndk
android_require_command cmake
android_source_required "$source_dir" CMakeLists.txt "curl"

if [[ ! -d "$openssl_root" ]]; then
  echo "Building OpenSSL first"
  (cd "$script_dir/../openssl" && ./build.sh)
fi
android_require_dependency_output "$openssl_root" "OpenSSL"

rm -rf "$build_root" "$output_dir"
mkdir -p "$build_root"

for abi in $(android_abis); do
  echo "Building curl for $abi"
  variant_root="$build_root/$abi"
  build_dir="$variant_root/build"
  prefix="$variant_root/install"
  openssl_ssl="$openssl_root/libs/$abi/libssl.a"
  openssl_crypto="$openssl_root/libs/$abi/libcrypto.a"

  [[ -f "$openssl_ssl" ]] || android_fail "Missing OpenSSL SSL library for $abi: $openssl_ssl"
  [[ -f "$openssl_crypto" ]] || android_fail "Missing OpenSSL Crypto library for $abi: $openssl_crypto"

  android_cmake_configure "$source_dir" "$build_dir" "$prefix" "$abi" \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_STATIC_LIBS=ON \
    -DBUILD_CURL_EXE=OFF \
    -DBUILD_EXAMPLES=OFF \
    -DBUILD_TESTING=OFF \
    -DBUILD_LIBCURL_DOCS=OFF \
    -DBUILD_MISC_DOCS=OFF \
    -DENABLE_CURL_MANUAL=OFF \
    -DHTTP_ONLY=ON \
    -DCURL_USE_OPENSSL=ON \
    -DCMAKE_USE_OPENSSL=ON \
    -DOPENSSL_ROOT_DIR="$openssl_root" \
    -DOPENSSL_INCLUDE_DIR="$openssl_root/include" \
    -DOPENSSL_SSL_LIBRARY="$openssl_ssl" \
    -DOPENSSL_CRYPTO_LIBRARY="$openssl_crypto" \
    -DOPENSSL_USE_STATIC_LIBS=TRUE \
    -DCURL_USE_LIBPSL=OFF \
    -DCURL_USE_LIBSSH2=OFF \
    -DCURL_BROTLI=OFF \
    -DCURL_ZSTD=OFF \
    -DUSE_NGHTTP2=OFF \
    -DUSE_LIBIDN2=OFF \
    -DCURL_DISABLE_LDAP=ON \
    -DCURL_DISABLE_LDAPS=ON
  cmake --build "$build_dir" --parallel "$(android_jobs)"
  cmake --install "$build_dir"

  android_copy_headers_once "$output_dir" "$prefix/include"
  curl_library="$(android_find_static_library "$prefix" libcurl.a)" ||
    android_fail "curl did not install libcurl.a for $abi"
  android_copy_library "$output_dir" "$abi" "$curl_library" libcurl.a
done

rm -rf "$build_root"
echo "Created $output_dir"
