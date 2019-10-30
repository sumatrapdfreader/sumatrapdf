#!/bin/sh

LC_ALL=C
export LC_ALL

test -z "$srcdir" && srcdir=.
stat=0

test "x$HBHEADERS" = x && HBHEADERS=`cd "$srcdir"; find . -maxdepth 1 -name 'hb*.h'`
test "x$EGREP" = x && EGREP='grep -E'


echo 'Checking that all public symbols are exported with HB_EXTERN'

for x in $HBHEADERS; do
	test -f "$srcdir/$x" -a ! -f "$x" && x="$srcdir/$x"
	$EGREP -B1 -n '^hb_' /dev/null "$x" |
	$EGREP -v '(^--|:hb_|-HB_EXTERN )' -A1
done |
grep . >&2 && stat=1

exit $stat
