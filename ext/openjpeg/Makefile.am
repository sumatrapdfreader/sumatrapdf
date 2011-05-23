MAINTAINERCLEANFILES = Makefile.in

includesdir = $(includedir)/openjpeg-$(MAJOR_NR).$(MINOR_NR)
includes_HEADERS = openjpeg.h

lib_LTLIBRARIES = libopenjpeg.la

libopenjpeg_la_CPPFLAGS = \
-I. \
-I$(top_srcdir)/libopenjpeg \
-I$(top_builddir)/libopenjpeg \
@LCMS1_CFLAGS@ \
@LCMS2_CFLAGS@
libopenjpeg_la_CFLAGS =
libopenjpeg_la_LIBADD = @LCMS1_LIBS@ @LCMS2_LIBS@
libopenjpeg_la_LDFLAGS = -no-undefined -version-info @lt_version@

libopenjpeg_la_SOURCES = \
bio.c \
cio.c \
dwt.c \
event.c \
image.c \
j2k.c \
j2k_lib.c \
jp2.c \
jpt.c \
mct.c \
mqc.c \
openjpeg.c \
pi.c \
raw.c \
t1.c \
t2.c \
tcd.c \
tgt.c \
bio.h \
cio.h \
dwt.h \
event.h \
fix.h \
image.h \
int.h \
j2k.h \
j2k_lib.h \
jp2.h \
jpt.h \
mct.h \
mqc.h \
pi.h \
raw.h \
t1.h \
t1_luts.h \
t2.h \
tcd.h \
tgt.h \
opj_malloc.h \
opj_includes.h

EXTRA_DIST = \
CMakeLists.txt

install-data-hook:
	cd $(DESTDIR)$(includedir) && \
	  rm -f openjpeg.h && \
	  $(LN_S) openjpeg-$(MAJOR_NR).$(MINOR_NR)/openjpeg.h \
	          openjpeg.h
	@rm -rf $(top_builddir)/report.txt
	@echo -e " (LA)\t$(libdir)/libopenjpeg.la" >> $(top_builddir)/report.txt
	@( $(call solist) ) >> $(top_builddir)/report.txt
	@echo -e " (A)\t$(base)/$(a)" >> $(top_builddir)/report.txt
	@echo -e " (H)\t$(includedir)/openjpeg-$(MAJOR_NR).$(MINOR_NR)/openjpeg.h" >> $(top_builddir)/report.txt
	@echo -e " (LN)\t$(includedir)/openjpeg.h" >> $(top_builddir)/report.txt

uninstall-hook:
	rm -f $(DESTDIR)$(includedir)/openjpeg.h

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
