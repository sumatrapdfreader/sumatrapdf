#
# Makefile for building tificc
# Originally written by Bob Friesenhahn, June 2003
# bugs introduced by Marti Maria 

# Don't require all the GNU mandated files
AUTOMAKE_OPTIONS = 1.7 foreign no-dependencies

AM_CPPFLAGS = -I$(top_builddir)/include -I$(top_srcdir)/include \
              -I$(top_srcdir)/utils/common -I$(top_builddir)/utils/common
                

if HasTIFF
bin_PROGRAMS = tificc
else
bin_PROGRAMS =
endif

tificc_LDADD = $(top_builddir)/src/liblcms2.la @TIFFICC_DEPLIBS@
tificc_LDFLAGS = @LDFLAGS@
tificc_SOURCES = tificc.c ../common/xgetopt.c ../common/vprf.c ../common/utils.h
man_MANS = tificc.1


EXTRA_DIST = $(man_MANS)
