#!/bin/sh
set -e

./configure --with-curl=no --disable-db --with_expat=no
make
cd tests
make imap-sample
