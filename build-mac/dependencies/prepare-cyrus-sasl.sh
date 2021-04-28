#! /bin/bash -

export PATH=/usr/bin:/bin:/usr/sbin:/sbin

version=2.1.26
ARCHIVE=cyrus-sasl-$version
ARCHIVE_NAME=$ARCHIVE.tar.gz
ARCHIVE_PATCH=$ARCHIVE.patch
#url=ftp://ftp.andrew.cmu.edu/pub/cyrus-mail/$ARCHIVE_NAME
#url=ftp://ftp.cyrusimap.org/cyrus-sasl/$ARCHIVE_NAME
#url=https://www.cyrusimap.org/releases/$ARCHIVE_NAME
url=https://github.com/cyrusimap/cyrus-sasl/releases/download/$ARCHIVE/$ARCHIVE_NAME
patchfile=cyrus-2.1.25-libetpan.patch

scriptdir="`pwd`"

current_dir="$scriptdir"
builddir="$current_dir/build/libsasl"
BUILD_TIMESTAMP=`date +'%Y%m%d%H%M%S'`
tempbuilddir="$builddir/workdir/$BUILD_TIMESTAMP"
mkdir -p "$tempbuilddir"
srcdir="$tempbuilddir/src"
logdir="$tempbuilddir/log"
resultdir="$builddir/builds"
tmpdir="$tempbuilddir/tmp"

mkdir -p "$resultdir"
mkdir -p "$logdir"
mkdir -p "$tmpdir"
mkdir -p "$srcdir"

if test -f "$resultdir/libsasl-$version-ios.tar.gz"; then
	echo already built
	cd "$scriptdir/.."
	tar xzf "$resultdir/libsasl-$version-ios.tar.gz"
	exit 0
fi

# download package file

if test -f "$current_dir/packages/$ARCHIVE_NAME" ; then
	:
else
	echo "download source package - $url"

	mkdir -p "$current_dir/packages"
  cd "$current_dir/packages"
	curl -L -O "$url"
	if test x$? != x0 ; then
		echo fetch of $ARCHIVE_NAME failed
		exit 1
	fi
fi

if [ ! -e "$current_dir/packages/$ARCHIVE_NAME" ]; then
    echo "Missing archive $ARCHIVE"
    exit 1
fi

echo "prepare sources"

cd "$srcdir"
tar -xzf "$current_dir/packages/$ARCHIVE_NAME"
if [ $? != 0 ]; then
    echo "Unable to decompress $ARCHIVE_NAME"
    exit 1
fi

logfile="$srcdir/$ARCHIVE/build.log"

echo "*** patching sources ***" > "$logfile" 2>&1

cd "$srcdir/$ARCHIVE"
patch -p1 < $current_dir/$patchfile
# patch source files
cd "$srcdir/$ARCHIVE/include"
sed -E 's/\.\/\$< /.\/\$<i386 /' < Makefile.am > Makefile.am.new
mv Makefile.am.new Makefile.am
sed -E 's/\.\/\$< /.\/\$<i386 /' < Makefile.in > Makefile.in.new
mv Makefile.in.new Makefile.in
cd "$srcdir/$ARCHIVE/lib"
sed -E 's/\$\(AR\) cru \.libs\/\$@ \$\(SASL_STATIC_OBJS\)/&; \$\(RANLIB\) .libs\/\$@/' < Makefile.in > Makefile.in.new
mv Makefile.in.new Makefile.in

echo "building tools"
echo "*** generating makemd5 ***" >> "$logfile" 2>&1

cd "$srcdir/$ARCHIVE"
export SDKROOT=
export IPHONEOS_DEPLOYMENT_TARGET=
./configure > "$logfile" 2>&1
if [[ "$?" != "0" ]]; then
  echo "CONFIGURE FAILED"
  exit 1
fi
cd include
make makemd5 >> "$logfile" 2>&1
if [[ "$?" != "0" ]]; then
  echo "BUILD FAILED"
  exit 1
fi
cd ..
echo generated makemd5i386 properly
mv "$srcdir/$ARCHIVE/include/makemd5" "$srcdir/$ARCHIVE/include/makemd5i386"
make clean >>"$logfile" 2>&1
make distclean >>"$logfile" 2>&1
find . -name config.cache -print0 | xargs -0 rm

cd "$srcdir/$ARCHIVE"

export LANG=en_US.US-ASCII

LIB_NAME=$ARCHIVE
TARGETS="iPhoneOS iPhoneSimulator"

SDK_IOS_MIN_VERSION=7.0
SDK_IOS_VERSION="`xcodebuild -showsdks 2>/dev/null | grep iphoneos | sed 's/.*iphoneos\(.*\)/\1/'`"
BUILD_DIR="$tmpdir/build"
INSTALL_PATH="${BUILD_DIR}/${LIB_NAME}/universal"
BITCODE_FLAGS="-fembed-bitcode"
if test "x$NOBITCODE" != x ; then
   BITCODE_FLAGS=""
fi

for TARGET in $TARGETS; do

    DEVELOPER="$(xcode-select --print-path)"
    SDK_ID="$(echo "$TARGET$SDK_IOS_VERSION" | tr A-Z a-z)"
    SYSROOT="$(xcodebuild -version -sdk "$SDK_ID" 2>/dev/null | egrep '^Path: ' | cut -d ' ' -f 2)"

    case $TARGET in
        (iPhoneOS)
            ARCH=arm
            MARCHS="armv7 armv7s arm64"
            EXTRA_FLAGS="$BITCODE_FLAGS -miphoneos-version-min=$SDK_IOS_MIN_VERSION"
            ;;
        (iPhoneSimulator)
            ARCH=i386
            MARCHS="i386 x86_64"
            EXTRA_FLAGS="-miphoneos-version-min=$SDK_IOS_MIN_VERSION"
            ;;
    esac

    for MARCH in $MARCHS; do
				echo "building for $TARGET - $MARCH"
				echo "*** building for $TARGET - $MARCH ***" >> "$logfile" 2>&1

        PREFIX=${BUILD_DIR}/${LIB_NAME}/${TARGET}${SDK_IOS_VERSION}${MARCH}
        rm -rf $PREFIX

        export CPPFLAGS="-arch ${MARCH} -isysroot ${SYSROOT}"
        export CFLAGS="${CPPFLAGS} -Os ${EXTRA_FLAGS}"

        OPENSSL="--with-openssl=$BUILD_DIR/openssl-1.0.0d/universal"
        PLUGINS="--enable-otp=no --enable-digest=no --with-des=no --enable-login"
        ./configure --host=${ARCH} --prefix=$PREFIX --enable-shared=no --enable-static=yes --with-pam=$BUILD_DIR/openpam-20071221/universal $PLUGINS >> "$logfile" 2>&1
        make -j 8 >> "$logfile" 2>&1
        if [[ "$?" != "0" ]]; then
          echo "CONFIGURE FAILED"
          cat "$logfile"
          exit 1
        fi
        cd lib
        make install >> "$logfile" 2>&1
        cd ..
        cd include
        make install >> "$logfile" 2>&1
        cd ..
        cd plugins
        make install >> "$logfile" 2>&1
        cd ..
        if [[ "$?" != "0" ]]; then
          echo "BUILD FAILED"
          cat "$logfile"
          exit 1
        fi
        make clean >> "$logfile" 2>&1
        make distclean >> "$logfile" 2>&1
        find . -name config.cache -print0 | xargs -0 rm
      done
done

echo "*** creating universal libs ***" >> "$logfile" 2>&1

rm -rf "$INSTALL_PATH"
mkdir -p "$INSTALL_PATH"
mkdir -p "$INSTALL_PATH/lib"
mkdir -p "$INSTALL_PATH/include/sasl"
cp `find ./include -name '*.h'` "${INSTALL_PATH}/include/sasl"
ALL_LIBS="libsasl2.a sasl2/libanonymous.a sasl2/libcrammd5.a sasl2/libplain.a sasl2/libsasldb.a sasl2/liblogin.a"
for lib in $ALL_LIBS; do
    dir="`dirname $lib`"
    if [[ "$dir" != "." ]]; then
        mkdir -p ${INSTALL_PATH}/lib/$dir
    fi
    LIBS=
    for TARGET in $TARGETS; do
        LIBS="$LIBS ${BUILD_DIR}/${LIB_NAME}/${TARGET}${SDK_IOS_VERSION}*/lib/${lib}"
    done
    lipo -create ${LIBS} -output "${INSTALL_PATH}/lib/${lib}"
done

echo "*** creating built package ***" >> "$logfile" 2>&1

cd "$BUILD_DIR"
mkdir -p libsasl-ios
cp -r "$INSTALL_PATH"/* libsasl-ios/
tar -czf "libsasl-$version-ios.tar.gz" libsasl-ios
mkdir -p "$resultdir"
mv "libsasl-$version-ios.tar.gz" "$resultdir"
cd "$resultdir"
ln -s "libsasl-$version-ios.tar.gz" "libsasl-prebuilt-ios.tar.gz"
rm -rf "$tempbuilddir"

cd "$scriptdir/.."
tar xzf "$resultdir/libsasl-$version-ios.tar.gz"

exit 0
