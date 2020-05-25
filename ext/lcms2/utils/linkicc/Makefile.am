#
# Makefile for building lcms sample programs
# Originally Written by Bob Friesenhahn, June 2003
# Additions and bugs by Marti Maria 

# Don't require all the GNU mandated files
AUTOMAKE_OPTIONS = 1.7 foreign no-dependencies

AM_CPPFLAGS =  -I$(top_builddir)/include -I$(top_srcdir)/include \
              -I$(top_srcdir)/utils/common -I$(top_builddir)/utils/common

bin_PROGRAMS = linkicc 

linkicc_LDADD = $(top_builddir)/src/liblcms2mt.la 
linkicc_LDFLAGS = @LDFLAGS@
linkicc_SOURCES = linkicc.c ../common/xgetopt.c ../common/vprf.c ../common/utils.h
man_MANS = linkicc.1

EXTRA_DIST = $(man_MANS)
