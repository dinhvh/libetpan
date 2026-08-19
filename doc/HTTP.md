# HTTP transports

libEtPan's HTTP-based protocols share the `mailhttp` request, response, and
transport API. The built-in drivers are libcurl and, on Apple platforms,
`NSURLSession`.

## Backend selection

Use these functions during process startup:

```c
mailhttp_backend_is_available(MAILHTTP_BACKEND_CURL);
mailhttp_set_backend(MAILHTTP_BACKEND_CURL);
mailhttp_get_backend();
```

Apple builds select `MAILHTTP_BACKEND_NSURLSESSION` by default. Other builds
select curl by default. `mailhttp_set_backend()` returns `-1` and leaves the
selection unchanged when the requested backend was not compiled.

The selection controls subsequently created transports. Existing sessions and
transports retain their original driver, so applications should set the
process-wide default before starting worker threads or creating protocol
sessions.

## Custom transports

`mailhttp_transport` is a C driver object containing backend data and
`perform`/`free` callbacks. A transport owns its backend data and is released
with `mailhttp_transport_free()`.

JMAP and ActiveSync retain their existing protocol transport setters. Gmail
accepts a common transport through `mailgmail_set_http_transport()`, which
takes ownership whether wrapping succeeds or fails. Feed accepts one through
`newsfeed_set_http_transport()` and takes ownership on success.

Requests and responses are binary-safe. A request can either accumulate its
response body or install a body-sink callback for streaming consumers such as
the feed parser.

## Security

Both built-in drivers verify TLS certificates and hostnames by default. The
shared API does not provide a global insecure mode.
