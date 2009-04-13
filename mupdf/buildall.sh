EXT=../ext
#make ZLIB_DIR=$EXT/zlib-1.2.3 JPEG_DIR=$EXT/jpeg6b JBIG2_DIR=$EXT/jbig2dec JASPER_DIR=$EXT/jasper/src/libjasper CFG=dbg64 -j2
#make ZLIB_DIR=$EXT/zlib-1.2.3 JPEG_DIR=$EXT/jpeg6b JBIG2_DIR=$EXT/jbig2dec JASPER_DIR=$EXT/jasper/src/libjasper CFG=dbg -j2
make ZLIB_DIR=$EXT/zlib-1.2.3 JPEG_DIR=$EXT/jpeg6b JBIG2_DIR=$EXT/jbig2dec JASPER_DIR=$EXT/jasper/src/libjasper CFG=rel -j4
make ZLIB_DIR=$EXT/zlib-1.2.3 JPEG_DIR=$EXT/jpeg6b JBIG2_DIR=$EXT/jbig2dec JASPER_DIR=$EXT/jasper/src/libjasper CFG=dbg -j4
