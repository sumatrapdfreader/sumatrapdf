# Process this file with automake to produce Makefile.in

NULL =
EXTRA_DIST =
CLEANFILES =
DISTCLEANFILES =
MAINTAINERCLEANFILES =

# Convenience targets:
lib:
	@$(MAKE) $(AM_MAKEFLAGS) -C $(top_builddir)/src fuzzing

$(top_builddir)/src/libharfbuzz-fuzzing.la: lib
$(top_builddir)/src/libharfbuzz-subset-fuzzing.la: lib

EXTRA_DIST += \
	README \
	run-shape-fuzzer-tests.py \
	run-subset-fuzzer-tests.py \
	CMakeLists.txt \
	$(NULL)

check_PROGRAMS = \
	hb-shape-fuzzer \
	hb-subset-fuzzer \
	hb-subset-get-codepoints-fuzzer \
	$(NULL)

AM_CPPFLAGS = \
	-DHB_DISABLE_DEPRECATED \
	-I$(top_srcdir)/src/ \
	-I$(top_builddir)/src/ \
	$(NULL)
LDADD = \
	$(top_builddir)/src/libharfbuzz-fuzzing.la \
	$(NULL)

hb_shape_fuzzer_SOURCES = \
	hb-fuzzer.hh \
	hb-shape-fuzzer.cc \
	main.cc \
	$(NULL)
hb_shape_fuzzer_LDADD = \
	$(LDADD) \
	$(NULL)
hb_shape_fuzzer_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	$(NULL)
hb_shape_fuzzer_DEPENDENCIES = \
	lib \
	$(NULL)

hb_subset_fuzzer_SOURCES = \
	hb-fuzzer.hh \
	hb-subset-fuzzer.cc \
	main.cc \
	$(NULL)
hb_subset_fuzzer_LDADD = \
	$(top_builddir)/src/libharfbuzz-subset-fuzzing.la \
	$(NULL)
hb_subset_fuzzer_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	$(NULL)
hb_subset_fuzzer_DEPENDENCIES = \
	lib \
	$(NULL)

hb_subset_get_codepoints_fuzzer_SOURCES = \
	hb-fuzzer.hh \
	hb-subset-get-codepoints-fuzzer.cc \
	main.cc \
	$(NULL)
hb_subset_get_codepoints_fuzzer_LDADD = \
	$(top_builddir)/src/libharfbuzz-subset-fuzzing.la \
	$(NULL)
hb_subset_get_codepoints_fuzzer_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	$(NULL)
hb_subset_get_codepoints_fuzzer_DEPENDENCIES = \
	lib \
	$(NULL)

check:
	EXEEXT="$(EXEEXT)" srcdir="$(srcdir)" builddir="$(builddir)" $(srcdir)/run-shape-fuzzer-tests.py
	EXEEXT="$(EXEEXT)" srcdir="$(srcdir)" builddir="$(builddir)" $(srcdir)/run-subset-fuzzer-tests.py

-include $(top_srcdir)/git.mk
