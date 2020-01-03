# Process this file with automake to produce Makefile.in

NULL =
SUBDIRS =
DIST_SUBDIRS =
BUILT_SOURCES =
EXTRA_DIST =
CLEANFILES =
DISTCLEANFILES =
MAINTAINERCLEANFILES =
DISTCHECK_CONFIGURE_FLAGS = --enable-introspection
TESTS =
check_PROGRAMS =

EXTRA_DIST += harfbuzz.cc

# Convenience targets:
lib: $(BUILT_SOURCES) libharfbuzz.la
libs: $(BUILT_SOURCES) $(lib_LTLIBRARIES)
tiny:
	$(MAKE) $(AM_MAKEFLAGS) CPPFLAGS="-Os -DHB_TINY $(CPPFLAGS)" libs
tinyz:
	$(MAKE) $(AM_MAKEFLAGS) CPPFLAGS="-Oz -DHB_TINY $(CPPFLAGS)" libs

lib_LTLIBRARIES = libharfbuzz.la

include Makefile.sources

HBCFLAGS =
HBLIBS =
HBNONPCLIBS =
HBDEPS =
HBSOURCES =  $(HB_BASE_sources)
HBSOURCES += $(HB_BASE_RAGEL_GENERATED_sources)
HBHEADERS = $(HB_BASE_headers)

if HAVE_PTHREAD
HBCFLAGS += $(PTHREAD_CFLAGS)
HBNONPCLIBS += $(PTHREAD_LIBS)
endif

if HAVE_GLIB
HBCFLAGS += $(GLIB_CFLAGS)
HBLIBS   += $(GLIB_LIBS)
HBDEPS   += $(GLIB_DEPS)
HBSOURCES += $(HB_GLIB_sources)
HBHEADERS += $(HB_GLIB_headers)
endif

if HAVE_FREETYPE
HBCFLAGS += $(FREETYPE_CFLAGS)
HBLIBS   += $(FREETYPE_LIBS)
# XXX
# The following creates a recursive dependency on FreeType if FreeType is
# built with HarfBuzz support enabled.  Newer pkg-config handles that just
# fine but pkg-config 0.26 as shipped in Ubuntu 14.04 crashes.  Remove
# in a year or two, or otherwise work around it...
#HBDEPS   += $(FREETYPE_DEPS)
HBSOURCES += $(HB_FT_sources)
HBHEADERS += $(HB_FT_headers)
endif

if HAVE_GRAPHITE2
HBCFLAGS += $(GRAPHITE2_CFLAGS)
HBLIBS   += $(GRAPHITE2_LIBS)
HBDEPS   += $(GRAPHITE2_DEPS)
HBSOURCES += $(HB_GRAPHITE2_sources)
HBHEADERS += $(HB_GRAPHITE2_headers)
endif

if HAVE_UNISCRIBE
HBCFLAGS += $(UNISCRIBE_CFLAGS)
HBNONPCLIBS += $(UNISCRIBE_LIBS)
HBSOURCES += $(HB_UNISCRIBE_sources)
HBHEADERS += $(HB_UNISCRIBE_headers)
endif

if HAVE_DIRECTWRITE
HBCFLAGS += $(DIRECTWRITE_CXXFLAGS)
HBNONPCLIBS += $(DIRECTWRITE_LIBS)
HBSOURCES += $(HB_DIRECTWRITE_sources)
HBHEADERS += $(HB_DIRECTWRITE_headers)
endif

if HAVE_GDI
HBCFLAGS += $(GDI_CXXFLAGS)
HBNONPCLIBS += $(GDI_LIBS)
HBSOURCES += $(HB_GDI_sources)
HBHEADERS += $(HB_GDI_headers)
endif

if HAVE_CORETEXT
HBCFLAGS += $(CORETEXT_CFLAGS)
HBNONPCLIBS += $(CORETEXT_LIBS)
HBSOURCES += $(HB_CORETEXT_sources)
HBHEADERS += $(HB_CORETEXT_headers)
endif


BUILT_SOURCES += \
	hb-version.h

$(srcdir)/hb-version.h: hb-version.h.in $(top_srcdir)/configure.ac
	$(AM_V_GEN) $(SED) \
		-e 's/[@]HB_VERSION_MAJOR@/$(HB_VERSION_MAJOR)/' \
		-e 's/[@]HB_VERSION_MINOR@/$(HB_VERSION_MINOR)/' \
		-e 's/[@]HB_VERSION_MICRO@/$(HB_VERSION_MICRO)/' \
		-e 's/[@]HB_VERSION@/$(HB_VERSION)/' \
		"$<" > "$@" || ($(RM) "$@"; false)

# Put the library together

HBLIBS += $(HBNONPCLIBS)

if OS_WIN32
export_symbols = -export-symbols harfbuzz.def
harfbuzz_def_dependency = harfbuzz.def
export_symbols_subset = -export-symbols harfbuzz-subset.def
harfbuzz_subset_def_dependency = harfbuzz-subset.def
export_symbols_icu = -export-symbols harfbuzz-icu.def
harfbuzz_icu_def_dependency = harfbuzz-icu.def
export_symbols_gobject = -export-symbols harfbuzz-gobject.def
harfbuzz_gobject_def_dependency = harfbuzz-gobject.def
chosen_linker = $(CXXLINK)
else
if WITH_LIBSTDCXX
chosen_linker = $(CXXLINK)
else
if HAVE_GCC
# Use a C linker for GCC, not C++; Don't link to libstdc++
chosen_linker = $(LINK)
else
chosen_linker = $(CXXLINK)
endif
endif
endif

@CODE_COVERAGE_RULES@

base_link_flags = $(AM_LDFLAGS) -lm -version-info $(HB_LIBTOOL_VERSION_INFO) -no-undefined
libharfbuzz_la_LINK = $(chosen_linker) $(libharfbuzz_la_LDFLAGS)
libharfbuzz_la_SOURCES = $(HBSOURCES) $(HBHEADERS)
libharfbuzz_la_CPPFLAGS = $(HBCFLAGS) $(CODE_COVERAGE_CFLAGS)
libharfbuzz_la_LDFLAGS = $(base_link_flags) $(export_symbols) $(CODE_COVERAGE_LDFLAGS)
libharfbuzz_la_LIBADD = $(HBLIBS)
EXTRA_libharfbuzz_la_DEPENDENCIES = $(harfbuzz_def_dependency)
pkginclude_HEADERS = $(HBHEADERS)
nodist_pkginclude_HEADERS =
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = harfbuzz.pc
cmakedir = $(libdir)/cmake/harfbuzz
cmake_DATA = harfbuzz-config.cmake
EXTRA_DIST += hb-version.h.in harfbuzz.pc.in harfbuzz-config.cmake.in

lib_LTLIBRARIES += libharfbuzz-subset.la
libharfbuzz_subset_la_SOURCES = $(HB_SUBSET_sources)
libharfbuzz_subset_la_CPPFLAGS = $(HBCFLAGS) $(CODE_COVERAGE_CFLAGS)
libharfbuzz_subset_la_LDFLAGS = $(base_link_flags) $(export_symbols_subset) $(CODE_COVERAGE_LDFLAGS)
libharfbuzz_subset_la_LIBADD = libharfbuzz.la
EXTRA_libharfbuzz_subset_la_DEPENDENCIES = $(harfbuzz_subset_def_dependency)
pkginclude_HEADERS += $(HB_SUBSET_headers)
pkgconfig_DATA += harfbuzz-subset.pc
EXTRA_DIST += harfbuzz-subset.pc.in

if HAVE_ICU
if HAVE_ICU_BUILTIN
HBCFLAGS += $(ICU_CFLAGS)
HBLIBS += $(ICU_LIBS)
HBSOURCES += $(HB_ICU_sources)
HBHEADERS += $(HB_ICU_headers)
else
lib_LTLIBRARIES += libharfbuzz-icu.la
libharfbuzz_icu_la_SOURCES = $(HB_ICU_sources)
libharfbuzz_icu_la_CPPFLAGS = $(HBCFLAGS) $(ICU_CFLAGS) $(CODE_COVERAGE_CFLAGS)
libharfbuzz_icu_la_LDFLAGS = $(base_link_flags) $(export_symbols_icu) $(CODE_COVERAGE_LDFLAGS)
libharfbuzz_icu_la_LIBADD = $(ICU_LIBS) libharfbuzz.la
EXTRA_libharfbuzz_icu_la_DEPENDENCIES = $(harfbuzz_icu_def_dependency)
pkginclude_HEADERS += $(HB_ICU_headers)
pkgconfig_DATA += harfbuzz-icu.pc
endif
endif
EXTRA_DIST += harfbuzz-icu.pc.in

if HAVE_GOBJECT
lib_LTLIBRARIES += libharfbuzz-gobject.la
libharfbuzz_gobject_la_LINK = $(chosen_linker) $(libharfbuzz_gobject_la_LDFLAGS)
libharfbuzz_gobject_la_SOURCES = $(HB_GOBJECT_DIST_sources)
nodist_libharfbuzz_gobject_la_SOURCES = $(HB_GOBJECT_NODIST_sources)
libharfbuzz_gobject_la_CPPFLAGS = $(HBCFLAGS) $(GOBJECT_CFLAGS) $(CODE_COVERAGE_CFLAGS)
libharfbuzz_gobject_la_LDFLAGS = $(base_link_flags) $(CODE_COVERAGE_LDFLAGS)
libharfbuzz_gobject_la_LIBADD = $(GOBJECT_LIBS) libharfbuzz.la
EXTRA_libharfbuzz_gobject_la_DEPENDENCIES = $(harfbuzz_gobject_def_dependency)
pkginclude_HEADERS += $(HB_GOBJECT_DIST_headers)
nodist_pkginclude_HEADERS += $(HB_GOBJECT_NODIST_headers)
pkgconfig_DATA += harfbuzz-gobject.pc

BUILT_SOURCES += \
	$(HB_GOBJECT_ENUM_sources) \
	$(HB_GOBJECT_ENUM_headers) \
	$(NULL)
DISTCLEANFILES += \
	$(HB_GOBJECT_ENUM_sources) \
	$(HB_GOBJECT_ENUM_headers) \
	$(NULL)
hb-gobject-enums.%: hb-gobject-enums.%.tmpl $(HBHEADERS)
	$(AM_V_GEN) PYTHONIOENCODING=UTF-8 $(GLIB_MKENUMS) \
		--identifier-prefix hb_ --symbol-prefix hb_gobject \
		--template $^ | \
	sed 's/_t_get_type/_get_type/g; s/_T (/ (/g' > "$@" \
	|| ($(RM) "$@"; false)
endif
EXTRA_DIST += \
	harfbuzz-gobject.pc.in \
	hb-gobject-enums.cc.tmpl \
	hb-gobject-enums.h.tmpl \
	$(NULL)


%.pc: %.pc.in $(top_builddir)/config.status
	$(AM_V_GEN) \
	$(SED)	-e 's@%prefix%@$(prefix)@g' \
		-e 's@%exec_prefix%@$(exec_prefix)@g' \
		-e 's@%libdir%@$(libdir)@g' \
		-e 's@%includedir%@$(includedir)@g' \
		-e 's@%libs_private%@$(HBNONPCLIBS)@g' \
		-e 's@%requires_private%@$(HBDEPS)@g' \
		-e 's@%VERSION%@$(VERSION)@g' \
	"$<" > "$@" \
	|| ($(RM) "$@"; false)

CLEANFILES += $(pkgconfig_DATA)


DEF_FILES = harfbuzz.def harfbuzz-subset.def harfbuzz-icu.def harfbuzz-deprecated-symbols.txt
if HAVE_GOBJECT
DEF_FILES += harfbuzz-gobject.def
endif
check: $(DEF_FILES) # For check-symbols.sh
CLEANFILES += $(DEF_FILES)
harfbuzz.def: $(HBHEADERS) $(HBNODISTHEADERS)
	$(AM_V_GEN) $(srcdir)/gen-def.py "$@" $^
harfbuzz-subset.def: $(HB_SUBSET_headers)
	$(AM_V_GEN) $(srcdir)/gen-def.py "$@" $^
harfbuzz-icu.def: $(HB_ICU_headers)
	$(AM_V_GEN) $(srcdir)/gen-def.py "$@" $^
harfbuzz-gobject.def: $(HB_GOBJECT_headers)
	$(AM_V_GEN) $(srcdir)/gen-def.py "$@" $^
harfbuzz-deprecated-symbols.txt: $(srcdir)/hb-deprecated.h
	$(AM_V_GEN) PLAIN_LIST=1 $(srcdir)/gen-def.py "$@" $^


GENERATORS = \
	gen-arabic-table.py \
	gen-def.py \
	gen-emoji-table.py \
	gen-indic-table.py \
	gen-os2-unicode-ranges.py \
	gen-tag-table.py \
	gen-ucd-table.py \
	gen-use-table.py \
	gen-vowel-constraints.py \
	$(NULL)
EXTRA_DIST += $(GENERATORS)

unicode-tables: \
	arabic-table \
	emoji-table \
	indic-table \
	tag-table \
	ucd-table \
	use-table \
	emoji-table \
	$(NULL)

arabic-table: gen-arabic-table.py ArabicShaping.txt UnicodeData.txt Blocks.txt
	$(AM_V_GEN) $(builddir)/$^ > $(srcdir)/hb-ot-shape-complex-arabic-table.hh \
	|| ($(RM) $(srcdir)/hb-ot-shape-complex-arabic-table.hh; false)
emoji-table: gen-emoji-table.py emoji-data.txt
	$(AM_V_GEN) $(builddir)/$^ > $(srcdir)/hb-unicode-emoji-table.hh \
	|| ($(RM) $(srcdir)/hb-unicode-emoji-table.hh; false)
indic-table: gen-indic-table.py IndicSyllabicCategory.txt IndicPositionalCategory.txt Blocks.txt
	$(AM_V_GEN) $(builddir)/$^ > $(srcdir)/hb-ot-shape-complex-indic-table.cc \
	|| ($(RM) $(srcdir)/hb-ot-shape-complex-indic-table.cc; false)
tag-table: gen-tag-table.py languagetags language-subtag-registry
	$(AM_V_GEN) $(builddir)/$^ > $(srcdir)/hb-ot-tag-table.hh \
	|| ($(RM) $(srcdir)/hb-ot-tag-table.hh; false)
ucd-table: gen-ucd-table.py ucd.nounihan.grouped.zip hb-common.h
	$(AM_V_GEN) $(builddir)/$^ > $(srcdir)/hb-ucd-table.hh \
	|| ($(RM) $(srcdir)/hb-ucd-table.hh; false)
use-table: gen-use-table.py IndicSyllabicCategory.txt IndicPositionalCategory.txt UnicodeData.txt Blocks.txt
	$(AM_V_GEN) $(builddir)/$^ > $(srcdir)/hb-ot-shape-complex-use-table.cc \
	|| ($(RM) $(srcdir)/hb-ot-shape-complex-use-table.cc; false)
vowel-constraints: gen-vowel-constraints.py HBIndicVowelConstraints.txt Scripts.txt
	$(AM_V_GEN) $(builddir)/$^ > $(srcdir)/hb-ot-shape-complex-vowel-constraints.cc \
	|| ($(RM) $(srcdir)/hb-ot-shape-complex-vowel-constraints.cc; false)


built-sources: $(BUILT_SOURCES)

.PHONY: unicode-tables arabic-table indic-table tag-table use-table vowel-constraints emoji-table built-sources

RAGEL_GENERATED = \
	$(patsubst %,$(srcdir)/%,$(HB_BASE_RAGEL_GENERATED_sources)) \
	$(NULL)
BUILT_SOURCES += $(RAGEL_GENERATED)
EXTRA_DIST += \
	$(HB_BASE_RAGEL_sources) \
	$(NULL)
# We decided to add ragel-generated files to git...
#MAINTAINERCLEANFILES += $(RAGEL_GENERATED)
$(srcdir)/%.hh: $(srcdir)/%.rl
	$(AM_V_GEN)(cd $(srcdir) && $(RAGEL) -e -F1 -o "$*.hh" "$*.rl") \
	|| ($(RM) "$@"; false)

harfbuzz.cc: Makefile.sources
	$(AM_V_GEN) \
	for f in \
		$(HB_BASE_sources) \
		$(HB_GLIB_sources) \
		$(HB_FT_sources) \
		$(HB_GRAPHITE2_sources) \
		$(HB_UNISCRIBE_sources) \
		$(HB_GDI_sources) \
		$(HB_DIRECTWRITE_sources) \
		$(HB_CORETEXT_sources) \
		; do echo '#include "'$$f'"'; done | \
	grep '[.]cc"' > $(srcdir)/harfbuzz.cc \
	|| ($(RM) $(srcdir)/harfbuzz.cc; false)
BUILT_SOURCES += harfbuzz.cc

noinst_PROGRAMS = \
	main \
	test \
	test-buffer-serialize \
	test-ot-meta \
	test-ot-name \
	test-gpos-size-params \
	test-gsub-would-substitute \
	$(NULL)
bin_PROGRAMS =

main_SOURCES = main.cc
main_CPPFLAGS = $(HBCFLAGS)
main_LDADD = libharfbuzz.la $(HBLIBS)

test_SOURCES = test.cc
test_CPPFLAGS = $(HBCFLAGS) $(FREETYPE_CFLAGS)
test_LDADD = libharfbuzz.la $(HBLIBS) $(FREETYPE_LIBS)

test_buffer_serialize_SOURCES = test-buffer-serialize.cc
test_buffer_serialize_CPPFLAGS = $(HBCFLAGS)
test_buffer_serialize_LDADD = libharfbuzz.la $(HBLIBS)

test_ot_meta_SOURCES = test-ot-meta.cc
test_ot_meta_CPPFLAGS = $(HBCFLAGS)
test_ot_meta_LDADD = libharfbuzz.la $(HBLIBS)

test_ot_name_SOURCES = test-ot-name.cc
test_ot_name_CPPFLAGS = $(HBCFLAGS)
test_ot_name_LDADD = libharfbuzz.la $(HBLIBS)

test_gpos_size_params_SOURCES = test-gpos-size-params.cc
test_gpos_size_params_CPPFLAGS = $(HBCFLAGS)
test_gpos_size_params_LDADD = libharfbuzz.la $(HBLIBS)

test_gsub_would_substitute_SOURCES = test-gsub-would-substitute.cc
test_gsub_would_substitute_CPPFLAGS = $(HBCFLAGS) $(FREETYPE_CFLAGS)
test_gsub_would_substitute_LDADD = libharfbuzz.la $(HBLIBS) $(FREETYPE_LIBS)

if HAVE_FREETYPE
if HAVE_CAIRO_FT
noinst_PROGRAMS += test-ot-color
test_ot_color_SOURCES = test-ot-color.cc
test_ot_color_CPPFLAGS = $(HBCFLAGS) $(FREETYPE_CFLAGS) $(CAIRO_FT_CFLAGS)
test_ot_color_LDADD = libharfbuzz.la $(HBLIBS) $(FREETYPE_LIBS) $(CAIRO_LIBS) $(CAIRO_FT_LIBS)
endif # HAVE_CAIRO_FT
endif # HAVE_FREETYPE

dist_check_SCRIPTS = \
	check-c-linkage-decls.sh \
	check-externs.sh \
	check-header-guards.sh \
	check-includes.sh \
	check-static-inits.sh \
	check-symbols.sh \
	$(NULL)
TESTS += $(dist_check_SCRIPTS)

if !WITH_LIBSTDCXX
dist_check_SCRIPTS += \
	check-libstdc++.sh \
	$(NULL)
endif

check_PROGRAMS += \
	dump-indic-data \
	dump-khmer-data \
	dump-myanmar-data \
	dump-use-data \
	$(NULL)
dump_indic_data_SOURCES = dump-indic-data.cc hb-ot-shape-complex-indic-table.cc
dump_indic_data_CPPFLAGS = $(HBCFLAGS)
dump_indic_data_LDADD = libharfbuzz.la $(HBLIBS)
dump_khmer_data_SOURCES = dump-khmer-data.cc hb-ot-shape-complex-indic-table.cc
dump_khmer_data_CPPFLAGS = $(HBCFLAGS)
dump_khmer_data_LDADD = libharfbuzz.la $(HBLIBS)
dump_myanmar_data_SOURCES = dump-myanmar-data.cc hb-ot-shape-complex-indic-table.cc
dump_myanmar_data_CPPFLAGS = $(HBCFLAGS)
dump_myanmar_data_LDADD = libharfbuzz.la $(HBLIBS)
dump_use_data_SOURCES = dump-use-data.cc hb-ot-shape-complex-use-table.cc
dump_use_data_CPPFLAGS = $(HBCFLAGS)
dump_use_data_LDADD = libharfbuzz.la $(HBLIBS)

COMPILED_TESTS = test-algs test-iter test-meta test-number test-ot-tag test-unicode-ranges test-bimap
COMPILED_TESTS_CPPFLAGS = $(HBCFLAGS) -DMAIN -UNDEBUG
COMPILED_TESTS_LDADD = libharfbuzz.la $(HBLIBS)
check_PROGRAMS += $(COMPILED_TESTS)
TESTS += $(COMPILED_TESTS)

test_algs_SOURCES = test-algs.cc hb-static.cc
test_algs_CPPFLAGS = $(COMPILED_TESTS_CPPFLAGS)
test_algs_LDADD = $(COMPILED_TESTS_LDADD)

test_iter_SOURCES = test-iter.cc hb-static.cc
test_iter_CPPFLAGS = $(COMPILED_TESTS_CPPFLAGS)
test_iter_LDADD = $(COMPILED_TESTS_LDADD)

test_meta_SOURCES = test-meta.cc hb-static.cc
test_meta_CPPFLAGS = $(COMPILED_TESTS_CPPFLAGS)
test_meta_LDADD = $(COMPILED_TESTS_LDADD)

test_number_SOURCES = test-number.cc hb-number.cc
test_number_CPPFLAGS = $(COMPILED_TESTS_CPPFLAGS)
test_number_LDADD = $(COMPILED_TESTS_LDADD)

test_ot_tag_SOURCES = hb-ot-tag.cc
test_ot_tag_CPPFLAGS = $(COMPILED_TESTS_CPPFLAGS)
test_ot_tag_LDADD = $(COMPILED_TESTS_LDADD)

test_unicode_ranges_SOURCES = test-unicode-ranges.cc
test_unicode_ranges_CPPFLAGS = $(COMPILED_TESTS_CPPFLAGS)
test_unicode_ranges_LDADD = $(COMPILED_TESTS_LDADD)

test_bimap_SOURCES = test-bimap.cc hb-static.cc
test_bimap_CPPFLAGS = $(COMPILED_TESTS_CPPFLAGS)
test_bimap_LDADD = $(COMPILED_TESTS_LDADD)

TESTS_ENVIRONMENT = \
	srcdir="$(srcdir)" \
	MAKE="$(MAKE) $(AM_MAKEFLAGS)" \
	HBSOURCES="$(HBSOURCES)" \
	HBHEADERS="$(HBHEADERS)" \
	$(NULL)

if HAVE_INTROSPECTION

-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS = HarfBuzz-0.0.gir # What does the 0 mean anyway?!
INTROSPECTION_SCANNER_ARGS = -I$(srcdir) -n hb --identifier-prefix=hb_ --warn-all
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)
INTROSPECTION_SCANNER_ENV = CC="$(CC)"

HarfBuzz-0.0.gir: libharfbuzz.la libharfbuzz-gobject.la
HarfBuzz_0_0_gir_INCLUDES = GObject-2.0
HarfBuzz_0_0_gir_CFLAGS = \
	$(INCLUDES) \
	$(HBCFLAGS) \
	-DHB_H \
	-DHB_H_IN \
	-DHB_OT_H \
	-DHB_OT_H_IN \
	-DHB_AAT_H \
	-DHB_AAT_H_IN \
	-DHB_GOBJECT_H \
	-DHB_GOBJECT_H_IN \
	-DHAVE_GOBJECT \
	-DHB_EXTERN= \
	$(NULL)
HarfBuzz_0_0_gir_LIBS = \
	libharfbuzz.la \
	libharfbuzz-gobject.la \
	$(NULL)
HarfBuzz_0_0_gir_FILES = \
	$(HBHEADERS) \
	$(HBSOURCES) \
	$(HB_GOBJECT_sources) \
	$(HB_GOBJECT_headers) \
	$(NULL)

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelib_DATA)

endif

-include $(top_srcdir)/git.mk
