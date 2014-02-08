# makefile for jbig2dec
# under Microsoft Visual C++
#
# To compile zlib.dll:
#  Get zlib >= 1.1.4, unzip and rename to zlib,
#  cd zlib, copy nt\zlib.dnt zlib.dnt, then nmake -f nt\makefile.nt
# To compile libpng.lib:
#  Get libpng >= 1.2.5, unzip then rename to libpng,
#  cd libpng, nmake -f scripts\makefile.vcwin32

LIBPNGDIR=../libpng
ZLIBDIR=../zlib
# define iff you're linking to libpng
# LIBPNG_CFLAGS=-DHAVE_LIBPNG -I$(LIBPNGDIR) -I$(ZLIBDIR)
# LIBPNG_LDFLAGS=$(LIBPNGDIR)/libpng.lib $(ZLIBDIR)/zlib.lib /link /NODEFAULTLIB:LIBCMT
LIBPNG_CFLAGS=
LIBPNG_LDFLAGS=

EXE=.exe
OBJ=.obj
NUL=
CFLAGS=-nologo -W4 -Zi -DHAVE_STRING_H=1 -D_CRT_SECURE_NO_WARNINGS $(LIBPNG_CFLAGS) 
CC=cl
FE=-Fe


# no libpng
#
OBJS=getopt$(OBJ) getopt1$(OBJ) jbig2$(OBJ) jbig2_arith$(OBJ) \
 jbig2_arith_iaid$(OBJ) jbig2_arith_int$(OBJ) jbig2_huffman$(OBJ) \
 jbig2_generic$(OBJ) jbig2_refinement$(OBJ) jbig2_halftone$(OBJ)\
 jbig2_image$(OBJ) jbig2_image_pbm$(OBJ)\
 jbig2_segment$(OBJ) jbig2_symbol_dict$(OBJ) jbig2_text$(OBJ) \
 jbig2_mmr$(OBJ) jbig2_page$(OBJ) jbig2_metadata$(OBJ) \
 jbig2dec$(OBJ) sha1$(OBJ)

# libpng - if you link to libpng, uncomment these lines.
#
#OBJS=getopt$(OBJ) getopt1$(OBJ) jbig2$(OBJ) jbig2_arith$(OBJ) \
# jbig2_arith_iaid$(OBJ) jbig2_arith_int$(OBJ) jbig2_huffman$(OBJ) \
# jbig2_generic$(OBJ) jbig2_refinement$(OBJ) jbig2_halftone$(OBJ)\
# jbig2_image$(OBJ) jbig2_image_pbm$(OBJ) jbig2_image_png$(OBJ) \
# jbig2_segment$(OBJ) jbig2_symbol_dict$(OBJ) jbig2_text$(OBJ) \
# jbig2_mmr$(OBJ) jbig2_page$(OBJ) jbig2_metadata$(OBJ) \
# jbig2dec$(OBJ) sha1$(OBJ)

HDRS=getopt.h jbig2.h jbig2_arith.h jbig2_arith_iaid.h jbig2_arith_int.h \
 jbig2_generic.h jbig2_huffman.h jbig2_hufftab.h jbig2_image.h \
 jbig2_mmr.h jbig2_priv.h jbig2_symbol_dict.h jbig2_metadata.h \
 config_win32.h sha1.h

all: jbig2dec$(EXE)

jbig2dec$(EXE): $(OBJS)
	$(CC) $(CFLAGS) $(FE)jbig2dec$(EXE) $(OBJS) $(LIBPNG_LDFLAGS) 

getopt$(OBJ): getopt.c getopt.h
	$(CC) $(CFLAGS) -c getopt.c

getopt1$(OBJ): getopt1.c getopt.h
	$(CC) $(CFLAGS) -c getopt1.c

jbig2$(OBJ): jbig2.c $(HDRS)
	$(CC) $(CFLAGS) -c jbig2.c

jbig2_arith$(OBJ): jbig2_arith.c $(HDRS)
	$(CC) $(CFLAGS) -c jbig2_arith.c

jbig2_arith_iaid$(OBJ): jbig2_arith_iaid.c $(HDRS)
	$(CC) $(CFLAGS) -c jbig2_arith_iaid.c

jbig2_arith_int$(OBJ): jbig2_arith_int.c $(HDRS)
	$(CC) $(CFLAGS) -c jbig2_arith_int.c

jbig2_generic$(OBJ): jbig2_generic.c $(HDRS)
	$(CC) $(CFLAGS) -c jbig2_generic.c

jbig2_refinement$(OBJ): jbig2_refinement.c $(HDRS)
	$(CC) $(CFLAGS) -c jbig2_refinement.c

jbig2_huffman$(OBJ): jbig2_huffman.c $(HDRS)
	$(CC) $(CFLAGS) -c jbig2_huffman.c

jbig2_image$(OBJ): jbig2_image.c $(HDRS)
	$(CC) $(CFLAGS) -c jbig2_image.c

jbig2_image_pbm$(OBJ): jbig2_image_pbm.c $(HDRS)
	$(CC) $(CFLAGS) -c jbig2_image_pbm.c

jbig2_image_png$(OBJ): jbig2_image_png.c $(HDRS)
	$(CC) $(CFLAGS) -c jbig2_image_png.c

jbig2_halftone$(OBJ): jbig2_halftone.c $(HDRS)
	$(CC) $(CFLAGS) -c jbig2_halftone.c

jbig2_mmr$(OBJ): jbig2_mmr.c $(HDRS)
	$(CC) $(CFLAGS) -c jbig2_mmr.c

jbig2_page$(OBJ): jbig2_page.c $(HDRS)
	$(CC) $(CFLAGS) -c jbig2_page.c

jbig2_segment$(OBJ): jbig2_segment.c $(HDRS)
	$(CC) $(CFLAGS) -c jbig2_segment.c

jbig2_symbol_dict$(OBJ): jbig2_symbol_dict.c $(HDRS)
	$(CC) $(CFLAGS) -c jbig2_symbol_dict.c

jbig2_text$(OBJ): jbig2_text.c $(HDRS)
	$(CC) $(CFLAGS) -c jbig2_text.c

jbig2_metadata$(OBJ): jbig2_metadata.c $(HDRS)
	$(CC) $(CFLAGS) -c jbig2_metadata.c

jbig2dec$(OBJ): jbig2dec.c $(HDRS)
	$(CC) $(CFLAGS) -c jbig2dec.c

sha1$(OBJ): sha1.c $(HDRS)
	$(CC) $(CFLAGS) -c sha1.c

clean:
	-del $(OBJS)
	-del jbig2dec$(EXE)
	-del jbig2dec.ilk
	-del jbig2dec.pdb
	-del pbm2png$(EXE)
	-del pbm2png.ilk
	-del pbm2png.pdb
	-del vc70.pdb
	-del vc60.pdb
	-del vc50.pdb

