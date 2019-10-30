# Process this file with automake to produce Makefile.in

NULL =
EXTRA_DIST =
SUBDIRS = api shaping fuzzing subset

EXTRA_DIST += \
	CMakeLists.txt \
	$(NULL)

# Convenience targets:
lib:
	@$(MAKE) $(AM_MAKEFLAGS) -C $(top_builddir)/src lib

-include $(top_srcdir)/git.mk
