#!/bin/sh

set -eu

script_dir=`dirname "$0"`

extract_armor()
{
  begin="$1"
  end="$2"
  input="$3"
  output="$4"

  awk -v begin="$begin" -v end="$end" '
    index($0, begin) { inside = 1 }
    inside { print }
    index($0, end) { inside = 0 }
  ' "$input" > "$output"
}

if ! command -v gpg >/dev/null 2>&1 ; then
  echo "skip: gpg command not available"
  exit 77
fi

if test ! -x "$script_dir/mailpgp-gnupg-interop-test" ; then
  echo "skip: mailpgp-gnupg-interop-test helper not built"
  exit 77
fi

tmpdir=${TMPDIR:-/tmp}/libetpan-mailpgp-gnupg-fixture-$$
mkdir "$tmpdir"
trap 'rm -rf "$tmpdir"' 0 1 2 3 15

GNUPGHOME="$tmpdir/gnupg"
export GNUPGHOME
mkdir "$GNUPGHOME"
chmod 700 "$GNUPGHOME"

cat > "$tmpdir/key.conf" <<EOF
Key-Type: RSA
Key-Length: 2048
Key-Usage: sign
Subkey-Type: RSA
Subkey-Length: 2048
Subkey-Usage: encrypt
Name-Real: Alice Example
Name-Email: alice@example.test
Expire-Date: 0
%no-protection
%commit
EOF

if ! gpg --batch --pinentry-mode loopback --generate-key "$tmpdir/key.conf" \
    >/dev/null 2>&1 ; then
  echo "skip: gpg could not generate temporary test key"
  exit 77
fi
gpg --batch --armor --export alice@example.test > "$tmpdir/alice-public.asc"
gpg --batch --armor --export-secret-keys alice@example.test \
  > "$tmpdir/alice-secret.asc"

printf 'Content-Type: text/plain\r\n\r\nGnuPG OpenPGP interop body\r\n' \
  > "$tmpdir/body.eml"
printf 'Content-Type: text/plain\r\n\r\nGnuPG OpenPGP tampered body\r\n' \
  > "$tmpdir/body-tampered.eml"

gpg --batch --yes --armor --detach-sign \
  --local-user alice@example.test \
  --output "$tmpdir/signature.asc" "$tmpdir/body.eml"

{
  printf 'Content-Type: multipart/signed; protocol="application/pgp-signature"; micalg=pgp-sha256; boundary="pgp-boundary"\r\n'
  printf '\r\n'
  printf -- '--pgp-boundary\r\n'
  cat "$tmpdir/body.eml"
  printf '\r\n--pgp-boundary\r\n'
  printf 'Content-Type: application/pgp-signature; name="signature.asc"\r\n'
  printf 'Content-Transfer-Encoding: 7bit\r\n'
  printf 'Content-Disposition: attachment; filename="signature.asc"\r\n'
  printf '\r\n'
  cat "$tmpdir/signature.asc"
  printf '\r\n--pgp-boundary--\r\n'
} > "$tmpdir/gpg-signed.eml"

"$script_dir/mailpgp-gnupg-interop-test" verify \
  "$tmpdir/alice-public.asc" "$tmpdir/gpg-signed.eml"

{
  printf 'Content-Type: multipart/signed; protocol="application/pgp-signature"; micalg=pgp-sha256; boundary="pgp-boundary"\r\n'
  printf '\r\n'
  printf -- '--pgp-boundary\r\n'
  cat "$tmpdir/body-tampered.eml"
  printf '\r\n--pgp-boundary\r\n'
  printf 'Content-Type: application/pgp-signature; name="signature.asc"\r\n'
  printf 'Content-Transfer-Encoding: 7bit\r\n'
  printf 'Content-Disposition: attachment; filename="signature.asc"\r\n'
  printf '\r\n'
  cat "$tmpdir/signature.asc"
  printf '\r\n--pgp-boundary--\r\n'
} > "$tmpdir/gpg-tampered-signed.eml"

if "$script_dir/mailpgp-gnupg-interop-test" verify \
    "$tmpdir/alice-public.asc" "$tmpdir/gpg-tampered-signed.eml" \
    >/dev/null 2>&1 ; then
  echo "GnuPG tampered signature unexpectedly verified"
  exit 1
fi

cat > "$tmpdir/eve-key.conf" <<EOF
Key-Type: RSA
Key-Length: 2048
Key-Usage: sign
Name-Real: Eve Example
Name-Email: eve@example.test
Expire-Date: 0
%no-protection
%commit
EOF

gpg --batch --pinentry-mode loopback --generate-key "$tmpdir/eve-key.conf" \
  >/dev/null 2>&1
gpg --batch --yes --armor --detach-sign \
  --local-user eve@example.test \
  --output "$tmpdir/eve-signature.asc" "$tmpdir/body.eml"

{
  printf 'Content-Type: multipart/signed; protocol="application/pgp-signature"; micalg=pgp-sha256; boundary="pgp-boundary"\r\n'
  printf '\r\n'
  printf -- '--pgp-boundary\r\n'
  cat "$tmpdir/body.eml"
  printf '\r\n--pgp-boundary\r\n'
  printf 'Content-Type: application/pgp-signature; name="signature.asc"\r\n'
  printf 'Content-Transfer-Encoding: 7bit\r\n'
  printf 'Content-Disposition: attachment; filename="signature.asc"\r\n'
  printf '\r\n'
  cat "$tmpdir/eve-signature.asc"
  printf '\r\n--pgp-boundary--\r\n'
} > "$tmpdir/gpg-unknown-signer.eml"

if "$script_dir/mailpgp-gnupg-interop-test" verify \
    "$tmpdir/alice-public.asc" "$tmpdir/gpg-unknown-signer.eml" \
    >/dev/null 2>&1 ; then
  echo "GnuPG unknown signer unexpectedly verified"
  exit 1
fi

gpg --batch --yes --trust-model always --armor --encrypt \
  --recipient alice@example.test \
  --output "$tmpdir/encrypted.asc" "$tmpdir/body.eml"

{
  printf 'Content-Type: multipart/encrypted; protocol="application/pgp-encrypted"; boundary="pgp-boundary"\r\n'
  printf '\r\n'
  printf -- '--pgp-boundary\r\n'
  printf 'Content-Type: application/pgp-encrypted\r\n'
  printf '\r\n'
  printf 'Version: 1\r\n'
  printf '\r\n--pgp-boundary\r\n'
  printf 'Content-Type: application/octet-stream; name="encrypted.asc"\r\n'
  printf 'Content-Transfer-Encoding: 7bit\r\n'
  printf 'Content-Disposition: attachment; filename="encrypted.asc"\r\n'
  printf '\r\n'
  cat "$tmpdir/encrypted.asc"
  printf '\r\n--pgp-boundary--\r\n'
} > "$tmpdir/gpg-encrypted.eml"

"$script_dir/mailpgp-gnupg-interop-test" decrypt \
  "$tmpdir/alice-secret.asc" "$tmpdir/gpg-encrypted.eml" \
  "$tmpdir/decrypted-libetpan.eml"
diff -u "$tmpdir/body.eml" "$tmpdir/decrypted-libetpan.eml"

gpg --batch --yes --trust-model always --throw-keyids --armor --encrypt \
  --recipient alice@example.test \
  --output "$tmpdir/hidden-encrypted.asc" "$tmpdir/body.eml"

{
  printf 'Content-Type: multipart/encrypted; protocol="application/pgp-encrypted"; boundary="pgp-boundary"\r\n'
  printf '\r\n'
  printf -- '--pgp-boundary\r\n'
  printf 'Content-Type: application/pgp-encrypted\r\n'
  printf '\r\n'
  printf 'Version: 1\r\n'
  printf '\r\n--pgp-boundary\r\n'
  printf 'Content-Type: application/octet-stream; name="encrypted.asc"\r\n'
  printf 'Content-Transfer-Encoding: 7bit\r\n'
  printf 'Content-Disposition: attachment; filename="encrypted.asc"\r\n'
  printf '\r\n'
  cat "$tmpdir/hidden-encrypted.asc"
  printf '\r\n--pgp-boundary--\r\n'
} > "$tmpdir/gpg-hidden-encrypted.eml"

"$script_dir/mailpgp-gnupg-interop-test" decrypt \
  "$tmpdir/alice-secret.asc" "$tmpdir/gpg-hidden-encrypted.eml" \
  "$tmpdir/hidden-decrypted-libetpan.eml"
diff -u "$tmpdir/body.eml" "$tmpdir/hidden-decrypted-libetpan.eml"

"$script_dir/mailpgp-gnupg-interop-test" encrypt \
  "$tmpdir/alice-public.asc" "$tmpdir/body.eml" \
  "$tmpdir/libetpan-encrypted.eml"
extract_armor "-----BEGIN PGP MESSAGE-----" "-----END PGP MESSAGE-----" \
  "$tmpdir/libetpan-encrypted.eml" "$tmpdir/libetpan-encrypted.asc"
gpg --batch --yes --decrypt \
  --output "$tmpdir/libetpan-decrypted-gpg.eml" \
  "$tmpdir/libetpan-encrypted.asc" >/dev/null 2>&1
diff -u "$tmpdir/body.eml" "$tmpdir/libetpan-decrypted-gpg.eml"

"$script_dir/mailpgp-gnupg-interop-test" sign \
  "$tmpdir/alice-secret.asc" "$tmpdir/body.eml" \
  "$tmpdir/libetpan-signed.eml"
"$script_dir/mailpgp-gnupg-interop-test" extract-signed \
  "$tmpdir/libetpan-signed.eml" "$tmpdir/libetpan-signed-data.eml" \
  "$tmpdir/libetpan-signature.asc"
gpg --batch --yes --verify "$tmpdir/libetpan-signature.asc" \
  "$tmpdir/libetpan-signed-data.eml" >/dev/null 2>&1

echo "mailpgp-gnupg-fixture-test: ok"
