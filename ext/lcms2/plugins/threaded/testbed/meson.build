threaded_testbed_sources = files(
  'threaded_testbed.c',
)

threaded_testbed = executable(
  'threaded_testbed',
  threaded_testbed_sources,
  dependencies: [liblcms2_threaded_dep, liblcms2_dep],
  c_args: cargs + ['-DPROFILES_DIR="@0@"'.format(profiles_dir / '')],
)

test(
  'threaded_testbed',
  threaded_testbed,
  timeout: 600,
)
