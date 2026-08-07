#!/usr/bin/env python3
#
# Local Fastmail OAuth helper for JMAP interop testing.

import argparse
import base64
import hashlib
import http.server
import json
import os
import secrets
import shlex
import socket
import string
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
import webbrowser


AUTHORIZE_URL = "https://api.fastmail.com/oauth/authorize"
TOKEN_URL = "https://api.fastmail.com/oauth/refresh"
SESSION_URL = "https://api.fastmail.com/jmap/session"
DEFAULT_SCOPE = "urn:ietf:params:jmap:core urn:ietf:params:jmap:mail"
DEFAULT_REDIRECT_HOST = "127.0.0.1"
DEFAULT_REDIRECT_PATH = "/fastmail-oauth"
DEFAULT_USER_AGENT = "libEtPan Fastmail JMAP OAuth test"
REFRESH_SKEW_SECONDS = 30


class OAuthError(Exception):
    def __init__(self, payload):
        self.payload = payload
        super().__init__(payload.get("error_description") or str(payload))


class CallbackResult:
    def __init__(self):
        self.path = None
        self.query = None
        self.error = None


def b64url(data):
    return base64.urlsafe_b64encode(data).decode("ascii").rstrip("=")


def pkce_verifier():
    alphabet = string.ascii_letters + string.digits + "-._~"
    return "".join(secrets.choice(alphabet) for _ in range(64))


def pkce_challenge(verifier):
    return b64url(hashlib.sha256(verifier.encode("ascii")).digest())


def token_preview(token):
    if len(token) <= 16:
        return "<redacted>"
    return "%s...%s" % (token[:8], token[-8:])


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


def post_form(url, values, timeout):
    data = urllib.parse.urlencode(values).encode("utf-8")
    request = urllib.request.Request(
        url,
        data=data,
        headers={
            "Content-Type": "application/x-www-form-urlencoded",
            "Accept": "application/json",
            "User-Agent": DEFAULT_USER_AGENT,
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


def free_port(host):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.bind((host, 0))
        return sock.getsockname()[1]


def make_callback_handler(result, expected_path):
    class CallbackHandler(http.server.BaseHTTPRequestHandler):
        def do_GET(self):
            parsed = urllib.parse.urlparse(self.path)
            result.path = parsed.path
            result.query = urllib.parse.parse_qs(parsed.query)

            if parsed.path != expected_path:
                result.error = "Unexpected callback path: %s" % parsed.path
                self.send_response(404)
                self.end_headers()
                self.wfile.write(b"Unexpected OAuth callback path.\n")
                return

            if "error" in result.query:
                self.send_response(400)
                self.end_headers()
                self.wfile.write(b"Fastmail OAuth authorization failed.\n")
                return

            self.send_response(200)
            self.send_header("Content-Type", "text/plain; charset=utf-8")
            self.end_headers()
            self.wfile.write(
                b"Fastmail OAuth authorization complete. "
                b"You can close this browser tab.\n"
            )

        def log_message(self, format, *args):
            return

    return CallbackHandler


def wait_for_callback(host, port, path, timeout):
    result = CallbackResult()
    server = http.server.HTTPServer((host, port),
        make_callback_handler(result, path))
    server.timeout = timeout
    server.handle_request()
    server.server_close()

    if result.query is None:
        raise OAuthError({
            "error": "callback_timeout",
            "error_description": "Timed out waiting for the OAuth redirect.",
        })
    if result.error:
        raise OAuthError({
            "error": "invalid_callback",
            "error_description": result.error,
        })
    return result.query


def authorization_url(args, redirect_uri, state, challenge):
    values = {
        "client_id": args.client_id,
        "redirect_uri": redirect_uri,
        "response_type": "code",
        "scope": args.scope,
        "code_challenge": challenge,
        "code_challenge_method": "S256",
        "state": state,
    }
    return "%s?%s" % (AUTHORIZE_URL, urllib.parse.urlencode(values))


def exchange_code(args, code, redirect_uri, verifier):
    return post_form(TOKEN_URL, {
        "client_id": args.client_id,
        "redirect_uri": redirect_uri,
        "grant_type": "authorization_code",
        "code": code,
        "code_verifier": verifier,
    }, args.http_timeout)


def refresh_access_token(args, refresh_token):
    return post_form(TOKEN_URL, {
        "client_id": args.client_id,
        "grant_type": "refresh_token",
        "refresh_token": refresh_token,
    }, args.http_timeout)


def annotate_token(args, token):
    saved = dict(token)
    now = int(time.time())
    expires_in = int(saved.get("expires_in", 3600))

    saved["expires_at"] = now + expires_in
    saved["client_id"] = args.client_id
    saved["requested_email"] = args.email
    saved["scope"] = saved.get("scope", args.scope)
    saved["session_url"] = args.session_url
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
    if (token.get("requested_email") and
            token.get("requested_email") != args.email):
        return False
    saved_scope = token.get("scope")
    return not saved_scope or saved_scope == args.scope


def probe_jmap_session(args, access_token):
    request = urllib.request.Request(
        args.session_url,
        headers={
            "Authorization": "Bearer %s" % access_token,
            "Accept": "application/json",
            "User-Agent": DEFAULT_USER_AGENT,
        },
        method="GET",
    )

    with urllib.request.urlopen(request, timeout=args.http_timeout) as response:
        body = json.loads(response.read().decode("utf-8"))
        accounts = body.get("accounts", {})
        capabilities = body.get("capabilities", {})
        return {
            "status": response.status,
            "api_url": body.get("apiUrl"),
            "account_count": len(accounts),
            "capability_count": len(capabilities),
        }


def load_config(args):
    config = {}
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
    if args.scope is None:
        args.scope = config.get("scope", DEFAULT_SCOPE)
    if args.token_json is None:
        args.token_json = config.get("token_json")
    if args.session_url is None:
        args.session_url = config.get("session_url", SESSION_URL)

    if args.client_id is None:
        config_label = args.config if args.config else "the local config file"
        raise SystemExit(
            "error: --client-id is required unless client_id is set in %s" %
            config_label)


def parse_args(argv):
    parser = argparse.ArgumentParser(
        description="Get a Fastmail OAuth bearer token for JMAP testing."
    )
    parser.add_argument("email",
        help="Fastmail account email address to sign in as.")
    parser.add_argument("--config", default="tests/fastmail-oauth.local.json",
        help="Optional JSON config file with client_id, scope, token_json, "
        "or session_url.")
    parser.add_argument("--client-id",
        help="Fastmail OAuth client id. Fastmail must register this client.")
    parser.add_argument("--scope",
        help="Space-separated Fastmail OAuth scopes.")
    parser.add_argument("--redirect-host", default=DEFAULT_REDIRECT_HOST,
        help="Loopback host for the local OAuth callback server.")
    parser.add_argument("--redirect-path", default=DEFAULT_REDIRECT_PATH,
        help="Loopback callback path registered with Fastmail.")
    parser.add_argument("--port", type=int,
        help="Loopback callback port. Defaults to a free ephemeral port.")
    parser.add_argument("--token-file",
        help="Write only the access token to this file.")
    parser.add_argument("--json-file",
        help="Write the full token response JSON to this file.")
    parser.add_argument("--token-json",
        help="Read and update a cached token JSON file.")
    parser.add_argument("--env-file",
        help="Write shell exports for JMAP_SESSION_URL, JMAP_EMAIL, and "
        "JMAP_ACCESS_TOKEN.")
    parser.add_argument("--refresh", action="store_true",
        help="Refresh a saved/provided refresh token without browser auth.")
    parser.add_argument("--refresh-token",
        help="Refresh token to use with --refresh.")
    parser.add_argument("--force-auth", action="store_true",
        help="Ignore reusable cached tokens and run browser auth.")
    parser.add_argument("--no-browser", action="store_true",
        help="Print the authorization URL without opening a browser.")
    parser.add_argument("--print-token", action="store_true",
        help="Print the full access token to stdout.")
    parser.add_argument("--probe-jmap", action="store_true",
        help="Fetch the Fastmail JMAP Session object with the access token.")
    parser.add_argument("--session-url",
        help="JMAP Session URL for --probe-jmap and --env-file.")
    parser.add_argument("--http-timeout", type=int, default=300,
        help="HTTP timeout in seconds for callback and network requests.")

    args = parser.parse_args(argv)
    load_config(args)
    return args


def run_browser_auth(args):
    port = args.port if args.port is not None else free_port(args.redirect_host)
    redirect_uri = "http://%s:%d%s" % (
        args.redirect_host, port, args.redirect_path)
    state = secrets.token_urlsafe(32)
    verifier = pkce_verifier()
    url = authorization_url(args, redirect_uri, state, pkce_challenge(verifier))

    print("Sign in to Fastmail as %s." % args.email, file=sys.stderr)
    print("Authorization URL:\n%s" % url, file=sys.stderr)
    if not args.no_browser:
        webbrowser.open(url)

    query = wait_for_callback(args.redirect_host, port, args.redirect_path,
        args.http_timeout)
    returned_state = query.get("state", [None])[0]
    if returned_state != state:
        raise OAuthError({
            "error": "invalid_state",
            "error_description": "OAuth state did not match the local request.",
        })
    if "error" in query:
        raise OAuthError({
            "error": query["error"][0],
            "error_description": query.get("error_description",
                query["error"])[0],
        })

    code = query.get("code", [None])[0]
    if not code:
        raise OAuthError({
            "error": "missing_code",
            "error_description": "OAuth callback did not include a code.",
        })
    return exchange_code(args, code, redirect_uri, verifier)


def main(argv):
    args = parse_args(argv)
    token = None
    cached_token = None

    try:
        if args.token_json:
            cached_token = load_json_file(args.token_json)

        if (cached_token and not args.force_auth and not args.refresh and
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
                    if args.token_json:
                        save_json(args.token_json, token)
                        print("Updated token JSON %s" % args.token_json,
                            file=sys.stderr)
                except OAuthError as exc:
                    if args.refresh or exc.payload.get("error") != "invalid_grant":
                        raise
                    print("Saved refresh token is no longer valid; "
                        "starting browser auth.", file=sys.stderr)

        if token is None:
            if args.refresh:
                raise OAuthError({
                    "error": "missing_refresh_token",
                    "error_description": (
                        "--refresh requires --refresh-token or a saved token "
                        "JSON with refresh_token."
                    ),
                })
            token = annotate_token(args, run_browser_auth(args))
            if args.token_json:
                save_json(args.token_json, token)
                print("Wrote token JSON to %s" % args.token_json,
                    file=sys.stderr)
    except OAuthError as exc:
        print("OAuth failed: %s" % exc, file=sys.stderr)
        if "error" in exc.payload:
            print("error=%s" % exc.payload["error"], file=sys.stderr)
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
    if args.env_file:
        save_text(args.env_file,
            "export JMAP_SESSION_URL=%s\n"
            "export JMAP_EMAIL=%s\n"
            "export JMAP_ACCESS_TOKEN=%s" % (
                shlex.quote(args.session_url),
                shlex.quote(args.email),
                shlex.quote(access_token),
            ))
        print("Wrote JMAP environment exports to %s" % args.env_file,
            file=sys.stderr)

    print("Received %s token: %s" % (
        token.get("token_type", "bearer"),
        access_token if args.print_token else token_preview(access_token),
    ))
    print("email=%s" % args.email)
    print("scope=%s" % token.get("scope", args.scope))
    print("expires_in=%s" % token.get("expires_in", ""))
    if token.get("expires_at"):
        print("expires_at=%s" % token["expires_at"])
    if token.get("refresh_token"):
        print("refresh_token=<present>")

    if args.probe_jmap:
        try:
            probe = probe_jmap_session(args, access_token)
        except urllib.error.HTTPError as exc:
            print("JMAP Session probe failed: HTTP %d" % exc.code,
                file=sys.stderr)
            return 1
        print("JMAP Session status=%s accounts=%s capabilities=%s" % (
            probe["status"], probe["account_count"],
            probe["capability_count"],
        ))
        if probe["api_url"]:
            print("apiUrl=%s" % probe["api_url"])

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
