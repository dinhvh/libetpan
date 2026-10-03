#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=../android-common.sh
source "$script_dir/../android-common.sh"

build_version=1
openssl_build_version=3
json_c_build_version=1
package_name=rnp-android
source_dir="$android_repo_root/build-mac/dependencies/submodules/rnp"
build_root="$script_dir/build"
build_source_dir="$build_root/source"
package_dir="$script_dir/$package_name-$build_version"
zip_name="$package_name-$build_version.zip"
openssl_zip="$script_dir/../openssl/openssl-android-$openssl_build_version.zip"
json_c_zip="$script_dir/../json-c/json-c-android-$json_c_build_version.zip"

android_require_ndk
android_require_command cmake
android_require_command tar
android_require_command unzip
android_source_required "$source_dir" CMakeLists.txt "RNP"
android_source_required "$source_dir" src/libsexpp/CMakeLists.txt "RNP libsexpp"

if [[ ! -f "$openssl_zip" ]]; then
  echo "Building OpenSSL first"
  (cd "$script_dir/../openssl" && ./build.sh)
fi
if [[ ! -f "$json_c_zip" ]]; then
  echo "Building JSON-C first"
  (cd "$script_dir/../json-c" && ./build.sh)
fi

rm -rf "$build_root" "$package_dir"
mkdir -p "$build_root/third-party" "$build_source_dir"
(
  cd "$source_dir"
  tar --exclude=.git -cf - .
) | (
  cd "$build_source_dir"
  tar -xf -
)
sed -i 's|set(MKF ${MKF} "-DCMAKE_BUILD_TYPE=Release" "-DOPENSSL_ROOT_DIR=${OPENSSL_INCLUDE_DIR}/..")|set(MKF ${MKF} "-DCMAKE_BUILD_TYPE=Release" "-DOPENSSL_ROOT_DIR=${OPENSSL_INCLUDE_DIR}/.." "-DOPENSSL_INCLUDE_DIR=${OPENSSL_INCLUDE_DIR}" "-DOPENSSL_CRYPTO_LIBRARY=${OPENSSL_CRYPTO_LIBRARY}" "-DOPENSSL_SSL_LIBRARY=${OPENSSL_SSL_LIBRARY}")|' \
  "$build_source_dir/cmake/Modules/FindOpenSSLFeatures.cmake"
sed -i '/message(STATUS "Querying OpenSSL features")/a\
if(CMAKE_CROSSCOMPILING)\
  message(STATUS "Skipping executable OpenSSL feature probe while cross-compiling")\
  set(OPENSSL_SUPPORTED_FEATURES\
    AES-128-ECB AES-192-ECB AES-256-ECB AES-128-CBC AES-192-CBC AES-256-CBC\
    AES-128-OCB AES-192-OCB AES-256-OCB\
    CAMELLIA-128-ECB CAMELLIA-192-ECB CAMELLIA-256-ECB\
    DES-EDE3\
    MD5 SHA1 SHA224 SHA256 SHA384 SHA512 SHA3-256 SHA3-512 RIPEMD160\
    PRIME256V1 SECP384R1 SECP521R1 SECP256K1\
    RSAENCRYPTION DSAENCRYPTION DHKEYAGREEMENT ID-ECPUBLICKEY X25519 ED25519)\
  function(OpenSSLHasFeature FEATURE VARIABLE)\
    string(TOUPPER ${FEATURE} _feature_up)\
    set(${VARIABLE} FALSE PARENT_SCOPE)\
    if(${_feature_up} IN_LIST OPENSSL_SUPPORTED_FEATURES)\
      set(${VARIABLE} TRUE PARENT_SCOPE)\
    endif()\
  endfunction()\
  return()\
endif(CMAKE_CROSSCOMPILING)\
' "$build_source_dir/cmake/Modules/FindOpenSSLFeatures.cmake"
sed -i '/if(CMAKE_TOOLCHAIN_FILE)/i\
if(ANDROID_ABI)\
  set(MKF ${MKF} "-DANDROID_ABI=${ANDROID_ABI}")\
endif(ANDROID_ABI)\
if(ANDROID_PLATFORM)\
  set(MKF ${MKF} "-DANDROID_PLATFORM=${ANDROID_PLATFORM}")\
endif(ANDROID_PLATFORM)\
' "$build_source_dir/cmake/Modules/FindOpenSSLFeatures.cmake"
unzip -qo "$openssl_zip" -d "$build_root/third-party"
unzip -qo "$json_c_zip" -d "$build_root/third-party"
openssl_root="$build_root/third-party/openssl-android-$openssl_build_version"
json_c_root="$build_root/third-party/json-c-android-$json_c_build_version"

for abi in $(android_abis); do
  echo "Building RNP for $abi"
  variant_root="$build_root/$abi"
  build_dir="$variant_root/build"
  prefix="$variant_root/install"
  combined="$variant_root/librnp.a"
  openssl_crypto="$openssl_root/libs/$abi/libcrypto.a"
  openssl_ssl="$openssl_root/libs/$abi/libssl.a"
  json_c_library="$json_c_root/libs/$abi/libjson-c.a"
  cross_emulator="$(command -v true)"

  [[ -f "$openssl_crypto" ]] || android_fail "Missing OpenSSL Crypto library for $abi: $openssl_crypto"
  [[ -f "$openssl_ssl" ]] || android_fail "Missing OpenSSL SSL library for $abi: $openssl_ssl"
  [[ -f "$json_c_library" ]] || android_fail "Missing JSON-C library for $abi: $json_c_library"
  rm -f "$openssl_root/lib"
  ln -s "libs/$abi" "$openssl_root/lib"

  android_cmake_configure "$build_source_dir" "$build_dir" "$prefix" "$abi" \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_TESTING=OFF \
    -DDOWNLOAD_GTEST=OFF \
    -DENABLE_DOC=OFF \
    -DENABLE_BZIP2=OFF \
    -DENABLE_CRYPTO_REFRESH=OFF \
    -DENABLE_PQC=OFF \
    -DCRYPTO_BACKEND=openssl \
    -DOPENSSL_ROOT_DIR="$openssl_root" \
    -DOPENSSL_INCLUDE_DIR="$openssl_root/include" \
    -DOPENSSL_CRYPTO_LIBRARY="$openssl_crypto" \
    -DOPENSSL_SSL_LIBRARY="$openssl_ssl" \
    -DOPENSSL_USE_STATIC_LIBS=TRUE \
    -DJSON-C_INCLUDE_DIR="$json_c_root/include/json-c" \
    -DJSON-C_LIBRARY="$json_c_library" \
    -DCMAKE_CROSSCOMPILING_EMULATOR="$cross_emulator"
  cmake --build "$build_dir" --target librnp sexpp --parallel "$(android_jobs)"

  [[ -f "$build_dir/src/lib/librnp.a" ]] ||
    android_fail "RNP did not produce src/lib/librnp.a for $abi"
  [[ -f "$build_dir/src/libsexpp/libsexpp.a" ]] ||
    android_fail "RNP did not produce src/libsexpp/libsexpp.a for $abi"
  android_combine_static_libraries "$combined" \
    "$build_dir/src/lib/librnp.a" \
    "$build_dir/src/libsexpp/libsexpp.a"

  mkdir -p "$package_dir/include/rnp"
  cp "$build_source_dir/include/rnp/rnp.h" "$package_dir/include/rnp/"
  cp "$build_source_dir/include/rnp/rnp_err.h" "$package_dir/include/rnp/"
  cp "$build_dir/src/lib/rnp/rnp_export.h" "$package_dir/include/rnp/"
  cp "$build_dir/src/lib/version.h" "$package_dir/include/rnp/rnp_ver.h"
  android_copy_library "$package_dir" "$abi" "$combined" librnp.a
done

android_zip_package "$script_dir" "$package_dir" "$zip_name"
rm -rf "$build_root" "$package_dir"
echo "Created $script_dir/$zip_name"
