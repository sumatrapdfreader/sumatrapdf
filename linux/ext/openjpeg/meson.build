openjpeg_src_dir = '../../../ext/openjpeg/src/lib/openjp2'

openjpeg_sources = files(
  openjpeg_src_dir / 'bio.c',
  openjpeg_src_dir / 'cidx_manager.c',
  openjpeg_src_dir / 'cio.c',
  openjpeg_src_dir / 'dwt.c',
  openjpeg_src_dir / 'event.c',
  openjpeg_src_dir / 'function_list.c',
  openjpeg_src_dir / 'ht_dec.c',
  openjpeg_src_dir / 'image.c',
  openjpeg_src_dir / 'invert.c',
  openjpeg_src_dir / 'j2k.c',
  openjpeg_src_dir / 'jp2.c',
  openjpeg_src_dir / 'mct.c',
  openjpeg_src_dir / 'mqc.c',
  openjpeg_src_dir / 'openjpeg.c',
  openjpeg_src_dir / 'opj_clock.c',
  openjpeg_src_dir / 'phix_manager.c',
  openjpeg_src_dir / 'pi.c',
  openjpeg_src_dir / 'ppix_manager.c',
  openjpeg_src_dir / 'sparse_array.c',
  openjpeg_src_dir / 't1.c',
  openjpeg_src_dir / 't2.c',
  openjpeg_src_dir / 'tcd.c',
  openjpeg_src_dir / 'tgt.c',
  openjpeg_src_dir / 'thix_manager.c',
  openjpeg_src_dir / 'thread.c',
  openjpeg_src_dir / 'tpix_manager.c',
)

openjpeg_inc = include_directories(openjpeg_src_dir)

openjpeg_lib = static_library('openjpeg', openjpeg_sources,
  include_directories: openjpeg_inc,
  c_args: ['-DOPJ_STATIC', '-DUSE_JPIP'],
)

openjpeg_dep = declare_dependency(
  link_with: openjpeg_lib,
  include_directories: openjpeg_inc,
)
