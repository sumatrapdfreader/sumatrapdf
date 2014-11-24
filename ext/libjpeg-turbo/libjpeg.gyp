# based on https://github.com/zenoalbisser/chromium/blob/master/chromium/third_party/libjpeg_turbo/libjpeg.gyp
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  # This file is not used when use_system_libjepg==1. Settings for building with
  # the system libjpeg is in third_party/libjpeg/libjpeg.gyp.
  'variables': {
    #'shared_generated_dir': '<(SHARED_INTERMEDIATE_DIR)/third_party/libjpeg_turbo',
    'shared_generated_dir': '<(SHARED_INTERMEDIATE_DIR)/libjpeg_turbo',
    'conditions': [
      [ 'OS=="win"', {
        'object_suffix': 'obj',
      }, {
        'object_suffix': 'o',
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'libjpeg',
      'type': 'static_library',
      'include_dirs': [
        '.',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '.',
        ],
      },
      'defines': [
        #'WITH_SIMD', 'MOTION_JPEG_SUPPORTED',
      ],
      # TODO: check the sources the same as in current nmake
      'sources': [
        'jcapimin.c',
        'jcapistd.c',
        'jccoefct.c',
        'jccolor.c',
        'jcdctmgr.c',
        'jchuff.c',
        'jchuff.h',
        'jcinit.c',
        'jcmainct.c',
        'jcmarker.c',
        'jcmaster.c',
        'jcomapi.c',
        'jconfig.h',
        'jcparam.c',
        'jcphuff.c',
        'jcprepct.c',
        'jcsample.c',
        'jdapimin.c',
        'jdapistd.c',
        'jdatadst.c',
        'jdatasrc.c',
        'jdcoefct.c',
        'jdcolor.c',
        'jdct.h',
        'jddctmgr.c',
        'jdhuff.c',
        'jdhuff.h',
        'jdinput.c',
        'jdmainct.c',
        'jdmarker.c',
        'jdmaster.c',
        'jdmerge.c',
        'jdphuff.c',
        'jdpostct.c',
        'jdsample.c',
        'jerror.c',
        'jerror.h',
        'jfdctflt.c',
        'jfdctfst.c',
        'jfdctint.c',
        'jidctflt.c',
        'jidctfst.c',
        'jidctint.c',
        'jidctred.c',
        'jinclude.h',
        'jmemmgr.c',
        'jmemnobs.c',
        'jmemsys.h',
        'jmorecfg.h',
        'jpegint.h',
        'jpeglib.h',
        'jquant1.c',
        'jquant2.c',
        'jutils.c',
        'jversion.h',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '.',
        ],
      },
      'msvs_disabled_warnings': [4018, 4101],
      'conditions': [
        [ 'OS!="win"', {'product_name': 'jpeg_turbo'}],
        # Add target-specific source files.
        [ 'target_arch=="ia32"', {
          'sources': [
            'simd/jsimd_i386.c',
            'simd/jccolmmx.asm',
            'simd/jccolss2.asm',
            'simd/jcgrammx.asm',
            'simd/jcgrass2.asm',
            'simd/jcqnt3dn.asm',
            'simd/jcqntmmx.asm',
            'simd/jcqnts2f.asm',
            'simd/jcqnts2i.asm',
            'simd/jcqntsse.asm',
            'simd/jcsammmx.asm',
            'simd/jcsamss2.asm',
            'simd/jdcolmmx.asm',
            'simd/jdcolss2.asm',
            'simd/jdmermmx.asm',
            'simd/jdmerss2.asm',
            'simd/jdsammmx.asm',
            'simd/jdsamss2.asm',
            'simd/jf3dnflt.asm',
            'simd/jfmmxfst.asm',
            'simd/jfmmxint.asm',
            'simd/jfss2fst.asm',
            'simd/jfss2int.asm',
            'simd/jfsseflt.asm',
            'simd/ji3dnflt.asm',
            'simd/jimmxfst.asm',
            'simd/jimmxint.asm',
            'simd/jimmxred.asm',
            'simd/jiss2flt.asm',
            'simd/jiss2fst.asm',
            'simd/jiss2int.asm',
            'simd/jiss2red.asm',
            'simd/jisseflt.asm',
            'simd/jsimdcpu.asm',
          ],
        }],
        [ 'target_arch=="x64"', {
          'sources': [
            'simd/jsimd_x86_64.c',
            'simd/jccolss2-64.asm',
            'simd/jcgrass2-64.asm',
            'simd/jcqnts2f-64.asm',
            'simd/jcqnts2i-64.asm',
            'simd/jcsamss2-64.asm',
            'simd/jdcolss2-64.asm',
            'simd/jdmerss2-64.asm',
            'simd/jdsamss2-64.asm',
            'simd/jfss2fst-64.asm',
            'simd/jfss2int-64.asm',
            'simd/jfsseflt-64.asm',
            'simd/jiss2flt-64.asm',
            'simd/jiss2fst-64.asm',
            'simd/jiss2int-64.asm',
            'simd/jiss2red-64.asm',
          ],
        }],
        [ 'OS=="win"', {
          'variables': {
            'nasm_path': '<(nasm_dir)/nasm<(EXECUTABLE_SUFFIX)',
            'conditions': [
              [ 'target_arch=="ia32"', {
                'nasm_flags': [
                  '-f', 'win32',
                  # TODO: chrome version had equivalent of -I win
                  # but it seems like rules is executed in top-level directory
                  # so paths must be relative to it as well
                  # Note: /// looks stupid but is necessary so that gyp generates:
                  # "-I" "ext/libjpeg-turbo/win/"
                  # as cmd-line args to nasm because nasm insists on "-I win/" and
                  # doesn't work if it's just "-I win". We could switch to yasm
                  '-I', 'ext/libjpeg-turbo/win///',
                ],
              }, {
                # TODO: verify those
                'nasm_flags': [
                  '-f', 'win64',
                  '-I', 'ext/libjpeg-turbo/win///',
                ],
              }],
            ],
          },
        }],
      ],
      'rules': [
        {
          'rule_name': 'assemble',
          'extension': 'asm',
          'conditions': [
            [ 'target_arch!="arm"', {
              'inputs': [ '<(nasm_path)', ],
              'outputs': [
                '<(shared_generated_dir)/<(RULE_INPUT_ROOT).<(object_suffix)',
              ],
              'action': [
                '<(nasm_path)',
                '<@(nasm_flags)',
                '-I', "ext/libjpeg-turbo/simd///",
                '-o', '<(shared_generated_dir)/<(RULE_INPUT_ROOT).<(object_suffix)',
                '<(RULE_INPUT_PATH)',
              ],
              'process_outputs_as_sources': 1,
              'message': 'Building <(RULE_INPUT_ROOT).<(object_suffix)',
            }],
          ]
        },
      ],
    },
  ],
}

