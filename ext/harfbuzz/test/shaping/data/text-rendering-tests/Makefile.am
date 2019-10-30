# Process this file with automake to produce Makefile.in

NULL =

# Convenience targets:
lib:
	@$(MAKE) $(AM_MAKEFLAGS) -C $(top_builddir)/src lib

update:
	(cd $(srcdir) && ./update.sh)

EXTRA_DIST = \
	README \
	COPYING \
	update.sh \
	extract-tests.py \
	fonts \
	$(TESTS) \
	$(NULL)

TEST_EXTENSIONS = .tests
TESTS_LOG_COMPILER = $(srcdir)/../../run-tests.py $(top_builddir)/util/hb-shape$(EXEEXT)

include Makefile.sources

-include $(top_srcdir)/git.mk
