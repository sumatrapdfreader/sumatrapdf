## process this file with automake to generate Makefile.in

# require automake 1.7
AUTOMAKE_OPTIONS = foreign 1.7 dist-bzip2 dist-zip -Wall

lib_LTLIBRARIES = libjbig2dec.la
include_HEADERS = jbig2.h

libjbig2dec_la_LDFLAGS = -version-info @JBIG2DEC_LT_CURRENT@:@JBIG2DEC_LT_REVISION@:@JBIG2DEC_LT_AGE@
libjbig2dec_la_SOURCES = jbig2.c \
	jbig2_arith.c jbig2_arith_int.c jbig2_arith_iaid.c jbig2_huffman.c \
	jbig2_segment.c jbig2_page.c \
	jbig2_symbol_dict.c jbig2_text.c \
	jbig2_generic.c jbig2_refinement.c jbig2_mmr.c \
	jbig2_halftone.c \
	jbig2_image.c jbig2_image_pbm.c \
	os_types.h config_types.h config_win32.h \
	jbig2.h jbig2_priv.h jbig2_image.h \
	jbig2_arith.h jbig2_arith_iaid.h jbig2_arith_int.h \
	jbig2_huffman.h jbig2_hufftab.h jbig2_mmr.h \
	jbig2_generic.h jbig2_symbol_dict.h jbig2_text.h \
	jbig2_metadata.c jbig2_metadata.h

bin_PROGRAMS = jbig2dec
noinst_PROGRAMS = test_sha1 test_huffman test_arith

jbig2dec_SOURCES = jbig2dec.c sha1.c sha1.h \
	jbig2.h jbig2_image.h getopt.h \
	os_types.h config_types.h config_win32.h
jbig2dec_LDADD = libjbig2dec.la @LIBOBJS@

dist_man_MANS = jbig2dec.1

EXTRA_DIST = test_jbig2dec.py msvc.mak LICENSE CHANGES

MAINTAINERCLEANFILES = config_types.h.in

TESTS = test_sha1 test_jbig2dec.py test_huffman test_arith

test_sha1_SOURCES = sha1.c sha1.h
test_sha1_CFLAGS = -DTEST

test_arith_SOURCES = jbig2_arith.c
test_arith_CFLAGS = -DTEST
test_arith_LDADD = libjbig2dec.la

test_huffman_SOURCES = jbig2_huffman.c
test_huffman_CFLAGS = -DTEST
test_huffman_LDADD = libjbig2dec.la

