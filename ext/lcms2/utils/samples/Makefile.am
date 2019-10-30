#
# Makefile for building lcms sample programs
# Originally Written by Bob Friesenhahn, June 2003
# Additions and bugs by Marti Maria 

# Don't require all the GNU mandated files
AUTOMAKE_OPTIONS = 1.7 foreign no-dependencies

AM_CPPFLAGS =  -I$(top_builddir)/include -I$(top_srcdir)/include \
               -I$(top_srcdir)/utils/common -I$(top_builddir)/utils/common

bin_PROGRAMS = wtpt 

wtpt_LDADD = $(top_builddir)/src/liblcms2.la 
wtpt_LDFLAGS = @LDFLAGS@
wtpt_SOURCES = wtpt.c ../common/xgetopt.c ../common/vprf.c ../common/utils.h
wtpt_MANS = wtpt.1

EXTRA_DIST = $(man_MANS) roundtrip.c mktiff8.c mkgrayer.c mkcmy.c itufax.c
