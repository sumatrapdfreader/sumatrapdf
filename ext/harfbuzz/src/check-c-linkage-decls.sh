#!/bin/sh

LC_ALL=C
export LC_ALL

test -z "$srcdir" && srcdir=.
stat=0

test "x$HBHEADERS" = x && HBHEADERS=`cd "$srcdir"; find . -maxdepth 1 -name 'hb*.h'`
test "x$HBSOURCES" = x && HBSOURCES=`cd "$srcdir"; find . -maxdepth 1 -name 'hb*.cc'`

for x in $HBHEADERS; do
	test -f "$srcdir/$x" -a ! -f "$x" && x="$srcdir/$x"
	if ! grep -q HB_BEGIN_DECLS "$x" || ! grep -q HB_END_DECLS "$x"; then
		echo "Ouch, file $x does not have HB_BEGIN_DECLS / HB_END_DECLS, but it should"
		stat=1
	fi
done
for x in $HBSOURCES; do
	test -f "$srcdir/$x" -a ! -f "$x" && x="$srcdir/$x"
	if grep -q HB_BEGIN_DECLS "$x" || grep -q HB_END_DECLS "$x"; then
		echo "Ouch, file $x has HB_BEGIN_DECLS / HB_END_DECLS, but it shouldn't"
		stat=1
	fi
done

exit $stat
