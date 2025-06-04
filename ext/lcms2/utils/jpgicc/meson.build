jpgicc_sources = files(
  'iccjpeg.c',
  'jpgicc.c',
)

jpgicc_exe = executable(
  'jpgicc',
  jpgicc_sources,
  dependencies: [liblcms2_dep, jpeg_dep, common_dep, m_dep],
  install: true,
)

jpgicc_man = files(
  'jpgicc.1',
)

install_man(jpgicc_man)
