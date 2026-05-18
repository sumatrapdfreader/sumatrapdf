freetype_src_dir = '../../../ext/freetype/src'
freetype_inc_dir = '../../../ext/freetype/include'
freetype_mupdf_scripts = '../../../mupdf/scripts/freetype'

freetype_sources = files(
  freetype_src_dir / 'base/ftbase.c',
  freetype_src_dir / 'base/ftbbox.c',
  freetype_src_dir / 'base/ftbitmap.c',
  freetype_src_dir / 'base/ftdebug.c',
  freetype_src_dir / 'base/ftfstype.c',
  freetype_src_dir / 'base/ftgasp.c',
  freetype_src_dir / 'base/ftglyph.c',
  freetype_src_dir / 'base/ftinit.c',
  freetype_src_dir / 'base/ftotval.c',
  freetype_src_dir / 'base/ftstroke.c',
  freetype_src_dir / 'base/ftsynth.c',
  freetype_src_dir / 'base/ftsystem.c',
  freetype_src_dir / 'base/fttype1.c',
  freetype_src_dir / 'gzip/ftgzip.c',
  freetype_src_dir / 'cff/cff.c',
  freetype_src_dir / 'cid/type1cid.c',
  freetype_src_dir / 'psaux/psaux.c',
  freetype_src_dir / 'pshinter/pshinter.c',
  freetype_src_dir / 'psnames/psnames.c',
  freetype_src_dir / 'raster/raster.c',
  freetype_src_dir / 'sfnt/sfnt.c',
  freetype_src_dir / 'smooth/smooth.c',
  freetype_src_dir / 'truetype/truetype.c',
  freetype_src_dir / 'type1/type1.c',
)

freetype_inc = include_directories(freetype_inc_dir, freetype_mupdf_scripts)

freetype_lib = static_library('freetype', freetype_sources,
  include_directories: [freetype_inc, zlib_inc, brotli_inc],
  c_args: [
    '-DFT2_BUILD_LIBRARY',
    '-DFT_CONFIG_MODULES_H="slimftmodules.h"',
    '-DFT_CONFIG_OPTIONS_H="slimftoptions.h"',
    '-DFT_CONFIG_OPTION_USE_BROTLI',
    '-DFT_CONFIG_OPTION_SYSTEM_ZLIB',
  ],
  dependencies: [zlib_dep, brotli_dep],
)

freetype_dep = declare_dependency(
  link_with: freetype_lib,
  include_directories: freetype_inc,
)
