hb_view_sources = [
  'hb-view.cc',
  'options.cc',
  'ansi-print.cc',
  'helper-cairo.cc',
  'helper-cairo-ansi.cc',
  'view-cairo.cc',
]

hb_shape_sources = [
  'hb-shape.cc',
  'options.cc',
]

hb_ot_shape_closure_sources = [
  'hb-ot-shape-closure.cc',
  'options.cc',
]

hb_subset_cli_sources = [
  'hb-subset.cc',
  'options.cc',
  'options-subset.cc',
]

util_deps = [freetype_dep, cairo_dep, cairo_ft_dep, glib_dep]

if conf.get('HAVE_GLIB', 0) == 1
  if conf.get('HAVE_FREETYPE', 0) == 1 and conf.get('HAVE_CAIRO_FT', 0) == 1

    hb_view = executable('hb-view', hb_view_sources,
      cpp_args: cpp_args,
      include_directories: [incconfig, incsrc],
      dependencies: util_deps,
      link_with: [libharfbuzz],
      install: true,
    )
  endif

  hb_shape = executable('hb-shape', hb_shape_sources,
    cpp_args: cpp_args,
    include_directories: [incconfig, incsrc],
    dependencies: util_deps,
    link_with: [libharfbuzz],
    install: true,
  )

  hb_subset = executable('hb-subset', hb_subset_cli_sources,
    cpp_args: cpp_args,
    include_directories: [incconfig, incsrc],
    dependencies: util_deps,
    link_with: [libharfbuzz, libharfbuzz_subset],
    install: true,
  )

  hb_ot_shape_closure = executable('hb-ot-shape-closure', hb_ot_shape_closure_sources,
    cpp_args: cpp_args,
    include_directories: [incconfig, incsrc],
    dependencies: util_deps,
    link_with: [libharfbuzz],
    install: true,
  )
else
  # Disable tests that use this
  hb_shape = disabler()
  hb_subset = disabler()
endif
