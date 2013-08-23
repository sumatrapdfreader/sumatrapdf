#!/bin/bash

rm -rf docs/browse

FILES=$(find include source platform -name '*.[ch]')

echo running ctags to make xref
ctags -x $FILES > tags-xref

for input in $FILES
do
	output=docs/browse/$input.html
	mkdir -p $(dirname $output)
	echo $input $output
	python scripts/tohtml.py < $input > $output
done

rm tags-xref
