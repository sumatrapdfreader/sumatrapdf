#!/bin/sh

LC_ALL=C
export LC_ALL

test -z "$srcdir" && srcdir=.
stat=0

test "x$HBHEADERS" = x && HBHEADERS=`cd "$srcdir"; find . -maxdepth 1 -name 'hb*.h'`
test "x$HBSOURCES" = x && HBSOURCES=`cd "$srcdir"; find . -maxdepth 1 -name 'hb-*.cc' -or -name 'hb-*.hh'`


echo 'Checking that public header files #include "hb-common.h" or "hb.h" first (or none)'

for x in $HBHEADERS; do
	test -f "$srcdir/$x" -a ! -f "$x" && x="$srcdir/$x"
	grep '#.*\<include\>' "$x" /dev/null | head -n 1
done |
grep -v '"hb-common[.]h"' |
grep -v '"hb[.]h"' |
grep -v 'hb-common[.]h:' |
grep -v 'hb[.]h:' |
grep . >&2 && stat=1


echo 'Checking that source files #include a private header first (or none)'

for x in $HBSOURCES; do
	test -f "$srcdir/$x" -a ! -f "$x" && x="$srcdir/$x"
	grep '#.*\<include\>' "$x" /dev/null | head -n 1
done |
grep -v '"hb-.*[.]hh"' |
grep -v 'hb[.]hh' |
grep . >&2 && stat=1


echo 'Checking that there is no #include <hb-*.h>'
for x in $HBHEADERS $HBSOURCES; do
	test -f "$srcdir/$x" && x="$srcdir/$x"
	grep '#.*\<include\>.*<.*hb' "$x" /dev/null >&2 && stat=1
done


exit $stat
