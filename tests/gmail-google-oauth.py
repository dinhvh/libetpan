#!/usr/bin/env python3
#
# Local Google OAuth device-code helper for Gmail HTTP interop testing.

import argparse
import json
import os
import sys
import time
import urllib.error
import urllib.parse
import urllib.request


DEFAULT_CONFIG_FILE = "tests/gmail-google-oauth.local.json"
DEFAULT_SCOPE = "https://www.googleapis.com/auth/gmail.readonly"
DEVICE_CODE_URL = "https://oauth2.googleapis.com/device/code"
TOKEN_URL = "https://oauth2.googleapis.com/token"
GMAIL_PROFILE_URL = "https://gmail.googleapis.com/gmail/v1/users/me/profile"
DEVICE_CODE_GRANT = "urn:ietf:params:oauth:grant-type:device_code"
REFRESH_SKEW_SECONDS = 30


class OAuthError(Exception):
    def __init__(self, payload):
        self.payload = payload
        super().__init__(payload.get("error_description") or str(payload))


def post_form(url, values, timeout):
    data = urllib.parse.urlencode(values).encode("utf-8")
    request = urllib.request.Request(
        url,
        data=data,
        headers={
            "Content-Type": "application/x-www-form-urlencoded",
            "Accept": "application/json",
            "User-Agent": "libEtPan Gmail OAuth test",
        },
        method="POST",
    )

    try:
        with urllib.request.urlopen(request, timeout=timeout) as response:
            return json.loads(response.read().decode("utf-8"))
    except urllib.error.HTTPError as exc:
        body = exc.read().decode("utf-8", errors="replace")
        try:
            payload = json.loads(body)
        except json.JSONDecodeError:
            payload = {
                "error": "http_error",
                "error_description": "HTTP %d: %s" % (exc.code, body),
            }
        raise OAuthError(payload)


def request_device_code(args):
    values = {
        "client_id": args.client_id,
        "scope": args.scope,
    }
    return post_form(DEVICE_CODE_URL, values, args.http_timeout)


def poll_for_token(args, device_code):
    deadline = time.monotonic() + int(device_code["expires_in"])
    interval = int(device_code.get("interval", 5))

    while time.monotonic() < deadline:
        time.sleep(interval)
        try:
            values = {
                "grant_type": DEVICE_CODE_GRANT,
                "client_id": args.client_id,
                "device_code": device_code["device_code"],
            }
            if args.client_secret:
                values["client_secret"] = args.client_secret
            return post_form(TOKEN_URL, values, args.http_timeout)
        except OAuthError as exc:
            error = exc.payload.get("error")
            if error == "authorization_pending":
                print("Waiting for browser sign-in...", file=sys.stderr)
                continue
            if error == "slow_down":
                interval += 5
                print("Google asked us to slow polling.", file=sys.stderr)
                continue
            raise

    raise OAuthError({
        "error": "expired_token",
        "error_description": "The device code expired before sign-in completed.",
    })


def refresh_access_token(args, refresh_token):
    values = {
        "grant_type": "refresh_token",
        "client_id": args.client_id,
        "refresh_token": refresh_token,
    }
    if args.client_secret:
        values["client_secret"] = args.client_secret
    return post_form(TOKEN_URL, values, args.http_timeout)


def load_json_file(path):
    try:
        with open(path, "r", encoding="utf-8") as f:
            return json.load(f)
    except FileNotFoundError:
        return None


def save_json(path, value):
    with open(path, "w", encoding="utf-8") as f:
        json.dump(value, f, indent=2, sort_keys=True)
        f.write("\n")


def save_text(path, value):
    with open(path, "w", encoding="utf-8") as f:
        f.write(value)
        f.write("\n")


def token_preview(token):
    if len(token) <= 16:
        return "<redacted>"
    return "%s...%s" % (token[:8], token[-8:])


def annotate_token(args, token):
    saved = dict(token)
    now = int(time.time())
    expires_in = int(saved.get("expires_in", 3600))

    saved["expires_at"] = now + expires_in
    saved["client_id"] = args.client_id
    if args.client_secret:
        saved["client_secret"] = args.client_secret
    saved["scope"] = args.scope
    return saved


def token_is_current(token):
    access_token = token.get("access_token")
    expires_at = token.get("expires_at")
    if not access_token or not expires_at:
        return False

    try:
        return int(expires_at) > int(time.time()) + REFRESH_SKEW_SECONDS
    except (TypeError, ValueError):
        return False


def token_matches_args(args, token):
    if token.get("client_id") and token.get("client_id") != args.client_id:
        return False
    if token.get("scope") and token.get("scope") != args.scope:
        return False
    return True


def probe_gmail(args, access_token):
    request = urllib.request.Request(
        GMAIL_PROFILE_URL,
        headers={
            "Authorization": "Bearer %s" % access_token,
            "Accept": "application/json",
            "User-Agent": "libEtPan Gmail OAuth test",
        },
        method="GET",
    )

    try:
        with urllib.request.urlopen(request, timeout=args.http_timeout) as response:
            body = response.read().decode("utf-8", errors="replace")
            return response.status, response.reason, body
    except urllib.error.HTTPError as exc:
        body = exc.read().decode("utf-8", errors="replace")
        return exc.code, exc.reason, body


def parse_args(argv):
    parser = argparse.ArgumentParser(
        description="Get a Google OAuth token with device-code flow for "
        "local Gmail HTTP testing."
    )
    parser.add_argument("--config", default=DEFAULT_CONFIG_FILE,
        help="JSON config file. Use an empty value to disable config loading.")
    parser.add_argument("--client-id",
        help="Google OAuth client ID for a TV/limited-input device app.")
    parser.add_argument("--client-secret",
        help="OAuth client secret, if required by the selected Google client.")
    parser.add_argument("--scope",
        help="Space-separated scopes to request.")
    parser.add_argument("--token-file",
        help="Write only the access token to this file.")
    parser.add_argument("--json-file",
        help="Write the full token response JSON to this file.")
    parser.add_argument("--token-json",
        help="Read and update a cached token JSON file.")
    parser.add_argument("--refresh", action="store_true",
        help="Refresh a saved/provided refresh token without device-code auth.")
    parser.add_argument("--refresh-token",
        help="Refresh token to use with --refresh.")
    parser.add_argument("--force-auth", action="store_true",
        help="Ignore reusable cached tokens and run device-code auth.")
    parser.add_argument("--print-token", action="store_true",
        help="Print the full access token to stdout.")
    parser.add_argument("--probe-gmail", action="store_true",
        help="Send a Gmail users.getProfile request with the access token.")
    parser.add_argument("--http-timeout", type=int, default=30,
        help="HTTP timeout in seconds.")
    args = parser.parse_args(argv)
    load_config(args)
    return args


def load_config(args):
    config = {}
    cached_token = {}

    if args.config:
        config_path = args.config
        if not os.path.exists(config_path) and not os.path.isabs(config_path):
            config_path = os.path.join(os.path.dirname(__file__),
                os.path.basename(config_path))
        try:
            with open(config_path, "r", encoding="utf-8") as f:
                config = json.load(f)
        except FileNotFoundError:
            pass

    if args.client_id is None:
        args.client_id = config.get("client_id")
    if args.client_secret is None:
        args.client_secret = config.get("client_secret")
    if args.scope is None:
        args.scope = config.get("scope")
    if args.token_json is None:
        args.token_json = config.get("token_json")

    if args.token_json:
        cached_token = load_json_file(args.token_json) or {}

    if args.client_id is None:
        args.client_id = cached_token.get("client_id")
    if args.client_secret is None:
        args.client_secret = cached_token.get("client_secret")
    if args.scope is None:
        args.scope = cached_token.get("scope", DEFAULT_SCOPE)

    if args.client_id is None:
        config_label = args.config if args.config else "the local config file"
        raise SystemExit(
            "error: --client-id is required unless client_id is set in %s" %
            config_label)


def print_oauth_hint(payload):
    error = payload.get("error")
    if error == "invalid_client":
        print(
            "hint=Use a Google OAuth client configured for TVs and Limited "
            "Input devices. If the token endpoint requires it, include "
            "client_secret in the local config or pass --client-secret.",
            file=sys.stderr,
        )
    if error == "invalid_scope":
        print(
            "hint=Check that the Gmail API is enabled for the Google Cloud "
            "project and that the requested Gmail scope is allowed by the "
            "OAuth consent screen.",
            file=sys.stderr,
        )


def main(argv):
    args = parse_args(argv)
    token = None
    cached_token = None

    try:
        if args.token_json:
            cached_token = load_json_file(args.token_json)

        if (cached_token and not args.force_auth and
                token_matches_args(args, cached_token) and
                token_is_current(cached_token)):
            token = cached_token
            print("Using cached access token from %s" % args.token_json,
                file=sys.stderr)

        if token is None and not args.force_auth:
            refresh_token = args.refresh_token
            if refresh_token is None and cached_token:
                refresh_token = cached_token.get("refresh_token")
            if refresh_token:
                try:
                    token = annotate_token(args,
                        refresh_access_token(args, refresh_token))
                    if "refresh_token" not in token:
                        token["refresh_token"] = refresh_token
                    if args.token_json:
                        save_json(args.token_json, token)
                        print("Updated token JSON %s" % args.token_json,
                            file=sys.stderr)
                except OAuthError as exc:
                    if args.refresh or exc.payload.get("error") != "invalid_grant":
                        raise
                    print("Saved refresh token is no longer valid; "
                        "starting device-code auth.", file=sys.stderr)

        if token is None:
            if args.refresh:
                raise OAuthError({
                    "error": "missing_refresh_token",
                    "error_description": (
                        "--refresh requires --refresh-token or a saved token "
                        "JSON with refresh_token."
                    ),
                })

            device_code = request_device_code(args)
            print("Open %s and enter code %s" % (
                device_code.get("verification_url",
                    device_code.get("verification_uri",
                        "https://www.google.com/device")),
                device_code["user_code"],
            ))

            token = annotate_token(args, poll_for_token(args, device_code))
            if args.token_json:
                save_json(args.token_json, token)
                print("Wrote token JSON to %s" % args.token_json,
                    file=sys.stderr)
    except OAuthError as exc:
        print("OAuth failed: %s" % exc, file=sys.stderr)
        if "error" in exc.payload:
            print("error=%s" % exc.payload["error"], file=sys.stderr)
        print_oauth_hint(exc.payload)
        return 1

    access_token = token.get("access_token")
    if not access_token:
        print("OAuth response did not include an access_token.", file=sys.stderr)
        return 1

    if args.token_file:
        save_text(args.token_file, access_token)
        print("Wrote access token to %s" % args.token_file, file=sys.stderr)

    if args.json_file:
        save_json(args.json_file, annotate_token(args, token))
        print("Wrote token JSON to %s" % args.json_file, file=sys.stderr)

    print("Received %s token: %s" % (
        token.get("token_type", "Bearer"),
        access_token if args.print_token else token_preview(access_token),
    ))
    print("scope=%s" % token.get("scope", args.scope))
    if token.get("expires_at"):
        print("expires_at=%s" % token.get("expires_at"))
    print("expires_in=%s" % token.get("expires_in", ""))
    if token.get("refresh_token"):
        print("refresh_token=<present>")

    if args.probe_gmail:
        status, reason, body = probe_gmail(args, access_token)
        print("Gmail profile status=%s reason=%s body_len=%s" % (
            status, reason, len(body),
        ))
        if status < 200 or status >= 300:
            print(body[:500], file=sys.stderr)
            return 1

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
