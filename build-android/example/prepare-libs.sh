#!/bin/sh
set -e
# Populate app/src/main/cpp/prebuilt/ with the libetpan static lib + its three
# dependencies (per ABI) and the public headers, sourced from the artifacts
# produced by build-android/build.sh. Re-runnable; fails loudly if anything is
# missing.

here="$(cd "$(dirname "$0")" && pwd)"
build_android="$(cd "$here/.." && pwd)"
dest="$here/app/src/main/cpp/prebuilt"

etpan_dir="$build_android/build/libetpan-android-7"
ssl_dir="$build_android/dependencies/build/openssl-android-3"
sasl_dir="$build_android/dependencies/build/cyrus-sasl-android-4"
iconv_dir="$build_android/dependencies/build/iconv-android-1"

for artifact_dir in "$etpan_dir" "$ssl_dir" "$sasl_dir" "$iconv_dir" ; do
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
  cp "$iconv_dir/libs/$abi/libiconv.a"   "$dest/$abi/libiconv.a"
done

# Headers: full libetpan public API (from the build tree) + sasl.
mkdir -p "$dest/include/libetpan" "$dest/include/sasl"
cp "$build_android/include/libetpan/"*.h "$dest/include/libetpan/"
cp "$sasl_dir/include/sasl/"*.h "$dest/include/sasl/"

echo "OK: prebuilt populated at $dest"
ls "$dest"/arm64-v8a
