# Phase 1: Minimal test binary to verify MuPDF works on Linux
# Phase 2+: Full application sources will be added here

common_deps = [
  mupdf_dep,
  zlib_dep,
  freetype_dep,
  harfbuzz_dep,
  jpegturbo_dep,
  openjpeg_dep,
  jbig2dec_dep,
  lcms2_dep,
  brotli_dep,
  extract_dep,
  mujs_dep,
  libdjvu_dep,
  libwebp_dep,
  gumbo_dep,
]

common_link_args = ['-lm', '-lpthread']

# The main application binary (currently the renderer test)
sumatrapdf = executable('sumatrapdf',
  files('test_mupdf.c'),
  mupdf_resource_objs,
  include_directories: [
    include_directories('../../mupdf/include'),
  ],
  dependencies: common_deps,
  link_args: common_link_args,
  install: true,
)

test('mupdf-smoke', sumatrapdf, args: [])

# ─── Install targets ──────────────────────────────────────────────────────────

dist_dir = meson.project_source_root() / 'linux' / 'dist'

install_data(dist_dir / 'sumatrapdf.desktop',
  install_dir: get_option('datadir') / 'applications')

install_data(dist_dir / 'org.sumatrapdf.SumatraPDF.metainfo.xml',
  install_dir: get_option('datadir') / 'metainfo')

# Icons
icon_src = meson.project_source_root() / 'gfx'
install_data(icon_src / 'SumatraPDF-256x256x32.png',
  install_dir: get_option('datadir') / 'icons' / 'hicolor' / '256x256' / 'apps',
  rename: 'sumatrapdf.png')
install_data(icon_src / 'SumatraPDF-128x128x32.png',
  install_dir: get_option('datadir') / 'icons' / 'hicolor' / '128x128' / 'apps',
  rename: 'sumatrapdf.png')
install_data(icon_src / 'SumatraPDF-48x48x32.png',
  install_dir: get_option('datadir') / 'icons' / 'hicolor' / '48x48' / 'apps',
  rename: 'sumatrapdf.png')
