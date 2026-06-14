#!/bin/sh

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
top_srcdir=$(CDPATH= cd -- "$script_dir/.." && pwd)

cd "$top_srcdir"

echo "Release metadata to review in configure.ac:"
sed -n '/^m4_define(\[maj_version\]/,/^m4_define(\[api_compatibility\]/p' configure.ac
echo
echo "Before tagging, update these values in configure.ac when needed:"
echo "  - maj_version, min_version, mic_version"
echo "  - api_current, api_revision, api_compatibility"
echo

test -f src/main/libetpan_version.h || {
  echo "error: src/main/libetpan_version.h is missing." >&2
  echo "Run ./configure after updating configure.ac, then run this script again." >&2
  exit 1
}

test -f libetpan-config.h || {
  echo "error: libetpan-config.h is missing." >&2
  echo "Run ./configure after updating configure.ac, then run this script again." >&2
  exit 1
}

configure_value()
{
  sed -n "s/^m4_define(\\[$1\\], \\[\\(.*\\)\\])$/\\1/p" configure.ac
}

header_value()
{
  sed -n "s/^#define $1 \\(.*\\)$/\\1/p" src/main/libetpan_version.h
}

check_generated_version()
{
  config_name="$1"
  header_name="$2"
  config_value=$(configure_value "$config_name")
  generated_value=$(header_value "$header_name")

  if test "x$config_value" != "x$generated_value" ; then
    echo "error: src/main/libetpan_version.h is stale for $header_name." >&2
    echo "configure.ac has $config_name=$config_value, generated header has $generated_value." >&2
    echo "Run ./configure after updating configure.ac, then run this script again." >&2
    exit 1
  fi
}

check_generated_version maj_version LIBETPAN_VERSION_MAJOR
check_generated_version min_version LIBETPAN_VERSION_MINOR
check_generated_version mic_version LIBETPAN_VERSION_MICRO
check_generated_version api_current LIBETPAN_API_CURRENT
check_generated_version api_revision LIBETPAN_API_REVISION
check_generated_version api_compatibility LIBETPAN_API_COMPATIBILITY

echo "Refreshing Windows public header artifacts..."
cp -f src/main/libetpan_version.h build-windows/libetpan_version.h
(cd build-windows && ./gen-public-headers.sh > build_headers.list)

echo "Refreshing Swift Package Manager public headers..."
build-spm/update-public-headers.sh

echo
echo "Release preparation complete."
echo "Review the generated changes, then run the normal build and test matrix."
