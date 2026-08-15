#!/bin/bash
for F in $(find source platform -name '*.c')
do
	awk -f scripts/find-try-return.awk $F
done
