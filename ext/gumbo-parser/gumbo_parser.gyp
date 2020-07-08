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
      'target_name': 'gumbo_parser',
      'type': 'static_library',
      'cflags': ['-std=c99', '-Wall'],
      'sources': [
        'src/attribute.c',
        'src/attribute.h',
        'src/char_ref.c',
        'src/char_ref.h',
        'src/error.c',
        'src/error.h',
        'src/gumbo.h',
        'src/insertion_mode.h',
        'src/parser.c',
        'src/parser.h',
        'src/string_buffer.c',
        'src/string_buffer.h',
        'src/string_piece.c',
        'src/string_piece.h',
        'src/tag.c',
        'src/token_type.h',
        'src/tokenizer.c',
        'src/tokenizer.h',
        'src/tokenizer_states.h',
        'src/utf8.c',
        'src/utf8.h',
        'src/util.c',
        'src/util.h',
        'src/vector.c',
        'src/vector.h',
      ],
    },
    {
      'target_name': 'gumbo_parser_unittests',
      'type': 'executable',
      'dependencies': [
        'gtest.gyp:gtest',
        'gtest.gyp:gtest_main',
        'gumbo_parser',
      ],
      'include_dirs': [
        '.',
        '..',
        'src',
      ],
      'sources': [
        'tests/attribute.cc',
        'tests/char_ref.cc',
        'tests/parser.cc',
        'tests/string_buffer.cc',
        'tests/string_piece.cc',
        'tests/test_utils.cc',
        'tests/test_utils.h',
        'tests/tokenizer.cc',
        'tests/utf8.cc',
        'tests/vector.cc',
      ],
    },
  ],
}
