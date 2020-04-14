#!/bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`

# name of the current package
PKG_NAME=`basename \`(cd $srcdir; pwd)\``

# default configure options
conf_flags="--enable-debug"

DIE=0
libtool=0
gettext=0
run_automake=0
libtoolize=libtoolize

if which glibtoolize >/dev/null 2>&1 ; then
    libtoolize=glibtoolize
fi

missing() {
  echo
  echo "**Error**: You must have \`$1' installed to compile $PKG_NAME."
  echo "Download the appropriate package for your distribution,"
  echo "or get the source tarball at ftp://ftp.gnu.org/pub/gnu/"
  DIE=1
}

(aclocal --version) < /dev/null > /dev/null 2>&1 || missing aclocal
(autoconf --version) < /dev/null > /dev/null 2>&1 || missing autoconf

grep "^AM_GNU_GETTEXT" $srcdir/configure.ac >/dev/null && gettext=1
grep "^AC_PROG_LIBTOOL" $srcdir/configure.ac >/dev/null && libtool=1
grep "^AM_INIT_AUTOMAKE" $srcdir/configure.ac >/dev/null && run_automake=1

if test "$gettext" -eq 1; then
  grep "sed.*POTFILES" $srcdir/configure.ac >/dev/null || \
  (gettextize --version) < /dev/null > /dev/null 2>&1 || missing gettext
fi

if test "$libtool" -eq 1; then
  ($libtoolize --version) < /dev/null > /dev/null 2>&1 || missing libtool
fi

if test "$DIE" -eq 1; then
  exit 1
fi

if test -z "$*"; then
  echo "**Warning**: I am going to run \`configure' with default arguments."
  echo "If you wish to pass any others to it, please specify them on the"
  echo \`$0\'" command line."
  echo
else
  unset conf_flags
fi

if test  "$gettext" -eq 1; then
  echo "Running gettextize...  Ignore non-fatal messages."
  echo "no" | gettextize --force --copy
fi

if test  "$libtool" -eq 1; then
  echo "Running libtoolize..."
  $libtoolize --force --copy
fi

echo "Running aclocal..."
aclocal -I m4

echo "Running autoheader..."
autoheader
echo "Running autoconf ..."
autoconf

if test  "$run_automake" -eq 1; then
  echo "Running automake..."
  automake --add-missing --foreign --force --copy
fi


if test x$NOCONFIGURE = x; then
  echo Running $srcdir/configure $conf_flags "$@" ...
  $srcdir/configure $conf_flags "$@" \
  && echo Now type \`make\' to compile $PKG_NAME
else
  echo Skipping configure process.
fi

#if which jade >/dev/null; then
#	echo "Generate documentation ..."
#	(cd doc && make doc)
#fi
