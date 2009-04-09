#!/bin/sh
# Run this to set up the build system: configure, makefiles, etc.

package="jasper"

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

cd "$srcdir"

if ! test -d acaux; then
  echo "Creating config aux dir 'acaux'..."
  mkdir acaux
fi

DIE=0

echo "checking for autoconf... "
(autoconf --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have autoconf installed to compile $package."
	echo "Download the appropriate package for your distribution,"
	echo "or get the source tarball at ftp://ftp.gnu.org/pub/gnu/"
	DIE=1
}

echo "checking for automake... "
AUTOMAKE=automake
ACLOCAL=aclocal
($AUTOMAKE --version) < /dev/null > /dev/null 2>&1 || AUTOMAKE=
echo "checking for aclocal... "
($ACLOCAL --version) < /dev/null > /dev/null 2>&1 || ACLOCAL=
test -z $AUTOMAKE || test -z $ACLOCAL && {
	echo
	echo "You must have automake installed to compile $package."
	echo "Download the appropriate package for your distribution,"
	echo "or get the source tarball at ftp://ftp.gnu.org/pub/gnu/"
	DIE=1
}

echo "checking for libtool... "
(libtoolize --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have libtool installed to compile $package."
	echo "Download the appropriate package for your system,"
	echo "or get the source from one of the GNU ftp sites"
	echo "listed in http://www.gnu.org/order/ftp.html"
	DIE=1
}

if test "$DIE" -eq 1; then
  exit 1
fi


echo "Generating configuration files for $package, please wait...."

echo "  $ACLOCAL $ACLOCAL_FLAGS"
$ACLOCAL $ACLOCAL_FLAGS

echo "  autoheader"
autoheader

echo "  libtoolize --automake --copy"
libtoolize --automake

echo "  $AUTOMAKE --add-missing --copy $AUTOMAKE_FLAGS"
$AUTOMAKE --add-missing $AUTOMAKE_FLAGS 

echo "  autoconf"
autoconf

if test -z "$*"; then
        echo "I am going to run ./configure with no arguments - if you wish "
        echo "to pass any to it, please specify them on the $0 command line."
else
	echo "running ./configure $@"
fi

$srcdir/configure "$@" && echo
