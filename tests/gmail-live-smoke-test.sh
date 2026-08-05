#!/bin/sh
#
# Live Gmail HTTP smoke test.
#
# This wrapper keeps Gmail's OAuth device-code flow outside libEtPan and runs
# the low-level Gmail HTTP sample with a caller-provided bearer token.

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
top_dir=$(CDPATH= cd -- "$script_dir/.." && pwd)

config_file=${LIBETPAN_GMAIL_OAUTH_CONFIG:-"$script_dir/gmail-google-oauth.local.json"}
token_json=${LIBETPAN_GMAIL_TOKEN_JSON:-}
user=${LIBETPAN_GMAIL_USER:-}
sample_args=${LIBETPAN_GMAIL_SAMPLE_ARGS:-}
max_results=${LIBETPAN_GMAIL_MAX_RESULTS:-5}
fetch_first_message=0

usage() {
  cat <<EOF
usage: $(basename "$0") [--config PATH] [--token-json PATH] [--user USER]
       [--max-results N] [--fetch-first-message] [-- SAMPLE_ARGS...]

Environment overrides:
  LIBETPAN_GMAIL_OAUTH_CONFIG
  LIBETPAN_GMAIL_TOKEN_JSON
  LIBETPAN_GMAIL_USER
  LIBETPAN_GMAIL_MAX_RESULTS
  LIBETPAN_GMAIL_SAMPLE_ARGS
EOF
}

json_value() {
  key=$1
  file=$2
  python3 - "$key" "$file" <<'PY'
import json
import sys

key = sys.argv[1]
path = sys.argv[2]
try:
    with open(path, "r", encoding="utf-8") as f:
        value = json.load(f).get(key)
except FileNotFoundError:
    value = None
if value is not None:
    print(value)
PY
}

while [ $# -gt 0 ]; do
  case "$1" in
    --config)
      config_file=$2
      shift 2
      ;;
    --token-json)
      token_json=$2
      shift 2
      ;;
    --user)
      user=$2
      shift 2
      ;;
    --max-results)
      max_results=$2
      shift 2
      ;;
    --fetch-first-message)
      fetch_first_message=1
      shift
      ;;
    --help)
      usage
      exit 0
      ;;
    --)
      shift
      sample_args="$sample_args $*"
      break
      ;;
    *)
      usage >&2
      exit 2
      ;;
  esac
done

if [ -f "$config_file" ]; then
  if [ -z "$token_json" ]; then
    token_json=$(json_value token_json "$config_file")
  fi
  if [ -z "$user" ]; then
    user=$(json_value user "$config_file")
  fi
fi

if [ -z "$token_json" ]; then
  token_json="/tmp/libetpan-gmail-token.json"
fi

if [ ! -x "$script_dir/gmail-sample" ]; then
  if [ -x "$script_dir/.libs/gmail-sample" ]; then
    sample="$script_dir/.libs/gmail-sample"
  else
    printf '%s\n' "error: build tests/gmail-sample first" >&2
    printf '%s\n' "hint: cd $top_dir && make -C tests gmail-sample" >&2
    exit 2
  fi
else
  sample="$script_dir/gmail-sample"
fi

if [ -f "$config_file" ]; then
  client_id=$(json_value client_id "$config_file")
elif [ -f "$token_json" ]; then
  client_id=$(json_value client_id "$token_json")
else
  client_id=
fi

if [ -z "$client_id" ] && [ ! -f "$token_json" ]; then
  echo "skip: set LIBETPAN_GMAIL_TOKEN_JSON or create $config_file with client_id"
  exit 77
fi

if [ -n "$client_id" ]; then
  set -- "$script_dir/gmail-google-oauth.py" \
    --config "$config_file" \
    --token-json "$token_json" \
    --refresh \
    --probe-gmail
  "$@" >/tmp/libetpan-gmail-oauth-refresh.log
fi

access_token=$(json_value access_token "$token_json")
if [ -z "$access_token" ]; then
  printf '%s\n' "error: OAuth helper did not write an access_token to $token_json" >&2
  exit 1
fi

printf 'Gmail token_json=%s\n' "$token_json" >&2
if [ -n "$user" ]; then
  printf 'Gmail user=%s\n' "$user" >&2
fi

set -- "$sample" \
  --oauth-token "$access_token" \
  --max-results "$max_results"

if [ -n "$user" ]; then
  set -- "$@" --user "$user"
fi

if [ "$fetch_first_message" -eq 1 ]; then
  set -- "$@" --fetch-first-message
fi

if [ -n "$sample_args" ]; then
  # shellcheck disable=SC2086
  set -- "$@" $sample_args
fi

"$@"
