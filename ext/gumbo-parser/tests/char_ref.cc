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
//
// GUnit char_ref tests.  These are quick smoke tests, mostly to identify
// crashing bugs so that they can be fixed without having to debug
// multi-language tests.  As such, they focus on coverage rather than
// completeness.  For testing the full spec, use char_ref_py_tests, which share
// their testdata with the Python html5lib library.

#include "char_ref.h"

#include <stdio.h>
#include <string.h>

#include "gtest/gtest.h"
#include "test_utils.h"
#include "utf8.h"

namespace {

class CharRefTest : public GumboTest {
 protected:
  bool ConsumeCharRef(const char* input) {
    return ConsumeCharRef(input, ' ', false);
  }

  bool ConsumeCharRef(
      const char* input, int additional_allowed_char, bool is_in_attribute) {
    text_ = input;
    utf8iterator_init(&parser_, input, strlen(input), &iter_);
    bool result = consume_char_ref(
        &parser_, &iter_, additional_allowed_char, is_in_attribute, &output_);
    fflush(stdout);
    return result;
  }

  Utf8Iterator iter_;
  OneOrTwoCodepoints output_;
};

TEST_F(CharRefTest, Whitespace) {
  EXPECT_TRUE(ConsumeCharRef(" &nbsp;"));
  EXPECT_EQ(kGumboNoChar, output_.first);
  EXPECT_EQ(kGumboNoChar, output_.second);
}

TEST_F(CharRefTest, NumericHex) {
  EXPECT_TRUE(ConsumeCharRef("&#x12ab;"));
  EXPECT_EQ(0x12ab, output_.first);
  EXPECT_EQ(kGumboNoChar, output_.second);
}

TEST_F(CharRefTest, NumericDecimal) {
  EXPECT_TRUE(ConsumeCharRef("&#1234;"));
  EXPECT_EQ(1234, output_.first);
  EXPECT_EQ(kGumboNoChar, output_.second);
}

TEST_F(CharRefTest, NumericInvalidDigit) {
  errors_are_expected_ = true;
  EXPECT_FALSE(ConsumeCharRef("&#google"));
  EXPECT_EQ(kGumboNoChar, output_.first);
  EXPECT_EQ(kGumboNoChar, output_.second);
  EXPECT_EQ('&', utf8iterator_current(&iter_));
}

TEST_F(CharRefTest, NumericNoSemicolon) {
  errors_are_expected_ = true;
  EXPECT_FALSE(ConsumeCharRef("&#1234google"));
  EXPECT_EQ(1234, output_.first);
  EXPECT_EQ(kGumboNoChar, output_.second);
  EXPECT_EQ('g', utf8iterator_current(&iter_));
}

TEST_F(CharRefTest, NumericReplacement) {
  errors_are_expected_ = true;
  EXPECT_FALSE(ConsumeCharRef("&#X82"));
  // Low quotation mark character.
  EXPECT_EQ(0x201A, output_.first);
  EXPECT_EQ(kGumboNoChar, output_.second);
}

TEST_F(CharRefTest, NumericInvalid) {
  errors_are_expected_ = true;
  EXPECT_FALSE(ConsumeCharRef("&#xDA00"));
  EXPECT_EQ(0xFFFD, output_.first);
  EXPECT_EQ(kGumboNoChar, output_.second);
}

TEST_F(CharRefTest, NumericUtfInvalid) {
  errors_are_expected_ = true;
  EXPECT_FALSE(ConsumeCharRef("&#x007"));
  EXPECT_EQ(0x7, output_.first);
  EXPECT_EQ(kGumboNoChar, output_.second);
}

TEST_F(CharRefTest, NamedReplacement) {
  EXPECT_TRUE(ConsumeCharRef("&lt;"));
  EXPECT_EQ('<', output_.first);
  EXPECT_EQ(kGumboNoChar, output_.second);
}

TEST_F(CharRefTest, NamedReplacementNoSemicolon) {
  errors_are_expected_ = true;
  EXPECT_FALSE(ConsumeCharRef("&gt"));
  EXPECT_EQ('>', output_.first);
  EXPECT_EQ(kGumboNoChar, output_.second);
}

TEST_F(CharRefTest, NamedReplacementWithInvalidUtf8) {
  errors_are_expected_ = true;
  EXPECT_TRUE(ConsumeCharRef("&\xc3\xa5"));
  EXPECT_EQ(kGumboNoChar, output_.first);
  EXPECT_EQ(kGumboNoChar, output_.second);
}

TEST_F(CharRefTest, NamedReplacementInvalid) {
  errors_are_expected_ = true;
  EXPECT_FALSE(ConsumeCharRef("&google;"));
  EXPECT_EQ(kGumboNoChar, output_.first);
  EXPECT_EQ(kGumboNoChar, output_.second);
  EXPECT_EQ('&', utf8iterator_current(&iter_));
}

// TEST_F(CharRefTest, NamedReplacementInvalidNoSemicolon) {
//  EXPECT_FALSE(ConsumeCharRef("&google"));
//  EXPECT_EQ(kGumboNoChar, output_.first);
//  EXPECT_EQ(kGumboNoChar, output_.second);
//  EXPECT_EQ('&', utf8iterator_current(&iter_));
//}

TEST_F(CharRefTest, AdditionalAllowedChar) {
  EXPECT_TRUE(ConsumeCharRef("&\"", '"', false));
  EXPECT_EQ(kGumboNoChar, output_.first);
  EXPECT_EQ(kGumboNoChar, output_.second);
  EXPECT_EQ('&', utf8iterator_current(&iter_));
}

TEST_F(CharRefTest, InAttribute) {
  EXPECT_TRUE(ConsumeCharRef("&noted", ' ', true));
  EXPECT_EQ(kGumboNoChar, output_.first);
  EXPECT_EQ(kGumboNoChar, output_.second);
  EXPECT_EQ('&', utf8iterator_current(&iter_));
}

TEST_F(CharRefTest, MultiChars) {
  EXPECT_TRUE(ConsumeCharRef("&notindot;"));
  EXPECT_EQ(0x22F5, output_.first);
  EXPECT_EQ(0x0338, output_.second);
}

TEST_F(CharRefTest, CharAfter) {
  EXPECT_TRUE(ConsumeCharRef("&lt;x"));
  EXPECT_EQ('<', output_.first);
  EXPECT_EQ(kGumboNoChar, output_.second);
  EXPECT_EQ('x', utf8iterator_current(&iter_));
}

}  // namespace
