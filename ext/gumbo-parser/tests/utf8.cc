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

#include "utf8.h"

#include <string.h>

#include "gtest/gtest.h"
#include "error.h"
#include "gumbo.h"
#include "parser.h"
#include "test_utils.h"

namespace {

// Tests for utf8.c
class Utf8Test : public GumboTest {
 protected:
  void Advance(int num_chars) {
    for (int i = 0; i < num_chars; ++i) {
      utf8iterator_next(&input_);
    }
  }

  void ResetText(const char* text) {
    text_ = text;
    utf8iterator_init(&parser_, text, strlen(text), &input_);
  }

  GumboError* GetFirstError() {
    return static_cast<GumboError*>(parser_._output->errors.data[0]);
  }

  int GetNumErrors() { return parser_._output->errors.length; }

  Utf8Iterator input_;
};

TEST_F(Utf8Test, EmptyString) {
  ResetText("");
  EXPECT_EQ(-1, utf8iterator_current(&input_));
}

TEST_F(Utf8Test, GetPosition_EmptyString) {
  ResetText("");
  GumboSourcePosition pos;

  utf8iterator_get_position(&input_, &pos);
  EXPECT_EQ(1, pos.line);
  EXPECT_EQ(1, pos.column);
  EXPECT_EQ(0, pos.offset);
}

TEST_F(Utf8Test, Null) {
  // Can't use ResetText, as the implicit strlen will choke on the null.
  text_ = "\0f";
  utf8iterator_init(&parser_, text_, 2, &input_);

  EXPECT_EQ(0, utf8iterator_current(&input_));
  EXPECT_EQ('\0', *utf8iterator_get_char_pointer(&input_));
  utf8iterator_next(&input_);
  EXPECT_EQ('f', utf8iterator_current(&input_));
  EXPECT_EQ('f', *utf8iterator_get_char_pointer(&input_));
}

TEST_F(Utf8Test, OneByteChar) {
  ResetText("a");

  EXPECT_EQ(0, GetNumErrors());
  EXPECT_EQ('a', utf8iterator_current(&input_));
  EXPECT_EQ('a', *utf8iterator_get_char_pointer(&input_));

  GumboSourcePosition pos;
  utf8iterator_get_position(&input_, &pos);
  EXPECT_EQ(1, pos.line);
  EXPECT_EQ(1, pos.column);
  EXPECT_EQ(0, pos.offset);

  utf8iterator_next(&input_);
  EXPECT_EQ(-1, utf8iterator_current(&input_));
}

TEST_F(Utf8Test, ContinuationByte) {
  ResetText("\x85");

  EXPECT_EQ(1, GetNumErrors());
  EXPECT_EQ(0xFFFD, utf8iterator_current(&input_));
  EXPECT_EQ('\x85', *utf8iterator_get_char_pointer(&input_));

  errors_are_expected_ = true;
  GumboError* error = GetFirstError();
  EXPECT_EQ(GUMBO_ERR_UTF8_INVALID, error->type);
  EXPECT_EQ('\x85', *error->original_text);
  EXPECT_EQ(0x85, error->v.codepoint);

  utf8iterator_next(&input_);
  EXPECT_EQ(-1, utf8iterator_current(&input_));
}

TEST_F(Utf8Test, MultipleContinuationBytes) {
  ResetText("a\x85\xA0\xC2x\x9A");
  EXPECT_EQ('a', utf8iterator_current(&input_));

  utf8iterator_next(&input_);
  EXPECT_EQ(0xFFFD, utf8iterator_current(&input_));

  utf8iterator_next(&input_);
  EXPECT_EQ(0xFFFD, utf8iterator_current(&input_));

  utf8iterator_next(&input_);
  EXPECT_EQ(0xFFFD, utf8iterator_current(&input_));

  utf8iterator_next(&input_);
  EXPECT_EQ('x', utf8iterator_current(&input_));

  utf8iterator_next(&input_);
  EXPECT_EQ(0xFFFD, utf8iterator_current(&input_));

  utf8iterator_next(&input_);
  EXPECT_EQ(-1, utf8iterator_current(&input_));

  utf8iterator_next(&input_);
  EXPECT_EQ(4, GetNumErrors());
}

TEST_F(Utf8Test, OverlongEncoding) {
  // \xC0\x75 = 11000000 01110101.
  ResetText("\xC0\x75");

  ASSERT_EQ(1, GetNumErrors());
  EXPECT_EQ(0xFFFD, utf8iterator_current(&input_));
  EXPECT_EQ('\xC0', *utf8iterator_get_char_pointer(&input_));

  errors_are_expected_ = true;
  GumboError* error = GetFirstError();
  EXPECT_EQ(GUMBO_ERR_UTF8_INVALID, error->type);
  EXPECT_EQ(1, error->position.line);
  EXPECT_EQ(1, error->position.column);
  EXPECT_EQ(0, error->position.offset);
  EXPECT_EQ('\xC0', *error->original_text);
  EXPECT_EQ(0xC0, error->v.codepoint);

  utf8iterator_next(&input_);
  EXPECT_EQ(0x75, utf8iterator_current(&input_));
  EXPECT_EQ('\x75', *utf8iterator_get_char_pointer(&input_));

  utf8iterator_next(&input_);
  EXPECT_EQ(-1, utf8iterator_current(&input_));
}

TEST_F(Utf8Test, OverlongEncodingWithContinuationByte) {
  // \xC0\x85 = 11000000 10000101.
  ResetText("\xC0\x85");

  ASSERT_EQ(1, GetNumErrors());
  EXPECT_EQ(0xFFFD, utf8iterator_current(&input_));
  EXPECT_EQ('\xC0', *utf8iterator_get_char_pointer(&input_));

  utf8iterator_next(&input_);
  EXPECT_EQ(0xFFFD, utf8iterator_current(&input_));

  errors_are_expected_ = true;
  GumboError* error = GetFirstError();
  EXPECT_EQ(GUMBO_ERR_UTF8_INVALID, error->type);
  EXPECT_EQ(1, error->position.line);
  EXPECT_EQ(1, error->position.column);
  EXPECT_EQ(0, error->position.offset);
  EXPECT_EQ('\xC0', *error->original_text);
  EXPECT_EQ(0xC0, error->v.codepoint);

  utf8iterator_next(&input_);
  EXPECT_EQ(-1, utf8iterator_current(&input_));
}

TEST_F(Utf8Test, TwoByteChar) {
  // \xC3\xA5 = 11000011 10100101.
  ResetText("\xC3\xA5o");

  EXPECT_EQ(0, GetNumErrors());
  // Codepoint = 000 11100101 = 0xE5.
  EXPECT_EQ(0xE5, utf8iterator_current(&input_));
  EXPECT_EQ('\xC3', *utf8iterator_get_char_pointer(&input_));

  GumboSourcePosition pos;
  utf8iterator_get_position(&input_, &pos);
  EXPECT_EQ(1, pos.line);
  EXPECT_EQ(1, pos.column);
  EXPECT_EQ(0, pos.offset);

  utf8iterator_next(&input_);
  EXPECT_EQ('o', utf8iterator_current(&input_));

  utf8iterator_get_position(&input_, &pos);
  EXPECT_EQ(1, pos.line);
  EXPECT_EQ(2, pos.column);
  EXPECT_EQ(2, pos.offset);
}

TEST_F(Utf8Test, TwoByteChar2) {
  // \xC2\xA5 = 11000010 10100101.
  ResetText("\xC2\xA5");

  EXPECT_EQ(0, GetNumErrors());
  // Codepoint = 000 10100101 = 0xA5.
  EXPECT_EQ(0xA5, utf8iterator_current(&input_));
  EXPECT_EQ('\xC2', *utf8iterator_get_char_pointer(&input_));

  utf8iterator_next(&input_);
  EXPECT_EQ(-1, utf8iterator_current(&input_));
}

TEST_F(Utf8Test, ThreeByteChar) {
  // \xE3\xA7\xA7 = 11100011 10100111 10100111
  ResetText("\xE3\xA7\xA7\xB0");

  EXPECT_EQ(0, GetNumErrors());
  // Codepoint = 00111001 11100111 = 0x39E7
  EXPECT_EQ(0x39E7, utf8iterator_current(&input_));
  EXPECT_EQ('\xE3', *utf8iterator_get_char_pointer(&input_));

  utf8iterator_next(&input_);
  EXPECT_EQ(1, GetNumErrors());
  EXPECT_EQ(0xFFFD, utf8iterator_current(&input_));
  EXPECT_EQ('\xB0', *utf8iterator_get_char_pointer(&input_));

  GumboSourcePosition pos;
  utf8iterator_get_position(&input_, &pos);
  EXPECT_EQ(1, pos.line);
  EXPECT_EQ(2, pos.column);
  EXPECT_EQ(3, pos.offset);

  utf8iterator_next(&input_);
  EXPECT_EQ(-1, utf8iterator_current(&input_));
}

TEST_F(Utf8Test, FourByteChar) {
  // \xC3\x9A = 11000011 10011010
  // \xF1\xA7\xA7\xA7 = 11110001 10100111 10100111 10100111
  ResetText("\xC3\x9A\xF1\xA7\xA7\xA7");

  // Codepoint = 000 11011010 = 0xDA.
  EXPECT_EQ(0xDA, utf8iterator_current(&input_));
  EXPECT_EQ('\xC3', *utf8iterator_get_char_pointer(&input_));

  utf8iterator_next(&input_);
  // Codepoint = 00110 01111001 11100111 = 0x679E7.
  EXPECT_EQ(0x679E7, utf8iterator_current(&input_));
  EXPECT_EQ('\xF1', *utf8iterator_get_char_pointer(&input_));

  utf8iterator_next(&input_);
  EXPECT_EQ(-1, utf8iterator_current(&input_));
}

TEST_F(Utf8Test, FourByteCharWithoutContinuationChars) {
  // \xF1\xA7\xA7\xA7 = 11110001 10100111 10100111 10100111
  ResetText("\xF1\xA7\xA7-");

  EXPECT_EQ(1, GetNumErrors());
  EXPECT_EQ(0xFFFD, utf8iterator_current(&input_));
  EXPECT_EQ('\xF1', *utf8iterator_get_char_pointer(&input_));

  utf8iterator_next(&input_);
  EXPECT_EQ('-', utf8iterator_current(&input_));

  utf8iterator_next(&input_);
  EXPECT_EQ(-1, utf8iterator_current(&input_));
}

TEST_F(Utf8Test, FiveByteCharIsError) {
  ResetText("\xF6\xA7\xA7\xA7\xA7x");

  EXPECT_EQ(1, GetNumErrors());
  EXPECT_EQ(0xFFFD, utf8iterator_current(&input_));

  utf8iterator_next(&input_);
  utf8iterator_next(&input_);
  utf8iterator_next(&input_);
  utf8iterator_next(&input_);
  EXPECT_EQ(0xFFFD, utf8iterator_current(&input_));
  utf8iterator_next(&input_);
  EXPECT_EQ('x', utf8iterator_current(&input_));
}

TEST_F(Utf8Test, SixByteCharIsError) {
  ResetText("\xF8\xA7\xA7\xA7\xA7\xA7x");

  EXPECT_EQ(1, GetNumErrors());
  EXPECT_EQ(0xFFFD, utf8iterator_current(&input_));

  utf8iterator_next(&input_);
  utf8iterator_next(&input_);
  utf8iterator_next(&input_);
  utf8iterator_next(&input_);
  utf8iterator_next(&input_);
  EXPECT_EQ(0xFFFD, utf8iterator_current(&input_));
  utf8iterator_next(&input_);
  EXPECT_EQ('x', utf8iterator_current(&input_));
}

TEST_F(Utf8Test, SevenByteCharIsError) {
  ResetText("\xFC\xA7\xA7\xA7\xA7\xA7\xA7x");

  EXPECT_EQ(1, GetNumErrors());
  EXPECT_EQ(0xFFFD, utf8iterator_current(&input_));

  utf8iterator_next(&input_);
  utf8iterator_next(&input_);
  utf8iterator_next(&input_);
  utf8iterator_next(&input_);
  utf8iterator_next(&input_);
  utf8iterator_next(&input_);
  EXPECT_EQ(0xFFFD, utf8iterator_current(&input_));
  utf8iterator_next(&input_);
  EXPECT_EQ('x', utf8iterator_current(&input_));
}

TEST_F(Utf8Test, 0xFFIsError) {
  ResetText("\xFFx");

  EXPECT_EQ(1, GetNumErrors());
  EXPECT_EQ(0xFFFD, utf8iterator_current(&input_));

  utf8iterator_next(&input_);
  EXPECT_EQ('x', utf8iterator_current(&input_));
}

TEST_F(Utf8Test, InvalidControlCharIsError) {
  ResetText("\x1Bx");

  EXPECT_EQ(1, GetNumErrors());
  EXPECT_EQ(0xFFFD, utf8iterator_current(&input_));

  utf8iterator_next(&input_);
  EXPECT_EQ('x', utf8iterator_current(&input_));
}

TEST_F(Utf8Test, TruncatedInput) {
  ResetText("\xF1\xA7");

  EXPECT_EQ(1, GetNumErrors());
  EXPECT_EQ(0xFFFD, utf8iterator_current(&input_));

  errors_are_expected_ = true;
  GumboError* error = GetFirstError();
  EXPECT_EQ(GUMBO_ERR_UTF8_TRUNCATED, error->type);
  EXPECT_EQ(1, error->position.line);
  EXPECT_EQ(1, error->position.column);
  EXPECT_EQ(0, error->position.offset);
  EXPECT_EQ('\xF1', *error->original_text);
  EXPECT_EQ(0xF1A7, error->v.codepoint);

  utf8iterator_next(&input_);
  EXPECT_EQ(-1, utf8iterator_current(&input_));
}

TEST_F(Utf8Test, Html5SpecExample) {
  // This example has since been removed from the spec, and the spec has been
  // changed to reference the Unicode Standard 6.2, 5.22 "Best practices for
  // U+FFFD substitution."
  ResetText("\x41\x98\xBA\x42\xE2\x98\x43\xE2\x98\xBA\xE2\x98");

  EXPECT_EQ('A', utf8iterator_current(&input_));

  utf8iterator_next(&input_);
  EXPECT_EQ(0xFFFD, utf8iterator_current(&input_));

  utf8iterator_next(&input_);
  EXPECT_EQ(0xFFFD, utf8iterator_current(&input_));

  utf8iterator_next(&input_);
  EXPECT_EQ('B', utf8iterator_current(&input_));

  utf8iterator_next(&input_);
  EXPECT_EQ(0xFFFD, utf8iterator_current(&input_));

  utf8iterator_next(&input_);
  EXPECT_EQ('C', utf8iterator_current(&input_));

  utf8iterator_next(&input_);
  // \xE2\x98\xBA = 11100010 10011000 10111010
  // Codepoint = 00100110 00111010 = 0x263A
  EXPECT_EQ(0x263A, utf8iterator_current(&input_));

  utf8iterator_next(&input_);
  EXPECT_EQ(0xFFFD, utf8iterator_current(&input_));
  utf8iterator_next(&input_);
}

TEST_F(Utf8Test, MultipleEOFReads) {
  ResetText("a");
  Advance(2);
  EXPECT_EQ(-1, utf8iterator_current(&input_));

  utf8iterator_next(&input_);
  EXPECT_EQ(-1, utf8iterator_current(&input_));
}

TEST_F(Utf8Test, AsciiOnly) {
  ResetText("hello");
  Advance(4);

  EXPECT_EQ('o', utf8iterator_current(&input_));
  EXPECT_EQ('o', *utf8iterator_get_char_pointer(&input_));

  GumboSourcePosition pos;
  utf8iterator_get_position(&input_, &pos);
  EXPECT_EQ(1, pos.line);
  EXPECT_EQ(5, pos.column);
  EXPECT_EQ(4, pos.offset);

  Advance(1);
  EXPECT_EQ(-1, utf8iterator_current(&input_));
}

TEST_F(Utf8Test, NewlinePosition) {
  ResetText("a\nnewline");
  Advance(1);

  // Newline itself should register as being at the end of a line.
  GumboSourcePosition pos;
  utf8iterator_get_position(&input_, &pos);
  EXPECT_EQ(1, pos.line);
  EXPECT_EQ(2, pos.column);
  EXPECT_EQ(1, pos.offset);

  // The next character should be at the next line.
  Advance(1);
  utf8iterator_get_position(&input_, &pos);
  EXPECT_EQ(2, pos.line);
  EXPECT_EQ(1, pos.column);
  EXPECT_EQ(2, pos.offset);
}

TEST_F(Utf8Test, TabPositionFreshTabstop) {
  ResetText("a\n\ttab");
  Advance(sizeof("a\n\t") - 1);

  GumboSourcePosition pos;
  utf8iterator_get_position(&input_, &pos);
  EXPECT_EQ(2, pos.line);
  EXPECT_EQ(8, pos.column);
  EXPECT_EQ(3, pos.offset);
}

TEST_F(Utf8Test, TabPositionMidTabstop) {
  ResetText("a tab\tinline");
  Advance(sizeof("a tab\t") - 1);

  GumboSourcePosition pos;
  utf8iterator_get_position(&input_, &pos);
  EXPECT_EQ(1, pos.line);
  EXPECT_EQ(8, pos.column);
  EXPECT_EQ(6, pos.offset);
}

TEST_F(Utf8Test, ConfigurableTabstop) {
  options_.tab_stop = 4;
  ResetText("a\n\ttab");
  Advance(sizeof("a\n\t") - 1);

  GumboSourcePosition pos;
  utf8iterator_get_position(&input_, &pos);
  EXPECT_EQ(2, pos.line);
  EXPECT_EQ(4, pos.column);
  EXPECT_EQ(3, pos.offset);
}

TEST_F(Utf8Test, CRLF) {
  ResetText("Windows\r\nlinefeeds");
  Advance(sizeof("Windows") - 1);

  EXPECT_EQ('\n', utf8iterator_current(&input_));
  EXPECT_EQ('\n', *utf8iterator_get_char_pointer(&input_));

  GumboSourcePosition pos;
  utf8iterator_get_position(&input_, &pos);
  EXPECT_EQ(1, pos.line);
  // The carriage return should be ignore in column calculations, treating the
  // CRLF combination as one character.
  EXPECT_EQ(8, pos.column);
  // However, it should not be ignored in computing offsets, which are often
  // used by other tools to index into the original buffer.  We don't expect
  // other unicode-aware tools to have the same \r\n handling as HTML5.
  EXPECT_EQ(8, pos.offset);
}

TEST_F(Utf8Test, CarriageReturn) {
  ResetText("Mac\rlinefeeds");
  Advance(sizeof("Mac") - 1);

  EXPECT_EQ('\n', utf8iterator_current(&input_));
  // We don't change the original pointer, which is part of the const input
  // buffer.  original_text pointers will see a carriage return as original
  // written.
  EXPECT_EQ('\r', *utf8iterator_get_char_pointer(&input_));

  GumboSourcePosition pos;
  utf8iterator_get_position(&input_, &pos);
  EXPECT_EQ(1, pos.line);
  EXPECT_EQ(4, pos.column);
  EXPECT_EQ(3, pos.offset);

  Advance(1);
  EXPECT_EQ('l', utf8iterator_current(&input_));
  EXPECT_EQ('l', *utf8iterator_get_char_pointer(&input_));

  utf8iterator_get_position(&input_, &pos);
  EXPECT_EQ(2, pos.line);
  EXPECT_EQ(1, pos.column);
  EXPECT_EQ(4, pos.offset);
}

TEST_F(Utf8Test, Matches) {
  ResetText("\xC2\xA5goobar");
  Advance(1);
  EXPECT_TRUE(utf8iterator_maybe_consume_match(&input_, "goo", 3, true));
  EXPECT_EQ('b', utf8iterator_current(&input_));
}

TEST_F(Utf8Test, MatchesOverflow) {
  ResetText("goo");
  EXPECT_FALSE(utf8iterator_maybe_consume_match(&input_, "goobar", 6, true));
  EXPECT_EQ('g', utf8iterator_current(&input_));
}

TEST_F(Utf8Test, MatchesEof) {
  ResetText("goo");
  EXPECT_TRUE(utf8iterator_maybe_consume_match(&input_, "goo", 3, true));
  EXPECT_EQ(-1, utf8iterator_current(&input_));
}

TEST_F(Utf8Test, MatchesCaseSensitivity) {
  ResetText("gooBAR");
  EXPECT_FALSE(utf8iterator_maybe_consume_match(&input_, "goobar", 6, true));
  EXPECT_EQ('g', utf8iterator_current(&input_));
}

TEST_F(Utf8Test, MatchesCaseInsensitive) {
  ResetText("gooBAR");
  EXPECT_TRUE(utf8iterator_maybe_consume_match(&input_, "goobar", 6, false));
  EXPECT_EQ(-1, utf8iterator_current(&input_));
}

TEST_F(Utf8Test, MatchFollowedByNullByte) {
  // Can't use ResetText, as the implicit strlen will choke on the null.
  text_ = "CDATA\0f";
  utf8iterator_init(&parser_, text_, 7, &input_);

  EXPECT_TRUE(utf8iterator_maybe_consume_match(
      &input_, "cdata", sizeof("cdata") - 1, false));

  EXPECT_EQ(0, utf8iterator_current(&input_));
  EXPECT_EQ('\0', *utf8iterator_get_char_pointer(&input_));
  utf8iterator_next(&input_);
  EXPECT_EQ('f', utf8iterator_current(&input_));
  EXPECT_EQ('f', *utf8iterator_get_char_pointer(&input_));
}

TEST_F(Utf8Test, MarkReset) {
  ResetText("this is a test");
  Advance(5);
  EXPECT_EQ('i', utf8iterator_current(&input_));
  utf8iterator_mark(&input_);

  Advance(3);
  EXPECT_EQ('a', utf8iterator_current(&input_));

  GumboError error;
  utf8iterator_fill_error_at_mark(&input_, &error);
  EXPECT_EQ('i', *error.original_text);
  EXPECT_EQ(1, error.position.line);
  EXPECT_EQ(6, error.position.column);
  EXPECT_EQ(5, error.position.offset);

  utf8iterator_reset(&input_);
  EXPECT_EQ('i', utf8iterator_current(&input_));
  EXPECT_EQ('i', *utf8iterator_get_char_pointer(&input_));

  GumboSourcePosition position;
  utf8iterator_get_position(&input_, &position);
  EXPECT_EQ(1, error.position.line);
  EXPECT_EQ(6, error.position.column);
  EXPECT_EQ(5, error.position.offset);
}

}  // namespace
