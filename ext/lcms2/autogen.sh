#!/bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

DIE=0
ACLOCAL_FLAGS="-I m4"

(test -f $srcdir/configure.ac) || {
    echo -n "**Error**: Directory $srcdir does not look like the"
    echo " top-level package directory"
    exit 1
}

(autoconf --version) < /dev/null > /dev/null 2>&1 || {
  echo
  echo "**Error**: You must have autoconf installed."
  echo "Download the appropriate package for your distribution,"
  echo "or get the source tarball at ftp://ftp.gnu.org/pub/gnu/"
  DIE=1
}

(grep "^LT_INIT" $srcdir/configure.ac >/dev/null) && {
  (libtool --version) < /dev/null > /dev/null 2>&1 || {
    echo
    echo "**Error**: You must have libtool installed."
    echo "You can get it from: ftp://ftp.gnu.org/pub/gnu/"
    DIE=1
  }
}

(automake --version) < /dev/null > /dev/null 2>&1 || {
  echo
  echo "**Error**: You must have automake installed."
  echo "You can get it from: ftp://ftp.gnu.org/pub/gnu/"
  DIE=1
  NO_AUTOMAKE=yes
}

# if no automake, don't bother testing for aclocal
test -n "$NO_AUTOMAKE" || (aclocal --version) < /dev/null > /dev/null 2>&1 || {
  echo
  echo "**Error**: Missing aclocal.  The version of automake"
  echo "installed doesn't appear recent enough."
  echo "You can get automake from ftp://ftp.gnu.org/pub/gnu/"
  DIE=1
}

if test "$DIE" -eq 1; then
  exit 1
fi

if test -z "$*"; then
  echo "**Warning**: I am going to run configure with no arguments."
  echo "If you wish to pass any to it, please specify them on the"
  echo $0 " command line."
  echo
fi

case $CC in
xlc )
  am_opt=--include-deps;;
esac

      aclocalinclude="$ACLOCAL_FLAGS"

      if grep "^LT_INIT" configure.ac >/dev/null; then
	if test -z "$NO_LIBTOOLIZE" ; then 
	  echo "Running libtoolize..."
	  libtoolize --force --copy
	fi
      fi
      echo "Running aclocal $aclocalinclude ..."
      aclocal $aclocalinclude
      if grep "^AC_CONFIG_HEADERS" configure.ac >/dev/null; then
	echo "Running autoheader..."
	autoheader
      fi
      echo "Running automake --add-missing -copy --gnu -Wno-portability $am_opt ..."
      automake --add-missing --copy --gnu -Wno-portability $am_opt
      echo "Running autoconf ..."
      autoconf

conf_flags="--enable-maintainer-mode"

if test x$NOCONFIGURE = x; then
  echo "Running $srcdir/configure $conf_flags $@ ..."
  $srcdir/configure $conf_flags "$@" \
  && echo "Now type make to compile." || exit 1
else
  echo "Skipping configure process."
fi
