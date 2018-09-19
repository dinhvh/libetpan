#!/bin/sh
set -e

IOSSDK="`xcodebuild -showsdks 2>/dev/null | grep iphoneos | sed 's/.*iphoneos\(.*\)/\1/'`"
OSXSDK="`xcodebuild -showsdks 2>/dev/null | grep macosx | sed 's/.*macosx\(.*\)/\1/'`"

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
  xcodebuild -project build-mac/libetpan.xcodeproj -sdk iphoneos$IOSSDK -scheme "libetpan ios" build ARCHS="armv7 armv7s arm64" > /dev/null
  echo Building library for iPhoneSimulator
  xcodebuild -project build-mac/libetpan.xcodeproj -sdk iphonesimulator$IOSSDK -scheme "libetpan ios" build ARCHS="i386 x86_64" > /dev/null

  echo Building library for Mac
  xcodebuild -project build-mac/libetpan.xcodeproj -sdk macosx$OSXSDK -scheme "static libetpan" build > /dev/null
  echo Building framework for Mac
  xcodebuild -project build-mac/libetpan.xcodeproj -sdk macosx$OSXSDK -scheme "libetpan" build > /dev/null
fi
