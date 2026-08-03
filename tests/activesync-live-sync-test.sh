#!/bin/sh
#
# Live Microsoft ActiveSync sync smoke test.
#
# This wrapper keeps credentials out of argv history where possible by
# reusing the local OAuth helper config and cached token JSON.

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
top_dir=$(CDPATH= cd -- "$script_dir/.." && pwd)

config_file=${LIBETPAN_ACTIVESYNC_OAUTH_CONFIG:-"$script_dir/activesync-ms-oauth.local.json"}
token_json=${LIBETPAN_ACTIVESYNC_TOKEN_JSON:-}
state_file=${LIBETPAN_ACTIVESYNC_STATE_FILE:-}
server=${LIBETPAN_ACTIVESYNC_SERVER:-}
login=${LIBETPAN_ACTIVESYNC_LOGIN:-}
extra_args=${LIBETPAN_ACTIVESYNC_SAMPLE_ARGS:-}
email_suite=0
validate_cert_file=${LIBETPAN_ACTIVESYNC_VALIDATE_CERT_FILE:-}

usage() {
  cat <<EOF
usage: $(basename "$0") [--config PATH] [--token-json PATH] [--state-file PATH]
       [--server URL] [--login USER] [--email-suite]
       [--validate-cert-file PATH] [-- SAMPLE_ARGS...]

Environment overrides:
  LIBETPAN_ACTIVESYNC_OAUTH_CONFIG
  LIBETPAN_ACTIVESYNC_TOKEN_JSON
  LIBETPAN_ACTIVESYNC_STATE_FILE
  LIBETPAN_ACTIVESYNC_SERVER
  LIBETPAN_ACTIVESYNC_LOGIN
  LIBETPAN_ACTIVESYNC_SAMPLE_ARGS
  LIBETPAN_ACTIVESYNC_VALIDATE_CERT_FILE
EOF
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
    --state-file)
      state_file=$2
      shift 2
      ;;
    --server)
      server=$2
      shift 2
      ;;
    --login)
      login=$2
      shift 2
      ;;
    --email-suite)
      email_suite=1
      shift
      ;;
    --validate-cert-file)
      validate_cert_file=$2
      shift 2
      ;;
    --help)
      usage
      exit 0
      ;;
    --)
      shift
      extra_args="$extra_args $*"
      break
      ;;
    *)
      usage >&2
      exit 2
      ;;
  esac
done

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

if [ -f "$config_file" ]; then
  if [ -z "$token_json" ]; then
    token_json=$(json_value token_json "$config_file")
  fi
  if [ -z "$state_file" ]; then
    state_file=$(json_value state_file "$config_file")
  fi
  if [ -z "$server" ]; then
    server=$(json_value activesync_url "$config_file")
  fi
  if [ -z "$login" ]; then
    login=$(json_value login "$config_file")
  fi
fi

if [ -z "$token_json" ]; then
  token_json="/tmp/libetpan-eas-token.json"
fi

if [ -z "$state_file" ]; then
  state_file="/tmp/libetpan-activesync-state.json"
fi

if [ -z "$server" ]; then
  server="https://eas.outlook.com/Microsoft-Server-ActiveSync"
fi

if [ -z "$login" ]; then
  login=$(json_value login "$token_json")
fi

client_id=$(json_value client_id "$token_json")
tenant=$(json_value tenant "$token_json")
oauth_version=$(json_value oauth_version "$token_json")
servertype=$(json_value servertype "$token_json")
scope=$(json_value scope "$token_json")
resource=$(json_value resource "$token_json")

if [ -z "$login" ]; then
  printf '%s\n' "error: set LIBETPAN_ACTIVESYNC_LOGIN or login in $config_file" >&2
  printf '%s\n' "hint: pass --login email@example.com to reuse $token_json" >&2
  exit 2
fi

if [ ! -x "$script_dir/activesync-sample" ]; then
  if [ -x "$script_dir/.libs/activesync-sample" ]; then
    sample="$script_dir/.libs/activesync-sample"
  else
    printf '%s\n' "error: build tests/activesync-sample first" >&2
    printf '%s\n' "hint: cd $top_dir && make -C tests activesync-sample" >&2
    exit 2
  fi
else
  sample="$script_dir/activesync-sample"
fi

if [ -n "$client_id" ]; then
  set -- "$script_dir/activesync-ms-device-oauth.py" \
    --config "$config_file" \
    --client-id "$client_id" \
    --token-json "$token_json" \
    --refresh \
    --probe-activesync \
    --login "$login" \
    --activesync-url "$server"
  if [ -n "$tenant" ]; then
    set -- "$@" --tenant "$tenant"
  fi
  if [ -n "$oauth_version" ]; then
    set -- "$@" --oauth-version "$oauth_version"
  fi
  if [ -n "$servertype" ]; then
    set -- "$@" --servertype "$servertype"
  fi
  if [ -n "$scope" ]; then
    set -- "$@" --scope "$scope"
  fi
  if [ -n "$resource" ]; then
    set -- "$@" --resource "$resource"
  fi
else
  set -- "$script_dir/activesync-ms-device-oauth.py" \
    --config "$config_file" \
    --token-json "$token_json" \
    --refresh \
    --probe-activesync \
    --login "$login" \
    --activesync-url "$server"
fi

"$@" >/tmp/libetpan-activesync-oauth-refresh.log

access_token=$(json_value access_token "$token_json")
if [ -z "$access_token" ]; then
  printf '%s\n' "error: OAuth helper did not write an access_token to $token_json" >&2
  exit 1
fi

printf 'ActiveSync server=%s\n' "$server" >&2
printf 'ActiveSync login=%s\n' "$login" >&2
printf 'ActiveSync token_json=%s\n' "$token_json" >&2
printf 'ActiveSync state_file=%s\n' "$state_file" >&2

run_sample() {
  set -- "$sample" \
    --server "$server" \
    --login "$login" \
    --oauth-token "$access_token" \
    --state-file "$state_file" \
    --follow-redirect \
    --debug \
    "$@"

  if [ -n "$extra_args" ]; then
    # shellcheck disable=SC2086
    set -- "$@" $extra_args
  fi

  "$@"
}

run_suite_case() {
  name=$1
  shift
  saved_state_file=$state_file
  state_file="$saved_state_file.$name.$$"

  printf '\nActiveSync suite case: %s\n' "$name" >&2
  printf 'ActiveSync suite case state_file=%s\n' "$state_file" >&2
  if run_sample "$@"; then
    printf 'ActiveSync suite case passed: %s\n' "$name" >&2
  else
    status=$?
    printf 'ActiveSync suite case failed: %s status=%s\n' "$name" "$status" >&2
    suite_failed=1
    if [ -z "$suite_failures" ]; then
      suite_failures=$name
    else
      suite_failures="$suite_failures $name"
    fi
  fi
  state_file=$saved_state_file
}

if [ "$email_suite" -eq 1 ]; then
  suite_failed=0
  suite_failures=

  run_suite_case SendMail --send-self-test
  run_suite_case SmartReply --smart-reply-self-test
  run_suite_case SmartForward --smart-forward-self-test
  run_suite_case ResolveRecipients --resolve-self-test
  run_suite_case ResolveRecipientsCertificates --resolve-cert-self-test
  run_suite_case Search --search-self-test
  run_suite_case Ping --ping-self-test
  run_suite_case PingChange --ping-change-self-test
  run_suite_case DraftMutation --draft-self-test
  run_suite_case MultiSync --multi-sync-self-test
  run_suite_case Attachment --attachment-self-test
  run_suite_case MessageMutation --mutation-self-test
  run_suite_case FolderMutation --folder-self-test
  run_suite_case MoveItems --move-self-test
  if [ -n "$validate_cert_file" ]; then
    run_suite_case ValidateCert --validate-cert-self-test \
      --validate-cert-file "$validate_cert_file"
  fi

  if [ "$suite_failed" -ne 0 ]; then
    printf '\nActiveSync email suite failures: %s\n' "$suite_failures" >&2
    exit 1
  fi

  printf '\nActiveSync email suite passed.\n' >&2
  exit 0
fi

run_sample
