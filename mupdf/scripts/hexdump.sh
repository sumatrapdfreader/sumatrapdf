#!/bin/bash

FILE=$1
if [ ! -f "$FILE" ]
then
	echo usage: bash scripts/hexdump.sh input.ttf
	exit 1
fi

NAME=$(basename "$FILE" | sed 's/[.-]/_/g')

echo "// This is an automatically generated file. Do not edit."
echo "const unsigned char _binary_$NAME[] ="
od -v -An -tx1 "$FILE" | sed 's/ /\\x/g;s/.*/"&"/'
echo ";"
echo "const unsigned int _binary_${NAME}_size = sizeof(_binary_${NAME}) - 1;"
