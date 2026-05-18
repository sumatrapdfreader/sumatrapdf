lcms2_src_dir = '../../../ext/lcms2/src'
lcms2_inc_dir = '../../../ext/lcms2/include'

lcms2_sources = files(
  lcms2_src_dir / 'cmsalpha.c',
  lcms2_src_dir / 'cmscam02.c',
  lcms2_src_dir / 'cmscgats.c',
  lcms2_src_dir / 'cmscnvrt.c',
  lcms2_src_dir / 'cmserr.c',
  lcms2_src_dir / 'cmsgamma.c',
  lcms2_src_dir / 'cmsgmt.c',
  lcms2_src_dir / 'cmshalf.c',
  lcms2_src_dir / 'cmsintrp.c',
  lcms2_src_dir / 'cmsio0.c',
  lcms2_src_dir / 'cmsio1.c',
  lcms2_src_dir / 'cmslut.c',
  lcms2_src_dir / 'cmsmd5.c',
  lcms2_src_dir / 'cmsmtrx.c',
  lcms2_src_dir / 'cmsnamed.c',
  lcms2_src_dir / 'cmsopt.c',
  lcms2_src_dir / 'cmspack.c',
  lcms2_src_dir / 'cmspcs.c',
  lcms2_src_dir / 'cmsplugin.c',
  lcms2_src_dir / 'cmsps2.c',
  lcms2_src_dir / 'cmssamp.c',
  lcms2_src_dir / 'cmssm.c',
  lcms2_src_dir / 'cmstypes.c',
  lcms2_src_dir / 'cmsvirt.c',
  lcms2_src_dir / 'cmswtpnt.c',
  lcms2_src_dir / 'cmsxform.c',
)

lcms2_inc = include_directories(lcms2_inc_dir)

lcms2_lib = static_library('lcms2', lcms2_sources,
  include_directories: lcms2_inc,
)

lcms2_dep = declare_dependency(
  link_with: lcms2_lib,
  include_directories: lcms2_inc,
)
