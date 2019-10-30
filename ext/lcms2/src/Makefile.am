#
# Makefile for building lcms 2 library
#

# Don't require all the GNU mandated files
AUTOMAKE_OPTIONS = 1.7 foreign

# CFLAGS = -pedantic -Wall -std=c99 -O3

includedir = ${prefix}/include

# Shared libraries built in this directory
lib_LTLIBRARIES = liblcms2.la

LIBRARY_CURRENT    = @LIBRARY_CURRENT@
LIBRARY_REVISION   = @LIBRARY_REVISION@
LIBRARY_AGE        = @LIBRARY_AGE@

AM_CPPFLAGS = -I$(top_builddir)/include -I$(top_srcdir)/include

liblcms2_la_LDFLAGS = -no-undefined \
  -version-info $(LIBRARY_CURRENT):$(LIBRARY_REVISION):$(LIBRARY_AGE)

liblcms2_la_LIBADD = $(LCMS_LIB_DEPLIBS)

liblcms2_la_SOURCES = \
  cmscnvrt.c cmserr.c cmsgamma.c cmsgmt.c cmsintrp.c cmsio0.c cmsio1.c cmslut.c \
  cmsplugin.c cmssm.c cmsmd5.c cmsmtrx.c cmspack.c cmspcs.c cmswtpnt.c cmsxform.c \
  cmssamp.c cmsnamed.c cmscam02.c cmsvirt.c cmstypes.c cmscgats.c cmsps2.c cmsopt.c \
  cmshalf.c cmsalpha.c lcms2_internal.h

