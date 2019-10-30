#
# Makefile for building jpegicc
# Written by Bob Friesenhahn, June 2003
# Bugs introduced by Marti Maria on October 2004

# Don't require all the GNU mandated files
AUTOMAKE_OPTIONS = 1.7 foreign no-dependencies

AM_CPPFLAGS = -I$(top_builddir)/include -I$(top_srcdir)/include \
              -I$(top_srcdir)/utils/common -I$(top_builddir)/utils/common
if HasJPEG
bin_PROGRAMS = jpgicc
else
bin_PROGRAMS = 
endif

jpgicc_LDADD = $(top_builddir)/src/liblcms2.la @JPEGICC_DEPLIBS@
jpgicc_LDFLAGS = @LDFLAGS@
jpgicc_SOURCES = jpgicc.c iccjpeg.c ../common/xgetopt.c ../common/vprf.c ../common/utils.h
man_MANS = jpgicc.1

EXTRA_DIST = iccjpeg.h $(man_MANS)
