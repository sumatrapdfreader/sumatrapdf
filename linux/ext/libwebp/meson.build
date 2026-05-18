libwebp_src_dir = '../../../ext/libwebp/src'
libwebp_root_dir = '../../../ext/libwebp'

libwebp_dec_sources = files(
  libwebp_src_dir / 'dec/alpha_dec.c',
  libwebp_src_dir / 'dec/buffer_dec.c',
  libwebp_src_dir / 'dec/frame_dec.c',
  libwebp_src_dir / 'dec/idec_dec.c',
  libwebp_src_dir / 'dec/io_dec.c',
  libwebp_src_dir / 'dec/quant_dec.c',
  libwebp_src_dir / 'dec/tree_dec.c',
  libwebp_src_dir / 'dec/vp8_dec.c',
  libwebp_src_dir / 'dec/vp8l_dec.c',
  libwebp_src_dir / 'dec/webp_dec.c',
)

libwebp_utils_sources = files(
  libwebp_src_dir / 'utils/bit_reader_utils.c',
  libwebp_src_dir / 'utils/bit_writer_utils.c',
  libwebp_src_dir / 'utils/color_cache_utils.c',
  libwebp_src_dir / 'utils/filters_utils.c',
  libwebp_src_dir / 'utils/huffman_encode_utils.c',
  libwebp_src_dir / 'utils/huffman_utils.c',
  libwebp_src_dir / 'utils/palette.c',
  libwebp_src_dir / 'utils/quant_levels_dec_utils.c',
  libwebp_src_dir / 'utils/quant_levels_utils.c',
  libwebp_src_dir / 'utils/random_utils.c',
  libwebp_src_dir / 'utils/rescaler_utils.c',
  libwebp_src_dir / 'utils/thread_utils.c',
  libwebp_src_dir / 'utils/utils.c',
)

libwebp_sharpyuv_sources = files(
  libwebp_root_dir / 'sharpyuv/sharpyuv.c',
  libwebp_root_dir / 'sharpyuv/sharpyuv_cpu.c',
  libwebp_root_dir / 'sharpyuv/sharpyuv_csp.c',
  libwebp_root_dir / 'sharpyuv/sharpyuv_dsp.c',
  libwebp_root_dir / 'sharpyuv/sharpyuv_gamma.c',
  libwebp_root_dir / 'sharpyuv/sharpyuv_neon.c',
  libwebp_root_dir / 'sharpyuv/sharpyuv_sse2.c',
)

libwebp_dsp_sources = files(
  libwebp_src_dir / 'dsp/alpha_processing.c',
  libwebp_src_dir / 'dsp/alpha_processing_sse2.c',
  libwebp_src_dir / 'dsp/alpha_processing_sse41.c',
  libwebp_src_dir / 'dsp/cost.c',
  libwebp_src_dir / 'dsp/cpu.c',
  libwebp_src_dir / 'dsp/dec.c',
  libwebp_src_dir / 'dsp/dec_clip_tables.c',
  libwebp_src_dir / 'dsp/dec_sse2.c',
  libwebp_src_dir / 'dsp/dec_sse41.c',
  libwebp_src_dir / 'dsp/filters.c',
  libwebp_src_dir / 'dsp/filters_sse2.c',
  libwebp_src_dir / 'dsp/lossless.c',
  libwebp_src_dir / 'dsp/lossless_sse2.c',
  libwebp_src_dir / 'dsp/lossless_sse41.c',
  libwebp_src_dir / 'dsp/rescaler.c',
  libwebp_src_dir / 'dsp/rescaler_sse2.c',
  libwebp_src_dir / 'dsp/ssim.c',
  libwebp_src_dir / 'dsp/ssim_sse2.c',
  libwebp_src_dir / 'dsp/upsampling.c',
  libwebp_src_dir / 'dsp/upsampling_sse2.c',
  libwebp_src_dir / 'dsp/upsampling_sse41.c',
  libwebp_src_dir / 'dsp/yuv.c',
  libwebp_src_dir / 'dsp/yuv_sse2.c',
  libwebp_src_dir / 'dsp/yuv_sse41.c',
)

libwebp_inc = include_directories('../../../ext/libwebp', '../../../ext/libwebp/src')

libwebp_lib = static_library('webp',
  libwebp_dec_sources + libwebp_utils_sources + libwebp_sharpyuv_sources + libwebp_dsp_sources,
  include_directories: libwebp_inc,
)

libwebp_dep = declare_dependency(
  link_with: libwebp_lib,
  include_directories: libwebp_inc,
)
