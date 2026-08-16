# Public Header Tooling Plan

## Goal

Create one Python tool that deterministically audits libEtPan headers, stores
the resulting public classification in local Automake declarations, validates
that classification, and provides stable output for the umbrella and platform
packaging systems.

The core definition is:

> `etpaninclude_HEADERS` is the canonical stored public set. Audit evidence
> consists of `LIBETPAN_EXPORT` annotations, public-header dependencies, and
> local no-export public declarations.

This keeps ownership and overrides beside each subsystem. `audit` proposes or
applies classification changes; `check` verifies that the stored result and
all derived artifacts are consistent.

## Proposed files

```text
tools/public_headers.py
tests/public-headers/
```

Use `tools/` because this script is primarily a maintainer, validation, and
packaging tool. Other reasonable names would be `scripts/` or `maintainer/`;
`tools/` is preferred because the script both inspects and updates artifacts
rather than merely wrapping one build.

There is no separate umbrella or extras manifest. Every header stored in
`etpaninclude_HEADERS` must be referenced by `libetpan.h`.

Public headers that intentionally contain no export annotation are protected
locally in their owning `Makefile.am`, for example:

```make
etpanpublic_noexport = \
    libetpan.h \
    libetpan_version.h
```

## Discovery algorithm

1. Locate the repository root from the script path, with an optional
   `--source-root` override.
2. Find the repository's distributed `Makefile.am` files in stable path order.
3. Parse their `etpaninclude_HEADERS`, `noinst_HEADERS`, and
   `etpanpublic_noexport` assignments and resolve each entry relative
   to the owning `Makefile.am`.
4. Scan headers for supported `LIBETPAN_EXPORT` declarations.
5. Separately traverse `src/main/libetpan.h` by parsing lexical includes of
   the exact form:

   ```c
   #include <libetpan/example.h>
   ```

6. Follow those includes recursively, regardless of preprocessor conditions.
   A conditional include such as `mailjmap.h` remains part of the declared API
   on every host.
7. Resolve each public basename uniquely under approved source roots. Do not
   search generated packaging directories such as `build-spm/include`.
8. Confirm these generated headers are declared by Automake and resolve their
   selected build-tree representations explicitly:

   ```text
   libetpan-config.h
   libetpan_version.h
   ```

9. Detect include cycles using a visited set.
10. Classify headers from export evidence, dependency closure, local no-export
    protection, and the stored Automake result.
11. Verify that `libetpan.h` references every header in the resulting public
    set and no header outside it.
12. Emit results in bytewise (`LC_ALL=C`) basename order.

The parser does not need to evaluate the C preprocessor. It only needs to
recognize project-qualified include directives. Comments, whitespace, CRLF
input, and non-UTF-8 legacy source bytes must be handled deterministically.

## Failure conditions

Discovery must fail with an actionable diagnostic when:

- A referenced `libetpan/*.h` header cannot be found.
- A public basename resolves to more than one source header.
- A resolved source escapes the approved source roots.
- An Automake public-header entry is missing or duplicated.
- A generated required header has no selected platform representation.
- The include graph contains malformed project-qualified includes.

Cycles are valid and should not fail; they should terminate through the
visited set.

## Command-line interface

Initial commands:

```sh
python3 tools/public_headers.py list
python3 tools/public_headers.py graph
python3 tools/public_headers.py audit
python3 tools/public_headers.py audit --write
python3 tools/public_headers.py audit --write --force
python3 tools/public_headers.py check
python3 tools/public_headers.py export-spm
python3 tools/public_headers.py generate-windows-list
python3 tools/public_headers.py export-android
python3 tools/public_headers.py export-android --destination PATH
```

Useful options:

```text
--source-root PATH
--build-root PATH
--umbrella PATH
--format names|source-paths|json
--include-dir PATH
--destination PATH
--output PATH
--write
--force
```

`audit` and `check` use the same discovery, parsing, include-graph, export
scanning, classification, and validation implementation. `audit` renders the
classification and proposed changes. `check` converts discrepancies into
errors and never writes files.

`audit --write` applies safe promotions and mechanical normalization but stops
on ambiguous demotions or conflicts. `audit --write --force` also applies the
complete deterministic promotion and demotion result. `--force` is invalid
without both `audit` and `--write`.

`list` writes only data to standard output. Diagnostics go to standard error.
This allows generated files to be updated with ordinary redirection.

By default, `list` emits public basenames only, not source paths grouped by
their directories. For example:

```text
acl.h
acl_types.h
mailimap.h
```

Use `--format source-paths` when the caller needs repository-relative source
locations such as `src/low-level/imap/mailimap.h`. All formats use one global
bytewise sort, so filesystem traversal order never affects output.

## Synchronizing `libetpan.h`

Every writing audit rewrites the marked generated include region in
`src/main/libetpan.h` from the final stored public classification. It preserves
the copyright, include guard, C++ linkage, and any handwritten content outside
that region.

The generated region references every header in the normalized union of
`etpaninclude_HEADERS`. Headers are grouped deterministically by their owning
source directory and then sorted by public basename. The umbrella itself is
not included recursively.

Feature conditions must also remain local to the owning `Makefile.am`. When a
public header requires a guard, its local Automake metadata supplies the exact
preprocessor condition used by the umbrella generator; no central umbrella
manifest duplicates that information. The initial implementation must
document and test the local metadata syntax before rewriting the existing
conditional JMAP include.

`audit --write` and `audit --write --force` update the umbrella atomically with
their `Makefile.am` changes and print a concise summary of added and removed
public-header references. `check` generates the expected umbrella in memory
and fails when the checked-in file differs, providing the read-only CI path.

## Preparing the root include directory

The Python tool does not materialize `include/libetpan`. Automake retains
ownership of the root symlink farm and rebuilds it with:

```sh
make stamp-prepare-target
```

For an out-of-tree build:

```sh
make -C BUILD_ROOT stamp-prepare-target
```

The existing target cleans the generated `include/libetpan` entries before
recreating them, so callers must not perform a separate manual clean.

After preparation, validate the result without modifying it:

```sh
python3 tools/public_headers.py check \
    --include-dir include/libetpan
```

The check fails if a header is missing, stale, broken, duplicated, linked to
the wrong source, or inconsistent with the authoritative
`etpaninclude_HEADERS` union. This avoids duplicating Automake's symlink-farm
implementation in Python while still enforcing one public-header set.

## Stable output contract

The exact name-format output is the static union of all distributed
`etpaninclude_HEADERS` declarations, independent of the current configure
feature selection. After reviewing the initial audit result, the stored set
contains 146 headers. Tests fixture the
reviewed result while the implementation always computes it from the local
Automake declarations.

The initial migration promoted these annotation-backed headers:

```text
mailimap_parser.h
mailimap_parser_hack.h
```

It demoted these headers to their local `noinst_HEADERS` declarations:

```text
imapdriver_tools.h
imapdriver_tools_private.h
mail.h
mailactivesync_codes.h
mailactivesync_wbxml.h
mailstream_compress.h
namespace_parser.h
namespace_sender.h
quota_parser.h
quota_sender.h
```

The generated umbrella now references all 180 reviewed public headers, with
the JMAP group retaining its feature condition.

## Initial public API decisions

Review the orphan headers using these signals:

- `LIBETPAN_EXPORT` declarations
- Inclusion by another public header
- Use by known consumers such as MailCore 2
- Whether the header exposes protocol API, implementation machinery, or only
  compatibility macros

The initial migration applied these decisions:

- Keep `clientid.h` public.
- Stop installing the unannotated parser, sender, and internal driver-tool
  headers listed above.
- Move `mail.h` to `noinst_HEADERS` rather than exposing its generic `TRUE`
  and `FALSE` macros through the umbrella.
- Keep `mailengine.h` public and add `LIBETPAN_EXPORT` to its supported public
  declarations. Guard and export `engine_app` consistently with its
  `DEBUG_ENGINE`-only definition.
- Review concrete storage-driver headers separately. MailCore 2 does not use
  their driver variables, while it does use the public
  `data_message_driver.h` API.

The repository currently defines `LIBETPAN_EXPORT`; there is no
`LIBETPAN_EXTERN` macro. Public variable declarations should use the existing
visibility macro unless a separate macro is intentionally introduced for
semantic clarity.

## Audit modes and classification policy

`audit` is read-only and reports only actions: headers to promote or demote,
the owning `Makefile.am` variable edits, umbrella includes to add or remove,
required umbrella regeneration, and conflicts. It does not print unchanged
public or internal headers. Each proposed action includes its evidence.

The deterministic policy is:

1. A header containing supported `LIBETPAN_EXPORT` declarations is public.
2. A header required by a public header is a public dependency.
3. A header in local `etpanpublic_noexport` is public even when it has
   no export annotation.
4. A header matching none of those conditions is an internal candidate.
5. Existing `etpaninclude_HEADERS` entries that conflict with the evidence are
   reported rather than silently accepted.

`audit --write` may:

- Promote unambiguous exported headers.
- Promote dependencies required by public headers.
- Remove promoted headers from `noinst_HEADERS`.
- Normalize and deduplicate affected header variables.
- Regenerate the public include block in `libetpan.h`.

It stops before demoting a header or resolving conflicting evidence.

`audit --write --force` additionally:

- Demotes headers classified internal from `etpaninclude_HEADERS` to the local
  `noinst_HEADERS`.
- Applies all deterministic promotions and demotions atomically.
- Regenerates `libetpan.h` from the final stored public set.

Forced mode does not bypass malformed Makefiles, missing files, ambiguous
basenames, path containment, parse failures, or filesystem safety checks. It
only authorizes API-surface changes that non-forced writing refuses to make.
Before writing, it prints counts and paths for promotions, demotions,
`Makefile.am` updates, and umbrella changes. On failure, no file is changed.

## Validation modes

`check` should support two related validations.

### Umbrella validation

- Every project-qualified include resolves uniquely.
- Every generated required header is represented.
- Every Automake-declared public header exists and resolves uniquely.
- The calculated result matches a checked-in expected-output fixture when one
  is requested.

### Installation validation

Given `--include-dir include/libetpan`:

- Every calculated public header is present.
- No unapproved extra header is present.
- Every symlink is valid and resolves inside the source or build tree.
- No duplicate public basename exists.
- No stale header remains from a previous configuration.

During migration, unexpected extras and feature-dependent omissions can be
reported without failing. CI should switch to strict equality after the API
decisions above are complete.

## Build-system integration

### Autotools

Add a maintainer target that runs after `stamp-prepare`:

```make
public-headers-check: stamp-prepare
	$(PYTHON) $(top_srcdir)/tools/public_headers.py check \
	    --include-dir $(top_builddir)/include/libetpan
```

Ordinary compilation must not require Python. Python is a maintainer, release,
and CI dependency.

Use the combined `etpaninclude_HEADERS` declarations as the authoritative
cross-platform set. Validate every umbrella reference and every materialized
or packaged destination against that set.

### Validating and updating `Makefile.am` files

The normal `check` command parses all relevant local header variables and
validates the canonical stored result. It reports:

- Public headers missing from Automake installation declarations
- Automake-installed headers outside the public set
- Duplicate declarations
- Ambiguous basenames
- The exact `Makefile.am` that owns each declaration

It also validates the umbrella include graph and, when `--include-dir` is
given, the materialized header directory. It shares its analyzer with
`audit`, but remains strictly read-only.

Writing audit modes edit only the recognized header-variable blocks in the
owning `Makefile.am`. They preserve unrelated variables, comments, and source
lists; use stable indentation and ordering; and refuse files whose syntax
cannot be safely round-tripped.

If eliminating the duplicated declarations is desirable later, prefer
generating one complete file such as:

```text
include/public-headers.am
```

from the authoritative header declarations and including that file from the
build.
The generated fragment must be checked in for release tarballs and verified by
the normal `check` command. Before adopting this model, confirm that header
installation paths, distribution rules, VPATH builds, and per-directory
Automake semantics remain correct.

### Swift Package Manager

Replace the recursive discovery logic in
`build-spm/update-public-headers.sh` with the Python exporter. The exporter
must clean stale headers, copy real files rather than symlinks, and verify each
copy.

Keep the shell script temporarily as a compatibility wrapper.

### Windows

Generate `build-windows/build_headers.list` from the same graph. Substitute:

```text
build-windows/libetpan-config.h
build-windows/libetpan_version.h
```

and add `src/windows/win_etpan.h` as an explicit platform-only header. Emit
Windows separators and stable ordering.

### Android

Replace broad `find ../src -name "*.h"` copying with an export of the exact
public set. If Android compilation needs private build headers, stage them in a
separate internal directory rather than packaging them as public headers.

`export-android` does not require a destination argument for the normal
repository workflow. Its default is:

```text
build-android/include/libetpan
```

The optional `--destination` exists for tests, out-of-tree builds, and Android
release-package staging. A positional destination is avoided because it makes
the ordinary command unnecessarily noisy and makes accidental exports to the
wrong directory easier.

### Xcode

Xcode can continue copying `include/libetpan` wholesale after strict
installation validation guarantees that directory contains exactly the public
set.

## Tests

Add unit tests using temporary fixture trees for:

- Simple and transitive includes
- Conditional includes
- Include cycles
- Missing headers
- Ambiguous basenames
- Duplicate Automake public-header declarations
- CRLF files
- Legacy non-UTF-8 bytes
- Comments and whitespace variants
- Stable bytewise ordering
- Generated-header substitution
- Windows path output
- Broken and escaping symlinks
- Missing and stale installed headers
- Destination cleanup and verified copying

Add an integration test that calculates the real repository set and compares
it with:

- `include/libetpan` after `make prepare`
- `build-spm/include/libetpan`
- `build-windows/build_headers.list`

## Rollout

### Change 1: read-only discovery

- Add the Python script and tests.
- Add `list`, `graph`, read-only `audit`, and non-strict `check` modes.
- Record and review current discrepancies.
- Do not change packaged headers yet.

### Change 2: API cleanup

- Add tested `audit --write` and `audit --write --force` modes.
- Add local `etpanpublic_noexport` protection where needed.
- Apply and review the proposed public-header classification.
- Correct public visibility annotations, including `mailengine.h`.
- Resolve JMAP generation across configurations.

### Change 3: shared generation

- Switch SwiftPM, Windows, and Android to the Python tool.
- Enable strict equality in CI.
- Retain compatibility wrappers where external workflows may call them.

### Change 4: remove obsolete discovery

- Delete duplicated recursive shell discovery.
- Remove temporary migration allowances.
- Document the rule for adding a new public header.

## Success criteria

- One shared analyzer computes public-header evidence, and local Automake
  declarations store the reviewed canonical result.
- Every supported public header is referenced by `libetpan.h`.
- Private headers are never installed or packaged.
- Conditional public APIs such as JMAP are discovered independently of the
  host configuration.
- Autotools, SwiftPM, Windows, Android, and Xcode ship the same cross-platform
  public set, plus documented platform-only headers.
- CI detects missing, extra, ambiguous, broken, or stale headers.
- Promoting or demoting a header updates only its local `Makefile.am`; all
  umbrella and platform artifacts derive from the stored result.
