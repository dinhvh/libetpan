#!/bin/bash

set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
repository_root="$(cd "$script_dir/.." && pwd)"
logfile="$(mktemp "${TMPDIR:-/tmp}/libetpan-bootstrap.XXXXXX")"
archive="$script_dir/autogen-result.tar.gz"
no_cache=false
configure_flags=(
  --quiet
  --with-curl=no
  --enable-debug
  --with-smime=no
  --with-sasl=no
  --with-icu=no
  --with-gnutls=no
  --with-rnp=no
  --with-openssl=no
)

echo "Bootstrap log: $logfile"

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
autogen_unavailable_reasons=()
if [[ ! -x "$repository_root/autogen.sh" ]]; then
  autogen_available=false
  autogen_unavailable_reasons+=("$repository_root/autogen.sh is missing or not executable")
fi
for command_name in aclocal autoconf automake; do
  if ! command -v "$command_name" >/dev/null 2>&1; then
    autogen_available=false
    autogen_unavailable_reasons+=("$command_name was not found in PATH")
  fi
done
if ! command -v glibtoolize >/dev/null 2>&1 && ! command -v libtoolize >/dev/null 2>&1; then
  autogen_available=false
  autogen_unavailable_reasons+=("neither glibtoolize nor libtoolize was found in PATH")
fi

cd "$repository_root"

if "$no_cache" || [[ ! -f "$archive" ]]; then
  if ! "$autogen_available"; then
    echo "Autotools are required to create build-mac/autogen-result.tar.gz" >&2
    printf '  - %s\n' "${autogen_unavailable_reasons[@]}" >&2
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
  if "$autogen_available"; then
    echo "Using build-mac/autogen-result.tar.gz"
  else
    echo "Autotools are unavailable; using build-mac/autogen-result.tar.gz"
    printf '  - %s\n' "${autogen_unavailable_reasons[@]}"
  fi
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
