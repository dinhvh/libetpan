#!/bin/sh

set -eu

script_dir=`dirname "$0"`
srcdir=${srcdir:-$script_dir}

if test -f "$srcdir/../unittest/mime-parser/data/input/mbox/jwz/118" ; then
  fixtures="$srcdir/../unittest/mime-parser/data/input/mbox/jwz"
elif test -f "$srcdir/unittest/mime-parser/data/input/mbox/jwz/118" ; then
  fixtures="$srcdir/unittest/mime-parser/data/input/mbox/jwz"
else
  echo "skip: S/MIME jwz fixtures not found"
  exit 77
fi

if ! command -v openssl >/dev/null 2>&1 ; then
  echo "skip: openssl command not available"
  exit 77
fi

if test ! -x "$script_dir/smime-openssl-interop-test" ; then
  echo "skip: smime-openssl-interop-test helper not built"
  exit 77
fi

tmpdir=${TMPDIR:-/tmp}/libetpan-smime-openssl-fixture-$$
mkdir "$tmpdir"
trap 'rm -rf "$tmpdir"' 0 1 2 3 15

openssl smime -verify -noverify \
  -in "$fixtures/118" \
  -out "$tmpdir/jwz-118.decoded" >/dev/null

if openssl smime -verify -noverify \
    -in "$fixtures/128" \
    -out "$tmpdir/jwz-128.decoded" >/dev/null 2>&1 ; then
  echo "jwz/128 unexpectedly verified with OpenSSL"
  exit 1
fi

openssl smime -pk7out -in "$fixtures/105" \
  -out "$tmpdir/jwz-105.pk7"
openssl smime -pk7out -in "$fixtures/121" \
  -out "$tmpdir/jwz-121.pk7"

openssl req -x509 -newkey rsa:2048 -nodes \
  -subj "/CN=libetpan test CA" \
  -days 3650 \
  -keyout "$tmpdir/ca.key" \
  -out "$tmpdir/ca.pem" >/dev/null 2>&1

openssl req -newkey rsa:2048 -nodes \
  -subj "/CN=Alice Example/emailAddress=alice@example.test" \
  -addext "subjectAltName=email:alice@example.test" \
  -addext "extendedKeyUsage=emailProtection" \
  -addext "keyUsage=digitalSignature,keyEncipherment" \
  -keyout "$tmpdir/alice.key" \
  -out "$tmpdir/alice.csr" >/dev/null 2>&1

openssl x509 -req \
  -in "$tmpdir/alice.csr" \
  -CA "$tmpdir/ca.pem" \
  -CAkey "$tmpdir/ca.key" \
  -CAcreateserial \
  -days 365 \
  -copy_extensions copy \
  -out "$tmpdir/alice.pem" >/dev/null 2>&1

printf 'Content-Type: text/plain\r\n\r\nOpenSSL S/MIME interop body\r\n' \
  > "$tmpdir/body.eml"

openssl smime -sign -binary \
  -in "$tmpdir/body.eml" \
  -signer "$tmpdir/alice.pem" \
  -inkey "$tmpdir/alice.key" \
  -out "$tmpdir/signed.eml"
openssl smime -verify -binary \
  -in "$tmpdir/signed.eml" \
  -CAfile "$tmpdir/ca.pem" \
  -out "$tmpdir/verified.eml" >/dev/null
diff -u "$tmpdir/body.eml" "$tmpdir/verified.eml"

"$script_dir/smime-openssl-interop-test" verify \
  "$tmpdir/ca.pem" "$tmpdir/signed.eml"

openssl smime -encrypt -binary \
  -in "$tmpdir/body.eml" \
  -out "$tmpdir/encrypted.eml" \
  "$tmpdir/alice.pem"
openssl smime -decrypt \
  -in "$tmpdir/encrypted.eml" \
  -recip "$tmpdir/alice.pem" \
  -inkey "$tmpdir/alice.key" \
  -out "$tmpdir/decrypted-openssl.eml"
diff -u "$tmpdir/body.eml" "$tmpdir/decrypted-openssl.eml"

"$script_dir/smime-openssl-interop-test" decrypt \
  "$tmpdir/alice.pem" "$tmpdir/alice.key" \
  "$tmpdir/encrypted.eml" "$tmpdir/decrypted-libetpan.eml"
diff -u "$tmpdir/body.eml" "$tmpdir/decrypted-libetpan.eml"

echo "smime-openssl-fixture-test: ok"
