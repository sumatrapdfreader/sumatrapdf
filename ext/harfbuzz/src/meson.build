hb_version_h = configure_file(
               command: [find_program('gen-hb-version.py'), meson.project_version(), '@OUTPUT@', '@INPUT@'],
               input: 'hb-version.h.in',
               output: 'hb-version.h',
               install: true,
               install_dir: join_paths(get_option('includedir'), meson.project_name()))

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
  'hb-array.hh',
  'hb-atomic.hh',
  'hb-bimap.hh',
  'hb-blob.cc',
  'hb-blob.hh',
  'hb-buffer-serialize.cc',
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
  'hb-dispatch.hh',
  'hb-draw.cc',
  'hb-draw.hh',
  'hb-face.cc',
  'hb-face.hh',
  'hb-fallback-shape.cc',
  'hb-font.cc',
  'hb-font.hh',
  'hb-iter.hh',
  'hb-kern.hh',
  'hb-machinery.hh',
  'hb-map.cc',
  'hb-map.hh',
  'hb-meta.hh',
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
  'hb-ot-color-cbdt-table.hh',
  'hb-ot-color-colr-table.hh',
  'hb-ot-color-cpal-table.hh',
  'hb-ot-color-sbix-table.hh',
  'hb-ot-color-svg-table.hh',
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
  'hb-ot-shape-complex-arabic-fallback.hh',
  'hb-ot-shape-complex-arabic-joining-list.hh',
  'hb-ot-shape-complex-arabic-table.hh',
  'hb-ot-shape-complex-arabic-win1256.hh',
  'hb-ot-shape-complex-arabic.cc',
  'hb-ot-shape-complex-arabic.hh',
  'hb-ot-shape-complex-default.cc',
  'hb-ot-shape-complex-hangul.cc',
  'hb-ot-shape-complex-hebrew.cc',
  'hb-ot-shape-complex-indic-table.cc',
  'hb-ot-shape-complex-indic.cc',
  'hb-ot-shape-complex-indic.hh',
  'hb-ot-shape-complex-khmer.cc',
  'hb-ot-shape-complex-khmer.hh',
  'hb-ot-shape-complex-myanmar.cc',
  'hb-ot-shape-complex-myanmar.hh',
  'hb-ot-shape-complex-thai.cc',
  'hb-ot-shape-complex-use-table.cc',
  'hb-ot-shape-complex-use.cc',
  'hb-ot-shape-complex-use.hh',
  'hb-ot-shape-complex-vowel-constraints.cc',
  'hb-ot-shape-complex-vowel-constraints.hh',
  'hb-ot-shape-complex.hh',
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
  'hb-ot-var-fvar-table.hh',
  'hb-ot-var-gvar-table.hh',
  'hb-ot-var-hvar-table.hh',
  'hb-ot-var-mvar-table.hh',
  'hb-ot-var.cc',
  'hb-ot-vorg-table.hh',
  'hb-pool.hh',
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
  'hb-buffer-deserialize-text.hh',
  'hb-number-parser.hh',
  'hb-ot-shape-complex-indic-machine.hh',
  'hb-ot-shape-complex-khmer-machine.hh',
  'hb-ot-shape-complex-myanmar-machine.hh',
  'hb-ot-shape-complex-use-machine.hh',
)
hb_base_ragel_sources = [
  'hb-buffer-deserialize-json.rl',
  'hb-buffer-deserialize-text.rl',
  'hb-number-parser.rl',
  'hb-ot-shape-complex-indic-machine.rl',
  'hb-ot-shape-complex-khmer-machine.rl',
  'hb-ot-shape-complex-myanmar-machine.rl',
  'hb-ot-shape-complex-use-machine.rl',
]

hb_base_headers = files(
  'hb-aat-layout.h',
  'hb-aat.h',
  'hb-blob.h',
  'hb-buffer.h',
  'hb-common.h',
  'hb-deprecated.h',
  'hb-draw.h',
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
  'hb-set.h',
  'hb-shape-plan.h',
  'hb-shape.h',
  'hb-style.h',
  'hb-unicode.h',
  'hb.h',
)
hb_base_headers += hb_version_h

# Optional Sources and Headers with external deps

hb_ft_sources = files('hb-ft.cc')
hb_ft_headers = files('hb-ft.h')

hb_glib_sources = files('hb-glib.cc')
hb_glib_headers = files('hb-glib.h')

hb_graphite2_sources = files('hb-graphite2.cc')
hb_graphite2_headers = files('hb-graphite2.h')

# System-dependent sources and headers

hb_coretext_sources = files('hb-coretext.cc')
hb_coretext_headers = files('hb-coretext.h')

hb_directwrite_sources = files('hb-directwrite.cc')
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
  'hb-subset-cff-common.cc',
  'hb-subset-cff-common.hh',
  'hb-subset-cff1.cc',
  'hb-subset-cff1.hh',
  'hb-subset-cff2.cc',
  'hb-subset-cff2.hh',
  'hb-subset-input.cc',
  'hb-subset-input.hh',
  'hb-subset-plan.cc',
  'hb-subset-plan.hh',
  'hb-subset-plan.hh',
  'hb-subset.cc',
  'hb-subset.hh',
  'hb-subset.hh',
)

hb_subset_headers = files('hb-subset.h')

hb_gobject_sources = files(
  'hb-gobject-structs.cc'
)

hb_gobject_headers = files(
  'hb-gobject.h',
  'hb-gobject-structs.h',
)

ragel = find_program('ragel', required: false)
if not ragel.found()
  warning('You have to install ragel if you are going to develop HarfBuzz itself')
else
  ragel_helper = find_program('gen-ragel-artifacts.py')
  foreach rl : hb_base_ragel_sources
    hh = rl.split('.')[0] + '.hh'
    custom_target('@0@'.format(hh),
      build_by_default: true,
      input: rl,
      output: hh,
      command: [ragel_helper, '@OUTPUT@', meson.current_source_dir(), '@INPUT@'],
    )
  endforeach
endif

custom_target('harfbuzz.cc',
  build_by_default: true,
  output: 'harfbuzz.cc',
  input: hb_base_sources + hb_glib_sources + hb_ft_sources +
         hb_graphite2_sources + hb_uniscribe_sources + hb_gdi_sources +
         hb_directwrite_sources + hb_coretext_sources,
  command: [find_program('gen-harfbuzzcc.py'),
            '@OUTPUT@', meson.current_source_dir(), '@INPUT@'],
)

incsrc = include_directories('.')

hb_sources = hb_base_sources + hb_base_ragel_generated_sources
hb_headers = hb_base_headers

harfbuzz_deps = [thread_dep, m_dep] + harfbuzz_extra_deps

libharfbuzz_link_language = 'c'

if conf.get('HAVE_FREETYPE', 0) == 1
  hb_sources += hb_ft_sources
  hb_headers += hb_ft_headers
  harfbuzz_deps += [freetype_dep]
endif

if conf.get('HAVE_GDI', 0) == 1
  hb_sources += hb_gdi_sources
  hb_headers += hb_gdi_headers
  harfbuzz_deps += gdi_uniscribe_deps
endif

if conf.get('HAVE_GRAPHITE2', 0) == 1
  hb_sources += hb_graphite2_sources
  hb_headers += hb_graphite2_headers
  harfbuzz_deps += [graphite2_dep]
endif

if conf.get('HAVE_GLIB', 0) == 1
  hb_sources += hb_glib_sources
  hb_headers += hb_glib_headers
  harfbuzz_deps += [glib_dep]
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

have_icu = conf.get('HAVE_ICU', 0) == 1
have_icu_builtin = conf.get('HAVE_ICU_BUILTIN', 0) == 1

if have_icu and have_icu_builtin
  hb_sources += hb_icu_sources
  hb_headers += hb_icu_headers
  harfbuzz_deps += [icu_dep]
endif

# harfbuzz
gen_def = find_program('gen-def.py')

harfbuzz_def_command_args = [gen_def, '@OUTPUT@', '@INPUT@']
if get_option('experimental_api')
  harfbuzz_def_command_args += '--experimental-api'
endif

harfbuzz_def = custom_target('harfbuzz.def',
    command: harfbuzz_def_command_args,
    input: hb_headers,
    output: 'harfbuzz.def')
defs_list = [harfbuzz_def]

version = '0.@0@.0'.format(hb_version_int)

extra_hb_cpp_args = []
if cpp.get_id() == 'msvc'
  if get_option('default_library') != 'static'
    extra_hb_cpp_args += '-DHB_DLL_EXPORT'
  endif
  hb_so_version = ''
else
  hb_so_version = '0'
endif

if get_option('fuzzer_ldflags') != ''
  extra_hb_cpp_args += ['-DHB_CUSTOM_MALLOC']
  hb_sources += 'failing-alloc.c'
  hb_subset_sources += 'failing-alloc.c'
  hb_icu_sources += 'failing-alloc.c'
  hb_gobject_sources += 'failing-alloc.c'
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

# harfbuzz-subset
harfbuzz_subset_def = custom_target('harfbuzz-subset.def',
    command: [gen_def, '@OUTPUT@', '@INPUT@'],
    input: hb_subset_headers,
    output: 'harfbuzz-subset.def')
defs_list += [harfbuzz_subset_def]

libharfbuzz_subset = library('harfbuzz-subset', hb_subset_sources,
  include_directories: incconfig,
  dependencies: [m_dep],
  link_with: [libharfbuzz],
  cpp_args: cpp_args + extra_hb_cpp_args,
  soversion: hb_so_version,
  version: version,
  install: true,
  darwin_versions: darwin_versions,
  link_language: 'c',
)

libharfbuzz_subset_dep = declare_dependency(
  link_with: libharfbuzz_subset,
  include_directories: incsrc,
  dependencies: [m_dep])

if get_option('tests').enabled()
  # TODO: MSVC gives the following,
  # error LNK2019: unresolved external symbol "unsigned __int64 const * const _hb_NullPool"
  if cpp.get_id() != 'msvc'
    noinst_programs = {
      'main': 'main.cc',
      'test-basics': 'test.cc',
      'test-buffer-serialize': 'test-buffer-serialize.cc',
      'test-ot-meta': 'test-ot-meta.cc',
      'test-ot-name': 'test-ot-name.cc',
      'test-ot-glyphname': 'test-ot-glyphname.cc',
      'test-ot-gpos-size-params': 'test-gpos-size-params.cc',
      'test-ot-gsub-would-substitute': 'test-gsub-would-substitute.cc',
    }
    foreach name, source : noinst_programs
      executable(name, source,
        include_directories: incconfig,
        cpp_args: cpp_args,
        dependencies: libharfbuzz_dep,
        install: false,
      )
    endforeach
  endif

  check_programs = {
    'dump-indic-data': ['dump-indic-data.cc', 'hb-ot-shape-complex-indic-table.cc'],
    'dump-khmer-data': ['dump-khmer-data.cc', 'hb-ot-shape-complex-indic-table.cc'],
    'dump-myanmar-data': ['dump-myanmar-data.cc', 'hb-ot-shape-complex-indic-table.cc'],
    'dump-use-data': ['dump-use-data.cc', 'hb-ot-shape-complex-use-table.cc'],
  }
  foreach name, source : check_programs
    executable(name, source,
      include_directories: incconfig,
      cpp_args: cpp_args,
      dependencies: libharfbuzz_dep,
      install: false,
    )
  endforeach

  compiled_tests = {
    'test-algs': ['test-algs.cc', 'hb-static.cc'],
    'test-array': ['test-array.cc'],
    'test-iter': ['test-iter.cc', 'hb-static.cc'],
    'test-meta': ['test-meta.cc', 'hb-static.cc'],
    'test-number': ['test-number.cc', 'hb-number.cc'],
    'test-ot-tag': ['hb-ot-tag.cc'],
    'test-unicode-ranges': ['test-unicode-ranges.cc'],
    'test-bimap': ['test-bimap.cc', 'hb-static.cc'],
  }
  foreach name, source : compiled_tests
    if cpp.get_id() == 'msvc' and source.contains('hb-static.cc')
      # TODO: MSVC doesn't like tests having hb-static.cc, fix them
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

pkgmod.generate(libharfbuzz_subset,
  description: 'HarfBuzz font subsetter',
  requires: ['harfbuzz = @0@'.format(meson.project_version())],
  subdirs: [meson.project_name()],
  version: meson.project_version(),
)

libharfbuzz_icu_dep = null_dep
if have_icu and not have_icu_builtin
  harfbuzz_icu_def = custom_target('harfbuzz-icu.def',
    command: [gen_def, '@OUTPUT@', '@INPUT@'],
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
    # link_language: 'c',
  )

  libharfbuzz_icu_dep = declare_dependency(
    link_with: libharfbuzz_icu,
    include_directories: incsrc,
    dependencies: icu_dep)

  pkgmod.generate(libharfbuzz_icu,
    description: 'HarfBuzz text shaping library ICU integration',
    requires: ['harfbuzz = @0@'.format(meson.project_version())],
    subdirs: [meson.project_name()],
    version: meson.project_version(),
  )

  install_headers(hb_icu_headers, subdir: meson.project_name())
endif

have_gobject = conf.get('HAVE_GOBJECT', 0) == 1

cmake_config = configuration_data()
cmake_config.set('libdir', '${prefix}/@0@'.format(get_option('libdir')))
cmake_config.set('includedir', '${prefix}/@0@'.format(get_option('includedir')))
cmake_config.set('HB_LIBTOOL_VERSION_INFO', hb_libtool_version_info)
cmake_config.set('have_gobject', '@0@'.format(have_gobject))
configure_file(input: 'harfbuzz-config.cmake.in',
  output: 'harfbuzz-config.cmake',
  configuration: cmake_config,
  install_dir: join_paths(get_option('libdir'), 'cmake', 'harfbuzz'),
)

libharfbuzz_gobject_dep = null_dep
if have_gobject
  gnome = import('gnome')

  h_templ = configure_file(
    input: 'hb-gobject-enums.h.tmpl',
    output: 'hb-gobject-enums-tmp.h.tmpl',
    configuration: configuration_data(),
    format: 'cmake')

  cc_templ = configure_file(
    input: 'hb-gobject-enums.cc.tmpl',
    output: 'hb-gobject-enums-tmp.cc.tmpl',
    configuration: configuration_data(),
    format: 'cmake')

  enums = gnome.mkenums('hb-gobject',
    sources: hb_headers,
    h_template: h_templ,
    c_template: cc_templ,
    identifier_prefix: 'hb_',
    symbol_prefix: 'hb_gobject',
  )

  enum_c = custom_target('hb-gobject-enums.cc',
    input: enums[0],
    output: 'hb-gobject-enums.cc',
    command: [find_program('fix_get_types.py'), '@INPUT@', '@OUTPUT@']
  )

  enum_h = custom_target('hb-gobject-enums.h',
    input: enums[1],
    output: 'hb-gobject-enums.h',
    command: [find_program('fix_get_types.py'), '@INPUT@', '@OUTPUT@'],
    install: true,
    install_dir: join_paths(get_option('prefix'), get_option('includedir'), meson.project_name()),
  )

  hb_gobject_sources += [enum_c]

  harfbuzz_gobject_def = custom_target('harfbuzz-gobject.def',
    command: [gen_def, '@OUTPUT@', '@INPUT@'],
    input: [hb_gobject_headers, enum_h],
    output: 'harfbuzz-gobject.def')
  defs_list += [harfbuzz_gobject_def]

  libharfbuzz_gobject = library('harfbuzz-gobject', [hb_gobject_sources, enum_c, enum_h],
    include_directories: incconfig,
    dependencies: [glib_dep, gobject_dep],
    link_with: [libharfbuzz],
    cpp_args: cpp_args + extra_hb_cpp_args,
    soversion: hb_so_version,
    version: version,
    install: true,
    darwin_versions: darwin_versions,
    link_language: 'c',
  )

  gir = find_program('g-ir-scanner', required: get_option('introspection'))
  build_gir = gir.found() and (not meson.is_cross_build() or get_option('introspection').enabled())

  build_gir = build_gir and get_option('default_library') != 'static'
  if not build_gir and get_option('introspection').enabled()
    error('Introspection support is requested but the default library option should be shared or both')
  endif

  if build_gir
    conf.set('HAVE_INTROSPECTION', 1)
    hb_gen_files_gir = gnome.generate_gir(libharfbuzz_gobject,
      sources: [hb_headers, hb_sources, hb_gobject_headers, hb_gobject_sources, enum_h],
      dependencies: libharfbuzz_dep,
      namespace: 'HarfBuzz',
      nsversion: '0.0',
      identifier_prefix: 'hb_',
      symbol_prefix: ['hb', 'hb_gobject'],
      includes: ['GObject-2.0'],
      export_packages: ['harfbuzz-gobject'],
      header: 'hb-gobject.h',
      install: true,
      extra_args:  ['--cflags-begin',
                    '-DHB_H',
                    '-DHB_H_IN',
                    '-DHB_OT_H',
                    '-DHB_OT_H_IN',
                    '-DHB_AAT_H',
                    '-DHB_AAT_H_IN',
                    '-DHB_GOBJECT_H',
                    '-DHB_GOBJECT_H_IN',
                    '-DHB_EXTERN=',
                    '--cflags-end'])
  endif

  if build_gir
    libharfbuzz_gobject_sources = hb_gen_files_gir
  else
    libharfbuzz_gobject_sources = hb_gobject_sources
  endif

  libharfbuzz_gobject_dep = declare_dependency(
    link_with: libharfbuzz_gobject,
    include_directories: incsrc,
    sources: libharfbuzz_gobject_sources,
    dependencies: [glib_dep, gobject_dep])

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
  env.set('srcdir', meson.current_source_dir())
  env.set('builddir', meson.current_build_dir())
  env.set('libs', meson.current_build_dir()) # TODO: Merge this with builddir after autotools removal
  HBSOURCES = []
  foreach f : hb_sources
    HBSOURCES += '@0@'.format(f)
  endforeach
  env.set('HBSOURCES', ' '.join(HBSOURCES))
  HBHEADERS = []
  foreach f : hb_headers
    HBHEADERS += '@0@'.format(f)
  endforeach
  env.set('HBHEADERS', ' '.join(HBHEADERS))

  if cpp.get_id() != 'msvc' and not meson.is_cross_build() # ensure the local tools are usable
    if meson.version().version_compare('>=0.55')
      dist_check_script += 'check-libstdc++'
    endif
    dist_check_script += ['check-static-inits', 'check-symbols']
  endif

  foreach name : dist_check_script
    if name == 'check-symbols'
      test_depends = defs_list
    else
      test_depends = []
    endif
    test(name, find_program(name + '.py'),
      env: env,
      depends: test_depends,
      suite: ['src'],
    )
  endforeach
endif

install_headers(hb_headers + hb_subset_headers, subdir: meson.project_name())
