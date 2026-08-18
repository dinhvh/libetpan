#!/bin/bash

set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
repository_root="$(cd "$script_dir/.." && pwd)"
logfile="$script_dir/bootstrap.log"
archive="$script_dir/autogen-result.tar.gz"
no_cache=false
configure_flags=(
  --with-expat=no
  --with-curl=no
  --enable-debug
  --with-smime=no
)

for argument in "$@"; do
  case "$argument" in
    --no-cache)
      no_cache=true
      ;;
    *)
      echo "Unknown argument: $argument" >&2
      echo "Usage: $0 [--no-cache]" >&2
      exit 1
      ;;
  esac
done

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

if "$no_cache" || [[ ! -f "$archive" ]]; then
  if ! "$autogen_available"; then
    echo "Autotools are required to create build-mac/autogen-result.tar.gz" >&2
    exit 1
  fi

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

make stamp-prepare-target >> "$logfile" 2>&1
make libetpan-config.h >> "$logfile" 2>&1

echo "Building macOS/iOS dependencies"
"$script_dir/dependencies/bootstrap.sh"

echo "libEtPan is ready to build from build-mac/libetpan.xcworkspace"
