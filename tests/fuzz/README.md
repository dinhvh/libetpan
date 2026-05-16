# libetpan IMAP response parser — libFuzzer harness

A coverage-guided fuzz harness for `mailimap_response_parse()`, the
top-level entry point for parsing server-side IMAP responses in
libetpan. Runs under LLVM's libFuzzer with AddressSanitizer and
LeakSanitizer to surface memory-safety and resource defects in the
IMAP parser.

This harness is what surfaced the four parser defects fixed in May 2026
(see ChangeLog for `Fix PERMANENTFLAGS cleanup null dereference`,
`Fix section parse cleanup leak`, `Limit IMAP literal allocation size`,
and the two ANNOTATEMORE cleanup commits) and the fifth leak fixed
in the same series.

## Dependencies

- `clang` with libFuzzer and AddressSanitizer support (Clang 6.0+)
- `libtool`, `autoconf`, `automake` for the libetpan build
- A few hundred MB of disk space for the fuzzer's input corpus

On Debian / Ubuntu:

```
sudo apt install clang autotools-dev libtool autoconf automake pkg-config
```

## Build

Build libetpan with AddressSanitizer instrumentation first:

```
cd <libetpan-root>
./autogen.sh
CC=clang CXX=clang++ ./configure \
    CFLAGS="-g -O1 -fsanitize=address" \
    CXXFLAGS="-g -O1 -fsanitize=address" \
    LDFLAGS="-fsanitize=address"
make -j$(nproc)
```

The static lib lands at `src/.libs/libetpan.a` (~9 MB with debug
symbols + ASan instrumentation).

Then build the harness against the instrumented lib:

```
clang -g -O1 -fsanitize=address,fuzzer \
    -I . -I .include -I src/data-types -I src/low-level/imap -I src/main \
    tests/fuzz/fuzz_imap_response.c src/.libs/libetpan.a \
    -lpthread -lz \
    -o fuzz_imap_response
```

`-I .include` is the libetpan-internal staged include namespace (so
`<libetpan/foo.h>`-style includes from within the parser resolve
correctly). `-I .` picks up the autotools-generated `config.h`.

## Run

```
mkdir -p corpus
./fuzz_imap_response -dict=tests/fuzz/imap.dict tests/fuzz/seeds/ corpus/
```

libFuzzer will:

- Read the seed responses from `tests/fuzz/seeds/`
- Mix in mutations guided by the IMAP keyword dictionary
- Save interesting (newly-covering) inputs into `corpus/`
- Write any crash/leak inputs to the current directory as `crash-*` or
  `leak-*` artifact files

To replay a specific input (e.g. a corpus member or a crash artifact)
without continuing to fuzz:

```
./fuzz_imap_response path/to/input.bin
```

Useful environment variables:

- `ASAN_OPTIONS=detect_leaks=1` — confirm LeakSanitizer is active
- `ASAN_OPTIONS=halt_on_error=1` — stop on first error
- `LLVM_PROFILE_FILE=...` if also collecting coverage profile data

## Files

- `fuzz_imap_response.c` — the harness
- `imap.dict` — keyword dictionary covering the major IMAP tokens
  (commands, responses, server-side flags, namespace tokens). Used by
  libFuzzer's grammar-aware mutation
- `seeds/` — a small set of hand-crafted valid IMAP responses covering
  different response shapes (untagged FETCH, server greeting, LIST,
  STATUS, BYE, tagged completion). Recommended starting corpus
- `README.md` — this file

## Reporting bugs found via this harness

If you find a security-relevant defect via this harness, please follow
the existing libetpan disclosure pattern: email the maintainer at
`hoa at dinhvh dot me` with a minimized reproducer and an analysis of
the affected code path. Attach the binary input that triggers it
(libFuzzer's `-minimize_crash` option will tighten an oversized
trigger). Please coordinate before public disclosure.
