#!/bin/sh
# Run this to generate all the initial makefiles, etc.

test -n "$srcdir" || srcdir=`dirname "$0"`
test -n "$srcdir" || srcdir=.

olddir=`pwd`
cd $srcdir

#printf "checking for ragel... "
#which ragel || {
#	echo "You need to install ragel... See http://www.complang.org/ragel/"
#	exit 1
#}

printf "checking for pkg-config... "
which pkg-config || {
	echo "*** No pkg-config found, please install it ***"
	exit 1
}

printf "checking for libtoolize... "
which glibtoolize || which libtoolize || {
	echo "*** No libtoolize (libtool) found, please install it ***"
	exit 1
}
printf "checking for gtkdocize... "
if which gtkdocize ; then
	gtkdocize --copy || exit 1
else
	echo "*** No gtkdocize (gtk-doc) found, skipping documentation ***"
	echo "EXTRA_DIST = " > gtk-doc.make
fi

printf "checking for autoreconf... "
which autoreconf || {
	echo "*** No autoreconf (autoconf) found, please install it ***"
	exit 1
}

echo "running autoreconf --force --install --verbose"
autoreconf --force --install --verbose || exit $?

cd $olddir
test -n "$NOCONFIGURE" || {
	echo "running configure $@"
	"$srcdir/configure" "$@"
}
