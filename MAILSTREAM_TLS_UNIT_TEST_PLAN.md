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

## Future Follow-Up

Add a STARTTLS-specific fixture once direct TLS round-trip coverage is stable.
That fixture should minimally script IMAP, POP3, and SMTP greetings and STARTTLS
commands, then upgrade the same connection to TLS. The assertions should focus
on libetpan low-stream replacement and post-upgrade read/write behavior.

## Non-Goals

- No live network services.
- No full OpenSSL/GnuTLS/CFStream cross-product matrix.
- No certificate authority or platform trust policy validation.
- No attempt to diagnose TLS library protocol compatibility.
