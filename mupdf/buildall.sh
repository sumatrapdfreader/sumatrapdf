EXT=../ext
#make ZLIB_DIR=$EXT/zlib-1.2.5 JPEG_DIR=$EXT/jpeg-8b JBIG2_DIR=$EXT/jbig2dec JASPER_DIR=$EXT/jasper/src/libjasper CFG=dbg64 -j2 -f Makefile.kjk
#make ZLIB_DIR=$EXT/zlib-1.2.5 JPEG_DIR=$EXT/jpeg-8b JBIG2_DIR=$EXT/jbig2dec JASPER_DIR=$EXT/jasper/src/libjasper CFG=dbg -j2 -f Makefile.kjk

#make ZLIB_DIR=$EXT/zlib-1.2.5 JPEG_DIR=$EXT/jpeg-8b JBIG2_DIR=$EXT/jbig2dec JASPER_DIR=$EXT/jasper/src/libjasper CFG=rel -j4 -f Makefile.kjk
#make ZLIB_DIR=$EXT/zlib-1.2.5 JPEG_DIR=$EXT/jpeg-8b JBIG2_DIR=$EXT/jbig2dec JASPER_DIR=$EXT/jasper/src/libjasper CFG=dbg -j4 -f Makefile.kjk

make ZLIB_DIR=$EXT/zlib-1.2.5 JPEG_DIR=$EXT/jpeg-8b JBIG2_DIR=$EXT/jbig2dec OPENJPEG_DIR=$EXT/openjpeg-1.3/libopenjpeg CFG=rel -j4 -f Makefile.kjk
make ZLIB_DIR=$EXT/zlib-1.2.5 JPEG_DIR=$EXT/jpeg-8b JBIG2_DIR=$EXT/jbig2dec OPENJPEG_DIR=$EXT/openjpeg-1.3/libopenjpeg CFG=dbg -j4 -f Makefile.kjk
