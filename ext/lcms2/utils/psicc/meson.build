psicc_sources = files(
  'psicc.c',
)

psicc_exe = executable(
  'psicc',
  psicc_sources,
  dependencies: [liblcms2_dep, common_dep],
  install: true,
)

psicc_man = files(
  'psicc.1',
)

install_man(psicc_man)
