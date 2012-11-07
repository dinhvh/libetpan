#!/bin/sh
if test ! -d libetpan.xcodeproj ; then
	exit 1;
fi

logfile="`pwd`/update.log"

cd ..
echo configuring
./autogen.sh > "$logfile" 2>&1
if [[ "$?" != "0" ]]; then
    echo "configure failed"
    exit 1
fi

make stamp-prepare-target >> "$logfile" 2>&1
make libetpan-config.h >> "$logfile" 2>&1
cd build-mac
mkdir -p include/libetpan >> "$logfile" 2>&1
cp -r ../include/libetpan/ include/libetpan/
cp ../config.h include
cp ../libetpan-config.h include

# build dependencies for iOS
sh ./prepare-ios.sh
