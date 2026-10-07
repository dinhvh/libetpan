#!/usr/bin/env python3
"""Capture deterministic charset fixtures from glibc iconv and standalone ICU.

Manual provider-comparison tool. Normal unit tests use literal generated data,
without loading this script, running iconv, or deriving their own expectations.
Requires Linux glibc and ICU 78.2 matching the recorded inventory baseline.
"""
import ctypes as C
import ctypes.util
import json
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / 'unittest/charconv'
DATA = ROOT / 'src/data-types/charconv-icu'
I32 = C.c_int32
libc = C.CDLL(ctypes.util.find_library('c'), use_errno=True)
libc.iconv_open.argtypes = [C.c_char_p, C.c_char_p]
libc.iconv_open.restype = C.c_void_p
libc.iconv.argtypes = [C.c_void_p, C.POINTER(C.c_char_p), C.POINTER(C.c_size_t),
                      C.POINTER(C.c_char_p), C.POINTER(C.c_size_t)]
libc.iconv.restype = C.c_size_t
libc.iconv_close.argtypes = [C.c_void_p]
BAD = C.c_size_t(-1).value
version = subprocess.check_output(['pkg-config', '--modversion', 'icu-uc'], text=True).strip()
icu = C.CDLL(ctypes.util.find_library('icuuc'))


def fn(name, args, result):
    f = getattr(icu, name + '_' + version.split('.')[0])
    f.argtypes, f.restype = args, result
    return f


op = fn('ucnv_open', [C.c_char_p, C.POINTER(I32)], C.c_void_p)
opkg = fn('ucnv_openPackage', [C.c_char_p, C.c_char_p, C.POINTER(I32)], C.c_void_p)
close = fn('ucnv_close', [C.c_void_p], None)
get_name = fn('ucnv_getName', [C.c_void_p, C.POINTER(I32)], C.c_char_p)
to_u = fn('ucnv_toUChars', [C.c_void_p, C.c_void_p, I32, C.c_char_p, I32, C.POINTER(I32)], I32)
from_u = fn('ucnv_fromUChars', [C.c_void_p, C.c_void_p, I32, C.c_void_p, I32, C.POINTER(I32)], I32)
setdata = fn('udata_setAppData', [C.c_char_p, C.c_void_p, C.POINTER(I32)], None)
# Runtime conversion is deliberately independent of libetpan. Register its
# reviewed input data with ICU's own public API rather than calling charconv.
section = (DATA / 'data.h').read_text().split('#else')[0 if sys.byteorder == 'big' else 1]
blob = C.create_string_buffer(bytes(int(x, 16) for x in re.findall(r'0x([0-9a-f]{2})', section)))
err = I32(0)
setdata(b'libetpan_charsets', blob, C.byref(err))
assert err.value <= 0
aliases = {}
manifest = json.loads((DATA / 'manifest.json').read_text())
for row in manifest['verified_aliases']:
    for name in row['names']:
        aliases[name.lower()] = (row['target'], False)
for row in manifest['tables']:
    for name in row['names']:
        aliases[name.lower()] = (Path(row['file']).stem, True)
for row in manifest['ambiguous_aliases']:
    aliases[row['name'].lower()] = (row['target'], False)
for name, target in [('UCS-2BE', 'UTF-16BE'), ('UCS-2LE', 'UTF-16LE'),
                     ('UCS-4BE', 'UTF-32BE'), ('UCS-4LE', 'UTF-32LE'),
                     ('WCHAR_T', 'UTF-32BE' if sys.byteorder == 'big' else 'UTF-32LE'),
                     ('UTF-7-IMAP', 'IMAP-mailbox-name')]:
    aliases[name.lower()] = (target, False)


def normalized_source(name):
    return {'gb2312': 'GBK', 'gb_2312-80': 'GBK', 'iso-2022-jp': 'ISO-2022-JP-2',
            'koi8_r': 'koi8-r', 'ks_c_5601-1987': 'euckr',
            'iso-8859-8-i': 'iso-8859-8', 'iso-8859-8-e': 'iso-8859-8'}.get(name.lower(), name)


def reference(tocode, fromcode, raw, flush=True):
    cd = libc.iconv_open(tocode.encode(), fromcode.encode())
    if cd == BAD:
        return 1, b''
    try:
        inpbuf = C.create_string_buffer(raw)
        inp, left = C.cast(inpbuf, C.c_char_p), C.c_size_t(len(raw))
        buffer = C.create_string_buffer(32768)
        out, available = C.cast(buffer, C.c_char_p), C.c_size_t(len(buffer))
        n = libc.iconv(cd, C.byref(inp), C.byref(left), C.byref(out), C.byref(available))
        if n == BAD or left.value:
            return 3, b''
        if flush and libc.iconv(cd, None, None, C.byref(out), C.byref(available)) == BAD:
            return 3, b''
        return 0, buffer.raw[:len(buffer) - available.value]
    finally:
        libc.iconv_close(cd)


def open_icu(name):
    target, packaged = aliases.get(name.lower(), (name, False))
    err = I32(0)
    cd = opkg(b'libetpan_charsets', target.encode(), C.byref(err)) if packaged else op(target.encode(), C.byref(err))
    return cd, err.value


def icu_reference(tocode, fromcode, raw):
    source, serr = open_icu(fromcode)
    target, terr = open_icu(tocode)
    try:
        if serr > 0 or terr > 0:
            return 1, b''
        unicode = C.create_string_buffer(65536)
        err = I32(0)
        n = to_u(source, unicode, len(unicode)//2, raw, len(raw), C.byref(err))
        if err.value > 0:
            return 3, b''
        if tocode.lower() in ['ucs-2be', 'ucs-2le'] and any(
                0xd800 <= int.from_bytes(unicode.raw[i:i+2], sys.byteorder) <= 0xdfff
                for i in range(0, n*2, 2)):
            return 3, b''
        output = C.create_string_buffer(65536)
        err = I32(0)
        size = from_u(target, output, len(output), unicode, n, C.byref(err))
        return (3, b'') if err.value > 0 else (0, output.raw[:size])
    finally:
        if source:
            close(source)
        if target:
            close(target)


BYTE_VECTORS = {}


def cbytes(raw):
    if raw not in BYTE_VECTORS:
        BYTE_VECTORS[raw] = len(BYTE_VECTORS)
    return '{ CHARSET_BYTES_%d, %d }' % (BYTE_VECTORS[raw], len(raw))



def expectation(value):
    status, raw = value
    return '{ %d, %s }' % (status, cbytes(raw))


def main():
    if sys.byteorder != 'little' or C.sizeof(C.c_wchar) != 4:
        raise RuntimeError('Review native-format fixtures before changing the recorded platform baseline')
    if version != manifest['icu_generation_version']:
        raise RuntimeError('Review and update manifest ICU baseline before refreshing fixtures')
    names = [n.removesuffix('//') for n in subprocess.check_output(['iconv', '--list'], text=True).split()]
    assert len(names) == len(set(names)) == 1180, 'refresh inventory deliberately for another provider'
    provenance = subprocess.check_output(['iconv', '--version'], text=True).splitlines()[0]
    (DEST / 'data').mkdir(exist_ok=True)
    (DEST / 'data/iconv-glibc-charsets.txt').write_text(
        '# ' + provenance + '\n# Captured names: remove only final display //; ICU '+version+'\n' + '\n'.join(names) + '\n')
    groups = json.loads((DATA / 'manifest.json').read_text())
    family = {n: row['group'] for row in groups['tables'] for n in row['names']}
    # These boundary/distinctive characters deliberately span regional repertoires.
    candidates = 'é€ĄČĞŒȘȚΑБאشあ漢가中ấԱა⠁😀〜～￥¥髙纊'
    result = {'singlebyte': [], 'multibyte': []}
    report = []
    for name in names:
        source = normalized_source(name)
        possible = [(ch, reference(name, 'UTF-8', ch.encode())) for ch in candidates]
        represented = [(ch, raw) for ch, (status, raw) in possible if status == 0]
        onebyte = all(len(raw) <= 1 for ch, raw in represented)
        if re.search(r'UTF|UCS|10646|WCHAR|ISO-2022|ISO2022|JISX0213', name.upper()):
            onebyte = False
        generation_notes = []
        if onebyte:
            # Complete valid-byte repertoire, not merely an ASCII availability probe.
            raw = bytes(b for b in range(256) if reference('UTF-8', source, bytes([b]))[0] == 0)
            status, utf8 = reference('UTF-8', source, raw)
            assert status == 0 and raw, name
        else:
            text = 'A\0' + ''.join(ch for ch, _ in represented[:3])
            for ch, _ in represented:
                if ch not in '😀〜～￥¥髙纊':
                    continue
                code, trial = reference(name, 'UTF-8', (text + ch).encode())
                if code == 0 and reference('UTF-8', source, trial)[0] == 0:
                    text += ch
                else:
                    generation_notes.append('Reference could not decode combined sample with U+%04X' % ord(ch))
            status, raw = reference(name, 'UTF-8', text.encode())
            if status != 0:
                text = represented[0][0] if represented else '0'
                status, raw = reference(name, 'UTF-8', text.encode())
            assert status == 0 and raw, name
            status, utf8 = reference('UTF-8', source, raw)
            assert status == 0, name
        # The legacy iconv wrapper does not flush. Record that behavior explicitly;
        # modified UTF-7 now uses the native helper, which does finish its shift.
        native = name.upper() == 'UTF-7-IMAP'
        idec = reference('UTF-8', source, raw, flush=native)
        ienc = reference(name, 'UTF-8', utf8, flush=native)
        udec = icu_reference('UTF-8', source, raw)
        uenc = icu_reference(name, 'UTF-8', utf8)
        dual_icu = source.lower() in ['iso-2022-jp', 'iso-2022-jp-2', 'shift_jis', 'shift-jis', 'euc-jp', 'eucjp']
        platform = int(name == 'WCHAR_T')
        converter, status = open_icu(name)
        if converter:
            err = I32(0)
            canonical = get_name(converter, C.byref(err)).decode()
            if canonical in ['UTF-16', 'UTF-32', 'UTF16_PlatformEndian',
                             'UTF16_OppositeEndian', 'UTF32_PlatformEndian',
                             'UTF32_OppositeEndian']:
                platform |= 2
            close(converter)
        if name.upper().startswith('OSF') and len(reference(name, 'UTF-8', b'A')[1]) in [2, 4]:
            platform |= 2
        note = 'valid-byte repertoire' if onebyte else 'non-ASCII/binary/BOM/shift sample'
        if platform:
            note += '; recorded little-endian native/Unicode provider format'
        if generation_notes:
            note += '; ' + '; '.join(generation_notes)
        if idec != udec or ienc != uenc:
            note += '; explicit ICU mapping/substitution/shift difference or unsupported name'
        line = '  { %s, %s, %s, %s, %s, %s, %s, %s, %s, %d, %d },' % (
            json.dumps(name), json.dumps(family.get(name, source)), json.dumps(note),
            cbytes(raw), cbytes(utf8), expectation(idec), expectation(ienc),
            expectation(udec), expectation(uenc), int(dual_icu), platform)
        result['singlebyte' if onebyte else 'multibyte'].append(line)
        report.append({'name': name, 'group': family.get(name, source), 'icu_decode_status': udec[0],
                       'icu_encode_status': uenc[0], 'iconv_decode_status': idec[0],
                       'iconv_encode_status': ienc[0], 'differences': idec != udec or ienc != uenc, 'reference_notes': generation_notes})
    for kind, lines in result.items():
        content = ['/* Generated by tests/generate-charconv-charset-fixtures.py. */',
                   '/* Reference: '+provenance+'; ICU '+version+'. */',
                   '#include "charset_cases.h"', '#include "charset_case_vectors.h"', '',
                   'const struct charset_case charset_'+kind+'_cases[] = {'] + lines + ['};',
                   'const size_t charset_'+kind+'_count = sizeof(charset_'+kind+'_cases) /',
                   '    sizeof(charset_'+kind+'_cases[0]);', '']
        (DEST / ('charset_'+kind+'_cases.c')).write_text('\n'.join(content))
    vectors = ['/* Generated literal byte vectors; no runtime oracle. */']
    for raw, index in BYTE_VECTORS.items():
        vectors.append('#define CHARSET_BYTES_%d "%s"' % (index, ''.join('\\x%02x' % b for b in raw)))
    (DEST / 'charset_case_vectors.h').write_text('\n'.join(vectors)+'\n')
    (DEST / 'data/provider-comparison.json').write_text(json.dumps(report, indent=2)+'\n')
    print('Generated', {k: len(v) for k, v in result.items()}, 'ICU decode/encode support',
          sum(x['icu_decode_status'] == 0 for x in report), sum(x['icu_encode_status'] == 0 for x in report))


if __name__ == '__main__':
    main()
