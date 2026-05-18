gumbo_src_dir = '../../../ext/gumbo-parser/src'

gumbo_sources = files(
  gumbo_src_dir / 'attribute.c',
  gumbo_src_dir / 'char_ref.c',
  gumbo_src_dir / 'error.c',
  gumbo_src_dir / 'parser.c',
  gumbo_src_dir / 'string_buffer.c',
  gumbo_src_dir / 'string_piece.c',
  gumbo_src_dir / 'tag.c',
  gumbo_src_dir / 'tokenizer.c',
  gumbo_src_dir / 'utf8.c',
  gumbo_src_dir / 'util.c',
  gumbo_src_dir / 'vector.c',
)

gumbo_inc = include_directories(gumbo_src_dir)

gumbo_lib = static_library('gumbo', gumbo_sources,
  include_directories: gumbo_inc,
)

gumbo_dep = declare_dependency(
  link_with: gumbo_lib,
  include_directories: gumbo_inc,
)
