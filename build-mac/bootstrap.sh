#!/bin/bash

set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
repository_root="$(cd "$script_dir/.." && pwd)"
logfile="$script_dir/bootstrap.log"
archive="$script_dir/autogen-result.tar.gz"
configure_flags=(
  --with-expat=no
  --with-curl=no
  --enable-debug
  --with-smime=no
)

autogen_available=true
if [[ ! -x "$repository_root/autogen.sh" ]]; then
  autogen_available=false
fi
for command_name in aclocal autoconf automake; do
  if ! command -v "$command_name" >/dev/null 2>&1; then
    autogen_available=false
  fi
done
if ! command -v glibtoolize >/dev/null 2>&1 && ! command -v libtoolize >/dev/null 2>&1; then
  autogen_available=false
fi

cd "$repository_root"

if "$autogen_available"; then
  echo "Generating the autotools build files"
  SDKROOT= IPHONEOS_DEPLOYMENT_TARGET= \
    ./autogen.sh "${configure_flags[@]}" >"$logfile" 2>&1 || {
      cat "$logfile"
      exit 1
    }

  echo "Creating build-mac/autogen-result.tar.gz"
  {
    find . \
      -path './build-mac/dependencies/submodules' -prune -o \
      -name '*.in' -print0
    printf '%s\0' configure install-sh config.sub missing config.guess
  } | tar --null -T - -czf "$archive"
else
  echo "Autotools are unavailable; using build-mac/autogen-result.tar.gz"
  tar -xzf "$archive"
  SDKROOT= IPHONEOS_DEPLOYMENT_TARGET= \
    ./configure "${configure_flags[@]}" >"$logfile" 2>&1 || {
      cat "$logfile"
      exit 1
    }
fi

echo "Building macOS/iOS dependencies"
"$script_dir/dependencies/bootstrap.sh"

echo "libEtPan is ready to build from build-mac/libetpan.xcworkspace"
