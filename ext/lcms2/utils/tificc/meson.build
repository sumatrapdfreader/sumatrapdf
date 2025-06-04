tificc_sources = files(
  'tificc.c',
)

tificc_exe = executable(
  'tificc',
  tificc_sources,
  dependencies: [liblcms2_dep, common_dep, tiff_dep],
  install: true,
)

tificc_man = files(
  'tificc.1',
)

install_man(tificc_man)
