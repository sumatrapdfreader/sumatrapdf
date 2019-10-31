#!/bin/bash

LIST=$(echo resources/cmaps/* | sort)

for f in $LIST
do
	b=$(basename $f)
	echo "#include \"cmaps/$b.h\""
	python scripts/cmapdump.py > source/pdf/cmaps/$b.h $f
done


echo "static pdf_cmap *table[] = {"
for f in $LIST
do
	b=$(basename $f)
	c=$(echo $b | tr - _)
	echo "&cmap_$c,"
done
echo "};"
