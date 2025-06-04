lcms2_fast_float_sources = files(
  'fast_16_tethra.c',
  'fast_8_curves.c',
  'fast_8_matsh.c',
  'fast_8_matsh_sse.c',
  'fast_8_tethra.c',
  'fast_float_15bits.c',
  'fast_float_15mats.c',
  'fast_float_cmyk.c',
  'fast_float_curves.c',
  'fast_float_internal.h',
  'fast_float_lab.c',
  'fast_float_matsh.c',
  'fast_float_separate.c',
  'fast_float_sup.c',
  'fast_float_tethra.c',
)

lcms2_fast_float_incdir = include_directories('../include', '.')

if host_machine.system() == 'windows'
 if get_option('default_library') == 'shared'

     lcms2_fast_float_rc = configure_file(
        input: 'lcms2_fast_float.rc.in',
        configuration: version_cfg,
        output: 'lcms2_fast_float.rc',
    )

    lcms2_fast_float_sources += win.compile_resources(lcms2_fast_float_rc)
  endif
endif

liblcms2_fast_float = library(
  'lcms2_fast_float',
  lcms2_fast_float_sources,
  gnu_symbol_visibility: 'hidden',
  dependencies: [liblcms2_dep, m_dep],
  include_directories: lcms2_fast_float_incdir,
  c_args: cargs,
  install: true,
)

liblcms2_fast_float_dep = declare_dependency(
  link_with: liblcms2_fast_float,
  include_directories: lcms2_fast_float_incdir,
)

extra_libraries += liblcms2_fast_float
