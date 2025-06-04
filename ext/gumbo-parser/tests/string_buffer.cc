// Copyright 2011 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: jdtang@google.com (Jonathan Tang)

#include "string_buffer.h"

#include <stdlib.h>
#include <string.h>

#include "gtest/gtest.h"
#include "test_utils.h"
#include "util.h"

namespace {

#define INIT_GUMBO_STRING(varname, literal) \
  GumboStringPiece varname = {literal, sizeof(literal) - 1}

class GumboStringBufferTest : public GumboTest {
 protected:
  GumboStringBufferTest() { gumbo_string_buffer_init(&parser_, &buffer_); }

  ~GumboStringBufferTest() { gumbo_string_buffer_destroy(&parser_, &buffer_); }

  void NullTerminateBuffer() { buffer_.data[buffer_.length++] = 0; }

  GumboStringBuffer buffer_;
};

TEST_F(GumboStringBufferTest, Reserve) {
  gumbo_string_buffer_reserve(&parser_, 21, &buffer_);
  EXPECT_EQ(40, buffer_.capacity);
  strcpy(buffer_.data, "01234567890123456789");
  buffer_.length = 20;
  NullTerminateBuffer();
  EXPECT_EQ(21, buffer_.length);
  EXPECT_STREQ("01234567890123456789", buffer_.data);
}

TEST_F(GumboStringBufferTest, AppendString) {
  INIT_GUMBO_STRING(str, "01234567");
  gumbo_string_buffer_append_string(&parser_, &str, &buffer_);
  NullTerminateBuffer();
  EXPECT_STREQ("01234567", buffer_.data);
}

TEST_F(GumboStringBufferTest, AppendStringWithResize) {
  INIT_GUMBO_STRING(str, "01234567");
  gumbo_string_buffer_append_string(&parser_, &str, &buffer_);
  gumbo_string_buffer_append_string(&parser_, &str, &buffer_);
  NullTerminateBuffer();
  EXPECT_STREQ("0123456701234567", buffer_.data);
}

TEST_F(GumboStringBufferTest, AppendCodepoint_1Byte) {
  gumbo_string_buffer_append_codepoint(&parser_, 'a', &buffer_);
  NullTerminateBuffer();
  EXPECT_STREQ("a", buffer_.data);
}

TEST_F(GumboStringBufferTest, AppendCodepoint_2Bytes) {
  gumbo_string_buffer_append_codepoint(&parser_, 0xE5, &buffer_);
  NullTerminateBuffer();
  EXPECT_STREQ("\xC3\xA5", buffer_.data);
}

TEST_F(GumboStringBufferTest, AppendCodepoint_3Bytes) {
  gumbo_string_buffer_append_codepoint(&parser_, 0x39E7, &buffer_);
  NullTerminateBuffer();
  EXPECT_STREQ("\xE3\xA7\xA7", buffer_.data);
}

TEST_F(GumboStringBufferTest, AppendCodepoint_4Bytes) {
  gumbo_string_buffer_append_codepoint(&parser_, 0x679E7, &buffer_);
  NullTerminateBuffer();
  EXPECT_STREQ("\xF1\xA7\xA7\xA7", buffer_.data);
}

TEST_F(GumboStringBufferTest, ToString) {
  gumbo_string_buffer_reserve(&parser_, 8, &buffer_);
  strcpy(buffer_.data, "012345");
  buffer_.length = 7;

  char* dest = gumbo_string_buffer_to_string(&parser_, &buffer_);
  EXPECT_STREQ("012345", dest);
  gumbo_parser_deallocate(&parser_, dest);
}

}  // namespace
