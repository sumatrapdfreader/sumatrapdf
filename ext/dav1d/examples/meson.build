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
# Build definition for the dav1d examples
#

# Leave subdir if examples are disabled
if not get_option('enable_examples')
    subdir_done()
endif


# dav1d player sources
dav1dplay_sources = files(
    'dav1dplay.c',
    'dp_fifo.c',
    'dp_renderer_placebo.c',
    'dp_renderer_sdl.c',
)

sdl2_dependency = dependency('sdl2', version: '>= 2.0.1', required: true)

if sdl2_dependency.found()
    dav1dplay_deps = [sdl2_dependency, libm_dependency]
    dav1dplay_cflags = []

    placebo_dependency = dependency('libplacebo', version: '>= 3.110.0', required: false)

    if placebo_dependency.found()
        dav1dplay_deps += placebo_dependency
        dav1dplay_cflags += '-DHAVE_PLACEBO'

        # If libplacebo is found, we might be able to use Vulkan
        # with it, in which case we need the Vulkan library too.
        vulkan_dependency = dependency('vulkan', required: false)
        if vulkan_dependency.found()
            dav1dplay_deps += vulkan_dependency
            dav1dplay_cflags += '-DHAVE_VULKAN'
        endif
    endif

    dav1dplay = executable('dav1dplay',
        dav1dplay_sources,
        rev_target,

        link_with : [libdav1d, dav1d_input_objs],
        include_directories : [dav1d_inc_dirs],
        dependencies : [getopt_dependency, dav1dplay_deps],
        install : true,
        c_args : dav1dplay_cflags,
    )
endif
