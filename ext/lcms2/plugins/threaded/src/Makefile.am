#
# Makefile for building lcms2_threaded plugin
#

# Don't require all the GNU mandated files
AUTOMAKE_OPTIONS = 1.7 foreign

includedir = ${prefix}/include

# Shared libraries built in this directory
lib_LTLIBRARIES = liblcms2_threaded.la

LIBRARY_CURRENT    = 1
LIBRARY_REVISION   = 0
LIBRARY_AGE        = 0

AM_CPPFLAGS = -Ofast -I$(top_builddir)/include -I$(top_srcdir)/include -I$(srcdir)/../include -I$(builddir)/../include

liblcms2_threaded_la_LDFLAGS = -no-undefined \
  -version-info $(LIBRARY_CURRENT):$(LIBRARY_REVISION):$(LIBRARY_AGE)

liblcms2_threaded_la_LIBADD = $(LCMS_LIB_DEPLIBS) $(top_builddir)/src/liblcms2.la  

liblcms2_threaded_la_SOURCES = threaded_split.c threaded_core.c threaded_main.c  threaded_scheduler.c threaded_internal.h



