#!/usr/bin/env bash

set -euo pipefail

repository_root="$(cd "$(dirname "$0")/.." && pwd)"
dependencies="$repository_root/build-mac/dependencies/build"
output_root="$repository_root/.build/apple-unix-ios"
deployment_target=15.6

build_variant() {
  local sdk="$1" arch="$2" slice="$3" target="$4"
  local sdk_path compiler build_dir crypto ssl sasl json rnp
  sdk_path="$(xcrun --sdk "$sdk" --show-sdk-path)"
  compiler="$(xcrun --sdk "$sdk" --find clang)"
  build_dir="$output_root/$sdk-$arch"
  crypto="$dependencies/OpenSSL-Crypto.xcframework/$slice"
  ssl="$dependencies/OpenSSL-SSL.xcframework/$slice"
  sasl="$dependencies/CyrusSASL.xcframework/$slice"
  json="$dependencies/JsonC.xcframework/$slice"
  rnp="$dependencies/RNP.xcframework/$slice"
  for library in "$crypto/libcrypto.a" "$ssl/libssl.a" "$sasl/libsasl2.a" "$json/libjson-c.a" "$rnp/librnp.a"; do
    test -f "$library" || { echo "Missing dependency: $library" >&2; return 1; }
  done
  mkdir -p "$build_dir"
  (
    cd "$build_dir"
    export CC="$compiler" OBJC="$compiler"
    export CXX="$(xcrun --sdk "$sdk" --find clang++)"
    export CFLAGS="-target $target -isysroot $sdk_path -O2"
    export CXXFLAGS="$CFLAGS" OBJCFLAGS="$CFLAGS"
    export CPPFLAGS="-I$crypto/Headers -I$sasl/Headers -I$json/Headers -I$rnp/Headers -I$sdk_path/usr/include/libxml2"
    export LDFLAGS="-target $target -isysroot $sdk_path -L$crypto -L$ssl -L$sasl -L$json -L$rnp"
    # Explicit target flags keep pkg-config from selecting host Homebrew libraries.
    export PKG_CONFIG_LIBDIR="$build_dir/pkgconfig" PKG_CONFIG_PATH=""
    mkdir -p "$PKG_CONFIG_LIBDIR"
    printf '%s\n' \
      'Name: librnp' 'Description: iOS RNP XCFramework slice' 'Version: 0.0' \
      "Cflags: -I$rnp/Headers" \
      "Libs: -L$rnp -lrnp -L$crypto -lcrypto -L$json -ljson-c -lc++ -lz" \
      > "$PKG_CONFIG_LIBDIR/librnp.pc"
    export JSON_C_CFLAGS="-I$json/Headers" JSON_C_LIBS="-L$json -ljson-c"
    export LIBXML2_CFLAGS="-I$sdk_path/usr/include/libxml2" LIBXML2_LIBS="-lxml2"
    "$repository_root/configure" \
      --build="$("$repository_root/config.guess")" --host="$arch-apple-darwin" \
      --enable-static --disable-shared --disable-db --disable-lockfile \
      --with-openssl=yes --with-smime=openssl --with-sasl=yes \
      --with-curl=no --with-gnutls=no --with-icu=no \
      --with-rnp=yes --with-json=yes --with-jmap=yes --with-feed=yes
    make libetpan-config.h stamp-prepare-target
    make -C src -j"$(sysctl -n hw.ncpu)"
    test -s src/.libs/libetpan.a
    "$compiler" $CFLAGS $LDFLAGS -dynamiclib \
      -Wl,-force_load,src/.libs/libetpan.a -Wl,-undefined,error \
      -lrnp -ljson-c -lsasl2 -lssl -lcrypto -lc++ -liconv -lxml2 -lz \
      -framework Foundation -framework CFNetwork -framework Security \
      -framework CoreFoundation -o libetpan-link-smoke.dylib
    xcrun vtool -show-build libetpan-link-smoke.dylib
  )
}

build_variant iphoneos arm64 ios-arm64 "arm64-apple-ios$deployment_target"
build_variant iphonesimulator arm64 ios-arm64_x86_64-simulator "arm64-apple-ios$deployment_target-simulator"
build_variant iphonesimulator x86_64 ios-arm64_x86_64-simulator "x86_64-apple-ios$deployment_target-simulator"
echo "iOS Unix build and link checks passed: $output_root"
