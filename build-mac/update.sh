#!/bin/sh
if test ! -d libetpan.xcodeproj ; then
	exit 1;
fi

logfile="`pwd`/update.log"

cd ..

if test "x$1" = xprepare ; then
  echo preparing
  ./autogen.sh > "$logfile" 2>&1
  # Package every build aux file autogen.sh produced. Newer autoconf/automake/
  # libtool require compile, depcomp and ltmain.sh at configure time; omitting
  # them makes ./configure abort with "cannot find required auxiliary files".
  # Only include the ones that exist so the command works across tool versions.
  aux_files=""
  for f in configure install-sh config.sub config.guess missing compile depcomp ltmain.sh ar-lib test-driver ; do
    test -f "$f" && aux_files="$aux_files $f"
  done
  tar czf build-mac/autogen-result.tar.gz `find . -name '*.in'` $aux_files
  exit 0
elif test "x$1" = xprepare-clean ; then
  if test -f Makefile ; then
    make maintainer-clean >/dev/null
    cd build-mac
    rm -rf libsasl-ios
    rm -rf dependencies/build
  fi
  exit 0
fi

if test "x$SRCROOT" = x ; then
  echo Should be run from Xcode
  exit 1
fi

if test "x$ACTION" = x ; then
  ACTION=build
fi

if test "x$ACTION" = xbuild -o "x$ACTION" = xinstall ; then
  
  md5 build-mac/autogen-result.tar.gz > build-mac/autogen-result.md5.new
  if ! cmp -s build-mac/autogen-result.md5 build-mac/autogen-result.md5.new ; then
    rm -f Makefile
  fi
  rm -f build-mac/autogen-result.md5.new
  if test ! -f Makefile ; then
    echo configuring
    tar xzf build-mac/autogen-result.tar.gz
    export SDKROOT=
    export IPHONEOS_DEPLOYMENT_TARGET=
    ./configure --with-expat=no --with-curl=no --enable-debug > "$logfile" 2>&1
    if [[ "$?" != "0" ]]; then
      cat "$logfile"
      echo "configure failed"
      exit 1
    fi

    make stamp-prepare-target >> "$logfile" 2>&1
    make libetpan-config.h >> "$logfile" 2>&1
    md5 build-mac/autogen-result.tar.gz > build-mac/autogen-result.md5
  fi
  if test "x$PLATFORM_NAME" = xiphoneos -o "x$PLATFORM_NAME" = xiphonesimulator ; then
    if test ! -d build-mac/libsasl-ios ; then
      # build dependencies for iOS
      cd build-mac
      sh ./prepare-ios.sh
    fi
  fi
elif test "x$ACTION" = xclean ; then
  if test -f Makefile ; then
    make distclean >/dev/null
    cd build-mac
    rm -f autogen-result.md5
    rm -rf libsasl-ios
    rm -rf dependencies/build
  fi
fi

