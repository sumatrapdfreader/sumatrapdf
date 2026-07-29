# Copyright © 2018, VideoLAN and dav1d authors
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

#
# Build definition for the dav1d tests
#

# Leave subdir if tests are disabled
if not get_option('enable_tests')
    subdir_done()
endif

checkasm_dependency = dependency('checkasm',
    fallback: ['checkasm', 'checkasm_dep'],
    required: false
)

if is_asm_enabled and checkasm_dependency.found()

    checkasm_args = [
        '-DCHECKASM_HAVE_GENERIC=@0@'.format(cdata.get('HAVE_C11_GENERIC'))
    ]

    checkasm_sources = files(
        'checkasm/checkasm.c',
        'checkasm/msac.c',
        'checkasm/pal.c',
        'checkasm/refmvs.c',
    )

    checkasm_tmpl_sources = files(
        'checkasm/cdef.c',
        'checkasm/filmgrain.c',
        'checkasm/ipred.c',
        'checkasm/itx.c',
        'checkasm/loopfilter.c',
        'checkasm/looprestoration.c',
        'checkasm/mc.c',
    )

    checkasm_bitdepth_objs = []
    foreach bitdepth : dav1d_bitdepths
        checkasm_bitdepth_lib = static_library(
            'checkasm_bitdepth_@0@'.format(bitdepth),
            checkasm_tmpl_sources,
            include_directories: dav1d_inc_dirs,
            dependencies : [stdatomic_dependencies, checkasm_dependency],
            c_args: ['-DBITDEPTH=@0@'.format(bitdepth)] + checkasm_args,
            install: false,
            build_by_default: false,
        )
        checkasm_bitdepth_objs += checkasm_bitdepth_lib.extract_all_objects(recursive: true)
    endforeach

    checkasm = executable('checkasm',
        checkasm_sources,

        objects: [
            checkasm_bitdepth_objs,
            libdav1d.extract_all_objects(recursive: true),
            ],

        include_directories: dav1d_inc_dirs,
        build_by_default: false,
        dependencies: [
            thread_dependency,
            libm_dependency,
            checkasm_dependency,
            ],
        c_args: checkasm_args,
        )

    test('checkasm', checkasm, suite: 'checkasm', timeout: 180)
    benchmark('checkasm', checkasm, suite: 'checkasm', timeout: 3600, args: '--bench')
endif

c99_extension_flag = cc.first_supported_argument(
    '-Werror=c11-extensions',
    '-Werror=c99-c11-compat',
    '-Wc11-extensions',
    '-Wc99-c11-compat',
)

# dav1d_api_headers
foreach header : dav1d_api_headers
    target = header + '_test'

    header_test_exe = executable(target,
        'header_test.c',
        include_directories: dav1d_inc_dirs,
        c_args: ['-DDAV1D_TEST_HEADER="@0@"'.format(header), c99_extension_flag],
        build_by_default: true
    )

    test(target, header_test_exe, suite: 'headers')
endforeach


# fuzzing binaries
subdir('libfuzzer')

# seek stress test binary, depends on dav1d cli tool
if (get_option('enable_tools') and get_option('enable_seek_stress'))
    seek_stress_sources = files('seek_stress.c')
    seek_stress = executable('seek_stress',
        seek_stress_sources, rev_target,
        objects: [
            dav1d.extract_objects('dav1d_cli_parse.c'),
            dav1d_input_objs.extract_objects('input/input.c', 'input/ivf.c'),
        ],
        include_directories: [dav1d_inc_dirs, include_directories('../tools')],
        link_with: libdav1d,
        dependencies: [
            thread_dependency,
            rt_dependency,
            getopt_dependency,
            libm_dependency,
        ],
    )
endif

# Include dav1d test data repository with additional tests
if get_option('testdata_tests')
    subdir('dav1d-test-data')
endif
