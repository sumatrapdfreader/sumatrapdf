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

#include "string_piece.h"

#include <stdlib.h>
#include <string.h>

#include "gtest/gtest.h"
#include "parser.h"
#include "test_utils.h"
#include "util.h"

namespace {

typedef GumboTest GumboStringPieceTest;

#define INIT_GUMBO_STRING(varname, literal) \
  GumboStringPiece varname = {literal, sizeof(literal) - 1}

TEST_F(GumboStringPieceTest, Equal) {
  INIT_GUMBO_STRING(str1, "foo");
  INIT_GUMBO_STRING(str2, "foo");
  EXPECT_TRUE(gumbo_string_equals(&str1, &str2));
}

TEST_F(GumboStringPieceTest, NotEqual_DifferingCase) {
  INIT_GUMBO_STRING(str1, "foo");
  INIT_GUMBO_STRING(str2, "Foo");
  EXPECT_FALSE(gumbo_string_equals(&str1, &str2));
}

TEST_F(GumboStringPieceTest, NotEqual_Str1Shorter) {
  INIT_GUMBO_STRING(str1, "foo");
  INIT_GUMBO_STRING(str2, "foobar");
  EXPECT_FALSE(gumbo_string_equals(&str1, &str2));
}

TEST_F(GumboStringPieceTest, NotEqual_Str2Shorter) {
  INIT_GUMBO_STRING(str1, "foobar");
  INIT_GUMBO_STRING(str2, "foo");
  EXPECT_FALSE(gumbo_string_equals(&str1, &str2));
}

TEST_F(GumboStringPieceTest, NotEqual_DifferentText) {
  INIT_GUMBO_STRING(str1, "bar");
  INIT_GUMBO_STRING(str2, "foo");
  EXPECT_FALSE(gumbo_string_equals(&str1, &str2));
}

TEST_F(GumboStringPieceTest, CaseEqual) {
  INIT_GUMBO_STRING(str1, "foo");
  INIT_GUMBO_STRING(str2, "fOO");
  EXPECT_TRUE(gumbo_string_equals_ignore_case(&str1, &str2));
}

TEST_F(GumboStringPieceTest, CaseNotEqual_Str2Shorter) {
  INIT_GUMBO_STRING(str1, "foobar");
  INIT_GUMBO_STRING(str2, "foo");
  EXPECT_FALSE(gumbo_string_equals_ignore_case(&str1, &str2));
}

TEST_F(GumboStringPieceTest, Copy) {
  GumboParser parser;
  parser._options = &kGumboDefaultOptions;
  INIT_GUMBO_STRING(str1, "bar");
  GumboStringPiece str2;
  gumbo_string_copy(&parser, &str2, &str1);
  EXPECT_TRUE(gumbo_string_equals(&str1, &str2));
  gumbo_parser_deallocate(&parser, (void*) str2.data);
}

}  // namespace
