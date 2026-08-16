#!/bin/sh

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
top_srcdir=$(CDPATH= cd -- "$script_dir/.." && pwd)

exec python3 "$top_srcdir/tools/public_headers.py" \
  --source-root "$top_srcdir" \
  export-spm "$@"
