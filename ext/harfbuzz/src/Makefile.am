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

EXTRA_DIST += harfbuzz.cc harfbuzz-subset.cc
EXTRA_DIST += meson.build
EXTRA_DIST += fix_get_types.py

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
HB_HAS_GLIB_DEF = define HB_HAS_GLIB 1
else
HB_HAS_GLIB_DEF = undef HB_HAS_GLIB
endif

if HAVE_FREETYPE
HBCFLAGS += $(FREETYPE_CFLAGS)
HBLIBS   += $(FREETYPE_LIBS)
HBDEPS   += $(FREETYPE_DEPS)
HBSOURCES += $(HB_FT_sources)
HBHEADERS += $(HB_FT_headers)
HB_HAS_FREETYPE_DEF = define HB_HAS_FREETYPE 1
else
HB_HAS_FREETYPE_DEF = undef HB_HAS_FREETYPE
endif

if HAVE_GRAPHITE2
HBCFLAGS += $(GRAPHITE2_CFLAGS)
HBLIBS   += $(GRAPHITE2_LIBS)
HBDEPS   += $(GRAPHITE2_DEPS)
HBSOURCES += $(HB_GRAPHITE2_sources)
HBHEADERS += $(HB_GRAPHITE2_headers)
HB_HAS_GRAPHITE_DEF = define HB_HAS_GRAPHITE 1
else
HB_HAS_GRAPHITE_DEF = undef HB_HAS_GRAPHITE
endif

if HAVE_UNISCRIBE
HBCFLAGS += $(UNISCRIBE_CFLAGS)
HBNONPCLIBS += $(UNISCRIBE_LIBS)
HBSOURCES += $(HB_UNISCRIBE_sources)
HBHEADERS += $(HB_UNISCRIBE_headers)
HB_HAS_UNISCRIBE_DEF = define HB_HAS_UNISCRIBE 1
else
HB_HAS_UNISCRIBE_DEF = undef HB_HAS_UNISCRIBE
endif

if HAVE_DIRECTWRITE
HBCFLAGS += $(DIRECTWRITE_CXXFLAGS)
HBNONPCLIBS += $(DIRECTWRITE_LIBS)
HBSOURCES += $(HB_DIRECTWRITE_sources)
HBHEADERS += $(HB_DIRECTWRITE_headers)
HB_HAS_DIRECTWRITE_DEF = define HB_HAS_DIRECTWRITE 1
else
HB_HAS_DIRECTWRITE_DEF = undef HB_HAS_DIRECTWRITE
endif

if HAVE_GDI
HBCFLAGS += $(GDI_CXXFLAGS)
HBNONPCLIBS += $(GDI_LIBS)
HBSOURCES += $(HB_GDI_sources)
HBHEADERS += $(HB_GDI_headers)
HB_HAS_GDI_DEF = define HB_HAS_GDI 1
else
HB_HAS_GDI_DEF = undef HB_HAS_GDI
endif

if HAVE_CORETEXT
HBCFLAGS += $(CORETEXT_CFLAGS)
HBNONPCLIBS += $(CORETEXT_LIBS)
HBSOURCES += $(HB_CORETEXT_sources)
HBHEADERS += $(HB_CORETEXT_headers)
HB_HAS_CORETEXT_DEF = define HB_HAS_CORETEXT 1
else
HB_HAS_CORETEXT_DEF = undef HB_HAS_CORETEXT
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
EXTRA_DIST += hb-version.h.in hb-features.h.in harfbuzz.pc.in harfbuzz-config.cmake.in

lib_LTLIBRARIES += libharfbuzz-subset.la
libharfbuzz_subset_la_LINK = $(chosen_linker) $(libharfbuzz_subset_la_LDFLAGS)
libharfbuzz_subset_la_SOURCES = $(HB_SUBSET_sources)
libharfbuzz_subset_la_CPPFLAGS = $(HBCFLAGS) $(CODE_COVERAGE_CFLAGS)
libharfbuzz_subset_la_LDFLAGS = $(base_link_flags) $(export_symbols_subset) $(CODE_COVERAGE_LDFLAGS)
libharfbuzz_subset_la_LIBADD = libharfbuzz.la
EXTRA_libharfbuzz_subset_la_DEPENDENCIES = $(harfbuzz_subset_def_dependency)
pkginclude_HEADERS += $(HB_SUBSET_headers)
pkgconfig_DATA += harfbuzz-subset.pc
EXTRA_DIST += harfbuzz-subset.pc.in

harfbuzz-subset.cc: Makefile.sources
	$(AM_V_GEN) \
	LANG=C; \
	for f in \
		$(HB_BASE_sources) \
		$(HB_SUBSET_sources) \
		; do echo '#include "'$$f'"'; done | \
	sort -u | \
	grep '[.]cc"' > $(srcdir)/harfbuzz-subset.cc \
	|| ($(RM) $(srcdir)/harfbuzz-subset.cc; false)
BUILT_SOURCES += harfbuzz-subset.cc

if HAVE_ICU
if HAVE_ICU_BUILTIN
HBCFLAGS += $(ICU_CFLAGS)
HBLIBS += $(ICU_LIBS)
HBSOURCES += $(HB_ICU_sources)
HBHEADERS += $(HB_ICU_headers)
HB_HAS_ICU_DEF = define HB_HAS_ICU 1
else
lib_LTLIBRARIES += libharfbuzz-icu.la
libharfbuzz_icu_la_SOURCES = $(HB_ICU_sources)
libharfbuzz_icu_la_CPPFLAGS = $(HBCFLAGS) $(ICU_CFLAGS) $(CODE_COVERAGE_CFLAGS)
libharfbuzz_icu_la_LDFLAGS = $(base_link_flags) $(export_symbols_icu) $(CODE_COVERAGE_LDFLAGS)
libharfbuzz_icu_la_LIBADD = $(ICU_LIBS) libharfbuzz.la
EXTRA_libharfbuzz_icu_la_DEPENDENCIES = $(harfbuzz_icu_def_dependency)
pkginclude_HEADERS += $(HB_ICU_headers)
pkgconfig_DATA += harfbuzz-icu.pc
HB_HAS_ICU_DEF = undef HB_HAS_ICU
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


BUILT_SOURCES += \
	hb-features.h
DISTCLEANFILES += \
	hb-features.h

hb-features.h: hb-features.h.in $(top_builddir)/config.status
	$(AM_V_GEN) $(SED) \
		-e 's/mesondefine HB_HAS_FREETYPE/$(HB_HAS_FREETYPE_DEF)/' \
		-e 's/mesondefine HB_HAS_GDI/$(HB_HAS_GDI_DEF)/' \
		-e 's/mesondefine HB_HAS_GDI/$(HB_HAS_GDI_DEF)/' \
		-e 's/mesondefine HB_HAS_GRAPHITE/$(HB_HAS_GRAPHITE_DEF)/' \
		-e 's/mesondefine HB_HAS_GLIB/$(HB_HAS_GLIB_DEF)/' \
		-e 's/mesondefine HB_HAS_UNISCRIBE/$(HB_HAS_UNISCRIBE_DEF)/' \
		-e 's/mesondefine HB_HAS_DIRECTWRITE/$(HB_HAS_DIRECTWRITE_DEF)/' \
		-e 's/mesondefine HB_HAS_CORETEXT/$(HB_HAS_CORETEXT_DEF)/' \
		-e 's/mesondefine HB_HAS_ICU/$(HB_HAS_ICU_DEF)/' \
		"$<" > "$@" || ($(RM) "$@"; false)


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
harfbuzz.def: $(top_builddir)/config.status
harfbuzz.def: $(HBHEADERS)
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
	gen-arabic-joining-list.py \
	gen-arabic-table.py \
	gen-def.py \
	gen-emoji-table.py \
	gen-harfbuzzcc.py \
	gen-hb-version.py \
	gen-indic-table.py \
	gen-os2-unicode-ranges.py \
	gen-ragel-artifacts.py \
	gen-tag-table.py \
	gen-ucd-table.py \
	gen-use-table.py \
	gen-vowel-constraints.py \
	$(NULL)
EXTRA_DIST += $(GENERATORS)

built-sources: $(BUILT_SOURCES)

.PHONY: built-sources

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
	LANG=C; \
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
	sort -u | \
	grep '[.]cc"' > $(srcdir)/harfbuzz.cc \
	|| ($(RM) $(srcdir)/harfbuzz.cc; false)
BUILT_SOURCES += harfbuzz.cc

noinst_PROGRAMS = \
	main \
	test \
	test-buffer-serialize \
	test-ot-meta \
	test-ot-name \
	test-ot-glyphname \
	test-gpos-size-params \
	test-gsub-would-substitute \
	test-use-table \
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

test_ot_glyphname_SOURCES = test-ot-glyphname.cc
test_ot_glyphname_CPPFLAGS = $(HBCFLAGS)
test_ot_glyphname_LDADD = libharfbuzz.la $(HBLIBS)

test_use_table_SOURCES = test-use-table.cc
test_use_table_CPPFLAGS = $(HBCFLAGS)
test_use_table_LDADD = libharfbuzz.la $(HBLIBS)

test_gpos_size_params_SOURCES = test-gpos-size-params.cc
test_gpos_size_params_CPPFLAGS = $(HBCFLAGS)
test_gpos_size_params_LDADD = libharfbuzz.la $(HBLIBS)

test_gsub_would_substitute_SOURCES = test-gsub-would-substitute.cc
test_gsub_would_substitute_CPPFLAGS = $(HBCFLAGS) $(FREETYPE_CFLAGS)
test_gsub_would_substitute_LDADD = libharfbuzz.la $(HBLIBS) $(FREETYPE_LIBS)

COMPILED_TESTS = \
	test-algs \
	test-array \
	test-bimap \
	test-iter \
	test-machinery \
	test-map \
	test-multimap \
	test-number \
	test-ot-tag \
	test-priority-queue \
	test-set \
	test-serialize \
	test-unicode-ranges \
	test-vector \
	test-repacker \
	test-classdef-graph \
	$(NULL)
COMPILED_TESTS_CPPFLAGS = $(HBCFLAGS) -DMAIN -UNDEBUG
COMPILED_TESTS_LDADD = libharfbuzz.la $(HBLIBS)
check_PROGRAMS += $(COMPILED_TESTS)
TESTS += $(COMPILED_TESTS)

test_algs_SOURCES = test-algs.cc hb-static.cc
test_algs_CPPFLAGS = $(COMPILED_TESTS_CPPFLAGS)
test_algs_LDADD = $(COMPILED_TESTS_LDADD)

test_array_SOURCES = test-array.cc
test_array_CPPFLAGS = $(HBCFLAGS)
test_array_LDADD = libharfbuzz.la $(HBLIBS)

test_bimap_SOURCES = test-bimap.cc hb-static.cc
test_bimap_CPPFLAGS = $(COMPILED_TESTS_CPPFLAGS)
test_bimap_LDADD = $(COMPILED_TESTS_LDADD)

test_iter_SOURCES = test-iter.cc hb-static.cc
test_iter_CPPFLAGS = $(COMPILED_TESTS_CPPFLAGS)
test_iter_LDADD = $(COMPILED_TESTS_LDADD)

test_machinery_SOURCES = test-machinery.cc hb-static.cc
test_machinery_CPPFLAGS = $(COMPILED_TESTS_CPPFLAGS)
test_machinery_LDADD = $(COMPILED_TESTS_LDADD)

test_map_SOURCES = test-map.cc hb-static.cc
test_map_CPPFLAGS = $(COMPILED_TESTS_CPPFLAGS)
test_map_LDADD = $(COMPILED_TESTS_LDADD)

test_multimap_SOURCES = test-multimap.cc hb-static.cc
test_multimap_CPPFLAGS = $(COMPILED_TESTS_CPPFLAGS)
test_multimap_LDADD = $(COMPILED_TESTS_LDADD)

test_number_SOURCES = test-number.cc hb-number.cc
test_number_CPPFLAGS = $(COMPILED_TESTS_CPPFLAGS)
test_number_LDADD = $(COMPILED_TESTS_LDADD)

test_ot_tag_SOURCES = hb-ot-tag.cc
test_ot_tag_CPPFLAGS = $(COMPILED_TESTS_CPPFLAGS)
test_ot_tag_LDADD = $(COMPILED_TESTS_LDADD)

test_priority_queue_SOURCES = test-priority-queue.cc hb-static.cc
test_priority_queue_CPPFLAGS = $(HBCFLAGS)
test_priority_queue_LDADD = libharfbuzz.la $(HBLIBS)

test_repacker_SOURCES = test-repacker.cc hb-static.cc graph/gsubgpos-context.cc
test_repacker_CPPFLAGS = $(HBCFLAGS)
test_repacker_LDADD = libharfbuzz.la libharfbuzz-subset.la $(HBLIBS)

test_classdef_graph_SOURCES = graph/test-classdef-graph.cc hb-static.cc graph/gsubgpos-context.cc
test_classdef_graph_CPPFLAGS = $(HBCFLAGS)
test_classdef_graph_LDADD = libharfbuzz.la libharfbuzz-subset.la $(HBLIBS)

test_set_SOURCES = test-set.cc hb-static.cc
test_set_CPPFLAGS = $(COMPILED_TESTS_CPPFLAGS)
test_set_LDADD = $(COMPILED_TESTS_LDADD)

test_serialize_SOURCES = test-serialize.cc hb-static.cc
test_serialize_CPPFLAGS = $(COMPILED_TESTS_CPPFLAGS)
test_serialize_LDADD = $(COMPILED_TESTS_LDADD)

test_unicode_ranges_SOURCES = test-unicode-ranges.cc
test_unicode_ranges_CPPFLAGS = $(COMPILED_TESTS_CPPFLAGS)
test_unicode_ranges_LDADD = $(COMPILED_TESTS_LDADD)

test_vector_SOURCES = test-vector.cc hb-static.cc
test_vector_CPPFLAGS = $(COMPILED_TESTS_CPPFLAGS)
test_vector_LDADD = $(COMPILED_TESTS_LDADD)

dist_check_SCRIPTS = \
	check-c-linkage-decls.py \
	check-externs.py \
	check-header-guards.py \
	check-includes.py \
	check-static-inits.py \
	check-symbols.py \
	$(NULL)
TESTS += $(dist_check_SCRIPTS)

if !WITH_LIBSTDCXX
dist_check_SCRIPTS += \
	check-libstdc++.py \
	$(NULL)
endif

TESTS_ENVIRONMENT = \
	srcdir="$(srcdir)" \
	base_srcdir="$(srcdir)" \
	builddir="$(builddir)" \
	MAKE="$(MAKE) $(AM_MAKEFLAGS)" \
	HBSOURCES="$(HBSOURCES)" \
	HBHEADERS="$(HBHEADERS)" \
	LDD="$(LDD)" \
	NM="$(NM)" \
	OBJDUMP="$(OBJDUMP)" \
	OTOOL="$(OTOOL)" \
	$(NULL)

if HAVE_INTROSPECTION

-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS = HarfBuzz-0.0.gir # What does the 0 mean anyway?!
INTROSPECTION_SCANNER_ARGS = \
	-I$(srcdir) \
	--warn-all --verbose \
	--namespace=HarfBuzz \
	--nsversion=0.0 \
	--symbol-prefix=hb \
	--symbol-prefix=hb_gobject \
	--identifier-prefix=hb_ \
	--pkg-export=harfbuzz-gobject \
	--c-include=hb-gobject.h
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)
INTROSPECTION_SCANNER_ENV = CC="$(CC)"

HarfBuzz-0.0.gir: libharfbuzz.la libharfbuzz-gobject.la
HarfBuzz_0_0_gir_INCLUDES = GObject-2.0 freetype2-2.0
HarfBuzz_0_0_gir_CFLAGS = \
	$(INCLUDES) \
	$(HBCFLAGS) \
	-DHB_NO_SINGLE_HEADER_ERROR \
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
