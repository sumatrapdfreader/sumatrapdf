zlib_sources = files(
  '../../../ext/zlib/adler32.c',
  '../../../ext/zlib/compress.c',
  '../../../ext/zlib/crc32.c',
  '../../../ext/zlib/deflate.c',
  '../../../ext/zlib/gzclose.c',
  '../../../ext/zlib/gzlib.c',
  '../../../ext/zlib/gzread.c',
  '../../../ext/zlib/gzwrite.c',
  '../../../ext/zlib/inffast.c',
  '../../../ext/zlib/inflate.c',
  '../../../ext/zlib/inftrees.c',
  '../../../ext/zlib/trees.c',
  '../../../ext/zlib/zutil.c',
)

zlib_inc = include_directories('../../../ext/zlib')

zlib_lib = static_library('zlib', zlib_sources,
  include_directories: zlib_inc,
)

zlib_dep = declare_dependency(
  link_with: zlib_lib,
  include_directories: zlib_inc,
)
