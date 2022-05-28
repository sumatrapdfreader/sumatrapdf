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

if not get_option('enable_docs')
    subdir_done()
endif

doxygen = find_program('doxygen')
dot = find_program('dot')

conf_data = configuration_data()
conf_data.set('DOXYGEN_INPUT', dav1d_src_root / 'include/dav1d')
conf_data.set('DOXYGEN_STRIP', dav1d_src_root / 'include')
conf_data.set('DOXYGEN_OUTPUT', meson.current_build_dir())
doxyfile = configure_file(input: 'Doxyfile.in.in',
                          output: 'Doxyfile.in',
                          configuration: conf_data)

doxyfile_rev_target = vcs_tag(command: [
        'git', '--git-dir', dav1d_git_dir, 'describe', '--long', '--always'
    ],
    input: doxyfile,
    output: 'Doxyfile'
)

custom_target('doc',
              build_by_default: false,
              command: [doxygen, doxyfile_rev_target],
              output: ['html']
)
