jbig2dec_src_dir = '../../../ext/jbig2dec'

jbig2dec_sources = files(
  jbig2dec_src_dir / 'jbig2.c',
  jbig2dec_src_dir / 'jbig2_arith.c',
  jbig2dec_src_dir / 'jbig2_arith_iaid.c',
  jbig2dec_src_dir / 'jbig2_arith_int.c',
  jbig2dec_src_dir / 'jbig2_generic.c',
  jbig2dec_src_dir / 'jbig2_halftone.c',
  jbig2dec_src_dir / 'jbig2_huffman.c',
  jbig2dec_src_dir / 'jbig2_hufftab.c',
  jbig2dec_src_dir / 'jbig2_image.c',
  jbig2dec_src_dir / 'jbig2_mmr.c',
  jbig2dec_src_dir / 'jbig2_page.c',
  jbig2dec_src_dir / 'jbig2_refinement.c',
  jbig2dec_src_dir / 'jbig2_segment.c',
  jbig2dec_src_dir / 'jbig2_symbol_dict.c',
  jbig2dec_src_dir / 'jbig2_text.c',
)

jbig2dec_inc = include_directories(jbig2dec_src_dir)

jbig2dec_lib = static_library('jbig2dec', jbig2dec_sources,
  include_directories: jbig2dec_inc,
  c_args: ['-DHAVE_STDINT_H=1', '-DJBIG_NO_MEMENTO'],
)

jbig2dec_dep = declare_dependency(
  link_with: jbig2dec_lib,
  include_directories: jbig2dec_inc,
)
