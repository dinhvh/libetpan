# Bundled ICU charset resources

The optional single-byte tables are from unicode-org/icu-data revision
`bae5ca647fa41703393dbf00240ec1b807a9c353`, under the included Unicode license.
The source tables live in the pinned `dependencies/submodules/icu-data`
submodule; they are not copied into this directory. `manifest.json` records the selected
91 glibc groups (90 unique tables), their public names, and alias comparisons.
Each selected table was compared with glibc 2.43 for all 256 byte values and
encoding of every decoded character. ARMSCII-8 and tables with unreviewed
mapping differences are deliberately excluded.

`data.h` embeds a private ICU application data package, generated with ICU 78.2.
It contains both ASCII little-endian and big-endian images. Normal builds need
no ICU data-generation tools, files installed on the target, downloads, or
`ICU_DATA` environment override. Runtime uses `udata_setAppData` and
`ucnv_openPackage`; the application's system ICU data and global search paths
are not changed. ICU validates the converter data format when opening it.

Initialize the source dependencies with
`git submodule update --init dependencies/submodules/icu dependencies/submodules/icu-data`.
ICU is pinned to release 78.2, matching the current generation baseline. Android
builds its static common library and converter data through
`build-android/dependencies/icu/build.sh`; Linux uses system ICU.

Apple builds without iconv use CoreFoundation for general conversion. Its two
remaining email fixture gaps, VISCII and KOI8-T, use native UTF-8 conversion pairs
from `apple-legacy.h`, generated from the same reviewed UCM sources. Undefined
input bytes and malformed UTF-8 return conversion errors; unmappable output
characters become `?`. This fallback does not require linking ICU.

Regenerate with `python3 tests/generate-charconv-icu-data.py` from the repository
root, using the ICU tools matching the version recorded in the manifest. The
generator reads each table's `upstream_path` directly from `icu-data` and checks
that its checkout matches the reviewed revision. Source distribution builds use
the embedded header; regenerating it requires a Git checkout with the submodule.
The generation tools are maintainer requirements, not build dependencies.
After updating data or aliases, regenerate and review the literal fixtures with
`python3 tests/generate-charconv-charset-fixtures.py`, then run all three backend
configurations. Expected bytes come from standalone provider APIs, never from
libetpan at normal test runtime.

`aliases.h` supplies 109 exact spellings verified against current glibc in both
directions. It also routes 353 exact names to the selected optional tables.
The table also pins the 28 ambiguous ICU names to their recorded canonical
converters. Only the ICU path uses this table; dual-backend routing continues to prefer ICU
for the previously selected Japanese source encodings and iconv otherwise.
Source normalization already provided by libetpan is applied before routing.
The 75 original alias candidates with mapping/validity differences remain
unsupported under their rejected spelling; their comparison evidence is in
`manifest.json`. No broad punctuation normalization is applied.

The complete 1,180-name fixture inventory currently records ICU-only support
for 1,015 names in both directions and unknown-charset errors for 165 names.
These are names/aliases, not counts of distinct encodings. Common UTF-8 pairs
for UTF-7-IMAP use libetpan's native modified UTF-7 implementation independently
of ICU/iconv. UCS-2BE/LE reject surrogate code units and supplementary output;
UCS-4BE/LE use explicit no-BOM UTF-32. WCHAR_T is bridged only for four-byte
native wchar_t. ICU retains its substitution policy for malformed/unmappable
input, while iconv retains its existing replacement behavior.

Native Unicode and WCHAR_T fixtures record little-endian Linux provider behavior.
The unit runner explicitly reports those platform limits on other byte orders
or wchar_t layouts; validation on other architectures/providers remains pending.
