extract_src_dir = '../../../ext/extract/src'
extract_inc_dir = '../../../ext/extract/include'

extract_sources = files(
  extract_src_dir / 'alloc.c',
  extract_src_dir / 'astring.c',
  extract_src_dir / 'boxer.c',
  extract_src_dir / 'buffer.c',
  extract_src_dir / 'document.c',
  extract_src_dir / 'docx.c',
  extract_src_dir / 'docx_template.c',
  extract_src_dir / 'extract.c',
  extract_src_dir / 'html.c',
  extract_src_dir / 'join.c',
  extract_src_dir / 'json.c',
  extract_src_dir / 'mem.c',
  extract_src_dir / 'odt.c',
  extract_src_dir / 'odt_template.c',
  extract_src_dir / 'outf.c',
  extract_src_dir / 'rect.c',
  extract_src_dir / 'sys.c',
  extract_src_dir / 'text.c',
  extract_src_dir / 'xml.c',
  extract_src_dir / 'zip.c',
)

extract_inc = include_directories(extract_inc_dir, extract_src_dir)

extract_lib = static_library('extract', extract_sources,
  include_directories: extract_inc,
)

extract_dep = declare_dependency(
  link_with: extract_lib,
  include_directories: extract_inc,
)
