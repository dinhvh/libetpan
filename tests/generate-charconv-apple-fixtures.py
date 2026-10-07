#!/usr/bin/env python3
"""Capture Apple charset expectations from SDK iconv and CoreFoundation.

Maintainer tool for macOS. It links standalone provider APIs, never libetpan.
The glibc inventory and test inputs stay unchanged; unsupported Apple names
are recorded explicitly rather than skipped. Normal tests use literal bytes.
"""
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / 'unittest/charconv'

REFERENCE = r'''
#include <CoreFoundation/CoreFoundation.h>
#include <iconv.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "charset_cases.h"

static const char *normalized(const char *s) {
  const char *pairs[][2] = {{"gb2312","GBK"},{"gb_2312-80","GBK"},
    {"iso-2022-jp","ISO-2022-JP-2"},{"koi8_r","koi8-r"},
    {"ks_c_5601-1987","euckr"},{"iso-8859-8-i","iso-8859-8"},
    {"iso-8859-8-e","iso-8859-8"}};
  for (size_t i=0;i<sizeof(pairs)/sizeof(pairs[0]);i++)
    if (!strcasecmp(s,pairs[i][0])) return pairs[i][1];
  return s;
}
static CFStringEncoding encoding(const char *name) {
  CFStringRef s=CFStringCreateWithCString(NULL,name,kCFStringEncodingUTF8);
  if (!s) exit(2);
  CFStringEncoding e=CFStringConvertIANACharSetNameToEncoding(s);
  CFRelease(s); return e;
}
static int reference_cf(const char *to,const char *from,
    struct charset_bytes input,unsigned char **out,size_t *len) {
  CFStringEncoding f=encoding(from),t=encoding(to);
  if(f==kCFStringEncodingInvalidId || t==kCFStringEncodingInvalidId ||
     !CFStringIsEncodingAvailable(f) || !CFStringIsEncodingAvailable(t)) return 1;
  CFDataRef data=CFDataCreate(NULL,(const UInt8 *)input.data,input.length);
  if(!data) exit(2);
  CFStringRef string=CFStringCreateFromExternalRepresentation(NULL,data,f);
  CFRelease(data);
  if(!string) return 3;
  data=CFStringCreateExternalRepresentation(NULL,string,t,'?');
  CFRelease(string);
  if(!data) return 3;
  *len=CFDataGetLength(data); *out=malloc(*len+1);
  if(!*out) exit(2);
  memcpy(*out,CFDataGetBytePtr(data),*len); CFRelease(data); return 0;
}
static int reference_iconv(const char *to,const char *from,
    struct charset_bytes input,unsigned char **out,size_t *len) {
  iconv_t cd=iconv_open(to,from);
  if(cd==(iconv_t)-1) return 1;
  size_t capacity=input.length*6+16,left=input.length,room=capacity;
  char *in=(char *)input.data,*buffer=malloc(capacity),*p=buffer;
  if(!buffer) exit(2);
  for(;;) {
    size_t result=iconv(cd,&in,&left,&p,&room);
    if(left && room && errno==EILSEQ) {
      *p++='?';room--;in++;left--;continue;
    }
    iconv_close(cd);
    /* Preserve the legacy wrapper's partial output on a truncated tail. */
    (void)result;
    *out=(unsigned char *)buffer;*len=capacity-room;return 0;
  }
}
int main(void) {
  size_t total=charset_singlebyte_count+charset_multibyte_count;
  for(size_t i=0;i<total;i++) {
    const struct charset_case *c=i<charset_singlebyte_count ?
      &charset_singlebyte_cases[i] : &charset_multibyte_cases[i-charset_singlebyte_count];
    for(int mode=0;mode<2;mode++) for(int encode=0;encode<2;encode++) {
      const char *from=encode ? "UTF-8" : normalized(c->name);
      const char *to=encode ? c->name : "UTF-8";
      struct charset_bytes input=encode ? c->unicode : c->encoded;
      unsigned char *out=NULL;size_t length=0;int status=1;
      if(!strcasecmp(c->name,"UTF-7-IMAP")) {
        /* The native bridge has existing provider-independent golden bytes. */
        const struct charset_expectation *expected=encode ? &c->iconv_encode : &c->iconv_decode;
        printf("%s\t%d\t%d\t%d\t",c->name,mode,encode,expected->status);
        for(size_t j=0;j<expected->bytes.length;j++)
          printf("%02x",(unsigned char)expected->bytes.data[j]);
        putchar('\t');
        for(size_t j=0;j<input.length;j++) printf("%02x",(unsigned char)input.data[j]);
        puts("");continue;
      }
      CFStringEncoding e=encoding(from);
      int japanese=e==kCFStringEncodingISO_2022_JP || e==kCFStringEncodingISO_2022_JP_1 ||
        e==kCFStringEncodingISO_2022_JP_2 || e==kCFStringEncodingShiftJIS ||
        e==kCFStringEncodingDOSJapanese || e==kCFStringEncodingEUC_JP;
      if(mode || japanese) status=reference_cf(to,from,input,&out,&length);
      if(!mode && status==1) status=reference_iconv(to,from,input,&out,&length);
      printf("%s\t%d\t%d\t%d\t",c->name,mode,encode,status);
      for(size_t j=0;j<length;j++) printf("%02x",out[j]);
      putchar('\t');
      for(size_t j=0;j<input.length;j++) printf("%02x",(unsigned char)input.data[j]);
      puts("");free(out);
    }
  }
  return 0;
}
'''


def main():
    if sys.platform != 'darwin':
        raise RuntimeError('Run this maintainer tool on macOS with the Apple SDK')
    with tempfile.TemporaryDirectory(prefix='libetpan-apple-charsets-') as directory:
        work = Path(directory)
        (work / 'reference.c').write_text(REFERENCE)
        subprocess.run(['xcrun', '--sdk', 'macosx', 'clang', '-I' + str(DEST),
            str(work / 'reference.c'), str(DEST / 'charset_singlebyte_cases.c'),
            str(DEST / 'charset_multibyte_cases.c'), '-framework', 'CoreFoundation',
            '-liconv', '-o', str(work / 'reference')], check=True)
        lines = subprocess.check_output([str(work / 'reference')], text=True).splitlines()
    vectors = {}
    for line in (DEST / 'charset_case_vectors.h').read_text().splitlines():
        match = re.fullmatch(r'#define (\w+) "(.*)"', line)
        if match:
            raw = bytes(int(x, 16) for x in re.findall(r'\\x([0-9a-f]{2})', match[2]))
            vectors.setdefault(raw, match[1])
    header = ['/* Generated from standalone Apple APIs; see data/apple-provider-comparison.json. */',
              '#include "charset_case_vectors.h"']
    rows = {}
    report = []
    # Independent UCM/Python Unicode reference for the two native UTF-8 pairs.
    legacy = {}
    manifest = json.loads((ROOT / 'src/data-types/charconv-icu/manifest.json').read_text())
    for row in manifest['tables']:
        if row['group'] in ('VISCII', 'KOI8-T'):
            source = ROOT / 'dependencies/submodules/icu-data' / row['upstream_path']
            legacy[row['group']] = {int(byte, 16): int(cp, 16) for cp, byte in
                re.findall(r'<U([0-9A-Fa-f]+)>\s+\\x([0-9A-Fa-f]{2})\s+\|0', source.read_text())}
    for line in lines:
        name, mode, encode, status, hexadecimal, input_hex = line.split('\t')
        raw = bytes.fromhex(hexadecimal)
        if mode == '1' and name in legacy:
            table = legacy[name]
            try:
                if encode == '0':
                    raw = ''.join(chr(table[b]) for b in bytes.fromhex(input_hex)).encode('utf-8')
                else:
                    reverse = {cp: byte for byte, cp in table.items()}
                    raw = bytes(reverse.get(ord(c), ord('?')) for c in
                                bytes.fromhex(input_hex).decode('utf-8'))
                status = '0'
            except (KeyError, UnicodeDecodeError):
                status, raw = '3', b''
        if raw not in vectors:
            symbol = 'APPLE_CHARSET_BYTES_' + str(len(vectors))
            vectors[raw] = symbol
            header.append('#define %s "%s"' % (symbol, ''.join('\\x%02x' % b for b in raw)))
        rows.setdefault(name, []).append('{ %s, { %s, %d } }' % (status, vectors[raw], len(raw)))
        report.append({'name': name, 'backend': 'CoreFoundation' if mode == '1' else 'Apple iconv + Japanese CoreFoundation',
                       'direction': 'encode' if encode == '1' else 'decode', 'status': int(status),
                       'native_ucm_fallback': mode == '1' and name in legacy})
    header += ['static const struct { const char *name;',
               '  struct charset_expectation iconv_decode, iconv_encode, cf_decode, cf_encode;',
               '} apple_charset_cases[] = {']
    for name, entries in rows.items():
        header.append('  { %s, %s },' % (json.dumps(name), ', '.join(entries)))
    header += ['};', '']
    (DEST / 'charset_apple_cases.h').write_text('\n'.join(header))
    (DEST / 'data/apple-provider-comparison.json').write_text(json.dumps({
        'macos': subprocess.check_output(['sw_vers', '-productVersion'], text=True).strip(),
        'xcode': subprocess.check_output(['xcodebuild', '-version'], text=True).strip(),
        'source': 'Apple SDK iconv and CoreFoundation; no libetpan runtime oracle',
        'legacy_ucm_revision': manifest['revision'],
        'cases': report}, indent=2) + '\n')
    print('Recorded Apple expectations for', len(rows), 'names in both directions/backends')


if __name__ == '__main__':
    main()
