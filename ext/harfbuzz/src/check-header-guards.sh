#!/bin/sh

LC_ALL=C
export LC_ALL

test -z "$srcdir" && srcdir=.
stat=0

test "x$HBHEADERS" = x && HBHEADERS=`cd "$srcdir"; find . -maxdepth 1 -name 'hb*.h' ! -name 'hb-gobject-structs.h'`
test "x$HBSOURCES" = x && HBSOURCES=`cd "$srcdir"; find . -maxdepth 1 -name 'hb-*.cc' -or -name 'hb-*.hh'`

for x in $HBHEADERS $HBSOURCES; do
	test -f "$srcdir/$x" -a ! -f "$x" && x="$srcdir/$x"
	echo "$x" | grep -q '[^h]$' && continue;
	xx=`echo "$x" | sed 's@.*/@@'`
	tag=`echo "$xx" | tr 'a-z.-' 'A-Z_'`
	lines=`grep -w "$tag" "$x" | wc -l | sed 's/[ 	]*//g'`
	if test "x$lines" != x3; then
		echo "Ouch, header file $x does not have correct preprocessor guards"
		stat=1
	fi
done

exit $stat
