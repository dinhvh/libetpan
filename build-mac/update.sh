#!/bin/sh
if test ! -d libetpan.xcodeproj ; then
	exit 1;
fi

logfile="`pwd`/update.log"

cd ..

if test x$1 = xprepare ; then
  echo preparing
  ./autogen.sh > "$logfile" 2>&1
  tar czf build-mac/autogen-result.tar.gz `find . -name '*.in'` configure install-sh config.sub missing config.guess
  exit 0
elif test x$1 = xprepare-clean ; then
  if test -f Makefile ; then
    make maintainer-clean >/dev/null
    cd build-mac
    rm -rf libsasl-ios
    rm -rf dependencies/build
  fi
  exit 0
fi

if test x$SRCROOT = x ; then
  echo Should be run from Xcode
  exit 1
fi

if test x$ACTION = x ; then
  if test ! -f Makefile ; then
    echo configuring
    tar xzf build-mac/autogen-result.tar.gz
    ./configure --enable-debug > "$logfile" 2>&1
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
    make distclean >/dev/null
    cd build-mac
    rm -rf libsasl-ios
    rm -rf dependencies/build
  fi
fi

