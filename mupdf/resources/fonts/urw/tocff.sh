#!/bin/bash

# python scripts/makesubset.py -lig scripts/MES-2.TXT > resources/fonts/urw/subset.mes
# python scripts/makesubset.py -lig scripts/SECS.TXT > resources/fonts/urw/subset.secs
# python scripts/makesubset.py -lig scripts/WGL4.TXT > resources/fonts/urw/subset.wgl

for f in input/Nimbus*.t1
do
	tx -cff +S +T -b -n -g $(cat subset.mes) -A $f
done

for f in input/*Dingbats*.t1 input/*Symbol*.t1
do
	tx -cff +S +T -b -n -A $f
done
