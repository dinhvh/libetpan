# Mailstream TLS Unit Test Plan

## Goal

Catch libetpan-specific regressions in the mailstream TLS client paths without
testing external mail servers or trying to certify OpenSSL, GnuTLS, or
CFStream themselves.

## Scope

- Use one local, known-good OpenSSL TLS server fixture.
- Exercise each libetpan client backend that is compiled into the current
  build.
- Keep deterministic test sources under `unittest/`.
- Wire runnable automake test programs through `tests/Makefile.am`.

## Initial Test

Add `unittest/mailstream-ssl/backend_roundtrip_test.c`.

The test:

- starts a local TLS echo server on loopback with an embedded self-signed
  certificate and private key;
- selects one libetpan backend at a time with `mailstream_ssl_set_backend()`;
- connects through libetpan;
- verifies that the TLS callback is invoked;
- writes `ping`;
- reads `pong`;
- fetches the peer certificate when the backend exposes it;
- closes the stream cleanly.

The first implementation uses `mailstream_ssl_connect_timeout()` so the same
test can cover socket OpenSSL, socket GnuTLS, and CFStream direct-TLS client
paths. The server remains OpenSSL-only by design; changing the server backend
would mostly test TLS library interoperability rather than libetpan behavior.

## STARTTLS Follow-Up

The round-trip fixture also scripts IMAP, POP3, and SMTP greetings and STARTTLS
commands, then upgrades the same connection to TLS. For each compiled socket
backend it verifies the TLS callback, low-stream replacement, and a protocol
NOOP request and response after the upgrade. CFNetwork remains covered by the
direct-TLS case; its in-place STARTTLS path does not use the socket low-stream
replacement exercised here.

## Non-Goals

- No live network services.
- No full OpenSSL/GnuTLS/CFStream cross-product matrix.
- No certificate authority or platform trust policy validation.
- No attempt to diagnose TLS library protocol compatibility.
