MAINTAINERCLEANFILES = Makefile.in

lib_LTLIBRARIES = libopenjpeg_JPWL.la

OPJ_SRC = \
../bio.c \
../cio.c \
../dwt.c \
../event.c \
../image.c \
../j2k.c \
../j2k_lib.c \
../jp2.c \
../jpt.c \
../mct.c \
../mqc.c \
../openjpeg.c \
../pi.c \
../raw.c \
../t1.c \
../t1_generate_luts.c \
../t2.c \
../tcd.c \
../tgt.c \
../cidx_manager.c \
../phix_manager.c \
../ppix_manager.c \
../thix_manager.c \
../tpix_manager.c

libopenjpeg_JPWL_la_CPPFLAGS = \
-I. \
-I$(top_srcdir)/libopenjpeg \
-I$(top_builddir)/libopenjpeg \
-I$(top_srcdir)/libopenjpeg/jpwl \
-I$(top_builddir)/libopenjpeg/jpwl \
-DUSE_JPWL
libopenjpeg_JPWL_la_CFLAGS =
libopenjpeg_JPWL_la_LIBADD = -lm
libopenjpeg_JPWL_la_LDFLAGS = -no-undefined -version-info @lt_version@
libopenjpeg_JPWL_la_SOURCES = \
$(OPJ_SRC) \
crc.c \
jpwl.c \
jpwl_lib.c \
rs.c \
crc.h \
jpwl.h \
rs.h

install-data-hook:
	@echo -e " (LA)\t$(libdir)/libopenjpeg_JPWL.la" >> $(top_builddir)/report.txt
if BUILD_SHARED
	@( $(call solist) ) >> $(top_builddir)/report.txt
endif
if BUILD_STATIC
	@echo -e " (A)\t$(base)/$(a)" >> $(top_builddir)/report.txt
endif

solist    = $(foreach f, $(dll) $(so), echo -e ' $(SO_PREFIX)\t$(base)/$(f)' ;)
get_tok   = $(shell grep -E "^$(1)=" $(lib_LTLIBRARIES) | cut -d "'" -f 2)
base      = $(call get_tok,libdir)
so        = $(call get_tok,library_names)
a         = $(call get_tok,old_library)

if HAVE_WIN32
SO_PREFIX = (DLL)
dll       = $(call get_tok,dlname)
else
if HAVE_DARWIN
SO_PREFIX = (DY)
dll       =
else
SO_PREFIX = (SO)
dll       =
endif
endif
