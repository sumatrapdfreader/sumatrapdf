wtpt_srcs = files(
  'wtpt.c',
)

wtpt_exe = executable(
  'wtpt',
  wtpt_srcs,
  dependencies: [liblcms2_dep, common_dep, m_dep],
)

wtpt_man = files(
  'wtpt.1',
)

# install_man(wtpt_man)

examples = {
  'roundtrip': files('roundtrip.c'),
  'mktiff8': files('mktiff8.c'),
  # 'vericc': files('vericc.c'),
  # The following are commented out because they're LCMS 1
  # 'mkgrayer': files('mkgrayer.c'),
  # 'mkcmy': files('mkcmy.c'),
  # 'itufax': files('itufax.c'),
}

foreach exe, srcs : examples
  executable(
    exe,
    srcs,
    dependencies: [liblcms2_dep, m_dep],
  )
endforeach
