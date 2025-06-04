fast_float_testbed_srcs = files(
  'fast_float_testbed.c',
)

fast_float_testbed = executable(
  'fast_float_testbed',
  fast_float_testbed_srcs,
  dependencies: [liblcms2_fast_float_dep, liblcms2_dep, m_dep],
  c_args: cargs + ['-DPROFILES_DIR="@0@"'.format(profiles_dir / '')],
)

test(
  'fast_float_testbed',
  fast_float_testbed,
  timeout: 600,
)
