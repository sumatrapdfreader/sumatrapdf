#
# Meson project file for FreeType 2
#

# Copyright (C) 2020 by
# David Turner, Robert Wilhelm, and Werner Lemberg.
#
# This file is part of the FreeType project, and may only be used, modified,
# and distributed under the terms of the FreeType project license,
# LICENSE.TXT.  By continuing to use, modify, or distribute this file you
# indicate that you have read the license and understand and accept it
# fully.


project('freetype2', 'c',
  meson_version: '>= 0.55.0',
  default_options: ['default_library=both'],
)

#
# Rules to compile the FreeType 2 library itself
#


# Apparently meson doesn't provide a read_file() function, so instead
# running an external command is required.

python = import('python')
python_exe = python.find_installation(required: true)

ft2_version = run_command(python_exe,
  files('builds/meson/extract_freetype_version.py'),
  files('include/freetype/freetype.h')).stdout().strip()

ft2_libtool_version = run_command(python_exe,
  files('builds/meson/extract_libtool_version.py'),
  '--soversion',
  files('builds/unix/configure.raw')).stdout().strip()

ft2_includes = include_directories('include')


# Generate a custom `ftmodule.h` version based on the content of
# `modules.cfg`.

ftmodule_h = custom_target('ftmodule.h',
  output: 'ftmodule.h',
  input: 'modules.cfg',
  command: [python_exe, files('builds/meson/parse_modules_cfg.py'),
            '--format=ftmodule.h', '@INPUT@', '--output', '@OUTPUT@'],
  install: true,
  install_dir: 'include/freetype2/freetype/config',
)
ft2_sources = [ftmodule_h]


# FreeType 2 modules.

ft_main_modules = run_command(python_exe,
  files('builds/meson/parse_modules_cfg.py'),
  '--format=main-modules',
  files('modules.cfg')).stdout().strip().split()

ft2_sources += files([
  'src/base/ftbase.c',
  'src/base/ftinit.c',
])

foreach mod: ft_main_modules
  source = mod
  if mod == 'winfonts'
    source = 'winfnt'
  elif mod == 'cid'
    source = 'type1cid'
  endif
  ft2_sources += 'src/@0@/@1@.c'.format(mod, source)
endforeach

# NOTE: The `gzip` and `bzip2` aux modules are handled through options.
ft_aux_modules = run_command(python_exe,
  files('builds/meson/parse_modules_cfg.py'),
  '--format=aux-modules',
  files('modules.cfg')).stdout().strip().split()

foreach auxmod: ft_aux_modules
  source = auxmod
  # Most sources are named `src/<module>/<module>.c`, but there are a few
  # exceptions handled here.
  if auxmod == 'cache'
    source = 'ftcache'
  elif auxmod == 'lzw'
    source = 'ftlzw'
  elif auxmod == 'gzip' or auxmod == 'bzip2'
    # Handled through options instead, see below.
    continue
  endif
  ft2_sources += 'src/@0@/@1@.c'.format(auxmod, source)
endforeach


# FreeType 2 base extensions.
# Normally configured through `modules.cfg`.

base_extensions = run_command(python_exe,
  files('builds/meson/parse_modules_cfg.py'),
  '--format=base-extensions-list',
  files('modules.cfg')).stdout().split()

foreach ext: base_extensions
  ft2_sources += files('src/base/' + ext)
endforeach


# Header files.

ft2_public_headers = files([
  'include/freetype/freetype.h',
  'include/freetype/ftadvanc.h',
  'include/freetype/ftbbox.h',
  'include/freetype/ftbdf.h',
  'include/freetype/ftbitmap.h',
  'include/freetype/ftbzip2.h',
  'include/freetype/ftcache.h',
  'include/freetype/ftchapters.h',
  'include/freetype/ftcolor.h',
  'include/freetype/ftdriver.h',
  'include/freetype/fterrdef.h',
  'include/freetype/fterrors.h',
  'include/freetype/ftfntfmt.h',
  'include/freetype/ftgasp.h',
  'include/freetype/ftglyph.h',
  'include/freetype/ftgxval.h',
  'include/freetype/ftgzip.h',
  'include/freetype/ftimage.h',
  'include/freetype/ftincrem.h',
  'include/freetype/ftlcdfil.h',
  'include/freetype/ftlist.h',
  'include/freetype/ftlzw.h',
  'include/freetype/ftmac.h',
  'include/freetype/ftmm.h',
  'include/freetype/ftmodapi.h',
  'include/freetype/ftmoderr.h',
  'include/freetype/ftotval.h',
  'include/freetype/ftoutln.h',
  'include/freetype/ftparams.h',
  'include/freetype/ftpfr.h',
  'include/freetype/ftrender.h',
  'include/freetype/ftsizes.h',
  'include/freetype/ftsnames.h',
  'include/freetype/ftstroke.h',
  'include/freetype/ftsynth.h',
  'include/freetype/ftsystem.h',
  'include/freetype/fttrigon.h',
  'include/freetype/fttypes.h',
  'include/freetype/ftwinfnt.h',
  'include/freetype/t1tables.h',
  'include/freetype/ttnameid.h',
  'include/freetype/tttables.h',
  'include/freetype/tttags.h',
])

ft2_config_headers = files([
  'include/freetype/config/ftconfig.h',
  'include/freetype/config/ftheader.h',
  'include/freetype/config/ftstdlib.h',
  'include/freetype/config/integer-types.h',
  'include/freetype/config/mac-support.h',
  'include/freetype/config/public-macros.h',
])

ft2_defines = []


# System support file.

cc = meson.get_compiler('c')

# NOTE: msys2 on Windows has `unistd.h` and `fcntl.h` but not `sys/mman.h`!
has_unistd_h = cc.has_header('unistd.h')
has_fcntl_h = cc.has_header('fcntl.h')
has_sys_mman_h = cc.has_header('sys/mman.h')

if has_unistd_h
  ft2_defines += ['-DHAVE_UNISTD_H=1']
endif
if has_fcntl_h
  ft2_defines += ['-DHAVE_FCNTL_H']
endif

mmap_option = get_option('mmap')
if mmap_option.auto()
  use_mmap = has_unistd_h and has_fcntl_h and has_sys_mman_h
else
  use_mmap = mmap_option.enabled()
endif
if use_mmap
  # This version of ftsystem.c uses mmap() to read input font files.
  ft2_sources += files(['builds/unix/ftsystem.c',])
else
  ft2_sources += files(['src/base/ftsystem.c',])
endif


# Debug support file
#
# NOTE: Some specialized versions exist for other platforms not supported by
# Meson.  Most implementation differences are extremely minor, i.e., in the
# implementation of FT_Message() and FT_Panic(), and getting the `FT2_DEBUG`
# value from the environment, when this is supported.  A smaller refactor
# might make these platform-specific files much smaller, and could be moved
# into `ftsystem.c` as well.
#
if host_machine.system() == 'windows'
  ft2_debug_src = 'builds/windows/ftdebug.c'
else
  ft2_debug_src = 'src/base/ftdebug.c'
endif
ft2_sources += files([ft2_debug_src])

ft2_deps = []


# Generate `ftoption.h` based on available dependencies.

ftoption_command = [python_exe,
  files('builds/meson/process_ftoption_h.py'),
  '@INPUT@', '--output=@OUTPUT@']

# GZip support
zlib_option = get_option('zlib')
if zlib_option == 'disabled'
  ftoption_command += ['--disable=FT_CONFIG_OPTION_USE_ZLIB']
else
  ftoption_command += ['--enable=FT_CONFIG_OPTION_USE_ZLIB']
  if zlib_option == 'builtin'
    ftoption_command += ['--disable=FT_CONFIG_OPTION_SYSTEM_ZLIB']
  else
    # Probe for the system version.
    zlib_system = dependency('zlib', required: zlib_option == 'system')
    ft2_deps += [zlib_system]
    ftoption_command += ['--enable=FT_CONFIG_OPTION_SYSTEM_ZLIB']
  endif
  ft2_sources += files(['src/gzip/ftgzip.c',])
endif

# BZip2 support
#
# IMPORTANT NOTE: Without `static: false` here, Meson will find both the
# static library version and the shared library version when they are
# installed on the system, and will try to link them *both* to the final
# library!
bzip2_dep = meson.get_compiler('c').find_library('bz2',
              static: false,
              required: get_option('bzip2'))
if bzip2_dep.found()
  ftoption_command += ['--enable=FT_CONFIG_OPTION_USE_BZIP2']
  ft2_sources += files(['src/bzip2/ftbzip2.c',])
  ft2_deps += [bzip2_dep]
endif

# PNG support
libpng_dep = dependency('libpng', required: get_option('png'))
ftoption_command += ['--enable=FT_CONFIG_OPTION_USE_PNG']
ft2_deps += [libpng_dep]

# Harfbuzz support
harfbuzz_dep = dependency('harfbuzz',
                 version: '>= 1.8.0',
                 required: get_option('harfbuzz'))
ftoption_command += ['--enable=FT_CONFIG_OPTION_USE_HARFBUZZ']
ft2_deps += [harfbuzz_dep]

# Brotli decompression support
brotli_dep = dependency('libbrotlidec', required: get_option('brotli'))
ftoption_command += ['--enable=FT_CONFIG_OPTION_USE_BROTLI']
ft2_deps += [brotli_dep]

# We can now generate `ftoption.h`.
ftoption_h = custom_target('ftoption.h',
  input: 'include/freetype/config/ftoption.h',
  output: 'ftoption.h',
  command: ftoption_command,
  install: true,
  install_dir: 'include/freetype2/freetype/config',
)
ft2_sources += ftoption_h


# QUESTION: What if the compiler doesn't support `-D` but uses `/D` instead
# as on Windows?
#
# Other build systems have something like c_defines to list defines in a
# more portable way.  For now assume the compiler supports `-D` (hint: Visual
# Studio does).
ft2_defines += ['-DFT2_BUILD_LIBRARY=1']


# Ensure that the `ftoption.h` file generated above will be used to build
# FreeType.  Unfortunately, and very surprisingly, configure_file() does not
# support putting the output file in a sub-directory, so we have to override
# the default which is `<freetype/config/ftoption.h>`.
#
# It would be cleaner to generate the file directly into
# `${MESON_BUILD_DIR}/freetype/config/ftoption.h`.  See
# 'https://github.com/mesonbuild/meson/issues/2320' for details.
ft2_defines += ['-DFT_CONFIG_OPTIONS_H=<ftoption.h>']

ft2_c_args = ft2_defines
if cc.has_function_attribute('visibility:hidden')
  ft2_c_args += ['-fvisibility=hidden']
endif

ft2_lib = library('freetype',
  sources: ft2_sources + [ftmodule_h],
  c_args: ft2_c_args,
  include_directories: ft2_includes,
  dependencies: ft2_deps,
  install: true,
  version: ft2_libtool_version,
)


# To be used by other projects including this one through subproject().
freetype2_dep = declare_dependency(
  include_directories: ft2_includes,
  link_with: ft2_lib,
  version: ft2_libtool_version)


# NOTE: Using both `install_dir` and `subdir` doesn't seem to work below,
# i.e., the subdir value seems to be ignored, contrary to examples in the
# Meson documentation.
install_headers('include/ft2build.h',
   install_dir: 'include/freetype2')
install_headers(ft2_public_headers,
  install_dir: 'include/freetype2/freetype')
install_headers(ft2_config_headers,
  install_dir: 'include/freetype2/freetype/config')


# TODO(david): Declare_dependency() for using this in a Meson subproject
#
pkgconfig = import('pkgconfig')
pkgconfig.generate(ft2_lib,
  filebase: 'freetype2',
  name: 'FreeType 2',
  description: 'A free, high-quality, and portable font engine.',
  url: 'https://freetype.org',
  subdirs: 'freetype2',
  version: ft2_libtool_version,
)


# NOTE: Unlike the old `make refdoc` command, this generates the
# documentation under `$BUILD/docs/` since Meson doesn't support modifying
# the source root directory (which is a good thing).
gen_docs = custom_target('freetype2 reference documentation',
  output: 'docs',
  input: ft2_public_headers + ft2_config_headers,
  command: [python_exe,
    files('builds/meson/generate_reference_docs.py'),
    '--version=' + ft2_version,
    '--input-dir=' + meson.source_root(),
    '--output-dir=@OUTPUT@'
  ],
)

# EOF
