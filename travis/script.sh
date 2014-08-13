#!/bin/sh
set -e

if test "x`uname`" = xLinux ; then
  distdir=libetpan-travis-build
  ./autogen.sh --with-curl=no --disable-db --with-expat=no
  make dist distdir=$distdir
  tar xzf $distdir.tar.gz
  cd $distdir
  ./configure --with-curl=no --disable-db --with-expat=no
  make
  cd tests
  make imap-sample
else
  echo Building library for iPhoneOS
  xctool -project build-mac/libetpan.xcodeproj -sdk iphoneos7.1 -scheme "libetpan ios" build ARCHS="armv7 armv7s arm64"
  echo Building library for iPhoneSimulator
  xctool -project build-mac/libetpan.xcodeproj -sdk iphonesimulator7.1 -scheme "libetpan ios" build ARCHS="i386 x86_64"

  echo Building library for Mac
  xctool -project build-mac/libetpan.xcodeproj -sdk macosx10.8 -scheme "static libetpan" build
  echo Building framework for Mac
  xctool -project build-mac/libetpan.xcodeproj -sdk macosx10.8 -scheme "libetpan" build
fi
