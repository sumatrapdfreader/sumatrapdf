#!/bin/bash
#
# This scripts expects a to find the original CMap resources in thirdparty/cmap-resources.
#

rm -f build/cmaps/*
mkdir -p build/cmaps

function flatten {
	for DIR in $(echo thirdparty/cmap-resources/Adobe-*)
	do
		if [ -f $DIR/CMap/$1 ]
		then
			echo $DIR/CMap/$1
			python scripts/cmapflatten.py $DIR/CMap/$1 > build/cmaps/$1
		fi
	done
}

flatten GBK-EUC-H
flatten GBK2K-H
flatten GBKp-EUC-H
flatten UniCNS-UCS2-H
flatten UniCNS-UTF16-H
flatten UniGB-UCS2-H
flatten UniGB-UTF16-H
flatten UniJIS-UCS2-H
flatten UniJIS-UTF16-H
flatten UniKS-UCS2-H
flatten UniKS-UTF16-H

python scripts/cmapshare.py build/cmaps/GBK-X build/cmaps/GB*-H
python scripts/cmapshare.py build/cmaps/UniCNS-X build/cmaps/UniCNS-*-H
python scripts/cmapshare.py build/cmaps/UniGB-X build/cmaps/UniGB-*-H
python scripts/cmapshare.py build/cmaps/UniJIS-X build/cmaps/UniJIS-*-H
python scripts/cmapshare.py build/cmaps/UniKS-X build/cmaps/UniKS-*-H

for F in build/cmaps/*-X
do
	B=$(basename $F)
	python scripts/cmapclean.py $F > resources/cmaps/$B
done

for F in build/cmaps/*.shared
do
	B=$(basename $F .shared)
	python scripts/cmapclean.py $F > resources/cmaps/$B
done
