#!/bin/sh

set -eu

script_dir=`dirname "$0"`

if test "x${JMAP_ACCESS_TOKEN:-}" = x ; then
  echo "skip: set JMAP_ACCESS_TOKEN for live JMAP smoke testing"
  exit 77
fi

if test "x${JMAP_SESSION_URL:-}" = x && test "x${JMAP_EMAIL:-}" = x ; then
  echo "skip: set JMAP_SESSION_URL or JMAP_EMAIL for live JMAP smoke testing"
  exit 77
fi

exec "$script_dir/jmap-sample"
