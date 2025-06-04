linkicc_sources = files(
  'linkicc.c',
)

linkicc_exe = executable(
  'linkicc',
  linkicc_sources,
  dependencies: [liblcms2_dep, common_dep],
  install: true,
)

linkicc_man = files(
  'linkicc.1',
)

install_man(linkicc_man)
