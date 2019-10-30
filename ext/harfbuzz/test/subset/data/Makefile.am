# Process this file with automake to produce Makefile.in

NULL =
EXTRA_DIST =
CLEANFILES =
SUBDIRS =

EXTRA_DIST = \
	$(TESTS) \
	expected/basics \
	expected/full-font \
	expected/japanese \
	fonts \
	profiles \
	$(NULL)

# Convenience targets:
lib:
	@$(MAKE) $(AM_MAKEFLAGS) -C $(top_builddir)/src lib

TEST_EXTENSIONS = .tests
TESTS_LOG_COMPILER = $(srcdir)/../run-tests.py $(top_builddir)/util/hb-subset$(EXEEXT)
include Makefile.sources

-include $(top_srcdir)/git.mk
