#!/bin/sh

LC_ALL=C
export LC_ALL

test -z "$srcdir" && srcdir=.
test -z "$libs" && libs=.libs
stat=0

IGNORED_SYMBOLS='_fini\|_init\|_fdata\|_ftext\|_fbss\|__bss_start\|__bss_start__\|__bss_end__\|_edata\|_end\|_bss_end__\|__end__\|__gcov_.*\|llvm_.*'

if which nm 2>/dev/null >/dev/null; then
	:
else
	echo "check-symbols.sh: 'nm' not found; skipping test"
	exit 77
fi

tested=false
for soname in harfbuzz harfbuzz-subset harfbuzz-icu harfbuzz-gobject; do
	for suffix in so dylib; do
		so=$libs/lib$soname.$suffix
		if ! test -f "$so"; then continue; fi

		# On macOS, C symbols are prefixed with _
		symprefix=
		if test $suffix = dylib; then symprefix=_; fi

		EXPORTED_SYMBOLS=`nm "$so" | grep ' [BCDGINRST] .' | grep -v " $symprefix\\($IGNORED_SYMBOLS\\>\\)" | cut -d' ' -f3 | c++filt`

		prefix=$symprefix`basename "$so" | sed 's/libharfbuzz/hb/; s/-/_/g; s/[.].*//'`

		echo "Checking that $so does not expose internal symbols"
		if echo "$EXPORTED_SYMBOLS" | grep -v "^${prefix}\(_\|$\)"; then
			echo "Ouch, internal symbols exposed"
			stat=1
		fi

		def=$soname.def
		if ! test -f "$def"; then
			echo "'$def' not found; skipping"
		else
			echo "Checking that $so has the same symbol list as $def"
			{
				echo EXPORTS
				echo "$EXPORTED_SYMBOLS" | sed -e "s/^${symprefix}hb/hb/g"
				# cheat: copy the last line from the def file!
				tail -n1 "$def"
			} | c++filt | diff "$def" - >&2 || stat=1
		fi

		tested=true
	done
done
if ! $tested; then
	echo "check-symbols.sh: no shared libraries found; skipping test"
	exit 77
fi

exit $stat
