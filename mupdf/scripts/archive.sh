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
pigz -f -k -11 $O.tar
echo lzip $O.tar
plzip -9 -f -k $O.tar
echo zstd $O.tar
zstd -q -T0 -19 -f -k $O.tar
