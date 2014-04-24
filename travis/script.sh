#!/bin/sh
set -e

./autogen.sh --with-curl=no --disable-db --with-expat=no
make dist distdir=libetpan-travis-build
cd libetpan-travis-build
make
cd tests
make imap-sample
