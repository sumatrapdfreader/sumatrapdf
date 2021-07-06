project('harfbuzz', 'c', 'cpp',
  meson_version: '>= 0.47.0',
  version: '2.7.4',
  default_options: [
    'cpp_eh=none',          # Just to support msvc, we are passing -fno-rtti also anyway
    'cpp_rtti=false',       # Just to support msvc, we are passing -fno-exceptions also anyway
    'cpp_std=c++11',
    'wrap_mode=nofallback', # Use --wrap-mode=default to revert, https://github.com/harfbuzz/harfbuzz/pull/2548
  ],
)

hb_version_arr = meson.project_version().split('.')
hb_version_major = hb_version_arr[0].to_int()
hb_version_minor = hb_version_arr[1].to_int()
hb_version_micro = hb_version_arr[2].to_int()

# libtool versioning
hb_version_int = hb_version_major*10000 + hb_version_minor*100 + hb_version_micro
hb_libtool_version_info = '@0@:0:@0@'.format(hb_version_int)

pkgmod = import('pkgconfig')
cpp = meson.get_compiler('cpp')
null_dep = dependency('', required: false)

if cpp.get_id() == 'msvc'
  # Ignore several spurious warnings for things HarfBuzz does very commonly.
  # If a warning is completely useless and spammy, use '/wdXXXX' to suppress it
  # If a warning is harmless but hard to fix, use '/woXXXX' so it's shown once
  # NOTE: Only add warnings here if you are sure they're spurious
  msvc_args = [
    '/wd4018', # implicit signed/unsigned conversion
    '/wd4146', # unary minus on unsigned (beware INT_MIN)
    '/wd4244', # lossy type conversion (e.g. double -> int)
    '/wd4305', # truncating type conversion (e.g. double -> float)
    cpp.get_supported_arguments(['/utf-8']), # set the input encoding to utf-8
  ]
  add_project_arguments(msvc_args, language: ['c', 'cpp'])
  # Disable SAFESEH with MSVC for libs that use external deps that are built with MinGW
  # noseh_link_args = ['/SAFESEH:NO']
endif

add_project_link_arguments(cpp.get_supported_link_arguments([
  '-Bsymbolic-functions'
]), language: 'c')

add_project_arguments(cpp.get_supported_arguments([
  '-fno-exceptions',
  '-fno-rtti',
  '-fno-threadsafe-statics',
  '-fvisibility-inlines-hidden',
]), language: 'cpp')

if host_machine.cpu_family() == 'arm' and cpp.alignment('struct { char c; }') != 1
  if cpp.has_argument('-mstructure-size-boundary=8')
    add_project_arguments('-mstructure-size-boundary=8', language: 'cpp')
  endif
endif

check_headers = [
  ['unistd.h'],
  ['sys/mman.h'],
  ['stdbool.h'],
]

check_funcs = [
  ['atexit'],
  ['mprotect'],
  ['sysconf'],
  ['getpagesize'],
  ['mmap'],
  ['isatty'],
]

m_dep = cpp.find_library('m', required: false)

freetype_dep = null_dep
if not get_option('freetype').disabled()
  freetype_dep = dependency('freetype2', required: false)

  if (not freetype_dep.found() and
      cpp.get_id() == 'msvc' and
      cpp.has_header('ft2build.h'))
    freetype_dep = cpp.find_library('freetype', required: false)
  endif

  if not freetype_dep.found()
    # https://github.com/harfbuzz/harfbuzz/pull/2498
    freetype_dep = dependency('freetype2', required: get_option('freetype'),
                              fallback: ['freetype2', 'freetype_dep'],
                              default_options: ['harfbuzz=disabled'])
  endif
endif

glib_dep = dependency('glib-2.0', required: get_option('glib'),
                      fallback: ['glib', 'libglib_dep'])
gobject_dep = dependency('gobject-2.0', required: get_option('gobject'),
                         fallback: ['glib', 'libgobject_dep'])
fontconfig_dep = dependency('fontconfig', required: get_option('fontconfig'),
                            fallback: ['fontconfig', 'fontconfig_dep'])
graphite2_dep = dependency('graphite2', required: get_option('graphite'))

icu_dep = null_dep
if not get_option('icu').disabled()
  icu_dep = dependency('icu-uc', required: false)

  if (not icu_dep.found() and
      cpp.get_id() == 'msvc' and
      cpp.has_header('unicode/uchar.h') and
      cpp.has_header('unicode/unorm2.h') and
      cpp.has_header('unicode/ustring.h') and
      cpp.has_header('unicode/utf16.h') and
      cpp.has_header('unicode/uversion.h') and
      cpp.has_header('unicode/uscript.h'))
    if get_option('buildtype') == 'debug'
      icu_dep = cpp.find_library('icuucd', required: false)
    else
      icu_dep = cpp.find_library('icuuc', required: false)
    endif
  endif

  if not icu_dep.found()
    icu_dep = dependency('icu-uc', required: get_option('icu'))
  endif
endif

cairo_dep = null_dep
cairo_ft_dep = null_dep
if not get_option('cairo').disabled()
  cairo_dep = dependency('cairo', required: false)
  cairo_ft_dep = dependency('cairo-ft', required: false)

  if (not cairo_dep.found() and
      cpp.get_id() == 'msvc' and
      cpp.has_header('cairo.h'))
    cairo_dep = cpp.find_library('cairo', required: false)
    if cairo_dep.found() and cpp.has_function('cairo_ft_font_face_create_for_ft_face',
                                              prefix: '#include <cairo-ft.h>',
                                              dependencies: cairo_dep)
      cairo_ft_dep = cairo_dep
    endif
  endif

  if not cairo_dep.found()
    # Requires Meson 0.54.0 to use cairo subproject
    if meson.version().version_compare('>=0.54.0')
      # Note that we don't have harfbuzz -> cairo -> freetype2 -> harfbuzz fallback
      # dependency cycle here because we have configured freetype2 above with
      # harfbuzz support disabled, so when cairo will lookup freetype2 dependency
      # it will be forced to use that one.
      cairo_dep = dependency('cairo', fallback: 'cairo', required: get_option('cairo'))
      cairo_ft_dep = dependency('cairo-ft', fallback: 'cairo', required: get_option('cairo'))
    elif get_option('cairo').enabled()
      error('cairo feature is enabled but it cannot be found on the system and ' +
            'meson>=0.54.0 is required to build it as subproject')
    endif
  endif
endif

conf = configuration_data()
incconfig = include_directories('.')

add_project_arguments('-DHAVE_CONFIG_H', language: ['c', 'cpp'])

warn_cflags = [
  '-Wno-non-virtual-dtor',
]

cpp_args = cpp.get_supported_arguments(warn_cflags)

if glib_dep.found()
  conf.set('HAVE_GLIB', 1)
endif

if gobject_dep.found()
  conf.set('HAVE_GOBJECT', 1)
endif

if cairo_dep.found()
  conf.set('HAVE_CAIRO', 1)
endif

if cairo_ft_dep.found()
  conf.set('HAVE_CAIRO_FT', 1)
endif

if graphite2_dep.found()
  conf.set('HAVE_GRAPHITE2', 1)
endif

if icu_dep.found()
  conf.set('HAVE_ICU', 1)
endif

if get_option('icu_builtin')
  conf.set('HAVE_ICU_BUILTIN', 1)
endif

if get_option('experimental_api')
  conf.set('HB_EXPERIMENTAL_API', 1)
endif

if freetype_dep.found()
  conf.set('HAVE_FREETYPE', 1)
  check_freetype_funcs = [
    ['FT_Get_Var_Blend_Coordinates', {'deps': freetype_dep}],
    ['FT_Set_Var_Blend_Coordinates', {'deps': freetype_dep}],
    ['FT_Done_MM_Var', {'deps': freetype_dep}],
  ]

  if freetype_dep.type_name() == 'internal'
    foreach func: check_freetype_funcs
      name = func[0]
      conf.set('HAVE_@0@'.format(name.to_upper()), 1)
    endforeach
  else
    check_funcs += check_freetype_funcs
  endif
endif

if fontconfig_dep.found()
  conf.set('HAVE_FONTCONFIG', 1)
endif

gdi_uniscribe_deps = []
# GDI (Uniscribe) (Windows)
if host_machine.system() == 'windows' and not get_option('gdi').disabled()
  if (get_option('directwrite').enabled() and
      not (cpp.has_header('usp10.h') and cpp.has_header('windows.h')))
    error('GDI/Uniscribe was enabled explicitly, but required headers are missing.')
  endif

  gdi_deps_found = true
  foreach usplib : ['usp10', 'gdi32', 'rpcrt4']
    dep = cpp.find_library(usplib, required: get_option('gdi'))
    gdi_deps_found = gdi_deps_found and dep.found()
    gdi_uniscribe_deps += dep
  endforeach

  if gdi_deps_found
    conf.set('HAVE_UNISCRIBE', 1)
    conf.set('HAVE_GDI', 1)
  endif
endif

# DirectWrite (Windows)
directwrite_dep = null_dep
if host_machine.system() == 'windows' and not get_option('directwrite').disabled()
  if get_option('directwrite').enabled() and not cpp.has_header('dwrite_1.h')
    error('DirectWrite was enabled explicitly, but required header is missing.')
  endif

  directwrite_dep = cpp.find_library('dwrite', required: get_option('directwrite'))

  if directwrite_dep.found()
    conf.set('HAVE_DIRECTWRITE', 1)
  endif
endif

# CoreText (macOS)
coretext_deps = []
if host_machine.system() == 'darwin' and not get_option('coretext').disabled()
  app_services_dep = dependency('appleframeworks', modules: ['ApplicationServices'], required: false)
  if cpp.has_type('CTFontRef', prefix: '#include <ApplicationServices/ApplicationServices.h>', dependencies: app_services_dep)
    coretext_deps += [app_services_dep]
    conf.set('HAVE_CORETEXT', 1)
  # On iOS CoreText and CoreGraphics are stand-alone frameworks
  # Check for a different symbol to avoid getting cached result
  else
    coretext_dep = dependency('appleframeworks', modules: ['CoreText'], required: false)
    coregraphics_dep = dependency('appleframeworks', modules: ['CoreGraphics'], required: false)
    corefoundation_dep = dependency('appleframeworks', modules: ['CoreFoundation'], required: false)
    if cpp.has_type('CTRunRef', prefix: '#include <CoreText/CoreText.h>', dependencies: [coretext_dep, coregraphics_dep, corefoundation_dep])
      coretext_deps += [coretext_dep, coregraphics_dep, corefoundation_dep]
      conf.set('HAVE_CORETEXT', 1)
    elif get_option('coretext').enabled()
      error('CoreText was enabled explicitly, but required headers or frameworks are missing.')
    endif
  endif
endif

# threads
thread_dep = null_dep
if host_machine.system() != 'windows'
  thread_dep = dependency('threads', required: false)

  if thread_dep.found()
    conf.set('HAVE_PTHREAD', 1)
  else
    check_headers += ['sched.h']
    check_funcs += ['sched_yield', {'link_with': 'rt'}]
  endif
endif

conf.set_quoted('PACKAGE_NAME', 'HarfBuzz')
conf.set_quoted('PACKAGE_VERSION', meson.project_version())

foreach check : check_headers
  name = check[0]

  if cpp.has_header(name)
    conf.set('HAVE_@0@'.format(name.to_upper().underscorify()), 1)
  endif
endforeach

harfbuzz_extra_deps = []
foreach check : check_funcs
  name = check[0]
  opts = check.get(1, {})
  link_withs = opts.get('link_with', [])
  check_deps = opts.get('deps', [])
  extra_deps = []
  found = true

  # First try without linking
  found = cpp.has_function(name, dependencies: check_deps)

  if not found and link_withs.length() > 0
    found = true

    foreach link_with : link_withs
      dep = cpp.find_library(link_with, required: false)
      if dep.found()
        extra_deps += dep
      else
        found = false
      endif
    endforeach

    if found
      found = cpp.has_function(name, dependencies: check_deps + extra_deps)
    endif
  endif

  if found
    harfbuzz_extra_deps += extra_deps
    conf.set('HAVE_@0@'.format(name.to_upper()), 1)
  endif
endforeach

if cpp.links(files('meson-cc-tests/intel-atomic-primitives-test.c'), name: 'Intel atomics')
  conf.set('HAVE_INTEL_ATOMIC_PRIMITIVES', 1)
endif

if cpp.links(files('meson-cc-tests/solaris-atomic-operations.c'), name: 'Solaris atomic ops')
  conf.set('HAVE_SOLARIS_ATOMIC_OPS', 1)
endif

subdir('src')
subdir('util')

if not get_option('tests').disabled()
  subdir('test')
endif

# get_option('wrap_mode') isn't available in <0.49 and this
# is just an internal tool
if meson.version().version_compare('>=0.49')
  if (not get_option('benchmark').disabled() and
      get_option('wrap_mode') != 'nodownload' and
      host_machine.system() != 'windows' and
      not meson.is_subproject() and
      not meson.is_cross_build())
    subdir('perf')
  endif
endif

if not get_option('docs').disabled()
  subdir('docs')
endif

configure_file(output: 'config.h', configuration: conf)

build_summary = {
  'Directories':
    {'prefix': get_option('prefix'),
     'bindir': get_option('bindir'),
     'libdir': get_option('libdir'),
     'includedir': get_option('includedir'),
     'datadir': get_option('datadir'),
    },
  'Unicode callbacks (you want at least one)':
    {'Builtin': true,
     'Glib': conf.get('HAVE_GLIB', 0) == 1,
     'ICU': conf.get('HAVE_ICU', 0) == 1,
    },
  'Font callbacks (the more the merrier)':
    {'FreeType': conf.get('HAVE_FREETYPE', 0) == 1,
    },
  'Dependencies used for command-line utilities':
    {'Cairo': conf.get('HAVE_CAIRO', 0) == 1,
     'Fontconfig': conf.get('HAVE_FONTCONFIG', 0) == 1,
    },
  'Additional shapers':
    {'Graphite2': conf.get('HAVE_GRAPHITE2', 0) == 1,
    },
  'Platform shapers (not normally needed)':
    {'CoreText': conf.get('HAVE_CORETEXT', 0) == 1,
     'DirectWrite': conf.get('HAVE_DIRECTWRITE', 0) == 1,
     'GDI/Uniscribe': (conf.get('HAVE_GDI', 0) == 1) and (conf.get('HAVE_UNISCRIBE', 0) == 1),
    },
  'Other features':
    {'Documentation': conf.get('HAVE_GTK_DOC', 0) == 1,
     'GObject bindings': conf.get('HAVE_GOBJECT', 0) == 1,
     'Introspection': conf.get('HAVE_INTROSPECTION', 0) == 1,
    },
}
if meson.version().version_compare('>=0.53')
  foreach section_title, section : build_summary
    summary(section, bool_yn: true, section: section_title)
  endforeach
else
  summary = ['']
  foreach section_title, section : build_summary
    summary += '  @0@:'.format(section_title)
    foreach feature, value : section
      summary += '    @0@:'.format(feature)
      summary += '          @0@'.format(value)
    endforeach
    summary += ''
  endforeach
  message('\n'.join(summary))
endif
