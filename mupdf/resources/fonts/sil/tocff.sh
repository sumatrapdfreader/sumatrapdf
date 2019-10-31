#!/bin/bash

# python scripts/makesubset.py -lig scripts/MES-2.TXT > resources/fonts/sil/subset.mes
# python scripts/makesubset.py -sc -lig scripts/MES-2.TXT > resources/fonts/sil/subset.mes.sc

tx -cff +S +T -b -n -g $(cat subset.mes.sc) -A CharisSIL-5.000-developer/sources/CharisSIL-R-designsource.otf
tx -cff +S +T -b -n -g $(cat subset.mes) -A CharisSIL-5.000-developer/sources/CharisSIL-B-designsource.otf
tx -cff +S +T -b -n -g $(cat subset.mes) -A CharisSIL-5.000-developer/sources/CharisSIL-BI-designsource.otf
tx -cff +S +T -b -n -g $(cat subset.mes) -A CharisSIL-5.000-developer/sources/CharisSIL-I-designsource.otf
