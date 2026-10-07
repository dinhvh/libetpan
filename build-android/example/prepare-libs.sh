#!/bin/sh
set -e
# Populate app/src/main/cpp/prebuilt/ with the libetpan static lib + its
# dependencies (per ABI) and the public headers, sourced from the artifacts
# produced by build-android/build.sh. Re-runnable; fails loudly if anything is
# missing.

here="$(cd "$(dirname "$0")" && pwd)"
build_android="$(cd "$here/.." && pwd)"
dest="$here/app/src/main/cpp/prebuilt"

etpan_dir="$build_android/build/libetpan-android"
ssl_dir="$build_android/dependencies/build/openssl-android"
sasl_dir="$build_android/dependencies/build/cyrus-sasl-android"
icu_dir="$build_android/dependencies/build/icu-android"
json_c_dir="$build_android/dependencies/build/json-c-android"
curl_dir="$build_android/dependencies/build/curl-android"
libxml2_dir="$build_android/dependencies/build/libxml2-android"
rnp_dir="$build_android/dependencies/build/rnp-android"

for artifact_dir in "$etpan_dir" "$ssl_dir" "$sasl_dir" "$icu_dir" "$json_c_dir" "$curl_dir" "$libxml2_dir" "$rnp_dir" ; do
  if [ ! -d "$artifact_dir" ]; then
    echo "ERROR: missing artifact directory: $artifact_dir"
    echo "       run build-android/build.sh (with ANDROID_NDK set to r23+) first."
    exit 1
  fi
done

rm -rf "$dest"
mkdir -p "$dest"

for abi in arm64-v8a armeabi-v7a x86 x86_64 ; do
  mkdir -p "$dest/$abi"
  cp "$etpan_dir/libs/$abi/libetpan.a"   "$dest/$abi/libetpan.a"
  cp "$ssl_dir/libs/$abi/libssl.a"       "$dest/$abi/libssl.a"
  cp "$ssl_dir/libs/$abi/libcrypto.a"    "$dest/$abi/libcrypto.a"
  cp "$sasl_dir/libs/$abi/libsasl2.a"    "$dest/$abi/libsasl2.a"
  cp "$icu_dir/libs/$abi/libicuuc.a"    "$dest/$abi/libicuuc.a"
  cp "$icu_dir/libs/$abi/libicudata.a"  "$dest/$abi/libicudata.a"
  cp "$json_c_dir/libs/$abi/libjson-c.a" "$dest/$abi/libjson-c.a"
  cp "$curl_dir/libs/$abi/libcurl.a"     "$dest/$abi/libcurl.a"
  cp "$libxml2_dir/libs/$abi/libxml2.a"  "$dest/$abi/libxml2.a"
  cp "$rnp_dir/libs/$abi/librnp.a"       "$dest/$abi/librnp.a"
done

# Headers: full libetpan public API (from the build tree) + sasl.
mkdir -p "$dest/include/libetpan" "$dest/include/sasl" "$dest/include/json-c" "$dest/include/curl" "$dest/include/libxml" "$dest/include/rnp"
cp "$build_android/include/libetpan/"*.h "$dest/include/libetpan/"
cp "$sasl_dir/include/sasl/"*.h "$dest/include/sasl/"
cp "$json_c_dir/include/json-c/"*.h "$dest/include/json-c/"
cp "$curl_dir/include/curl/"*.h "$dest/include/curl/"
cp "$libxml2_dir/include/libxml2/libxml/"*.h "$dest/include/libxml/"
cp "$rnp_dir/include/rnp/"*.h "$dest/include/rnp/"

echo "OK: prebuilt populated at $dest"
ls "$dest"/arm64-v8a
