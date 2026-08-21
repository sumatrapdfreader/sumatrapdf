#!/bin/bash
for f in source/pdf/js/*.js
do
	echo Dumping $f
	sed -f scripts/jsdump.sed < $f > $f.h
done
