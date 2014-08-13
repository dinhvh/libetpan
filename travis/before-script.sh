#!/bin/sh
set -e

if test "x`uname`" = xLinux ; then
  sudo apt-get install libsasl2-dev libssl-dev zlib1g-dev
fi
