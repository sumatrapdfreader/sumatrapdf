# based on https://github.com/zenoalbisser/chromium/blob/master/chromium/third_party/libjpeg_turbo/libjpeg.gyp
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  # This file is not used when use_system_libjepg==1. Settings for building with
  # the system libjpeg is in third_party/libjpeg/libjpeg.gyp.
  'variables': {
    #'shared_generated_dir': '<(SHARED_INTERMEDIATE_DIR)/third_party/libjpeg_turbo',
    'shared_generated_dir': '<(SHARED_INTERMEDIATE_DIR)',
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
        '../ext/libjpeg-turbo/',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../ext/libjpeg-turbo/',
        ],
      },
      'defines': [
        #'WITH_SIMD', 'MOTION_JPEG_SUPPORTED',
      ],
      # TODO: check the sources the same as in current nmake
      'sources': [
        '../ext/libjpeg-turbo/jaricom.c',
        '../ext/libjpeg-turbo/jcapimin.c',
        '../ext/libjpeg-turbo/jcapistd.c',
        '../ext/libjpeg-turbo/jccoefct.c',
        '../ext/libjpeg-turbo/jccolor.c',
        '../ext/libjpeg-turbo/jcdctmgr.c',
        '../ext/libjpeg-turbo/jchuff.c',
        '../ext/libjpeg-turbo/jchuff.h',
        '../ext/libjpeg-turbo/jcinit.c',
        '../ext/libjpeg-turbo/jcmainct.c',
        '../ext/libjpeg-turbo/jcmarker.c',
        '../ext/libjpeg-turbo/jcmaster.c',
        '../ext/libjpeg-turbo/jcomapi.c',
        '../ext/libjpeg-turbo/jconfig.h',
        '../ext/libjpeg-turbo/jcparam.c',
        '../ext/libjpeg-turbo/jcphuff.c',
        '../ext/libjpeg-turbo/jcprepct.c',
        '../ext/libjpeg-turbo/jcsample.c',
        '../ext/libjpeg-turbo/jdapimin.c',
        '../ext/libjpeg-turbo/jdapistd.c',
        '../ext/libjpeg-turbo/jdarith.c',
        '../ext/libjpeg-turbo/jdatadst.c',
        '../ext/libjpeg-turbo/jdatasrc.c',
        '../ext/libjpeg-turbo/jdcoefct.c',
        '../ext/libjpeg-turbo/jdcolor.c',
        '../ext/libjpeg-turbo/jdct.h',
        '../ext/libjpeg-turbo/jddctmgr.c',
        '../ext/libjpeg-turbo/jdhuff.c',
        '../ext/libjpeg-turbo/jdhuff.h',
        '../ext/libjpeg-turbo/jdinput.c',
        '../ext/libjpeg-turbo/jdmainct.c',
        '../ext/libjpeg-turbo/jdmarker.c',
        '../ext/libjpeg-turbo/jdmaster.c',
        '../ext/libjpeg-turbo/jdmerge.c',
        '../ext/libjpeg-turbo/jdphuff.c',
        '../ext/libjpeg-turbo/jdpostct.c',
        '../ext/libjpeg-turbo/jdsample.c',
        '../ext/libjpeg-turbo/jerror.c',
        '../ext/libjpeg-turbo/jerror.h',
        '../ext/libjpeg-turbo/jfdctflt.c',
        '../ext/libjpeg-turbo/jfdctfst.c',
        '../ext/libjpeg-turbo/jfdctint.c',
        '../ext/libjpeg-turbo/jidctflt.c',
        '../ext/libjpeg-turbo/jidctfst.c',
        '../ext/libjpeg-turbo/jidctint.c',
        '../ext/libjpeg-turbo/jidctred.c',
        '../ext/libjpeg-turbo/jinclude.h',
        '../ext/libjpeg-turbo/jmemmgr.c',
        '../ext/libjpeg-turbo/jmemnobs.c',
        '../ext/libjpeg-turbo/jmemsys.h',
        '../ext/libjpeg-turbo/jmorecfg.h',
        '../ext/libjpeg-turbo/jpegint.h',
        '../ext/libjpeg-turbo/jpeglib.h',
        '../ext/libjpeg-turbo/jquant1.c',
        '../ext/libjpeg-turbo/jquant2.c',
        '../ext/libjpeg-turbo/jutils.c',
        '../ext/libjpeg-turbo/jversion.h',
      ],
      'msvs_disabled_warnings': [4018, 4101],
      'conditions': [
        [ 'OS!="win"', {'product_name': 'jpeg_turbo'}],
        # Add target-specific source files.
        [ 'target_arch=="ia32"', {
          'sources': [
            '../ext/libjpeg-turbo/simd/jsimd_i386.c',
            '../ext/libjpeg-turbo/simd/jccolmmx.asm',
            '../ext/libjpeg-turbo/simd/jccolss2.asm',
            '../ext/libjpeg-turbo/simd/jcgrammx.asm',
            '../ext/libjpeg-turbo/simd/jcgrass2.asm',
            '../ext/libjpeg-turbo/simd/jcqnt3dn.asm',
            '../ext/libjpeg-turbo/simd/jcqntmmx.asm',
            '../ext/libjpeg-turbo/simd/jcqnts2f.asm',
            '../ext/libjpeg-turbo/simd/jcqnts2i.asm',
            '../ext/libjpeg-turbo/simd/jcqntsse.asm',
            '../ext/libjpeg-turbo/simd/jcsammmx.asm',
            '../ext/libjpeg-turbo/simd/jcsamss2.asm',
            '../ext/libjpeg-turbo/simd/jdcolmmx.asm',
            '../ext/libjpeg-turbo/simd/jdcolss2.asm',
            '../ext/libjpeg-turbo/simd/jdmermmx.asm',
            '../ext/libjpeg-turbo/simd/jdmerss2.asm',
            '../ext/libjpeg-turbo/simd/jdsammmx.asm',
            '../ext/libjpeg-turbo/simd/jdsamss2.asm',
            '../ext/libjpeg-turbo/simd/jf3dnflt.asm',
            '../ext/libjpeg-turbo/simd/jfmmxfst.asm',
            '../ext/libjpeg-turbo/simd/jfmmxint.asm',
            '../ext/libjpeg-turbo/simd/jfss2fst.asm',
            '../ext/libjpeg-turbo/simd/jfss2int.asm',
            '../ext/libjpeg-turbo/simd/jfsseflt.asm',
            '../ext/libjpeg-turbo/simd/ji3dnflt.asm',
            '../ext/libjpeg-turbo/simd/jimmxfst.asm',
            '../ext/libjpeg-turbo/simd/jimmxint.asm',
            '../ext/libjpeg-turbo/simd/jimmxred.asm',
            '../ext/libjpeg-turbo/simd/jiss2flt.asm',
            '../ext/libjpeg-turbo/simd/jiss2fst.asm',
            '../ext/libjpeg-turbo/simd/jiss2int.asm',
            '../ext/libjpeg-turbo/simd/jiss2red.asm',
            '../ext/libjpeg-turbo/simd/jisseflt.asm',
            '../ext/libjpeg-turbo/simd/jsimdcpu.asm',
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
            'nasm_path': '../bin/nasm<(EXECUTABLE_SUFFIX)',
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
                  '-I', '../ext/libjpeg-turbo/win///',
                ],
              }, {
                # TODO: verify those
                'nasm_flags': [
                  '-f', 'win64',
                  '-I', '../ext/libjpeg-turbo/win///',
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
                '-I', "../ext/libjpeg-turbo/simd///",
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

