#!/bin/bash
rm -f source/fitz/icc/*.h
for f in resources/icc/*.icc
do
	b=$(basename $f)
	echo Dumping $b
	xxd -i $f | sed 's/unsigned/static const unsigned/' > source/fitz/icc/$b.h
done
