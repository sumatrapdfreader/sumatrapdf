# Copyright 2013 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

{
  'targets': [
    {
      'target_name': 'gtest',
      'type': 'static_library',
      'include_dirs': [
        'third_party/gtest',
        'third_party/gtest/include',
      ],
      'link_settings': {
        'ldflags': ['-lpthread'],
      },
      'dependencies': [
        'gtest_prod',
      ],
      'sources': [
        'third_party/gtest/include/gtest/gtest-death-test.h',
        'third_party/gtest/include/gtest/gtest-message.h',
        'third_party/gtest/include/gtest/gtest-param-test.h',
        'third_party/gtest/include/gtest/gtest-printers.h',
        'third_party/gtest/include/gtest/gtest-spi.h',
        'third_party/gtest/include/gtest/gtest-test-part.h',
        'third_party/gtest/include/gtest/gtest-typed-test.h',
        'third_party/gtest/include/gtest/gtest.h',
        'third_party/gtest/include/gtest/gtest_pred_impl.h',
        'third_party/gtest/include/gtest/internal/gtest-death-test-internal.h',
        'third_party/gtest/include/gtest/internal/gtest-filepath.h',
        'third_party/gtest/include/gtest/internal/gtest-internal.h',
        'third_party/gtest/include/gtest/internal/gtest-linked_ptr.h',
        'third_party/gtest/include/gtest/internal/gtest-param-util-generated.h',
        'third_party/gtest/include/gtest/internal/gtest-param-util.h',
        'third_party/gtest/include/gtest/internal/gtest-port.h',
        'third_party/gtest/include/gtest/internal/gtest-string.h',
        'third_party/gtest/include/gtest/internal/gtest-tuple.h',
        'third_party/gtest/include/gtest/internal/gtest-type-util.h',
        'third_party/gtest/src/gtest-all.cc',
        'third_party/gtest/src/gtest-death-test.cc',
        'third_party/gtest/src/gtest-filepath.cc',
        'third_party/gtest/src/gtest-internal-inl.h',
        'third_party/gtest/src/gtest-port.cc',
        'third_party/gtest/src/gtest-printers.cc',
        'third_party/gtest/src/gtest-test-part.cc',
        'third_party/gtest/src/gtest-typed-test.cc',
        'third_party/gtest/src/gtest.cc',
      ],
      'sources!': [
        'third_party/gtest/src/gtest-all.cc',  # Not needed by our build.
      ],
      'conditions': [
        ['OS == "mac" or OS == "ios"', {
          'sources': [
            'gtest_mac.h',
            'gtest_mac.mm',
            'platform_test_mac.mm'
          ],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
            ],
          },
        }],
        ['OS=="win" and (MSVS_VERSION=="2012" or MSVS_VERSION=="2012e")', {
          'defines': [
            '_VARIADIC_MAX=10',
          ],
          'direct_dependent_settings': {
            'defines': [
              '_VARIADIC_MAX=10',
            ],
          },
        }],
      ],
      'direct_dependent_settings': {
        'defines': [
          'UNIT_TEST',
        ],
        'include_dirs': [
          'third_party/gtest/include',  # So that gtest headers can find themselves.
        ],
        'target_conditions': [
          ['_type=="executable"', {
            'test': 1,
            'conditions': [
              ['OS=="mac"', {
                'run_as': {
                  'action????': ['${BUILT_PRODUCTS_DIR}/${PRODUCT_NAME}'],
                },
              }],
              ['OS=="win"', {
                'run_as': {
                  'action????': ['$(TargetPath)', '--gtest_print_time'],
                },
              }],
            ],
          }],
        ],
        'msvs_disabled_warnings': [4800],
      },
    },
    {
      'target_name': 'gtest_main',
      'type': 'static_library',
      'dependencies': [
        'gtest',
      ],
      'sources': [
        'third_party/gtest/src/gtest_main.cc',
      ],
    },
    {
      'target_name': 'gtest_prod',
      'toolsets': ['host', 'target'],
      'type': 'none',
      'sources': [
        'third_party/gtest/include/gtest/gtest_prod.h',
      ],
    },
  ],
}
