#!/bin/bash

REV=$(git describe --tags)
O=mupdf-$REV-source

echo git archive $O.tar
git archive --format=tar --prefix=$O/ HEAD > $O.tar

git submodule | while read R P T
do
	M=$(basename $P)
	echo git archive $O.$M.tar
	git archive --format=tar --remote=$P --prefix=$O/$P/ HEAD > $O.$M.tar
	tar Af $O.tar $O.$M.tar
	rm -f $O.$M.tar
done

echo gzip $O.tar
gzip -f -k $O.tar
echo xz $O.tar
xz -f -k $O.tar
