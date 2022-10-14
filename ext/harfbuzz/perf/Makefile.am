# Process this file with automake to produce Makefile.in

NULL =
EXTRA_DIST =
SUBDIRS =

EXTRA_DIST += \
	meson.build \
	benchmark-font.cc \
	benchmark-map.cc \
	benchmark-ot.cc \
	benchmark-set.cc \
	benchmark-shape.cc \
	benchmark-subset.cc \
	fonts \
	texts \
	$(NULL)

# Convenience targets:
lib:
	@$(MAKE) $(AM_MAKEFLAGS) -C $(top_builddir)/src lib

-include $(top_srcdir)/git.mk
