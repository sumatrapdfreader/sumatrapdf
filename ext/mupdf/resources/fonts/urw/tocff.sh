#!/bin/bash

# python3 scripts/makesubset.py -lig scripts/MES-2.TXT > resources/fonts/urw/subset.mes
# python3 scripts/makesubset.py -lig scripts/SECS.TXT > resources/fonts/urw/subset.secs
# python3 scripts/makesubset.py -lig scripts/WGL4.TXT > resources/fonts/urw/subset.wgl

tx -cff +S +T -b -n -g $(cat subset.box) -A input/NimbusBoxes.t1

for f in input/NimbusMono*.t1 input/NimbusRoman*.t1 input/NimbusSans*.t1
do
	tx -cff +S +T -b -n -g $(cat subset.mes) -A $f
done

for f in input/*Dingbats*.t1 input/*Symbol*.t1
do
	tx -cff +S +T -b -n -A $f
done
