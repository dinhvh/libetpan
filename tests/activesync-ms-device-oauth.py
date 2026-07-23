#!/usr/bin/env python3
#
# Local Microsoft OAuth device-code helper for ActiveSync interop testing.

import argparse
import base64
import json
import os
import sys
import time
import urllib.error
import urllib.parse
import urllib.request


DEFAULT_TENANT = "common"
DEFAULT_CONFIG_FILE = "tests/activesync-ms-oauth.local.json"
DEFAULT_SERVERTYPE = "office365"
SCOPE_BY_SERVERTYPE = {
    "office365": "offline_access https://outlook.office.com/.default",
    "personal-ms": (
        "offline_access https://outlook.office.com/EAS.AccessAsUser.All"
    ),
}
DEFAULT_RESOURCE = "https://outlook.office.com/"
DEFAULT_ACTIVESYNC_URL = (
    "https://outlook.office365.com/Microsoft-Server-ActiveSync"
)
DEFAULT_OAUTH_VERSION = "v2"
REFRESH_SKEW_SECONDS = 30
DEVICE_CODE_PATH_V1 = "/oauth2/devicecode"
TOKEN_PATH_V1 = "/oauth2/token"
DEVICE_CODE_PATH_V2 = "/oauth2/v2.0/devicecode"
TOKEN_PATH_V2 = "/oauth2/v2.0/token"
DEVICE_CODE_GRANT = "urn:ietf:params:oauth:grant-type:device_code"


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
            "User-Agent": "libEtPan ActiveSync OAuth test",
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


def authority_url(tenant, path):
    quoted_tenant = urllib.parse.quote(tenant.strip("/"), safe="")
    return "https://login.microsoftonline.com/%s%s" % (quoted_tenant, path)


def device_code_path(args):
    if args.oauth_version == "v1":
        return DEVICE_CODE_PATH_V1
    return DEVICE_CODE_PATH_V2


def token_path(args):
    if args.oauth_version == "v1":
        return TOKEN_PATH_V1
    return TOKEN_PATH_V2


def request_device_code(args):
    values = {
        "client_id": args.client_id,
    }
    if args.oauth_version == "v1":
        values["resource"] = args.resource
    else:
        values["scope"] = args.scope

    return post_form(
        authority_url(args.tenant, device_code_path(args)),
        values,
        args.http_timeout,
    )


def poll_for_token(args, device_code):
    deadline = time.monotonic() + int(device_code["expires_in"])
    interval = int(device_code.get("interval", 5))

    while time.monotonic() < deadline:
        time.sleep(interval)
        try:
            values = {
                "grant_type": DEVICE_CODE_GRANT,
                "client_id": args.client_id,
            }
            if args.oauth_version == "v1":
                values["code"] = device_code["device_code"]
            else:
                values["device_code"] = device_code["device_code"]

            return post_form(
                authority_url(args.tenant, token_path(args)),
                values,
                args.http_timeout,
            )
        except OAuthError as exc:
            error = exc.payload.get("error")
            if error == "authorization_pending":
                print("Waiting for browser sign-in...", file=sys.stderr)
                continue
            if error == "slow_down":
                interval += 5
                print("Microsoft asked us to slow polling.", file=sys.stderr)
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
    if args.oauth_version == "v1":
        values["resource"] = args.resource
    else:
        values["scope"] = args.scope

    return post_form(
        authority_url(args.tenant, token_path(args)),
        values,
        args.http_timeout,
    )


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


def token_payload_email(token):
    id_token = token.get("id_token")
    if not id_token or not isinstance(id_token, str):
        return None
    parts = id_token.split(".")
    if len(parts) < 2:
        return None
    try:
        payload = parts[1].replace("-", "+").replace("_", "/")
        payload += "=" * ((4 - len(payload) % 4) % 4)
        data = json.loads(base64.b64decode(payload).decode("utf-8"))
        return data.get("email") or data.get("preferred_username") or data.get("upn")
    except Exception:
        return None


def annotate_token(args, token):
    saved = dict(token)
    now = int(time.time())
    expires_in = int(saved.get("expires_in", 3600))

    saved["expires_at"] = now + expires_in
    saved["client_id"] = args.client_id
    saved["tenant"] = args.tenant
    saved["oauth_version"] = args.oauth_version
    saved["servertype"] = args.servertype
    if args.oauth_version == "v1":
        saved["resource"] = args.resource
    else:
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
    if token.get("tenant") and token.get("tenant") != args.tenant:
        return False
    if (token.get("oauth_version") and
            token.get("oauth_version") != args.oauth_version):
        return False
    if token.get("servertype") and token.get("servertype") != args.servertype:
        return False
    if args.oauth_version == "v1":
        return not token.get("resource") or token.get("resource") == args.resource
    return not token.get("scope") or token.get("scope") == args.scope


def print_oauth_hint(payload):
    description = payload.get("error_description", "")

    if "AADSTS70002" in description:
        print(
            "hint=AADSTS70002 usually means the app registration is not "
            "enabled as a public/mobile client. In Microsoft Entra, open "
            "App registrations > Authentication, add or enable a Mobile and "
            "desktop applications/public client platform, and enable "
            "Allow public client flows.",
            file=sys.stderr,
        )
    if "AADSTS70011" in description:
        print(
            "hint=AADSTS70011 means Microsoft rejected the requested scope. "
            "For Outlook.com ActiveSync, try "
            "https://outlook.office.com/EAS.AccessAsUser.All offline_access. "
            "For Microsoft 365 work or school accounts, add the delegated "
            "Office 365 Exchange Online permission EAS.AccessAsUser.All to "
            "this app registration, grant consent if required, then try "
            "https://outlook.office.com/.default offline_access.",
            file=sys.stderr,
        )


def anchor_mailbox_cookie(args, login):
    try:
        host = urllib.parse.urlparse(args.activesync_url).hostname
    except Exception:
        return None
    if host != "eas.outlook.com" or not login:
        return None
    return "DefaultAnchorMailbox=%s" % urllib.parse.quote(login, safe="@._~-")


def probe_needs_anchor_mailbox(args):
    try:
        host = urllib.parse.urlparse(args.activesync_url).hostname
    except Exception:
        return False
    return host == "eas.outlook.com"


def probe_activesync(args, access_token, login):
    headers = {
        "Authorization": "Bearer %s" % access_token,
        "MS-ASProtocolVersion": args.protocol_version,
        "User-Agent": "libEtPan ActiveSync OAuth test",
        "Connection": "close",
    }
    cookie = anchor_mailbox_cookie(args, login)
    if cookie:
        headers["Cookie"] = cookie

    request = urllib.request.Request(
        args.activesync_url,
        headers=headers,
        method="OPTIONS",
    )

    try:
        with urllib.request.urlopen(request, timeout=args.http_timeout) as response:
            body = response.read()
            return {
                "status": response.status,
                "reason": response.reason,
                "headers": dict(response.headers.items()),
                "body_len": len(body),
            }
    except urllib.error.HTTPError as exc:
        body = exc.read()
        return {
            "status": exc.code,
            "reason": exc.reason,
            "headers": dict(exc.headers.items()),
            "body_len": len(body),
        }


def header_value(headers, name):
    wanted = name.lower()
    for key, value in headers.items():
        if key.lower() == wanted:
            return value
    return None


def parse_args(argv):
    parser = argparse.ArgumentParser(
        description="Get a Microsoft OAuth token with device-code flow for "
        "local ActiveSync testing."
    )
    parser.add_argument("--config", default=DEFAULT_CONFIG_FILE,
        help="JSON config file. Use an empty value to disable config loading.")
    parser.add_argument("--client-id",
        help="Microsoft Entra application client ID.")
    parser.add_argument("--tenant",
        help="Tenant for login.microsoftonline.com; common, consumers, "
        "organizations, tenant domain, or tenant ID.")
    parser.add_argument("--login",
        help="Mailbox email address for Outlook ActiveSync routing.")
    parser.add_argument("--oauth-version", choices=("v1", "v2"),
        help="Use the v1 resource-based or v2 scope-based OAuth endpoints.")
    parser.add_argument("--servertype", choices=("office365", "personal-ms"),
        help="Account type used to select the default v2 scope.")
    parser.add_argument("--scope",
        help="Space-separated scopes to request with --oauth-version v2.")
    parser.add_argument("--resource",
        help="Resource URI to request with --oauth-version v1.")
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
    parser.add_argument("--probe-activesync", action="store_true",
        help="Send an ActiveSync OPTIONS request with the access token.")
    parser.add_argument("--activesync-url",
        help="ActiveSync endpoint for --probe-activesync.")
    parser.add_argument("--protocol-version", default="16.1",
        help="MS-ASProtocolVersion value for --probe-activesync.")
    parser.add_argument("--http-timeout", type=int, default=30,
        help="HTTP timeout in seconds.")
    args = parser.parse_args(argv)
    load_config(args)
    return args


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
    if args.tenant is None:
        args.tenant = config.get("tenant", DEFAULT_TENANT)
    if args.login is None:
        args.login = config.get("login")
    if args.oauth_version is None:
        args.oauth_version = config.get("oauth_version", DEFAULT_OAUTH_VERSION)
    if args.servertype is None:
        args.servertype = config.get("servertype", DEFAULT_SERVERTYPE)
    if args.scope is None:
        args.scope = config.get("scope",
            SCOPE_BY_SERVERTYPE[args.servertype])
    if args.resource is None:
        args.resource = config.get("resource", DEFAULT_RESOURCE)
    if args.token_json is None:
        args.token_json = config.get("token_json")
    if args.activesync_url is None:
        args.activesync_url = config.get("activesync_url",
            DEFAULT_ACTIVESYNC_URL)

    if args.client_id is None:
        config_label = args.config if args.config else "the local config file"
        raise SystemExit(
            "error: --client-id is required unless client_id is set in %s" %
            config_label)


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
            message = device_code.get("message")
            if message:
                print(message)
            else:
                print("Open %s and enter code %s" % (
                    device_code.get("verification_uri",
                        "https://microsoft.com/devicelogin"),
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
    print("oauth_version=%s" % args.oauth_version)
    print("servertype=%s" % args.servertype)
    if args.oauth_version == "v1":
        print("resource=%s" % args.resource)
    print("scope=%s" % token.get("scope", ""))
    if token.get("expires_at"):
        print("expires_at=%s" % token.get("expires_at"))
    print("expires_in=%s" % token.get("expires_in", ""))
    if token.get("refresh_token"):
        print("refresh_token=<present>")

    if args.probe_activesync:
        probe_login = args.login or token_payload_email(token)
        if anchor_mailbox_cookie(args, probe_login):
            print("ActiveSync probe sent DefaultAnchorMailbox for %s" %
                probe_login)
        elif probe_needs_anchor_mailbox(args):
            print("warning: eas.outlook.com probe has no --login, so no "
                "DefaultAnchorMailbox cookie was sent", file=sys.stderr)
        probe = probe_activesync(args, access_token, probe_login)
        print("ActiveSync OPTIONS status=%s reason=%s body_len=%s" % (
            probe["status"], probe["reason"], probe["body_len"],
        ))
        for name in (
            "MS-ASProtocolVersions",
            "MS-ASProtocolCommands",
            "X-MS-Location",
            "Location",
            "WWW-Authenticate",
        ):
            value = header_value(probe["headers"], name)
            if value is not None:
                print("%s=%s" % (name, value))
        if probe["status"] == 451:
            location = header_value(probe["headers"], "X-MS-Location")
            if location is not None:
                print("ActiveSync redirect endpoint=%s" % location)
            else:
                print("ActiveSync 451 did not include X-MS-Location",
                    file=sys.stderr)

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
