liblcms2_threaded_sources = files(
  'threaded_core.c',
  'threaded_main.c',
  'threaded_scheduler.c',
  'threaded_split.c',
)

lcms2_threaded_incdir = include_directories('../include', '.')

if host_machine.system() == 'windows'
  lcms2_threaded_rc = configure_file(
    input: 'lcms2_threaded.rc.in',
    configuration: version_cfg,
    output: 'lcms2_threaded.rc',
  )

  liblcms2_threaded_sources += win.compile_resources(lcms2_threaded_rc)
endif

liblcms2_threaded = library(
  'lcms2_threaded',
  liblcms2_threaded_sources,
  include_directories: lcms2_threaded_incdir,
  dependencies: [ liblcms2_dep, threads_dep ],
  c_args: cargs,
  install: true,
)

liblcms2_threaded_dep = declare_dependency(
  link_with: liblcms2_threaded,
  include_directories: lcms2_threaded_incdir,
)

extra_libraries += liblcms2_threaded
