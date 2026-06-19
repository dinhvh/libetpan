#!/bin/sh
set -e
# Populate app/src/main/cpp/prebuilt/ with the libetpan static lib + its three
# dependencies (per ABI) and the public headers, sourced from the artifacts
# produced by build-android/build.sh. Re-runnable; fails loudly if anything is
# missing.

here="$(cd "$(dirname "$0")" && pwd)"
build_android="$(cd "$here/.." && pwd)"
dest="$here/app/src/main/cpp/prebuilt"
tmp="$here/.prepare-tmp"

etpan_zip="$build_android/libetpan-android-7.zip"
ssl_zip="$build_android/dependencies/openssl/openssl-android-3.zip"
sasl_zip="$build_android/dependencies/cyrus-sasl/cyrus-sasl-android-4.zip"
iconv_zip="$build_android/dependencies/iconv/iconv-android-1.zip"

for z in "$etpan_zip" "$ssl_zip" "$sasl_zip" "$iconv_zip" ; do
  if [ ! -f "$z" ]; then
    echo "ERROR: missing artifact: $z"
    echo "       run build-android/build.sh (with ANDROID_NDK set to r23+) first."
    exit 1
  fi
done

rm -rf "$tmp" "$dest"
mkdir -p "$tmp" "$dest"
( cd "$tmp" && unzip -qo "$etpan_zip" && unzip -qo "$ssl_zip" \
            && unzip -qo "$sasl_zip" && unzip -qo "$iconv_zip" )

for abi in arm64-v8a armeabi-v7a x86 x86_64 ; do
  mkdir -p "$dest/$abi"
  cp "$tmp/libetpan-android-7/libs/$abi/libetpan.a"   "$dest/$abi/libetpan.a"
  cp "$tmp/openssl-android-3/libs/$abi/libssl.a"      "$dest/$abi/libssl.a"
  cp "$tmp/openssl-android-3/libs/$abi/libcrypto.a"   "$dest/$abi/libcrypto.a"
  cp "$tmp/cyrus-sasl-android-4/libs/$abi/libsasl2.a" "$dest/$abi/libsasl2.a"
  cp "$tmp/iconv-android-1/libs/$abi/libiconv.a"      "$dest/$abi/libiconv.a"
done

# Headers: full libetpan public API (from the build tree) + sasl.
mkdir -p "$dest/include/libetpan" "$dest/include/sasl"
cp "$build_android/include/libetpan/"*.h "$dest/include/libetpan/"
cp "$tmp/cyrus-sasl-android-4/include/sasl/"*.h "$dest/include/sasl/"

rm -rf "$tmp"
echo "OK: prebuilt populated at $dest"
ls "$dest"/arm64-v8a
