#!/bin/bash

mkdir -p doc/source
rm doc/source/*.html

FILES="fitz/*.[ch] draw/*.[ch] pdf/*.[ch] xps/*.[ch] cbz/*.[ch] apps/*.[ch]"

echo running ctags to make xref
ctags -x $FILES > tags-xref

for input in $FILES
do
	output=doc/source/$(basename $input).html
	echo $input $output
	python scripts/tohtml.py < $input > $output
done

rm tags-xref
