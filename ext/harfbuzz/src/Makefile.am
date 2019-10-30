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

# Convenience targets:
lib: $(BUILT_SOURCES) libharfbuzz.la libharfbuzz-subset.la
libs: $(BUILT_SOURCES) $(lib_LTLIBRARIES)
fuzzing: $(BUILT_SOURCES) libharfbuzz-fuzzing.la libharfbuzz-subset-fuzzing.la

lib_LTLIBRARIES = libharfbuzz.la

include Makefile.sources

HBCFLAGS =
HBLIBS =
HBNONPCLIBS =
HBDEPS =
HBSOURCES =  $(HB_BASE_sources)
HBSOURCES += $(HB_BASE_RAGEL_GENERATED_sources)
HBHEADERS = $(HB_BASE_headers)

if HAVE_OT
HBSOURCES += $(HB_OT_sources)
HBSOURCES += $(HB_OT_RAGEL_GENERATED_sources)
HBHEADERS += $(HB_OT_headers)
endif

if HAVE_FALLBACK
HBSOURCES += $(HB_FALLBACK_sources)
endif

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

if HAVE_CORETEXT
HBCFLAGS += $(CORETEXT_CFLAGS)
HBNONPCLIBS += $(CORETEXT_LIBS)
HBSOURCES += $(HB_CORETEXT_sources)
HBHEADERS += $(HB_CORETEXT_headers)
endif

if HAVE_UCDN
SUBDIRS += hb-ucdn
HBCFLAGS += -I$(srcdir)/hb-ucdn
HBLIBS   += hb-ucdn/libhb-ucdn.la
HBSOURCES += $(HB_UCDN_sources)
hb-ucdn/libhb-ucdn.la: ucdn
ucdn:
	@$(MAKE) $(AM_MAKEFLAGS) -C hb-ucdn
endif
DIST_SUBDIRS += hb-ucdn


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

FUZZING_CPPFLAGS = \
	-DHB_NDEBUG \
	-DHB_MAX_NESTING_LEVEL=3 \
	-DHB_SANITIZE_MAX_EDITS=3 \
	-DHB_SANITIZE_MAX_OPS_FACTOR=3 \
	-DHB_SANITIZE_MAX_OPS_MIN=128 \
	-DHB_BUFFER_MAX_LEN_FACTOR=3 \
	-DHB_BUFFER_MAX_LEN_MIN=8 \
	-DHB_BUFFER_MAX_LEN_DEFAULT=128 \
	-DHB_BUFFER_MAX_OPS_FACTOR=8 \
	-DHB_BUFFER_MAX_OPS_MIN=64 \
	-DHB_BUFFER_MAX_OPS_DEFAULT=1024 \
	$(NULL)
EXTRA_LTLIBRARIES = libharfbuzz-fuzzing.la libharfbuzz-subset-fuzzing.la

libharfbuzz_fuzzing_la_LINK = $(chosen_linker) $(libharfbuzz_fuzzing_la_LDFLAGS)
libharfbuzz_fuzzing_la_SOURCES = $(libharfbuzz_la_SOURCES)
libharfbuzz_fuzzing_la_CPPFLAGS = $(HBCFLAGS) $(FUZZING_CPPFLAGS)
libharfbuzz_fuzzing_la_LDFLAGS = $(AM_LDFLAGS)
libharfbuzz_fuzzing_la_LIBADD = $(libharfbuzz_la_LIBADD)
EXTRA_libharfbuzz_fuzzing_la_DEPENDENCIES = $(EXTRA_libharfbuzz_la_DEPENDENCIES)
CLEANFILES += libharfbuzz-fuzzing.la

libharfbuzz_subset_fuzzing_la_LINK = $(chosen_linker) $(libharfbuzz_subset_fuzzing_la_LDFLAGS)
libharfbuzz_subset_fuzzing_la_SOURCES = $(libharfbuzz_subset_la_SOURCES)
libharfbuzz_subset_fuzzing_la_CPPFLAGS = $(HBCFLAGS) $(FUZZING_CPPFLAGS)
libharfbuzz_subset_fuzzing_la_LDFLAGS = $(AM_LDFLAGS)
libharfbuzz_subset_fuzzing_la_LIBADD = $(libharfbuzz_subset_la_LIBADD)
EXTRA_libharfbuzz_subset_fuzzing_la_DEPENDENCIES = $(EXTRA_libharfbuzz_subset_la_DEPENDENCIES)
CLEANFILES += libharfbuzz-subset-fuzzing.la

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


DEF_FILES = harfbuzz.def harfbuzz-subset.def harfbuzz-icu.def
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


GENERATORS = \
	gen-arabic-table.py \
	gen-indic-table.py \
	gen-use-table.py \
	gen-def.py \
	$(NULL)
EXTRA_DIST += $(GENERATORS)

unicode-tables: arabic-table indic-table use-table

arabic-table: gen-arabic-table.py ArabicShaping.txt UnicodeData.txt Blocks.txt
	$(AM_V_GEN) $(builddir)/$^ > $(srcdir)/hb-ot-shape-complex-arabic-table.hh \
	|| ($(RM) $(srcdir)/hb-ot-shape-complex-arabic-table.hh; false)

indic-table: gen-indic-table.py IndicSyllabicCategory.txt IndicPositionalCategory.txt Blocks.txt
	$(AM_V_GEN) $(builddir)/$^ > $(srcdir)/hb-ot-shape-complex-indic-table.cc \
	|| ($(RM) $(srcdir)/hb-ot-shape-complex-indic-table.cc; false)

use-table: gen-use-table.py IndicSyllabicCategory.txt IndicPositionalCategory.txt UnicodeData.txt Blocks.txt
	$(AM_V_GEN) $(builddir)/$^ > $(srcdir)/hb-ot-shape-complex-use-table.cc \
	|| ($(RM) $(srcdir)/hb-ot-shape-complex-use-table.cc; false)

built-sources: $(BUILT_SOURCES)

.PHONY: unicode-tables arabic-table indic-table use-table built-sources

RAGEL_GENERATED = \
	$(patsubst %,$(srcdir)/%,$(HB_BASE_RAGEL_GENERATED_sources)) \
	$(patsubst %,$(srcdir)/%,$(HB_OT_RAGEL_GENERATED_sources)) \
	$(NULL)
BUILT_SOURCES += $(RAGEL_GENERATED)
EXTRA_DIST += \
	$(HB_BASE_RAGEL_sources) \
	$(HB_OT_RAGEL_sources) \
	$(NULL)
# We decided to add ragel-generated files to git...
#MAINTAINERCLEANFILES += $(RAGEL_GENERATED)
$(srcdir)/%.hh: $(srcdir)/%.rl
	$(AM_V_GEN)(cd $(srcdir) && $(RAGEL) -e -F1 -o "$*.hh" "$*.rl") \
	|| ($(RM) "$@"; false)

noinst_PROGRAMS = \
	main \
	test \
	test-buffer-serialize \
	test-size-params \
	test-would-substitute \
	$(NULL)
bin_PROGRAMS =

main_SOURCES = main.cc
main_CPPFLAGS = $(HBCFLAGS)
main_LDADD = libharfbuzz.la $(HBLIBS)

test_SOURCES = test.cc
test_CPPFLAGS = $(HBCFLAGS) $(FREETYPE_CFLAGS)
test_LDADD = libharfbuzz.la $(HBLIBS) $(FREETYPE_LIBS)

test_would_substitute_SOURCES = test-would-substitute.cc
test_would_substitute_CPPFLAGS = $(HBCFLAGS) $(FREETYPE_CFLAGS)
test_would_substitute_LDADD = libharfbuzz.la $(HBLIBS) $(FREETYPE_LIBS)

test_size_params_SOURCES = test-size-params.cc
test_size_params_CPPFLAGS = $(HBCFLAGS)
test_size_params_LDADD = libharfbuzz.la $(HBLIBS)

test_buffer_serialize_SOURCES = test-buffer-serialize.cc
test_buffer_serialize_CPPFLAGS = $(HBCFLAGS)
test_buffer_serialize_LDADD = libharfbuzz.la $(HBLIBS)

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

if HAVE_FREETYPE
if HAVE_CAIRO_FT
check_PROGRAMS += dump-emoji
dump_emoji_SOURCES = dump-emoji.cc
dump_emoji_CPPFLAGS = $(HBCFLAGS) $(FREETYPE_CFLAGS) $(CAIRO_FT_CFLAGS)
dump_emoji_LDADD = libharfbuzz.la $(HBLIBS) $(FREETYPE_LIBS) $(CAIRO_LIBS) $(CAIRO_FT_LIBS)
endif # HAVE_CAIRO_FT
endif # HAVE_FREETYPE

check_PROGRAMS += test-ot-tag test-unicode-ranges
TESTS += test-ot-tag test-unicode-ranges

test_ot_tag_SOURCES = hb-ot-tag.cc
test_ot_tag_CPPFLAGS = $(HBCFLAGS) -DMAIN
test_ot_tag_LDADD = libharfbuzz.la $(HBLIBS)

test_unicode_ranges_SOURCES = test-unicode-ranges.cc
test_unicode_ranges_LDADD = libharfbuzz.la $(HBLIBS)

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
	-DHB_GOBJECT_H \
	-DHB_GOBJECT_H_IN \
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
