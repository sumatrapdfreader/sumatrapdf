# Copyright © 2018-2022, VideoLAN and dav1d authors
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

project('dav1d', ['c'],
    version: '1.0.0',
    default_options: ['c_std=c99',
                      'warning_level=2',
                      'buildtype=release',
                      'b_ndebug=if-release'],
    meson_version: '>= 0.49.0')

dav1d_soname_version       = '6.7.0'
dav1d_api_version_array    = dav1d_soname_version.split('.')
dav1d_api_version_major    = dav1d_api_version_array[0]
dav1d_api_version_minor    = dav1d_api_version_array[1]
dav1d_api_version_revision = dav1d_api_version_array[2]

dav1d_src_root = meson.current_source_dir()
cc = meson.get_compiler('c')

# Configuratin data for config.h
cdata = configuration_data()

# Configuration data for config.asm
cdata_asm = configuration_data()

# Include directories
dav1d_inc_dirs = include_directories(['.', 'include/dav1d', 'include'])



#
# Option handling
#

# Bitdepth option
dav1d_bitdepths = get_option('bitdepths')
foreach bitdepth : ['8', '16']
    cdata.set10('CONFIG_@0@BPC'.format(bitdepth), dav1d_bitdepths.contains(bitdepth))
endforeach

# ASM option
is_asm_enabled = (get_option('enable_asm') == true and
    (host_machine.cpu_family() == 'x86' or
     (host_machine.cpu_family() == 'x86_64' and cc.get_define('__ILP32__') == '') or
     host_machine.cpu_family() == 'aarch64'      or
     host_machine.cpu_family().startswith('arm') or
     host_machine.cpu() == 'ppc64le'))
cdata.set10('HAVE_ASM', is_asm_enabled)

if is_asm_enabled and get_option('b_sanitize') == 'memory'
    error('asm causes false positive with memory sanitizer. Use \'-Denable_asm=false\'.')
endif

# Logging option
cdata.set10('CONFIG_LOG', get_option('logging'))

#
# OS/Compiler checks and defines
#

# Arguments in test_args will be used even on feature tests
test_args = []

optional_arguments = []

if host_machine.system() == 'linux'
    test_args += '-D_GNU_SOURCE'
    add_project_arguments('-D_GNU_SOURCE', language: 'c')
elif host_machine.system() == 'darwin'
    test_args += '-D_DARWIN_C_SOURCE'
    add_project_arguments('-D_DARWIN_C_SOURCE', language: 'c')
else
    test_args += '-D_POSIX_C_SOURCE=200112L'
    add_project_arguments('-D_POSIX_C_SOURCE=200112L', language: 'c')
endif

if host_machine.system() == 'windows'
    cdata.set('_WIN32_WINNT',           '0x0601')
    cdata.set('UNICODE',                1) # Define to 1 for Unicode (Wide Chars) APIs
    cdata.set('_UNICODE',               1) # Define to 1 for Unicode (Wide Chars) APIs
    cdata.set('__USE_MINGW_ANSI_STDIO', 1) # Define to force use of MinGW printf
    cdata.set('_CRT_DECLARE_NONSTDC_NAMES', 1) # Define to get off_t from sys/types.h on MSVC
    if cc.has_function('fseeko', prefix : '#include <stdio.h>', args : test_args)
        cdata.set('_FILE_OFFSET_BITS', 64) # Not set by default by Meson on Windows
    else
        cdata.set('fseeko', '_fseeki64')
        cdata.set('ftello', '_ftelli64')
    endif

    # On Windows, we use a compatibility layer to emulate pthread
    thread_dependency = []
    thread_compat_dep = declare_dependency(sources : files('src/win32/thread.c'))

    rt_dependency = []

    rc_version_array = meson.project_version().split('.')
    winmod = import('windows')
    rc_data = configuration_data()
    rc_data.set('PROJECT_VERSION_MAJOR', rc_version_array[0])
    rc_data.set('PROJECT_VERSION_MINOR', rc_version_array[1])
    rc_data.set('PROJECT_VERSION_REVISION', rc_version_array[2])
    rc_data.set('API_VERSION_MAJOR', dav1d_api_version_major)
    rc_data.set('API_VERSION_MINOR', dav1d_api_version_minor)
    rc_data.set('API_VERSION_REVISION', dav1d_api_version_revision)
    rc_data.set('COPYRIGHT_YEARS', '2022')
else
    thread_dependency = dependency('threads')
    thread_compat_dep = []

    rt_dependency = []
    if cc.has_function('clock_gettime', prefix : '#include <time.h>', args : test_args)
        cdata.set('HAVE_CLOCK_GETTIME', 1)
    elif host_machine.system() != 'darwin'
        rt_dependency = cc.find_library('rt', required: false)
        if not cc.has_function('clock_gettime', prefix : '#include <time.h>', args : test_args, dependencies : rt_dependency)
            error('clock_gettime not found')
        endif
        cdata.set('HAVE_CLOCK_GETTIME', 1)
    endif
endif

# check for fseeko on android. It is not always available if _FILE_OFFSET_BITS is defined to 64
have_fseeko = true
if host_machine.system() == 'android'
    if not cc.has_function('fseeko', prefix : '#include <stdio.h>', args : test_args)
        if cc.has_function('fseeko', prefix : '#include <stdio.h>', args : test_args + ['-U_FILE_OFFSET_BITS'])
            warning('Files larger than 2 gigabytes might not be supported in the dav1d CLI tool.')
            add_project_arguments('-U_FILE_OFFSET_BITS', language: 'c')
        elif get_option('enable_tools')
            error('dav1d CLI tool needs fseeko()')
        else
            have_fseeko = false
        endif
    endif
endif

libdl_dependency = []
if host_machine.system() == 'linux'
    libdl_dependency = cc.find_library('dl', required : false)
    if cc.has_function('dlsym', prefix : '#include <dlfcn.h>', args : test_args, dependencies : libdl_dependency)
        cdata.set('HAVE_DLSYM', 1)
    endif
endif

libm_dependency = cc.find_library('m', required: false)


# Header checks

stdatomic_dependencies = []
if not cc.check_header('stdatomic.h')
    if cc.get_id() == 'msvc'
        # we have a custom replacement for MSVC
        stdatomic_dependencies += declare_dependency(
            include_directories : include_directories('include/compat/msvc'),
        )
    elif cc.compiles('''int main() { int v = 0; return __atomic_fetch_add(&v, 1, __ATOMIC_SEQ_CST); }''',
                     name : 'GCC-style atomics', args : test_args)
        stdatomic_dependencies += declare_dependency(
            include_directories : include_directories('include/compat/gcc'),
        )
    else
        error('Atomics not supported')
    endif
endif

if host_machine.cpu_family().startswith('wasm')
    # enable atomics + bulk-memory features
    stdatomic_dependencies += thread_dependency.partial_dependency(compile_args: true)
endif

if cc.check_header('unistd.h')
    cdata.set('HAVE_UNISTD_H', 1)
endif

if cc.check_header('io.h')
    cdata.set('HAVE_IO_H', 1)
endif

if cc.check_header('pthread_np.h')
    cdata.set('HAVE_PTHREAD_NP_H', 1)
    test_args += '-DHAVE_PTHREAD_NP_H'
endif


# Function checks

if not cc.has_function('getopt_long', prefix : '#include <getopt.h>', args : test_args)
    getopt_dependency = declare_dependency(
        sources: files('tools/compat/getopt.c'),
        include_directories : include_directories('include/compat'),
    )
else
    getopt_dependency = []
endif

if cc.has_function('_aligned_malloc', prefix : '#include <malloc.h>', args : test_args)
    cdata.set('HAVE_ALIGNED_MALLOC', 1)
elif cc.has_function('posix_memalign', prefix : '#include <stdlib.h>', args : test_args)
    cdata.set('HAVE_POSIX_MEMALIGN', 1)
elif cc.has_function('memalign', prefix : '#include <malloc.h>', args : test_args)
    cdata.set('HAVE_MEMALIGN', 1)
endif

if (host_machine.cpu_family() == 'aarch64' or
    host_machine.cpu_family().startswith('arm') or
    host_machine.cpu() == 'ppc64le')
    if cc.has_function('getauxval', prefix : '#include <sys/auxv.h>', args : test_args)
        cdata.set('HAVE_GETAUXVAL', 1)
    endif
    if cc.has_function('elf_aux_info', prefix : '#include <sys/auxv.h>', args : test_args)
        cdata.set('HAVE_ELF_AUX_INFO', 1)
    endif
endif

pthread_np_prefix = '''
#include <pthread.h>
#ifdef HAVE_PTHREAD_NP_H
#include <pthread_np.h>
#endif
'''
if cc.has_function('pthread_getaffinity_np', prefix : pthread_np_prefix, args : test_args, dependencies : thread_dependency)
    cdata.set('HAVE_PTHREAD_GETAFFINITY_NP', 1)
endif

# Compiler flag tests

if cc.has_argument('-fvisibility=hidden')
    add_project_arguments('-fvisibility=hidden', language: 'c')
else
    warning('Compiler does not support -fvisibility=hidden, all symbols will be public!')
endif

# Compiler flags that should be set
# But when the compiler does not supports them
# it is not an error and silently tolerated
if cc.get_argument_syntax() != 'msvc'
    optional_arguments += [
      '-Wundef',
      '-Werror=vla',
      '-Wno-maybe-uninitialized',
      '-Wno-missing-field-initializers',
      '-Wno-unused-parameter',
      '-Wstrict-prototypes',
      '-Werror=missing-prototypes',
      '-Wshorten-64-to-32',
    ]
    if host_machine.cpu_family() == 'x86'
        optional_arguments += [
          '-msse2',
          '-mfpmath=sse',
        ]
    endif
else
    optional_arguments += [
      '-wd4028', # parameter different from declaration
      '-wd4090', # broken with arrays of pointers
      '-wd4996'  # use of POSIX functions
    ]
endif

if (get_option('buildtype') != 'debug' and get_option('buildtype') != 'plain')
    optional_arguments += '-fomit-frame-pointer'
    optional_arguments += '-ffast-math'
endif

if (host_machine.system() == 'darwin' and cc.get_id() == 'clang' and
    cc.version().startswith('11'))
    # Workaround for Xcode 11 -fstack-check bug, see #301
    optional_arguments += '-fno-stack-check'
endif

add_project_arguments(cc.get_supported_arguments(optional_arguments), language : 'c')

# libFuzzer related things
fuzzing_engine = get_option('fuzzing_engine')
if fuzzing_engine == 'libfuzzer'
    if not cc.has_argument('-fsanitize=fuzzer')
        error('fuzzing_engine libfuzzer requires "-fsanitize=fuzzer"')
    endif
    fuzzer_args = ['-fsanitize=fuzzer-no-link', '-fsanitize=fuzzer']
    add_project_arguments(cc.first_supported_argument(fuzzer_args), language : 'c')
endif

# Stack alignments flags

stackalign_flag = []
stackrealign_flag = []

cdata.set10('ENDIANNESS_BIG', host_machine.endian() == 'big')

if host_machine.cpu_family().startswith('x86')
    if get_option('stack_alignment') > 0
        stack_alignment = get_option('stack_alignment')
    elif host_machine.cpu_family() == 'x86_64'
        if cc.has_argument('-mpreferred-stack-boundary=6')
            stackalign_flag = ['-mpreferred-stack-boundary=6']
            stackrealign_flag = ['-mincoming-stack-boundary=4']
            stack_alignment = 32
        elif cc.has_argument('-mstack-alignment=64')
            stackalign_flag = ['-mstack-alignment=64']
            stackrealign_flag = ['-mstackrealign']
            stack_alignment = 32
        else
            stack_alignment = 16
        endif
    else
        if host_machine.system() == 'linux' or host_machine.system() == 'darwin'
            stack_alignment = 16
        elif cc.has_argument('-mpreferred-stack-boundary=4')
            stackalign_flag = ['-mpreferred-stack-boundary=4']
            stackrealign_flag = ['-mincoming-stack-boundary=2']
            stack_alignment = 16
        elif cc.has_argument('-mstack-alignment=16')
            stackalign_flag = ['-mstack-alignment=16']
            stackrealign_flag = ['-mstackrealign']
            stack_alignment = 16
        else
            stack_alignment = 4
        endif
    endif
    cdata_asm.set('STACK_ALIGNMENT', stack_alignment)
    cdata.set('STACK_ALIGNMENT', stack_alignment)
endif

cdata.set10('ARCH_AARCH64', host_machine.cpu_family() == 'aarch64' or host_machine.cpu() == 'arm64')
cdata.set10('ARCH_ARM',     host_machine.cpu_family().startswith('arm') and host_machine.cpu() != 'arm64')
if (is_asm_enabled and
    (host_machine.cpu_family() == 'aarch64' or
     host_machine.cpu_family().startswith('arm')))

   as_func_code = '''__asm__ (
".func meson_test"
".endfunc"
);
'''
    have_as_func = cc.compiles(as_func_code)
    cdata.set10('HAVE_AS_FUNC', have_as_func)

    # fedora package build infrastructure uses a gcc specs file to enable
    # '-fPIE' by default. The chosen way only adds '-fPIE' to the C compiler
    # with integrated preprocessor. It is not added to the standalone
    # preprocessor or the preprocessing stage of '.S' files. So we have to
    # compile code to check if we have to define PIC for the arm asm to
    # avoid absolute relocations when building for example checkasm.
    check_pic_code = '''
#if defined(PIC)
#error "PIC already defined"
#elif !(defined(__PIC__) || defined(__pic__))
#error "no pic"
#endif
'''
    if cc.compiles(check_pic_code)
        cdata.set('PIC', '3')
    endif
endif

cdata.set10('ARCH_X86', host_machine.cpu_family().startswith('x86'))
cdata.set10('ARCH_X86_64', host_machine.cpu_family() == 'x86_64')
cdata.set10('ARCH_X86_32', host_machine.cpu_family() == 'x86')

if host_machine.cpu_family().startswith('x86')
    cdata_asm.set('private_prefix', 'dav1d')
    cdata_asm.set10('ARCH_X86_64', host_machine.cpu_family() == 'x86_64')
    cdata_asm.set10('ARCH_X86_32', host_machine.cpu_family() == 'x86')
    cdata_asm.set10('PIC', true)

    # Convert SSE asm into (128-bit) AVX when compiler flags are set to use AVX instructions
    cdata_asm.set10('FORCE_VEX_ENCODING', cc.get_define('__AVX__') != '')
endif

cdata.set10('ARCH_PPC64LE', host_machine.cpu() == 'ppc64le')

# meson's cc.symbols_have_underscore_prefix() is unfortunately unrelieably
# when additional flags like '-fprofile-instr-generate' are passed via CFLAGS
# see following meson issue https://github.com/mesonbuild/meson/issues/5482
if (host_machine.system() == 'darwin' or
   (host_machine.system() == 'windows' and host_machine.cpu_family() == 'x86'))
    cdata.set10('PREFIX', true)
    cdata_asm.set10('PREFIX', true)
endif

#
# ASM specific stuff
#
if is_asm_enabled and host_machine.cpu_family().startswith('x86')

    # NASM compiler support

    nasm = find_program('nasm')

    # check NASM version
    if nasm.found()
        nasm_r = run_command(nasm, '-v', check: true)

        out = nasm_r.stdout().strip().split()
        if out[1].to_lower() == 'version'
            if out[2].version_compare('<2.14')
                error('nasm 2.14 or later is required, found nasm @0@'.format(out[2]))
            endif
        else
            error('unexpected nasm version string: @0@'.format(nasm_r.stdout()))
        endif
    endif

    # Generate config.asm
    config_asm_target = configure_file(output: 'config.asm', output_format: 'nasm', configuration: cdata_asm)

    if host_machine.system() == 'windows'
        nasm_format = 'win'
    elif host_machine.system() == 'darwin'
        nasm_format = 'macho'
    else
        nasm_format = 'elf'
    endif
    if host_machine.cpu_family() == 'x86_64'
        nasm_format += '64'
    else
        nasm_format += '32'
    endif

    nasm_gen = generator(nasm,
        output: '@BASENAME@.obj',
        depfile: '@BASENAME@.obj.ndep',
        arguments: [
            '-f', nasm_format,
            '-I', '@0@/src/'.format(dav1d_src_root),
            '-I', '@0@/'.format(meson.current_build_dir()),
            '-MQ', '@OUTPUT@', '-MF', '@DEPFILE@',
            '@EXTRA_ARGS@',
            '@INPUT@',
            '-o', '@OUTPUT@'
        ])
endif

use_gaspp = false
if (is_asm_enabled and
    (host_machine.cpu_family() == 'aarch64' or
     host_machine.cpu_family().startswith('arm')) and
    cc.get_argument_syntax() == 'msvc')
    gaspp = find_program('gas-preprocessor.pl')
    use_gaspp = true
    gaspp_gen = generator(gaspp,
        output: '@BASENAME@.obj',
        arguments: [
            '-as-type', 'armasm',
            '-arch', host_machine.cpu_family(),
            '--',
            host_machine.cpu_family() == 'aarch64' ? 'armasm64' : 'armasm',
            '-nologo',
            '-I@0@'.format(dav1d_src_root),
            '-I@0@/'.format(meson.current_build_dir()),
            '@INPUT@',
            '-c',
            '-o', '@OUTPUT@'
        ])
endif

# Generate config.h
config_h_target = configure_file(output: 'config.h', configuration: cdata)



#
# Include subdir meson.build files
# The order is important!

subdir('include')

subdir('doc')

subdir('src')

subdir('tools')

subdir('examples')

subdir('tests')
