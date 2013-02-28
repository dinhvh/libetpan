#!/bin/sh
if test ! -d libetpan.xcodeproj ; then
	exit 1;
fi

logfile="`pwd`/update.log"

cd ..

if test x$SRCROOT = x ; then
  echo Should be run from Xcode
  exit 1
fi

if test x$ACTION = x ; then
  if test ! -f Makefile ; then
    echo configuring
    ./autogen.sh > "$logfile" 2>&1
    if [[ "$?" != "0" ]]; then
      echo "configure failed"
      exit 1
    fi

    make stamp-prepare-target >> "$logfile" 2>&1
    make libetpan-config.h >> "$logfile" 2>&1
  fi
  if test x$PLATFORM_NAME = xiphoneos ; then
    if test ! -d build-mac/libsasl-ios ; then
      # build dependencies for iOS
      cd build-mac
      sh ./prepare-ios.sh
    fi
  fi
elif test x$ACTION = xclean ; then
  if test -f Makefile ; then
    make maintainer-clean >/dev/null
    cd build-mac
    rm -rf libsasl-ios
    rm -rf dependencies/build
  fi
fi

