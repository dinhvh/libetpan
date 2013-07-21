#!/bin/sh
set -e

./autogen.sh --with-curl=no --disable-db --with-expat=no
make
cd tests
make imap-sample
