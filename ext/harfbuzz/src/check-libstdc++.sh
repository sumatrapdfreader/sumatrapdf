#!/bin/sh

LC_ALL=C
export LC_ALL

test -z "$srcdir" && srcdir=.
test -z "$libs" && libs=.libs
stat=0


if which ldd 2>/dev/null >/dev/null; then
	LDD=ldd
else
	# macOS specific tool
	if which otool 2>/dev/null >/dev/null; then
		LDD="otool -L"
	else
		echo "check-libstdc++.sh: 'ldd' not found; skipping test"
		exit 77
	fi
fi

tested=false
# harfbuzz-icu links to libstdc++ because icu does.
# harfbuzz-subset uses libstdc++.
for soname in harfbuzz harfbuzz-gobject; do
	for suffix in so dylib; do
		so=$libs/lib$soname.$suffix
		if ! test -f "$so"; then continue; fi

		echo "Checking that we are not linking to libstdc++ or libc++ in $so"
		if $LDD $so | grep 'libstdc[+][+]\|libc[+][+]'; then
			echo "Ouch, linked to libstdc++ or libc++"
			stat=1
		fi
		tested=true
	done
done
if ! $tested; then
	echo "check-libstdc++.sh: libharfbuzz shared library not found; skipping test"
	exit 77
fi

exit $stat
