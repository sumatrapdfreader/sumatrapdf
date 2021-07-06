#
# Makefile for building lcms2_fast_float library
#

# Don't require all the GNU mandated files
AUTOMAKE_OPTIONS = 1.7 foreign

includedir = ${prefix}/include

# Shared libraries built in this directory
lib_LTLIBRARIES = liblcms2mt_fast_float.la

LIBRARY_CURRENT    = 1
LIBRARY_REVISION   = 1
LIBRARY_AGE        = 0

AM_CPPFLAGS = -Ofast -I$(top_builddir)/include -I$(top_srcdir)/include -I$(srcdir)/../include -I$(builddir)/../include

liblcms2mt_fast_float_la_LDFLAGS = -no-undefined \
  -version-info $(LIBRARY_CURRENT):$(LIBRARY_REVISION):$(LIBRARY_AGE)

liblcms2mt_fast_float_la_LIBADD = $(LCMS_LIB_DEPLIBS) $(top_builddir)/src/liblcms2mt.la

liblcms2mt_fast_float_la_SOURCES = fast_8_curves.c fast_8_matsh_sse.c fast_8_matsh.c fast_8_tethra.c \
  fast_16_tethra.c fast_float_15bits.c fast_float_15mats.c fast_float_cmyk.c fast_float_curves.c fast_float_matsh.c  \
  fast_float_separate.c fast_float_sup.c fast_float_tethra.c fast_float_lab.c fast_float_internal.h
