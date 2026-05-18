brotli_src_dir = '../../../ext/brotli/c'

brotli_sources = files(
  brotli_src_dir / 'common/constants.c',
  brotli_src_dir / 'common/context.c',
  brotli_src_dir / 'common/dictionary.c',
  brotli_src_dir / 'common/platform.c',
  brotli_src_dir / 'common/shared_dictionary.c',
  brotli_src_dir / 'common/transform.c',
  brotli_src_dir / 'dec/bit_reader.c',
  brotli_src_dir / 'dec/decode.c',
  brotli_src_dir / 'dec/huffman.c',
  brotli_src_dir / 'dec/state.c',
  brotli_src_dir / 'enc/backward_references.c',
  brotli_src_dir / 'enc/backward_references_hq.c',
  brotli_src_dir / 'enc/bit_cost.c',
  brotli_src_dir / 'enc/block_splitter.c',
  brotli_src_dir / 'enc/brotli_bit_stream.c',
  brotli_src_dir / 'enc/cluster.c',
  brotli_src_dir / 'enc/command.c',
  brotli_src_dir / 'enc/compound_dictionary.c',
  brotli_src_dir / 'enc/compress_fragment.c',
  brotli_src_dir / 'enc/compress_fragment_two_pass.c',
  brotli_src_dir / 'enc/dictionary_hash.c',
  brotli_src_dir / 'enc/encode.c',
  brotli_src_dir / 'enc/encoder_dict.c',
  brotli_src_dir / 'enc/entropy_encode.c',
  brotli_src_dir / 'enc/fast_log.c',
  brotli_src_dir / 'enc/histogram.c',
  brotli_src_dir / 'enc/literal_cost.c',
  brotli_src_dir / 'enc/memory.c',
  brotli_src_dir / 'enc/metablock.c',
  brotli_src_dir / 'enc/static_dict.c',
  brotli_src_dir / 'enc/utf8_util.c',
)

brotli_inc = include_directories('../../../ext/brotli/c/include')

brotli_lib = static_library('brotli', brotli_sources,
  include_directories: brotli_inc,
)

brotli_dep = declare_dependency(
  link_with: brotli_lib,
  include_directories: brotli_inc,
)
