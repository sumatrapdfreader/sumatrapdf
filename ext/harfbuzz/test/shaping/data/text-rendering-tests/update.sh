#!/bin/sh

dir=`dirname "$0"`

set -ex
if test -d text-rendering-tests; then
	cd text-rendering-tests
	git pull
	cd ..
else
	git clone https://github.com/unicode-org/text-rendering-tests
fi

test -d fonts && git rm -rf fonts
test -d fonts && (echo "fonts/ dir not empty; investigate."; false)
cp -a text-rendering-tests/fonts .
git add fonts

rmdir tests || true
test -d tests && git rm -rf tests || true
test -d tests && (echo "tests/ dir not empty; investigate."; false)
mkdir tests

echo "TESTS = \\" > Makefile.sources

DISABLED="DISBALED_TESTS = \\"
for x in text-rendering-tests/testcases/*.html; do
	test "x$x" = xtext-rendering-tests/testcases/index.html && continue
	out=tests/`basename "$x" .html`.tests
	"$dir"/extract-tests.py < "$x" > "$out"
	if grep -q "^$out$" DISABLED; then
		DISABLED="$DISABLED
	$out \\"
	else
		echo "	$out \\" >> Makefile.sources
	fi
done
git add tests

echo '	$(NULL)' >> Makefile.sources
echo >> Makefile.sources
echo "$DISABLED" >> Makefile.sources
echo '	$(NULL)' >> Makefile.sources
git add Makefile.sources

git commit -e -m "[test/text-rendering-tests] Update from upstream"
