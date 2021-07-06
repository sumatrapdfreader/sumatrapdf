# Process this file with automake to produce Makefile.in

NULL =
EXTRA_DIST =
CLEANFILES =
DISTCLEANFILES =
MAINTAINERCLEANFILES =

EXTRA_DIST += meson.build

include Makefile.sources

# Convenience targets:
lib:
	@$(MAKE) $(AM_MAKEFLAGS) -C $(top_builddir)/src lib
libs:
	@$(MAKE) $(AM_MAKEFLAGS) -C $(top_builddir)/src libs

bin_PROGRAMS =

AM_CPPFLAGS = \
	-DHB_DISABLE_DEPRECATED \
	-I$(top_srcdir)/src/ \
	-I$(top_builddir)/src/ \
	$(GLIB_CFLAGS) \
	$(FREETYPE_CFLAGS) \
	$(CAIRO_FT_CFLAGS) \
	$(NULL)
LDADD = \
	$(top_builddir)/src/libharfbuzz.la \
	-lm \
	$(GLIB_LIBS) \
	$(FREETYPE_LIBS) \
	$(NULL)

if HAVE_GLIB

if HAVE_FREETYPE
if HAVE_CAIRO_FT
hb_view_SOURCES = $(HB_VIEW_sources)
hb_view_LDADD = \
	$(LDADD) \
	$(CAIRO_LIBS) \
	$(CAIRO_FT_LIBS) \
	$(NULL)
bin_PROGRAMS += hb-view
endif # HAVE_CAIRO_FT
endif # HAVE_FREETYPE

hb_shape_SOURCES = $(HB_SHAPE_sources)
bin_PROGRAMS += hb-shape

hb_subset_SOURCES = $(HB_SUBSET_CLI_sources)
hb_subset_LDADD = \
	$(top_builddir)/src/libharfbuzz-subset.la \
	$(LDADD)

bin_PROGRAMS += hb-subset

hb_ot_shape_closure_SOURCES = $(HB_OT_SHAPE_CLOSURE_sources)
bin_PROGRAMS += hb-ot-shape-closure

endif # HAVE_GLIB

#if HAVE_FONTCONFIG
#hb_fc_list_SOURCES = \
#	hb-fc.cc \
#	hb-fc.h \
#	hb-fc-list.c \
#	$(NULL)
#hb_fc_list_LDADD = \
#	$(LDADD) \
#	$(FONTCONFIG_LIBS) \
#	$(NULL)
#bin_PROGRAMS += hb-fc-list
#endif # HAVE_FONTCONFIG

-include $(top_srcdir)/git.mk
