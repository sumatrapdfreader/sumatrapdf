#!/bin/bash
CXX=clang++
FONT=fonts/NotoNastaliqUrdu-Regular.ttf
TEXT=texts/fa-monologue.txt

$CXX ../util/hb-shape.cc ../util/options.cc ../src/harfbuzz.cc \
  -lm -fno-rtti -fno-exceptions -fno-omit-frame-pointer -DHB_NO_MT \
  -I../src $FLAGS $SOURCES \
  -DPACKAGE_NAME='""' -DPACKAGE_VERSION='""' \
  -DHAVE_GLIB $(pkg-config --cflags --libs glib-2.0) \
  -o hb-shape -g -O2 # -O3 \
  #-march=native -mtune=native \
  #-Rpass=loop-vectorize -Rpass-missed=loop-vectorize \
  #-Rpass-analysis=loop-vectorize -fsave-optimization-record

# -march=native: enable all vector instructions current CPU can offer
# -Rpass*: https://llvm.org/docs/Vectorizers.html#diagnostics

#sudo rm capture.syscap > /dev/null
#sysprof-cli -c "./a.out $@"
#sysprof capture.syscap

perf stat ./hb-shape -o /dev/null $FONT --text-file $TEXT --num-iterations=100 --font-funcs=ot
#perf record -g ./hb-shape -O '' -o /dev/null $FONT --text-file $TEXT --num-iterations=100 --font-funcs=ot
#perf report -g
