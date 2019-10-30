# Process this file with automake to produce Makefile.in

NULL =
EXTRA_DIST =
CLEANFILES =
SUBDIRS = data

# Convenience targets:
lib:
	@$(MAKE) $(AM_MAKEFLAGS) -C $(top_builddir)/src lib

EXTRA_DIST += \
	CMakeLists.txt \
	run-tests.py \
	subset_test_suite.py \
	$(NULL)

CLEANFILES += \
	subset_test_suite.py[co] \
	$(NULL)

-include $(top_srcdir)/git.mk
