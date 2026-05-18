project('sumatrapdf-linux', 'c', 'cpp',
  version: '3.6-dev',
  default_options: [
    'c_std=c11',
    'cpp_std=c++17',
    'warning_level=2',
    'buildtype=debugoptimized',
  ],
  meson_version: '>= 0.60.0',
)

cc = meson.get_compiler('c')
cpp = meson.get_compiler('cpp')

# Common flags
add_project_arguments('-DNDEBUG', language: ['c', 'cpp'])

# ─── External dependencies (system or subproject) ──────────────────────────────

# We build most dependencies from source (matching the Windows build)
# This ensures version consistency and avoids system library mismatches.

subdir('linux/ext/zlib')
subdir('linux/ext/freetype')
subdir('linux/ext/harfbuzz')
subdir('linux/ext/libjpeg-turbo')
subdir('linux/ext/openjpeg')
subdir('linux/ext/jbig2dec')
subdir('linux/ext/lcms2')
subdir('linux/ext/brotli')
subdir('linux/ext/extract')
subdir('linux/ext/gumbo-parser')
subdir('linux/ext/libwebp')
subdir('linux/ext/mujs')
subdir('linux/ext/libdjvu')

# MuPDF (core rendering engine)
subdir('linux/ext/mupdf')

# ─── Platform abstraction & test binary ────────────────────────────────────────

subdir('linux/src')
