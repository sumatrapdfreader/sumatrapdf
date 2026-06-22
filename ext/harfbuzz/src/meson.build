fs = import('fs')

hb_version_h = configure_file(
               command: [find_program('gen-hb-version.py'), meson.project_version(), '@OUTPUT@', '@INPUT@'],
               input: 'hb-version.h.in',
               output: 'hb-version.h',
               install: true,
               install_dir: get_option('includedir') / meson.project_name())

# Base and default-included sources and headers
hb_base_sources = files(
  'hb-aat-layout-ankr-table.hh',
  'hb-aat-layout-bsln-table.hh',
  'hb-aat-layout-common.hh',
  'hb-aat-layout-feat-table.hh',
  'hb-aat-layout-just-table.hh',
  'hb-aat-layout-kerx-table.hh',
  'hb-aat-layout-morx-table.hh',
  'hb-aat-layout-opbd-table.hh',
  'hb-aat-layout-trak-table.hh',
  'hb-aat-layout.cc',
  'hb-aat-layout.hh',
  'hb-aat-ltag-table.hh',
  'hb-aat-map.cc',
  'hb-aat-map.hh',
  'hb-algs.hh',
  'hb-alloc-pool.hh',
  'hb-array.hh',
  'hb-atomic.hh',
  'hb-bimap.hh',
  'hb-bit-page.hh',
  'hb-bit-set.hh',
  'hb-bit-set-invertible.hh',
  'hb-bit-vector.hh',
  'hb-blob.cc',
  'hb-blob.hh',
  'hb-buffer-serialize.cc',
  'hb-buffer-verify.cc',
  'hb-buffer.cc',
  'hb-buffer.hh',
  'hb-cache.hh',
  'hb-cff-interp-common.hh',
  'hb-cff-interp-cs-common.hh',
  'hb-cff-interp-dict-common.hh',
  'hb-cff1-interp-cs.hh',
  'hb-cff2-interp-cs.hh',
  'hb-common.cc',
  'hb-config.hh',
  'hb-debug.hh',
  'hb-decycler.hh',
  'hb-dispatch.hh',
  'hb-draw.cc',
  'hb-draw.hh',
  'hb-geometry.hh',
  'hb-paint.cc',
  'hb-paint.hh',
  'hb-paint-bounded.cc',
  'hb-paint-bounded.hh',
  'hb-paint-extents.cc',
  'hb-paint-extents.hh',
  'hb-face.cc',
  'hb-face.hh',
  'hb-face-builder.cc',
  'hb-fallback-shape.cc',
  'hb-font.cc',
  'hb-font.hh',
  'hb-free-pool.hh',
  'hb-iter.hh',
  'hb-kern.hh',
  'hb-limits.hh',
  'hb-machinery.hh',
  'hb-map.cc',
  'hb-map.hh',
  'hb-meta.hh',
  'hb-ms-feature-ranges.hh',
  'hb-multimap.hh',
  'hb-mutex.hh',
  'hb-null.hh',
  'hb-number.cc',
  'hb-number.hh',
  'hb-object.hh',
  'hb-open-file.hh',
  'hb-open-type.hh',
  'hb-ot-cff-common.hh',
  'hb-ot-cff1-std-str.hh',
  'hb-ot-cff1-table.cc',
  'hb-ot-cff1-table.hh',
  'hb-ot-cff2-table.cc',
  'hb-ot-cff2-table.hh',
  'hb-ot-cmap-table.hh',
  'hb-ot-color.cc',
  'hb-ot-face-table-list.hh',
  'hb-ot-face.cc',
  'hb-ot-face.hh',
  'hb-ot-font.cc',
  'hb-ot-gasp-table.hh',
  'hb-ot-glyf-table.hh',
  'hb-ot-hdmx-table.hh',
  'hb-ot-head-table.hh',
  'hb-ot-hhea-table.hh',
  'hb-ot-hmtx-table.hh',
  'hb-ot-kern-table.hh',
  'hb-ot-layout-base-table.hh',
  'hb-ot-layout-common.hh',
  'hb-ot-layout-gdef-table.hh',
  'hb-ot-layout-gpos-table.hh',
  'hb-ot-layout-gsub-table.hh',
  'hb-outline.hh',
  'hb-outline.cc',
  'OT/Color/CBDT/CBDT.hh',
  'OT/Color/COLR/COLR.hh',
  'OT/Color/CPAL/CPAL.hh',
  'OT/Color/sbix/sbix.hh',
  'OT/Color/svg/svg.hh',
  'OT/glyf/glyf.hh',
  'OT/glyf/glyf-helpers.hh',
  'OT/glyf/loca.hh',
  'OT/glyf/path-builder.hh',
  'OT/glyf/Glyph.hh',
  'OT/glyf/GlyphHeader.hh',
  'OT/glyf/SimpleGlyph.hh',
  'OT/glyf/CompositeGlyph.hh',
  'OT/glyf/SubsetGlyph.hh',
  'OT/Layout/types.hh',
  'OT/Layout/Common/Coverage.hh',
  'OT/Layout/Common/CoverageFormat1.hh',
  'OT/Layout/Common/CoverageFormat2.hh',
  'OT/Layout/Common/RangeRecord.hh',
  'OT/Layout/GDEF/GDEF.hh',
  'OT/Layout/GPOS/AnchorFormat1.hh',
  'OT/Layout/GPOS/AnchorFormat2.hh',
  'OT/Layout/GPOS/AnchorFormat3.hh',
  'OT/Layout/GPOS/Anchor.hh',
  'OT/Layout/GPOS/AnchorMatrix.hh',
  'OT/Layout/GPOS/ChainContextPos.hh',
  'OT/Layout/GPOS/Common.hh',
  'OT/Layout/GPOS/ContextPos.hh',
  'OT/Layout/GPOS/CursivePosFormat1.hh',
  'OT/Layout/GPOS/CursivePos.hh',
  'OT/Layout/GPOS/ExtensionPos.hh',
  'OT/Layout/GPOS/GPOS.hh',
  'OT/Layout/GPOS/LigatureArray.hh',
  'OT/Layout/GPOS/MarkArray.hh',
  'OT/Layout/GPOS/MarkBasePosFormat1.hh',
  'OT/Layout/GPOS/MarkBasePos.hh',
  'OT/Layout/GPOS/MarkLigPosFormat1.hh',
  'OT/Layout/GPOS/MarkLigPos.hh',
  'OT/Layout/GPOS/MarkMarkPosFormat1.hh',
  'OT/Layout/GPOS/MarkMarkPos.hh',
  'OT/Layout/GPOS/MarkRecord.hh',
  'OT/Layout/GPOS/PairPosFormat1.hh',
  'OT/Layout/GPOS/PairPosFormat2.hh',
  'OT/Layout/GPOS/PairPos.hh',
  'OT/Layout/GPOS/PairSet.hh',
  'OT/Layout/GPOS/PairValueRecord.hh',
  'OT/Layout/GPOS/PosLookup.hh',
  'OT/Layout/GPOS/PosLookupSubTable.hh',
  'OT/Layout/GPOS/SinglePosFormat1.hh',
  'OT/Layout/GPOS/SinglePosFormat2.hh',
  'OT/Layout/GPOS/SinglePos.hh',
  'OT/Layout/GPOS/ValueFormat.hh',
  'OT/Layout/GSUB/AlternateSet.hh',
  'OT/Layout/GSUB/AlternateSubstFormat1.hh',
  'OT/Layout/GSUB/AlternateSubst.hh',
  'OT/Layout/GSUB/ChainContextSubst.hh',
  'OT/Layout/GSUB/Common.hh',
  'OT/Layout/GSUB/ContextSubst.hh',
  'OT/Layout/GSUB/ExtensionSubst.hh',
  'OT/Layout/GSUB/GSUB.hh',
  'OT/Layout/GSUB/Ligature.hh',
  'OT/Layout/GSUB/LigatureSet.hh',
  'OT/Layout/GSUB/LigatureSubstFormat1.hh',
  'OT/Layout/GSUB/LigatureSubst.hh',
  'OT/Layout/GSUB/MultipleSubstFormat1.hh',
  'OT/Layout/GSUB/MultipleSubst.hh',
  'OT/Layout/GSUB/ReverseChainSingleSubstFormat1.hh',
  'OT/Layout/GSUB/ReverseChainSingleSubst.hh',
  'OT/Layout/GSUB/Sequence.hh',
  'OT/Layout/GSUB/SingleSubstFormat1.hh',
  'OT/Layout/GSUB/SingleSubstFormat2.hh',
  'OT/Layout/GSUB/SingleSubst.hh',
  'OT/Layout/GSUB/SubstLookup.hh',
  'OT/Layout/GSUB/SubstLookupSubTable.hh',
  'OT/name/name.hh',
  'OT/Var/VARC/coord-setter.hh',
  'OT/Var/VARC/VARC.cc',
  'OT/Var/VARC/VARC.hh',
  'hb-ot-layout-gsubgpos.hh',
  'hb-ot-layout-jstf-table.hh',
  'hb-ot-layout.cc',
  'hb-ot-layout.hh',
  'hb-ot-map.cc',
  'hb-ot-map.hh',
  'hb-ot-math-table.hh',
  'hb-ot-math.cc',
  'hb-ot-maxp-table.hh',
  'hb-ot-meta-table.hh',
  'hb-ot-meta.cc',
  'hb-ot-metrics.cc',
  'hb-ot-metrics.hh',
  'hb-ot-name-language-static.hh',
  'hb-ot-name-language.hh',
  'hb-ot-name-table.hh',
  'hb-ot-name.cc',
  'hb-ot-os2-table.hh',
  'hb-ot-os2-unicode-ranges.hh',
  'hb-ot-post-macroman.hh',
  'hb-ot-post-table.hh',
  'hb-ot-post-table-v2subset.hh',
  'hb-ot-shaper-arabic-fallback.hh',
  'hb-ot-shaper-arabic-joining-list.hh',
  'hb-ot-shaper-arabic-pua.hh',
  'hb-ot-shaper-arabic-table.hh',
  'hb-ot-shaper-arabic-win1256.hh',
  'hb-ot-shaper-arabic.cc',
  'hb-ot-shaper-arabic.hh',
  'hb-ot-shaper-default.cc',
  'hb-ot-shaper-hangul.cc',
  'hb-ot-shaper-hebrew.cc',
  'hb-ot-shaper-indic-table.cc',
  'hb-ot-shaper-indic.cc',
  'hb-ot-shaper-indic.hh',
  'hb-ot-shaper-khmer.cc',
  'hb-ot-shaper-myanmar.cc',
  'hb-ot-shaper-syllabic.cc',
  'hb-ot-shaper-syllabic.hh',
  'hb-ot-shaper-thai.cc',
  'hb-ot-shaper-use-table.hh',
  'hb-ot-shaper-use.cc',
  'hb-ot-shaper-vowel-constraints.cc',
  'hb-ot-shaper-vowel-constraints.hh',
  'hb-ot-shaper.hh',
  'hb-ot-shape-fallback.cc',
  'hb-ot-shape-fallback.hh',
  'hb-ot-shape-normalize.cc',
  'hb-ot-shape-normalize.hh',
  'hb-ot-shape.cc',
  'hb-ot-shape.hh',
  'hb-ot-stat-table.hh',
  'hb-ot-tag-table.hh',
  'hb-ot-tag.cc',
  'hb-ot-var-avar-table.hh',
  'hb-ot-var-common.hh',
  'hb-ot-var-cvar-table.hh',
  'hb-ot-var-fvar-table.hh',
  'hb-ot-var-gvar-table.hh',
  'hb-ot-var-hvar-table.hh',
  'hb-ot-var-mvar-table.hh',
  'hb-ot-var-varc-table.hh',
  'hb-ot-var.cc',
  'hb-ot-vorg-table.hh',
  'hb-priority-queue.hh',
  'hb-repacker.hh',
  'hb-sanitize.hh',
  'hb-serialize.hh',
  'hb-set-digest.hh',
  'hb-set.cc',
  'hb-set.hh',
  'hb-shape-plan.cc',
  'hb-shape-plan.hh',
  'hb-shape.cc',
  'hb-shaper-impl.hh',
  'hb-shaper-list.hh',
  'hb-shaper.cc',
  'hb-shaper.hh',
  'hb-static.cc',
  'hb-string-array.hh',
  'hb-style.cc',
  'hb-ucd-table.hh',
  'hb-ucd.cc',
  'hb-unicode-emoji-table.hh',
  'hb-unicode.cc',
  'hb-unicode.hh',
  'hb-utf.hh',
  'hb-vector.hh',
  'hb.hh',
)

hb_base_ragel_generated_sources = files(
  'hb-buffer-deserialize-json.hh',
  'hb-buffer-deserialize-text-glyphs.hh',
  'hb-buffer-deserialize-text-unicode.hh',
  'hb-number-parser.hh',
  'hb-ot-shaper-indic-machine.hh',
  'hb-ot-shaper-khmer-machine.hh',
  'hb-ot-shaper-myanmar-machine.hh',
  'hb-ot-shaper-use-machine.hh',
)
hb_base_ragel_sources = [
  'hb-buffer-deserialize-json.rl',
  'hb-buffer-deserialize-text-glyphs.rl',
  'hb-buffer-deserialize-text-unicode.rl',
  'hb-number-parser.rl',
  'hb-ot-shaper-indic-machine.rl',
  'hb-ot-shaper-khmer-machine.rl',
  'hb-ot-shaper-myanmar-machine.rl',
  'hb-ot-shaper-use-machine.rl',
]

hb_base_headers = files(
  'hb-aat-layout.h',
  'hb-aat.h',
  'hb-blob.h',
  'hb-buffer.h',
  'hb-common.h',
  'hb-cplusplus.hh',
  'hb-deprecated.h',
  'hb-draw.h',
  'hb-paint.h',
  'hb-face.h',
  'hb-font.h',
  'hb-map.h',
  'hb-ot-color.h',
  'hb-ot-deprecated.h',
  'hb-ot-font.h',
  'hb-ot-layout.h',
  'hb-ot-math.h',
  'hb-ot-meta.h',
  'hb-ot-metrics.h',
  'hb-ot-name.h',
  'hb-ot-shape.h',
  'hb-ot-var.h',
  'hb-ot.h',
  'hb-script-list.h',
  'hb-set.h',
  'hb-shape-plan.h',
  'hb-shape.h',
  'hb-style.h',
  'hb-unicode.h',
  'hb.h',
)
hb_base_headers += hb_version_h

# Optional Sources and Headers with external deps

hb_ft_sources = files('hb-ft.cc', 'hb-ft-colr.hh')
hb_ft_headers = files('hb-ft.h')

hb_fontations_sources = files()
hb_fontations_headers = files('hb-fontations.h')

hb_harfrust_sources = files('hb-harfrust.cc')
hb_harfrust_headers = files()

hb_kbts_sources = files('hb-kbts.cc')
hb_kbts_headers = files()

hb_glib_sources = files('hb-glib.cc')
hb_glib_headers = files('hb-glib.h')

hb_graphite2_sources = files('hb-graphite2.cc')
hb_graphite2_headers = files('hb-graphite2.h')

hb_wasm_sources = files(
  'hb-wasm-api.cc',
  'hb-wasm-api.hh',
  'hb-wasm-api-blob.hh',
  'hb-wasm-api-buffer.hh',
  'hb-wasm-api-common.hh',
  'hb-wasm-api-face.hh',
  'hb-wasm-api-font.hh',
  'hb-wasm-api-list.hh',
  'hb-wasm-api-shape.hh',
  'hb-wasm-shape.cc',
)
hb_wasm_headers = files()

# System-dependent sources and headers

hb_coretext_sources = files('hb-coretext.cc', 'hb-coretext.hh', 'hb-coretext-font.cc', 'hb-coretext-shape.cc')
hb_coretext_headers = files('hb-coretext.h')

hb_directwrite_sources = files('hb-directwrite.cc', 'hb-directwrite.hh', 'hb-directwrite-font.cc', 'hb-directwrite-shape.cc')
hb_directwrite_headers = files('hb-directwrite.h')

hb_gdi_sources = files('hb-gdi.cc')
hb_gdi_headers = files('hb-gdi.h')

hb_uniscribe_sources = files('hb-uniscribe.cc')
hb_uniscribe_headers = files('hb-uniscribe.h')

# Sources for libharfbuzz-gobject and libharfbuzz-icu
hb_icu_sources = files('hb-icu.cc')
hb_icu_headers = files('hb-icu.h')

# Sources for libharfbuzz-subset
hb_subset_sources = files(
  'hb-number.cc',
  'hb-number.hh',
  'hb-ot-cff1-table.cc',
  'hb-ot-cff2-table.cc',
  'hb-static.cc',
  'hb-subset-accelerator.hh',
  'hb-subset-cff-common.cc',
  'hb-subset-cff-common.hh',
  'hb-subset-cff1.cc',
  'hb-subset-cff2.cc',
  'hb-subset-cff2-to-cff1.cc',
  'hb-subset-cff2-to-cff1.hh',
  'hb-subset-input.cc',
  'hb-subset-input.hh',
  'hb-subset-instancer-iup.hh',
  'hb-subset-instancer-iup.cc',
  'hb-subset-instancer-solver.hh',
  'hb-subset-instancer-solver.cc',
  'hb-subset-plan.cc',
  'hb-subset-plan-layout.cc',
  'hb-subset-plan-var.cc',
  'hb-subset-plan.hh',
  'hb-subset-plan-member-list.hh',
  'hb-subset-serialize.cc',
  'hb-subset-table.hh',
  'hb-subset-table-layout.cc',
  'hb-subset-table-var.cc',
  'hb-subset-table-cff.cc',
  'hb-subset-table-color.cc',
  'hb-subset-table-other.cc',
  'graph/gsubgpos-context.cc',
  'graph/gsubgpos-context.hh',
  'graph/gsubgpos-graph.hh',
  'graph/pairpos-graph.hh',
  'graph/markbasepos-graph.hh',
  'graph/coverage-graph.hh',
  'graph/classdef-graph.hh',
  'graph/split-helpers.hh',
  'hb-subset.cc',
  'hb-subset.hh',
)

hb_subset_headers = files(
  'hb-subset.h',
  'hb-subset-serialize.h'
)

hb_gobject_sources = files(
  'hb-gobject-structs.cc'
)

hb_gobject_headers = files(
  'hb-gobject.h',
  'hb-gobject-structs.h',
)

hb_failing_alloc_sources = files('failing-alloc.c')

ragel = find_program('ragel', version: '6.10', required: false)
has_ragel = ragel.found()
if not has_ragel and get_option('ragel_subproject')
    ragel = subproject('ragel').get_variable('ragel')
    has_ragel = true
endif
if not has_ragel
  if not meson.is_subproject()
    warning('You have to install ragel if you are going to develop HarfBuzz itself')
  endif
else
  ragel_helper = find_program('gen-ragel-artifacts.py')
  foreach rl : hb_base_ragel_sources
    hh = rl.split('.')[0] + '.hh'
    custom_target('@0@'.format(hh),
      build_by_default: true,
      input: rl,
      output: hh,
      command: [ragel_helper, ragel, '@OUTPUT@', meson.current_source_dir(), '@INPUT@'],
    )
  endforeach
endif

custom_target('harfbuzz.cc',
  build_by_default: true,
  output: 'harfbuzz.cc',
  input: hb_base_sources + hb_glib_sources + hb_ft_sources +
         hb_graphite2_sources + hb_uniscribe_sources + hb_gdi_sources +
         hb_directwrite_sources + hb_coretext_sources + hb_wasm_sources,
  command: [find_program('gen-harfbuzzcc.py'),
            '@OUTPUT@', meson.current_source_dir(), '@INPUT@'],
)

incsrc = include_directories('.')

hb_sources = hb_base_sources + hb_base_ragel_generated_sources
hb_headers = hb_base_headers

common_harfbuzz_deps = [thread_dep, m_dep]
harfbuzz_deps = common_harfbuzz_deps + harfbuzz_extra_deps

if get_option('with_libstdcxx')
  chosen_linker = 'cpp'
else
  # Use a C linker, not C++; Don't link to libstdc++
  chosen_linker = 'c'
endif
libharfbuzz_link_language = chosen_linker

if conf.get('HAVE_FREETYPE', 0) == 1
  hb_sources += hb_ft_sources
  hb_headers += hb_ft_headers
  harfbuzz_deps += [freetype_dep]
endif

if conf.get('HAVE_GLIB', 0) == 1
  hb_sources += hb_glib_sources
  hb_headers += hb_glib_headers
  harfbuzz_deps += [glib_dep]
endif

# We set those here to not include the sources below that are of no use to
# GObject Introspection
gir_sources = hb_sources + hb_gobject_sources
gir_headers = hb_headers + hb_gobject_headers

if conf.get('HAVE_GDI', 0) == 1
  hb_sources += hb_gdi_sources
  hb_headers += hb_gdi_headers
  harfbuzz_deps += gdi_uniscribe_deps
endif

if conf.get('HAVE_GRAPHITE2', 0) == 1
  hb_sources += hb_graphite2_sources
  hb_headers += hb_graphite2_headers
  harfbuzz_deps += [graphite2_dep, graphite_dep]
endif

if conf.get('HAVE_WASM', 0) == 1
  hb_sources += hb_wasm_sources
  hb_headers += hb_wasm_headers
  harfbuzz_deps += wasm_dep
  #harfbuzz_deps += llvm_dep
endif

if conf.get('HAVE_UNISCRIBE', 0) == 1
  hb_sources += hb_uniscribe_sources
  hb_headers += hb_uniscribe_headers
endif

if conf.get('HAVE_DIRECTWRITE', 0) == 1
  hb_sources += hb_directwrite_sources
  hb_headers += hb_directwrite_headers
  harfbuzz_deps += directwrite_dep
  # hb-directwrite needs a C++ linker
  libharfbuzz_link_language = 'cpp'
endif

if conf.get('HAVE_CORETEXT', 0) == 1
  hb_sources += hb_coretext_sources
  hb_headers += hb_coretext_headers
  harfbuzz_deps += coretext_deps
endif

if conf.get('HAVE_FONTATIONS', 0) == 1 or conf.get('HAVE_HARFRUST', 0) == 1
  subdir('rust')
  harfbuzz_deps += [harfbuzz_rust_dep]
endif

if conf.get('HAVE_FONTATIONS', 0) == 1
  hb_sources += hb_fontations_sources
  hb_headers += hb_fontations_headers
endif

if conf.get('HAVE_HARFRUST', 0) == 1
  hb_sources += hb_harfrust_sources
  hb_headers += hb_harfrust_headers
endif

if conf.get('HAVE_KBTS', 0) == 1
  hb_sources += hb_kbts_sources
  hb_headers += hb_kbts_headers
  harfbuzz_deps += [kbts_dep]
endif

have_icu = conf.get('HAVE_ICU', 0) == 1
have_icu_builtin = conf.get('HAVE_ICU_BUILTIN', 0) == 1
if have_icu and have_icu_builtin
  hb_sources += hb_icu_sources
  hb_headers += hb_icu_headers
  harfbuzz_deps += [icu_dep]
endif

features = [
  'CAIRO',
  'CORETEXT',
  'DIRECTWRITE',
  'FREETYPE',
  'GDI',
  'GLIB',
  'GOBJECT',
  'GRAPHITE',
  'ICU',
  'UNISCRIBE',
  'WASM',
]

hb_enabled_features = configuration_data()
hb_supported_features = configuration_data()
foreach feature : features
  key = 'HB_HAS_@0@'.format(feature)
  hb_enabled_features.set(key, conf.get('HAVE_@0@'.format(feature), false))
  hb_supported_features.set(key, 1)
endforeach

# The enabled features. This file is installed.
hb_features_h = configure_file(input: 'hb-features.h.in',
                               output: 'hb-features.h',
                               configuration: hb_enabled_features,
                               install: true,
                               install_dir: get_option('includedir') / meson.project_name())

# This file is generated to convince gtk-doc to generate documentation for all
# HB_HAS_* macros, whether they are enabled for the current build or not.
# This file should not be installed.
hb_supported_features_h = configure_file(input: 'hb-features.h.in',
                               output: 'hb-supported-features.h',
                               configuration: hb_supported_features,
                               install: false)


# Base and default-included sources and headers
gen_def = find_program('gen-def.py')
gen_def_cmd = [gen_def, '@OUTPUT@', '@INPUT@']
if get_option('experimental_api')
  gen_def_cmd += '--experimental-api'
endif

# harfbuzz
harfbuzz_def = custom_target('harfbuzz.def',
    command: gen_def_cmd,
    input: hb_headers,
    output: 'harfbuzz.def')
defs_list = [harfbuzz_def]

version = '0.@0@.0'.format(hb_version_int)

extra_hb_cpp_args = []
if cpp_is_microsoft_compiler
  if get_option('default_library') != 'static'
    extra_hb_cpp_args += '-DHB_DLL_EXPORT'
  endif
  hb_so_version = ''
else
  hb_so_version = '0'
endif

if get_option('fuzzer_ldflags') != ''
  extra_hb_cpp_args += ['-DHB_CUSTOM_MALLOC']
  hb_sources += hb_failing_alloc_sources
  hb_subset_sources += hb_failing_alloc_sources
  hb_icu_sources += hb_failing_alloc_sources
  hb_gobject_sources += hb_failing_alloc_sources
endif

darwin_versions = [hb_version_int, '@0@.0.0'.format(hb_version_int)]

libharfbuzz = library('harfbuzz', hb_sources,
  include_directories: incconfig,
  dependencies: harfbuzz_deps,
  cpp_args: cpp_args + extra_hb_cpp_args,
  soversion: hb_so_version,
  version: version,
  install: true,
  darwin_versions: darwin_versions,
  link_language: libharfbuzz_link_language,
)

libharfbuzz_dep = declare_dependency(
  link_with: libharfbuzz,
  include_directories: incsrc,
  dependencies: harfbuzz_deps)
meson.override_dependency('harfbuzz', libharfbuzz_dep)

# harfbuzz-subset
libharfbuzz_subset_dep = null_dep
if not get_option('subset').disabled()
  harfbuzz_subset_def = custom_target('harfbuzz-subset.def',
      command: gen_def_cmd,
      input: hb_subset_headers,
      output: 'harfbuzz-subset.def')
  defs_list += [harfbuzz_subset_def]

  libharfbuzz_subset = library('harfbuzz-subset', hb_subset_sources,
    include_directories: incconfig,
    dependencies: common_harfbuzz_deps,
    link_with: [libharfbuzz],
    cpp_args: cpp_args + extra_hb_cpp_args,
    soversion: hb_so_version,
    version: version,
    install: true,
    darwin_versions: darwin_versions,
    link_language: chosen_linker,
  )

  custom_target('harfbuzz-subset.cc',
    build_by_default: true,
    output: 'harfbuzz-subset.cc',
    input: hb_base_sources + hb_subset_sources,
    command: [find_program('gen-harfbuzzcc.py'),
              '@OUTPUT@', meson.current_source_dir(), '@INPUT@'],
  )

  libharfbuzz_subset_dep = declare_dependency(
    link_with: libharfbuzz_subset,
    include_directories: incsrc,
    dependencies: [m_dep])
  meson.override_dependency('harfbuzz-subset', libharfbuzz_subset_dep)
endif

hb_raster_sources = [
  'hb-raster.cc',
  'hb-raster-image.cc',
  'hb-raster-image.hh',
  'hb-raster-utils.hh',
  'hb-raster-draw.cc',
  'hb-raster-svg-base.cc',
  'hb-raster-svg-base.hh',
  'hb-raster-svg-defs.cc',
  'hb-raster-svg-defs.hh',
  'hb-raster-svg-defs-scan.cc',
  'hb-raster-svg-defs-scan.hh',
  'hb-raster-svg-gradient.cc',
  'hb-raster-svg-gradient.hh',
  'hb-raster-svg-clip.cc',
  'hb-raster-svg-clip.hh',
  'hb-raster-svg-bbox.cc',
  'hb-raster-svg-bbox.hh',
  'hb-raster-svg-fill.cc',
  'hb-raster-svg-fill.hh',
  'hb-raster-svg-context.hh',
  'hb-raster-svg-use.cc',
  'hb-raster-svg-use.hh',
  'hb-raster-svg-render.cc',
  'hb-raster-svg-color.cc',
  'hb-raster-svg-color.hh',
  'hb-raster-svg-parse.cc',
  'hb-raster-svg-parse.hh',
  'hb-raster-paint.cc',
  'hb-raster-paint.hh',
  'hb-raster-svg.hh',
  'hb-static.cc',
]

hb_raster_headers = [
  'hb-raster.h',
]

hb_vector_sources = [
  'hb-vector.cc',
  'hb-vector-svg-draw.cc',
  'hb-vector-svg-path.cc',
  'hb-vector-svg-paint.cc',
  'hb-vector-svg-path.hh',
  'hb-vector-svg-subset.cc',
  'hb-vector-svg-subset.hh',
  'hb-vector-svg-utils.cc',
  'hb-vector-svg-utils.hh',
  'hb-static.cc',
]

hb_vector_headers = [
  'hb-vector.h',
]

libharfbuzz_raster_dep = null_dep
if not get_option('raster').disabled()
  libharfbuzz_raster = library('harfbuzz-raster', hb_raster_sources,
    include_directories: incconfig,
    dependencies: [m_dep],
    link_with: [libharfbuzz],
    cpp_args: cpp_args + extra_hb_cpp_args,
    soversion: hb_so_version,
    version: version,
    install: true,
    darwin_versions: darwin_versions,
    link_language: chosen_linker,
  )

  install_headers(hb_raster_headers, subdir: meson.project_name())

  libharfbuzz_raster_dep = declare_dependency(
    link_with: libharfbuzz_raster,
    include_directories: incsrc,
    dependencies: [libharfbuzz_dep])
  meson.override_dependency('harfbuzz-raster', libharfbuzz_raster_dep)

  harfbuzz_raster_def = custom_target('harfbuzz-raster.def',
    command: gen_def_cmd,
    input: hb_raster_headers,
    output: 'harfbuzz-raster.def')
  defs_list += [harfbuzz_raster_def]

  pkgmod.generate(libharfbuzz_raster,
    description: 'HarfBuzz CPU rasterizer',
    requires: ['harfbuzz = @0@'.format(meson.project_version())],
    subdirs: [meson.project_name()],
    version: meson.project_version(),
  )
endif

libharfbuzz_vector_dep = null_dep
if not get_option('vector').disabled()
  libharfbuzz_vector = library('harfbuzz-vector', hb_vector_sources,
    include_directories: incconfig,
    dependencies: [m_dep],
    link_with: [libharfbuzz],
    cpp_args: cpp_args + extra_hb_cpp_args,
    soversion: hb_so_version,
    version: version,
    install: true,
    darwin_versions: darwin_versions,
    link_language: chosen_linker,
  )

  install_headers(hb_vector_headers, subdir: meson.project_name())

  libharfbuzz_vector_dep = declare_dependency(
    link_with: libharfbuzz_vector,
    include_directories: incsrc,
    dependencies: [libharfbuzz_dep])
  meson.override_dependency('harfbuzz-vector', libharfbuzz_vector_dep)

  harfbuzz_vector_def = custom_target('harfbuzz-vector.def',
    command: gen_def_cmd,
    input: hb_vector_headers,
    output: 'harfbuzz-vector.def')
  defs_list += [harfbuzz_vector_def]

  pkgmod.generate(libharfbuzz_vector,
    description: 'HarfBuzz vector converter',
    requires: ['harfbuzz = @0@'.format(meson.project_version())],
    subdirs: [meson.project_name()],
    version: meson.project_version(),
  )
endif

libharfbuzz_cairo_dep = null_dep
if conf.get('HAVE_CAIRO', 0) == 1
  hb_cairo_sources = [
    'hb-cairo.cc',
    'hb-cairo-utils.cc',
    'hb-cairo-utils.hh',
    'hb-static.cc'
  ]

  hb_cairo_headers = [
    'hb-cairo.h',
  ]

  cairo_dep = dependency('cairo')

  libharfbuzz_cairo = library('harfbuzz-cairo', hb_cairo_sources,
    include_directories: incconfig,
    dependencies: [m_dep, cairo_dep],
    link_with: [libharfbuzz],
    cpp_args: cpp_args + extra_hb_cpp_args,
    soversion: hb_so_version,
    version: version,
    install: true,
    darwin_versions: darwin_versions,
    link_language: chosen_linker,
  )

  install_headers(hb_cairo_headers, subdir: meson.project_name())

  libharfbuzz_cairo_dep = declare_dependency(
    link_with: libharfbuzz_cairo,
    include_directories: incsrc,
    dependencies: [m_dep, cairo_dep])
  meson.override_dependency('harfbuzz-cairo', libharfbuzz_cairo_dep)

  harfbuzz_cairo_def = custom_target('harfbuzz-cairo.def',
    command: gen_def_cmd,
    input: hb_cairo_headers,
    output: 'harfbuzz-cairo.def')
  defs_list += [harfbuzz_cairo_def]

  pkgmod.generate(libharfbuzz_cairo,
    description: 'HarfBuzz cairo support',
    requires: ['harfbuzz = @0@'.format(meson.project_version())],
    subdirs: [meson.project_name()],
    version: meson.project_version(),
  )
endif


if get_option('tests').enabled()
  # TODO: Microsoft LINK gives the following because extern, non dllexport
  # symbols can only be used when linking against a static library
  # error LNK2019: unresolved external symbol "unsigned __int64 const * const _hb_NullPool"
  if not cpp_is_microsoft_compiler
    noinst_programs = {
      'main': 'main.cc',
      'test-basics': 'test.cc',
      'test-buffer-serialize': 'test-buffer-serialize.cc',
      'test-ot-meta': 'test-ot-meta.cc',
      'test-ot-name': 'test-ot-name.cc',
      'test-ot-glyphname': 'test-ot-glyphname.cc',
      'test-ot-gpos-size-params': 'test-gpos-size-params.cc',
      'test-ot-gsub-get-alternates': 'test-gsub-get-alternates.cc',
      'test-ot-gsub-would-substitute': 'test-gsub-would-substitute.cc',
      'test-use-table': 'test-use-table.cc',
    }
    foreach name, source : noinst_programs
      executable(name, source,
        include_directories: incconfig,
        dependencies: libharfbuzz_dep,
        install: false,
      )
    endforeach
  endif

  compiled_tests = {
    'test-algs': ['test-algs.cc', 'hb-static.cc'],
    'test-array': ['test-array.cc'],
    'test-bimap': ['test-bimap.cc', 'hb-static.cc'],
    'test-cff': ['test-cff.cc', 'hb-static.cc'],
    'test-classdef-graph': ['graph/test-classdef-graph.cc', 'hb-static.cc', 'graph/gsubgpos-context.cc'],
    'test-decycler': ['test-decycler.cc', 'hb-static.cc'],
    'test-iter': ['test-iter.cc', 'hb-static.cc'],
    'test-machinery': ['test-machinery.cc', 'hb-static.cc'],
    'test-map': ['test-map.cc', 'hb-static.cc'],
    'test-multimap': ['test-multimap.cc', 'hb-static.cc'],
    'test-number': ['test-number.cc', 'hb-number.cc'],
    'test-ot-tag': ['hb-ot-tag.cc'],
    'test-set': ['test-set.cc', 'hb-static.cc'],
    'test-serialize': ['test-serialize.cc', 'hb-static.cc'],
    'test-vector': ['test-vector.cc', 'hb-static.cc'],
    'test-repacker': ['test-repacker.cc', 'hb-static.cc', 'graph/gsubgpos-context.cc'],
    'test-instancer-solver': ['test-subset-instancer-solver.cc', 'hb-subset-instancer-solver.cc', 'hb-static.cc'],
    'test-priority-queue': ['test-priority-queue.cc', 'hb-static.cc'],
    'test-tuple-varstore': ['test-tuple-varstore.cc', 'hb-subset-instancer-solver.cc', 'hb-subset-instancer-iup.cc', 'hb-static.cc'],
    'test-item-varstore': ['test-item-varstore.cc', 'hb-subset-instancer-solver.cc', 'hb-subset-instancer-iup.cc', 'hb-static.cc'],
    'test-unicode-ranges': ['test-unicode-ranges.cc'],
  }
  foreach name, source : compiled_tests
    if cpp_is_microsoft_compiler and source.contains('hb-static.cc')
      # TODO: Microsoft compilers cannot link tests using hb-static.cc, fix them
      continue
    endif
    test(name, executable(name, source,
      include_directories: incconfig,
      cpp_args: cpp_args + ['-DMAIN', '-UNDEBUG'],
      dependencies: libharfbuzz_dep,
      install: false,
    ), suite: ['src'])
  endforeach
endif

pkgmod.generate(libharfbuzz,
  description: 'HarfBuzz text shaping library',
  subdirs: [meson.project_name()],
  version: meson.project_version(),
)

if not get_option('subset').disabled()
  pkgmod.generate(libharfbuzz_subset,
    description: 'HarfBuzz font subsetter',
    requires: ['harfbuzz = @0@'.format(meson.project_version())],
    subdirs: [meson.project_name()],
    version: meson.project_version(),
  )
endif

libharfbuzz_icu_dep = null_dep
if have_icu and not have_icu_builtin
  harfbuzz_icu_def = custom_target('harfbuzz-icu.def',
    command: gen_def_cmd,
    input: [hb_icu_headers],
    output: 'harfbuzz-icu.def')
  defs_list += [harfbuzz_icu_def]

  libharfbuzz_icu = library('harfbuzz-icu', [hb_icu_sources, hb_icu_headers],
    include_directories: incconfig,
    dependencies: icu_dep,
    link_with: [libharfbuzz],
    cpp_args: cpp_args + extra_hb_cpp_args,
    soversion: hb_so_version,
    version: version,
    install: true,
    darwin_versions: darwin_versions,
    # ICU links to stdc++ anyway so the default linker is good
    # link_language: chosen_linker,
  )

  libharfbuzz_icu_dep = declare_dependency(
    link_with: libharfbuzz_icu,
    include_directories: incsrc,
    dependencies: icu_dep)
  meson.override_dependency('harfbuzz-icu', libharfbuzz_icu_dep)

  pkgmod.generate(libharfbuzz_icu,
    description: 'HarfBuzz text shaping library ICU integration',
    requires: ['harfbuzz = @0@'.format(meson.project_version())],
    subdirs: [meson.project_name()],
    version: meson.project_version(),
  )

  install_headers(hb_icu_headers, subdir: meson.project_name())
endif

have_gobject = conf.get('HAVE_GOBJECT', 0) == 1

# This code (especially PACKAGE_INIT) kept similar to what CMake's own
# configure_package_config_file() generates, see
# https://cmake.org/cmake/help/latest/module/CMakePackageConfigHelpers.html#command:configure_package_config_file

cmake_config = configuration_data()
cmake_config_dir = cmake_package_install_dir / 'harfbuzz'

if meson.version().version_compare('>=1.3.0')
  cmake_package_prefix_dir = fs.relative_to(get_option('prefix'), get_option('prefix') / cmake_config_dir)
else
  relative_to = find_program('relative_to.py')
  cmake_package_prefix_dir = run_command(relative_to, get_option('prefix'), get_option('prefix') / cmake_config_dir, check: true).stdout().strip()
endif

cmake_package_prefix_dir = '${CMAKE_CURRENT_LIST_DIR}/@0@'.format(cmake_package_prefix_dir)

# Make all the relevant paths relative to our prefix, so we can later append
# them onto ${PACKAGE_PREFIX_DIR} to get the correct paths.

cmake_install_includedir = get_option('includedir')

if fs.is_absolute(cmake_install_includedir)
  if meson.version().version_compare('>=1.3.0')
    cmake_install_includedir = fs.relative_to(cmake_install_includedir, get_option('prefix'))
  else
    cmake_install_includedir = run_command(relative_to, cmake_install_includedir, get_option('prefix'), check: true).stdout().strip()
  endif
endif

cmake_install_libdir = get_option('libdir')

if fs.is_absolute(cmake_install_libdir)
  if meson.version().version_compare('>=1.3.0')
    cmake_install_libdir = fs.relative_to(cmake_install_libdir, get_option('prefix'))
  else
    cmake_install_libdir = run_command(relative_to, cmake_install_libdir, get_option('prefix'), check: true).stdout().strip()
  endif
endif

cmake_config.set('PACKAGE_INIT', '''
get_filename_component(PACKAGE_PREFIX_DIR "@0@" ABSOLUTE)

macro(set_and_check _var _file)
  set(${_var} "${_file}")
  if(NOT EXISTS "${_file}")
    message(FATAL_ERROR "File or directory ${_file} referenced by variable ${_var} does not exist !")
  endif()
endmacro()

macro(check_required_components _NAME)
  foreach(comp ${${_NAME}_FIND_COMPONENTS})
    if(NOT ${_NAME}_${comp}_FOUND)
      if(${_NAME}_FIND_REQUIRED_${comp})
        set(${_NAME}_FOUND FALSE)
      endif()
    endif()
  endforeach()
endmacro()
'''.format(cmake_package_prefix_dir))

cmake_config.set('PACKAGE_CMAKE_INSTALL_INCLUDEDIR', '${PACKAGE_PREFIX_DIR}/@0@'.format(cmake_install_includedir))
cmake_config.set('PACKAGE_CMAKE_INSTALL_LIBDIR', '${PACKAGE_PREFIX_DIR}/@0@'.format(cmake_install_libdir))
cmake_config.set('PACKAGE_INCLUDE_INSTALL_DIR', '${PACKAGE_PREFIX_DIR}/@0@/@1@'.format(cmake_install_includedir, meson.project_name()))
cmake_config.set('HARFBUZZ_VERSION', meson.project_version())
cmake_config.set('HB_HAVE_GOBJECT', have_gobject ? 'YES' : 'NO')
cmake_config.set('HB_LIBRARY_TYPE', get_option('default_library') == 'static' ? 'STATIC' : 'SHARED')

if get_option('default_library') == 'static'
  cmake_config.set('HB_LIB_PREFIX', '${CMAKE_STATIC_LIBRARY_PREFIX}')
  cmake_config.set('HB_LIB_SUFFIX', '${CMAKE_STATIC_LIBRARY_SUFFIX}')
elif host_machine.system() == 'darwin'
  cmake_config.set('HB_LIB_PREFIX', '${CMAKE_SHARED_LIBRARY_PREFIX}')
  cmake_config.set('HB_LIB_SUFFIX', '.@0@${CMAKE_SHARED_LIBRARY_SUFFIX}'.format(hb_so_version))
elif host_machine.system() == 'windows'
  cmake_config.set('HB_LIB_PREFIX', '${CMAKE_IMPORT_LIBRARY_PREFIX}')
  cmake_config.set('HB_LIB_SUFFIX', '${CMAKE_IMPORT_LIBRARY_SUFFIX}')
else
  cmake_config.set('HB_LIB_PREFIX', '${CMAKE_SHARED_LIBRARY_PREFIX}')
  cmake_config.set('HB_LIB_SUFFIX', '${CMAKE_SHARED_LIBRARY_SUFFIX}.@0@'.format(version))
endif

configure_file(input: 'harfbuzz-config.cmake.in',
  output: 'harfbuzz-config.cmake',
  configuration: cmake_config,
  install_dir: cmake_config_dir,
)

gobject_enums_c = []
gobject_enums_h = []
libharfbuzz_gobject_dep = null_dep
if have_gobject
  gnome = import('gnome')

  h_templ = configure_file(
    input: 'hb-gobject-enums.h.tmpl',
    output: 'hb-gobject-enums-tmp.h.tmpl',
    copy: true)

  cc_templ = configure_file(
    input: 'hb-gobject-enums.cc.tmpl',
    output: 'hb-gobject-enums-tmp.cc.tmpl',
    copy: true)

  enums = gnome.mkenums('hb-gobject',
    sources: hb_headers,
    h_template: h_templ,
    c_template: cc_templ,
    identifier_prefix: 'hb_',
    symbol_prefix: 'hb_gobject',
  )

  gobject_enums_c = custom_target('hb-gobject-enums.cc',
    input: enums[0],
    output: 'hb-gobject-enums.cc',
    command: [find_program('fix_get_types.py'), '@INPUT@', '@OUTPUT@']
  )

  gobject_enums_h = custom_target('hb-gobject-enums.h',
    input: enums[1],
    output: 'hb-gobject-enums.h',
    command: [find_program('fix_get_types.py'), '@INPUT@', '@OUTPUT@'],
    install: true,
    install_dir: get_option('prefix') / get_option('includedir') / meson.project_name(),
  )

  hb_gobject_sources += [gobject_enums_c]

  harfbuzz_gobject_def = custom_target('harfbuzz-gobject.def',
    command: gen_def_cmd,
    input: [hb_gobject_headers, gobject_enums_h],
    output: 'harfbuzz-gobject.def')
  defs_list += [harfbuzz_gobject_def]

  libharfbuzz_gobject = library('harfbuzz-gobject', [hb_gobject_sources, gobject_enums_c, gobject_enums_h],
    include_directories: incconfig,
    dependencies: [glib_dep, gobject_dep],
    link_with: [libharfbuzz],
    cpp_args: cpp_args + extra_hb_cpp_args,
    soversion: hb_so_version,
    version: version,
    install: true,
    darwin_versions: darwin_versions,
    link_language: chosen_linker,
  )

  gir = find_program('g-ir-scanner', required: get_option('introspection'))
  build_gir = gir.found() and (not meson.is_cross_build() or get_option('introspection').enabled())

  build_gir = build_gir and get_option('default_library') != 'static'
  if not build_gir and get_option('introspection').enabled()
    error('Introspection support is requested but the default library option should be shared or both')
  endif

  if build_gir
    conf.set('HAVE_INTROSPECTION', 1)
    hb_gen_files_gir = gnome.generate_gir([libharfbuzz_gobject, libharfbuzz],
      sources: [gir_headers, gir_sources, gobject_enums_h],
      dependencies: libharfbuzz_dep,
      namespace: 'HarfBuzz',
      nsversion: '0.0',
      identifier_prefix: 'hb_',
      symbol_prefix: ['hb', 'hb_gobject'],
      includes: ['GObject-2.0', 'freetype2-2.0'],
      export_packages: ['harfbuzz-gobject', 'harfbuzz'],
      header: 'hb-gobject.h',
      install: true,
      extra_args:  ['--cflags-begin',
                    '-DHB_NO_SINGLE_HEADER_ERROR',
                    '-DHAVE_GOBJECT',
                    '-DHB_EXTERN=',
                    '--cflags-end'])
  endif

  libharfbuzz_gobject_dep = declare_dependency(
    link_with: libharfbuzz_gobject,
    include_directories: incsrc,
    sources: build_gir ? hb_gen_files_gir : hb_gobject_sources,
    dependencies: [glib_dep, gobject_dep])
  meson.override_dependency('harfbuzz-gobject', libharfbuzz_gobject_dep)

  pkgmod.generate(libharfbuzz_gobject,
    description: 'HarfBuzz text shaping library GObject integration',
    requires: ['harfbuzz = @0@'.format(meson.project_version()), 'glib-2.0', 'gobject-2.0'],
    subdirs: [meson.project_name()],
    version: meson.project_version(),
  )

  install_headers(hb_gobject_headers, subdir: meson.project_name())
else
  if get_option('introspection').enabled()
    error('introspection requires gobject to be enabled')
  endif
endif

if get_option('tests').enabled()
  dist_check_script = [
    'check-c-linkage-decls',
    'check-externs',
    'check-header-guards',
    'check-includes',
  ]

  env = environment()
  HBSOURCES = []
  foreach f : hb_sources
    if meson.version().version_compare('>=1.4.0')
      f = f.full_path()
    endif
    HBSOURCES += '@0@'.format(f)
  endforeach
  env.set('HBSOURCES', ' '.join(HBSOURCES))
  HBHEADERS = []
  foreach f : hb_headers
    if meson.version().version_compare('>=1.4.0')
      f = f.full_path()
    endif
    HBHEADERS += '@0@'.format(f)
  endforeach
  env.set('HBHEADERS', ' '.join(HBHEADERS))

  if cpp.get_argument_syntax() != 'msvc' and not meson.is_cross_build() # ensure the local tools are usable
    dist_check_script += ['check-static-inits', 'check-symbols']
    if get_option('wasm').disabled() and not get_option('with_libstdcxx')
      dist_check_script += ['check-libstdc++']
    endif
  endif

  foreach name : dist_check_script
    test(name, find_program(name + '.py'),
      env: env,
      args: [
        meson.current_source_dir(),
        meson.project_source_root(),
        meson.current_build_dir(),
      ],
      depends: name == 'check-symbols' ? defs_list : [],
      suite: ['src'],
    )
  endforeach

  test('check-release-notes',
    find_program('check-release-notes.py'),
    args: [
      meson.current_source_dir() / '..' / 'NEWS',
      meson.project_version()
    ],
  )
endif

install_headers(hb_headers, subdir: meson.project_name())
if not get_option('subset').disabled()
  install_headers(hb_subset_headers, subdir: meson.project_name())
endif
