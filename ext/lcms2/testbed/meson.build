testcms_srcs = files(
  'testcms2.c',
  'testplugin.c',
  'zoo_icc.c',
)

testcms = executable(
  'testcms',
  testcms_srcs,
  dependencies: [liblcms2_dep, m_dep],
  c_args: cargs,
)

test(
  'testcms',
  testcms,
  workdir: meson.current_source_dir(),
  timeout: 600,
)
