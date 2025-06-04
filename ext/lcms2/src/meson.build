lcms2_srcs = files(
  'cmsalpha.c',
  'cmscam02.c',
  'cmscgats.c',
  'cmscnvrt.c',
  'cmserr.c',
  'cmsgamma.c',
  'cmsgmt.c',
  'cmshalf.c',
  'cmsintrp.c',
  'cmsio0.c',
  'cmsio1.c',
  'cmslut.c',
  'cmsmd5.c',
  'cmsmtrx.c',
  'cmsnamed.c',
  'cmsopt.c',
  'cmspack.c',
  'cmspcs.c',
  'cmsplugin.c',
  'cmsps2.c',
  'cmssamp.c',
  'cmssm.c',
  'cmstypes.c',
  'cmsvirt.c',
  'cmswtpnt.c',
  'cmsxform.c',
)

if host_machine.system() == 'windows'
    if get_option('default_library') == 'shared'

    lcms2_rc = configure_file(
        input: 'lcms2.rc.in',
        configuration: version_cfg,
        output: 'lcms2.rc',
        )

        lcms2_srcs += win.compile_resources(lcms2_rc)
        cargs += '-DCMS_DLL_BUILD=1'
    endif  
endif

liblcms2_lib = library(
  'lcms2',
  lcms2_srcs,
  include_directories: inc_dirs,
  gnu_symbol_visibility: 'hidden',
  dependencies: deps,
  c_args: cargs,
  version: library_version,
  # vs_module_defs: 'lcms2.def',
  install: true,
)

liblcms2_dep = declare_dependency(
  link_with: liblcms2_lib,
  include_directories: [inc_dirs, include_directories('.')],
)
