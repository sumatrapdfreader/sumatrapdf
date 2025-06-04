transicc_sources = files(
  'transicc.c',
)

transicc_exe = executable(
  'transicc',
  transicc_sources,
  dependencies: [liblcms2_dep, common_dep, m_dep],
  install: true,
)

transicc_man = files(
  'transicc.1',
)

install_man(transicc_man)
