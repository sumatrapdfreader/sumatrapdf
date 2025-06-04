project(
  'Little-CMS',
  'c',
  version: '2.16',
  meson_version: '>=0.49.0',
  default_options: ['c_std=gnu11']
)

version_components = meson.project_version().split('.')

library_version = '@0@.0.@1@'.format(
  version_components.get(0, 0),
  version_components.get(1, 0),
)

version_cfg = configuration_data()
version_cfg.set('LCMS2_VERSION_MAJOR', version_components.get(0, 0))
version_cfg.set('LCMS2_VERSION_MINOR', version_components.get(1, 0))
version_cfg.set('LCMS2_VERSION_MICRO', version_components.get(2, 0))
version_cfg.set_quoted('LCMS2_VERSION', meson.project_version())

cc = meson.get_compiler('c')

is_visual_studio = cc.get_argument_syntax() == 'msvc'

cargs = []

if not is_visual_studio and cc.has_function_attribute('visibility:hidden')
  cargs += '-DHAVE_FUNC_ATTRIBUTE_VISIBILITY=1'
endif

if host_machine.endian() == 'big'
  cargs += '-DWORDS_BIGENDIAN=1'
endif


# Check for threadsafe variants of gmtime
# MinGW needs _POSIX_C_SOURCE or _POSIX_THREAD_SAFE_FUNCTIONS defined
# to make gmtime_r and pthread_time.h available
if host_machine.system() == 'windows' and not is_visual_studio
  cargs += ['-D_POSIX_C_SOURCE=199503L']
endif

if cc.has_header_symbol(
  'time.h',
  'gmtime_r',
  args: cargs,
)
  cargs += '-DHAVE_GMTIME_R=1'
elif cc.has_header_symbol('time.h', 'gmtime_s')
  if cc.links(
    '''
    #include <time.h>

    int main() {
      time_t t;
      struct tm m;
      gmtime_s(&m, &t);
      return 0;
    }
    ''',
    name: 'gmtime_s can be used',
  )
    cargs += '-DHAVE_GMTIME_S=1'
  endif
endif

jpeg_dep = dependency('libjpeg', required: get_option('jpeg'))

tiff_dep = dependency('libtiff-4', required: get_option('tiff'))

if (
  not cc.compiles(
    '''
    #include <emmintrin.h>
    int main() { __m128i n = _mm_set1_epi8(42); }
  ''',
    name: 'supports SSE2 intrinsics',
  )
)
  cargs += '-DCMS_DONT_USE_SSE2=1'
endif


if is_visual_studio    
    m_dep = []
    threads_dep = []
    deps = []
else
    m_dep = cc.find_library('m', required: false)
    threads_dep = dependency('threads')

    if cc.has_function('pthread_mutex_lock', dependencies: threads_dep)
        cargs += '-DHasTHREADS=1'
    else
        cargs += '-DHasTHREADS=0'
    endif
    
    deps = [m_dep, threads_dep]
endif


if cc.has_header_symbol('time.h', 'timespec_get')
  cargs += '-DHAVE_TIMESPEC_GET=1'
endif

win = import('windows')

subdir('include')
subdir('src')
subdir('testbed')

if get_option('utils')
subdir('utils')
endif

if get_option('samples')
  subdir('utils/samples')
endif

extra_libraries = []
subdir('plugins')

pkg = import('pkgconfig')
pkg.generate(liblcms2_lib,
  description: 'LCMS Color Management Library',
  libraries: extra_libraries
)
