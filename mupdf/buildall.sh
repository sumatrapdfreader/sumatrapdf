EXT=../ext
# TODO: if this is still used, make compiling with CFG=rel optional through a command line argument
make ZLIB_DIR=$EXT/zlib-1.2.5 JPEG_DIR=$EXT/libjpeg JBIG2_DIR=$EXT/jbig2dec OPENJPEG_DIR=$EXT/openjpeg/libopenjpeg CFG=rel -j4 -f Makefile.kjk
make ZLIB_DIR=$EXT/zlib-1.2.5 JPEG_DIR=$EXT/libjpeg JBIG2_DIR=$EXT/jbig2dec OPENJPEG_DIR=$EXT/openjpeg/libopenjpeg CFG=dbg -j4 -f Makefile.kjk
