#!/bin/sh

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
top_srcdir=$(CDPATH= cd -- "$script_dir/.." && pwd)
public_include_dir="$top_srcdir/build-spm/include/libetpan"
tmp_list="$top_srcdir/build-spm/include/.spm-public-headers.tmp"
tmp_unsorted_list="$top_srcdir/build-spm/include/.spm-public-headers.unsorted.tmp"

files=""

list_headers()
{
  filename="$1"

  case "$filename" in
    libetpan-config.h|libetpan_version.h) return ;;
  esac

  case "$files" in
    *"[$filename]"*) return ;;
  esac

  matches=$(find "$top_srcdir/src" -name "$filename" -type f)
  if test "x$matches" = x ; then
    echo "error: public header not found: $filename" >&2
    exit 1
  fi

  count=$(printf '%s\n' "$matches" | wc -l | tr -d ' ')
  if test "$count" != 1 ; then
    echo "error: public header name is ambiguous: $filename" >&2
    printf '%s\n' "$matches" >&2
    exit 1
  fi

  path="$matches"
  printf '%s\n' "$path"
  files="$files[$filename]"

  subfilenames=$(LC_ALL=C sed -n 's/^#include <libetpan\/\(.*\)>$/\1/p' "$path")
  for include_file in $subfilenames ; do
    list_headers "$include_file"
  done
}

mkdir -p "$public_include_dir"

list_headers libetpan.h > "$tmp_unsorted_list"
sort "$tmp_unsorted_list" > "$tmp_list"
printf '%s\n' "$top_srcdir/libetpan-config.h" >> "$tmp_list"
printf '%s\n' "$top_srcdir/src/main/libetpan_version.h" >> "$tmp_list"

find "$public_include_dir" \( -type f -o -type l \) -name '*.h' -delete

while IFS= read -r header ; do
  test -f "$header" || {
    echo "error: missing header: $header" >&2
    exit 1
  }

  dest="$public_include_dir/$(basename "$header")"
  cp "$header" "$dest"
  cmp -s "$header" "$dest" || {
    echo "error: copied header differs: $dest" >&2
    exit 1
  }
done < "$tmp_list"

cat > "$public_include_dir/README.me" <<'EOF'
This directory contains copies of libEtPan public headers for Swift Package
Manager.

Do not edit these headers in place. Edit the source headers under src/ or the
generated headers at the repository root, then run:

  build-spm/update-public-headers.sh
EOF

rm -f "$tmp_list" "$tmp_unsorted_list"

echo "updated $public_include_dir"
