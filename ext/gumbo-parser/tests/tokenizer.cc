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

#include "tokenizer.h"

#include <stdio.h>

#include "gtest/gtest.h"
#include "test_utils.h"

extern const char* kGumboTagNames[];

namespace {

// Tests for tokenizer.c
class GumboTokenizerTest : public GumboTest {
 protected:
  GumboTokenizerTest() { gumbo_tokenizer_state_init(&parser_, "", 0); }

  virtual ~GumboTokenizerTest() {
    gumbo_tokenizer_state_destroy(&parser_);
    gumbo_token_destroy(&parser_, &token_);
  }

  void SetInput(const char* input) {
    text_ = input;
    gumbo_tokenizer_state_destroy(&parser_);
    gumbo_tokenizer_state_init(&parser_, input, strlen(input));
  }

  void Advance(int num_tokens) {
    for (int i = 0; i < num_tokens; ++i) {
      EXPECT_TRUE(gumbo_lex(&parser_, &token_));
      gumbo_token_destroy(&parser_, &token_);
    }
  }

  GumboToken token_;
};

TEST(GumboTagEnumTest, TagEnumIncludesAllTags) {
  EXPECT_EQ(150, GUMBO_TAG_UNKNOWN);
  EXPECT_STREQ("", kGumboTagNames[GUMBO_TAG_UNKNOWN]);
}

TEST_F(GumboTokenizerTest, PartialTag) {
  SetInput("<a");
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_EOF, token_.type);
}

TEST_F(GumboTokenizerTest, PartialTagWithAttributes) {
  SetInput("<a href=foo /");
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_EOF, token_.type);
}

TEST_F(GumboTokenizerTest, LexCharToken) {
  SetInput("a");
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_CHARACTER, token_.type);
  EXPECT_EQ(1, token_.position.column);
  EXPECT_EQ(1, token_.position.line);
  EXPECT_EQ(0, token_.position.offset);
  EXPECT_EQ('a', *token_.original_text.data);
  EXPECT_EQ(1, token_.original_text.length);
  EXPECT_EQ('a', token_.v.character);

  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_EOF, token_.type);
  EXPECT_EQ(1, token_.position.offset);
}

TEST_F(GumboTokenizerTest, LexCharRef) {
  SetInput("&nbsp; Text");
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_CHARACTER, token_.type);
  EXPECT_EQ(1, token_.position.column);
  EXPECT_EQ(1, token_.position.line);
  EXPECT_EQ(0, token_.position.offset);
  EXPECT_EQ('&', *token_.original_text.data);
  EXPECT_EQ(6, token_.original_text.length);
  EXPECT_EQ(0xA0, token_.v.character);

  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_WHITESPACE, token_.type);
  EXPECT_EQ(' ', *token_.original_text.data);
  EXPECT_EQ(' ', token_.v.character);
}

TEST_F(GumboTokenizerTest, LexCharRef_NotCharRef) {
  SetInput("&xyz");
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_CHARACTER, token_.type);
  EXPECT_EQ(0, token_.position.offset);
  EXPECT_EQ('&', token_.v.character);

  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_CHARACTER, token_.type);
  EXPECT_EQ(1, token_.position.offset);
  EXPECT_EQ('x', token_.v.character);
}

TEST_F(GumboTokenizerTest, LeadingWhitespace) {
  SetInput(
      "<div>\n"
      "  <span class=foo>");
  Advance(4);
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));  // <span>

  GumboTokenStartTag* start_tag = &token_.v.start_tag;
  EXPECT_EQ(GUMBO_TAG_SPAN, start_tag->tag);
  EXPECT_EQ(2, token_.position.line);
  EXPECT_EQ(3, token_.position.column);
  ASSERT_EQ(1, start_tag->attributes.length);

  GumboAttribute* clas =
      static_cast<GumboAttribute*>(start_tag->attributes.data[0]);
  EXPECT_STREQ("class", clas->name);
  EXPECT_EQ("class", ToString(clas->original_name));
  EXPECT_EQ(2, clas->name_start.line);
  EXPECT_EQ(9, clas->name_start.column);
  EXPECT_EQ(14, clas->name_end.column);
  EXPECT_STREQ("foo", clas->value);
  EXPECT_EQ("foo", ToString(clas->original_value));
  EXPECT_EQ(15, clas->value_start.column);
  EXPECT_EQ(18, clas->value_end.column);
}

TEST_F(GumboTokenizerTest, Doctype) {
  SetInput("<!doctype html>");
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  ASSERT_EQ(GUMBO_TOKEN_DOCTYPE, token_.type);
  EXPECT_EQ(0, token_.position.offset);

  GumboTokenDocType* doc_type = &token_.v.doc_type;
  EXPECT_FALSE(doc_type->force_quirks);
  EXPECT_FALSE(doc_type->has_public_identifier);
  EXPECT_FALSE(doc_type->has_system_identifier);
  EXPECT_STREQ("html", doc_type->name);
  EXPECT_STREQ("", doc_type->public_identifier);
  EXPECT_STREQ("", doc_type->system_identifier);
}

TEST_F(GumboTokenizerTest, DoctypePublic) {
  SetInput(
      "<!DOCTYPE html PUBLIC "
      "\"-//W3C//DTD XHTML 1.0 Transitional//EN\" "
      "'http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd'>");
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  ASSERT_EQ(GUMBO_TOKEN_DOCTYPE, token_.type);
  EXPECT_EQ(0, token_.position.offset);

  GumboTokenDocType* doc_type = &token_.v.doc_type;
  EXPECT_FALSE(doc_type->force_quirks);
  EXPECT_TRUE(doc_type->has_public_identifier);
  EXPECT_TRUE(doc_type->has_system_identifier);
  EXPECT_STREQ("html", doc_type->name);
  EXPECT_STREQ(
      "-//W3C//DTD XHTML 1.0 Transitional//EN", doc_type->public_identifier);
  EXPECT_STREQ("http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd",
      doc_type->system_identifier);
}

TEST_F(GumboTokenizerTest, DoctypeSystem) {
  SetInput("<!DOCtype root_element SYSTEM \"DTD_location\">");
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  ASSERT_EQ(GUMBO_TOKEN_DOCTYPE, token_.type);
  EXPECT_EQ(0, token_.position.offset);

  GumboTokenDocType* doc_type = &token_.v.doc_type;
  EXPECT_FALSE(doc_type->force_quirks);
  EXPECT_FALSE(doc_type->has_public_identifier);
  EXPECT_TRUE(doc_type->has_system_identifier);
  EXPECT_STREQ("root_element", doc_type->name);
  EXPECT_STREQ("DTD_location", doc_type->system_identifier);
}

TEST_F(GumboTokenizerTest, DoctypeUnterminated) {
  SetInput("<!DOCTYPE a PUBLIC''");
  EXPECT_FALSE(gumbo_lex(&parser_, &token_));
  ASSERT_EQ(GUMBO_TOKEN_DOCTYPE, token_.type);
  EXPECT_EQ(0, token_.position.offset);

  GumboTokenDocType* doc_type = &token_.v.doc_type;
  EXPECT_TRUE(doc_type->force_quirks);
  EXPECT_TRUE(doc_type->has_public_identifier);
  EXPECT_FALSE(doc_type->has_system_identifier);
  EXPECT_STREQ("a", doc_type->name);
  EXPECT_STREQ("", doc_type->system_identifier);
}

TEST_F(GumboTokenizerTest, RawtextEnd) {
  SetInput("<title>x ignores <tag></title>");
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_START_TAG, token_.type);
  EXPECT_EQ(GUMBO_TAG_TITLE, token_.v.start_tag.tag);

  gumbo_tokenizer_set_state(&parser_, GUMBO_LEX_RAWTEXT);
  gumbo_token_destroy(&parser_, &token_);
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_CHARACTER, token_.type);
  EXPECT_EQ('x', token_.v.character);
  gumbo_token_destroy(&parser_, &token_);

  Advance(9);
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_CHARACTER, token_.type);
  EXPECT_EQ('<', token_.v.character);
  gumbo_token_destroy(&parser_, &token_);

  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_CHARACTER, token_.type);
  EXPECT_EQ('t', token_.v.character);
  gumbo_token_destroy(&parser_, &token_);

  Advance(3);
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_END_TAG, token_.type);
  EXPECT_EQ(GUMBO_TAG_TITLE, token_.v.end_tag);
}

TEST_F(GumboTokenizerTest, RCDataEnd) {
  SetInput("<title>x</title>");
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_START_TAG, token_.type);
  EXPECT_EQ(GUMBO_TAG_TITLE, token_.v.start_tag.tag);

  gumbo_tokenizer_set_state(&parser_, GUMBO_LEX_RCDATA);
  gumbo_token_destroy(&parser_, &token_);
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_CHARACTER, token_.type);
  EXPECT_EQ('x', token_.v.character);

  gumbo_token_destroy(&parser_, &token_);
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_END_TAG, token_.type);
  EXPECT_EQ(GUMBO_TAG_TITLE, token_.v.end_tag);
}

TEST_F(GumboTokenizerTest, ScriptEnd) {
  SetInput("<script>x = '\"></';</script>");
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_START_TAG, token_.type);
  EXPECT_EQ(GUMBO_TAG_SCRIPT, token_.v.start_tag.tag);

  gumbo_tokenizer_set_state(&parser_, GUMBO_LEX_SCRIPT);
  gumbo_token_destroy(&parser_, &token_);
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_CHARACTER, token_.type);
  EXPECT_EQ('x', token_.v.character);

  gumbo_token_destroy(&parser_, &token_);
  Advance(6);
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_CHARACTER, token_.type);
  EXPECT_EQ('<', token_.v.character);

  gumbo_token_destroy(&parser_, &token_);
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_CHARACTER, token_.type);
  EXPECT_EQ('/', token_.v.character);

  gumbo_token_destroy(&parser_, &token_);
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_CHARACTER, token_.type);
  EXPECT_EQ('\'', token_.v.character);

  gumbo_token_destroy(&parser_, &token_);
  Advance(1);
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_END_TAG, token_.type);
  EXPECT_EQ(GUMBO_TAG_SCRIPT, token_.v.end_tag);
}

TEST_F(GumboTokenizerTest, ScriptEscapedEnd) {
  SetInput("<title>x</title>");
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_START_TAG, token_.type);
  EXPECT_EQ(GUMBO_TAG_TITLE, token_.v.start_tag.tag);

  gumbo_tokenizer_set_state(&parser_, GUMBO_LEX_SCRIPT_ESCAPED);
  gumbo_token_destroy(&parser_, &token_);
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_CHARACTER, token_.type);
  EXPECT_EQ('x', token_.v.character);

  gumbo_token_destroy(&parser_, &token_);
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_END_TAG, token_.type);
  EXPECT_EQ(GUMBO_TAG_TITLE, token_.v.end_tag);
}

TEST_F(GumboTokenizerTest, ScriptCommentEscaped) {
  SetInput(
      "<script><!-- var foo = x < 7 + '</div>-- <A href=\"foo\"></a>';\n"
      "-->\n"
      "</script>");
  Advance(1);
  gumbo_tokenizer_set_state(&parser_, GUMBO_LEX_SCRIPT);
  Advance(15);

  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_CHARACTER, token_.type);
  EXPECT_EQ('x', token_.v.character);

  gumbo_token_destroy(&parser_, &token_);
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_WHITESPACE, token_.type);
  EXPECT_EQ(' ', token_.v.character);

  gumbo_token_destroy(&parser_, &token_);
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_CHARACTER, token_.type);
  EXPECT_EQ('<', token_.v.character);

  gumbo_token_destroy(&parser_, &token_);
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_WHITESPACE, token_.type);
  EXPECT_EQ(' ', token_.v.character);

  gumbo_token_destroy(&parser_, &token_);
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_CHARACTER, token_.type);
  EXPECT_EQ('7', token_.v.character);

  gumbo_token_destroy(&parser_, &token_);
  Advance(4);
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_CHARACTER, token_.type);
  EXPECT_EQ('<', token_.v.character);

  gumbo_token_destroy(&parser_, &token_);
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_CHARACTER, token_.type);
  EXPECT_EQ('/', token_.v.character);

  gumbo_token_destroy(&parser_, &token_);
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_CHARACTER, token_.type);
  EXPECT_EQ('d', token_.v.character);
  gumbo_token_destroy(&parser_, &token_);
  Advance(25);
}

TEST_F(GumboTokenizerTest, ScriptEscapedEmbeddedLessThan) {
  SetInput("<script>/*<![CDATA[*/ x<7 /*]]>*/</script>");
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_START_TAG, token_.type);
  EXPECT_EQ(GUMBO_TAG_SCRIPT, token_.v.start_tag.tag);

  gumbo_tokenizer_set_state(&parser_, GUMBO_LEX_SCRIPT);
  gumbo_token_destroy(&parser_, &token_);
  Advance(14);
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_CHARACTER, token_.type);
  EXPECT_EQ('x', token_.v.character);

  gumbo_token_destroy(&parser_, &token_);
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_CHARACTER, token_.type);
  EXPECT_EQ('<', token_.v.character);

  gumbo_token_destroy(&parser_, &token_);
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_CHARACTER, token_.type);
  EXPECT_EQ('7', token_.v.character);

  gumbo_token_destroy(&parser_, &token_);
  Advance(8);
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_END_TAG, token_.type);
  EXPECT_EQ(GUMBO_TAG_SCRIPT, token_.v.end_tag);
}

TEST_F(GumboTokenizerTest, ScriptHasTagEmbedded) {
  SetInput("<script>var foo = '</div>';</script>");
  Advance(1);
  gumbo_tokenizer_set_state(&parser_, GUMBO_LEX_SCRIPT);
  Advance(11);

  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_CHARACTER, token_.type);
  EXPECT_EQ('<', token_.v.character);

  gumbo_token_destroy(&parser_, &token_);
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_CHARACTER, token_.type);
  EXPECT_EQ('/', token_.v.character);

  gumbo_token_destroy(&parser_, &token_);
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_CHARACTER, token_.type);
  EXPECT_EQ('d', token_.v.character);

  gumbo_token_destroy(&parser_, &token_);
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_CHARACTER, token_.type);
  EXPECT_EQ('i', token_.v.character);
}

TEST_F(GumboTokenizerTest, ScriptDoubleEscaped) {
  SetInput(
      "<script><!--var foo = '<a href=\"foo\"></a>\n"
      "<sCrIpt>i--<f</script>'-->;</script>");
  Advance(1);
  gumbo_tokenizer_set_state(&parser_, GUMBO_LEX_SCRIPT);
  Advance(34);

  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_CHARACTER, token_.type);
  EXPECT_EQ('<', token_.v.character);

  gumbo_token_destroy(&parser_, &token_);
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_CHARACTER, token_.type);
  EXPECT_EQ('s', token_.v.character);

  gumbo_token_destroy(&parser_, &token_);
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_CHARACTER, token_.type);
  EXPECT_EQ('C', token_.v.character);

  gumbo_token_destroy(&parser_, &token_);
  Advance(20);
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_CHARACTER, token_.type);
  EXPECT_EQ('-', token_.v.character);

  gumbo_token_destroy(&parser_, &token_);
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_CHARACTER, token_.type);
  EXPECT_EQ('-', token_.v.character);

  gumbo_token_destroy(&parser_, &token_);
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_CHARACTER, token_.type);
  EXPECT_EQ('>', token_.v.character);
}

TEST_F(GumboTokenizerTest, CData) {
  // SetInput uses strlen and so can't handle nulls.
  text_ = "<![CDATA[\0filler\0text\0]]>";
  gumbo_tokenizer_state_destroy(&parser_);
  gumbo_tokenizer_state_init(
      &parser_, text_, sizeof("<![CDATA[\0filler\0text\0]]>") - 1);
  gumbo_tokenizer_set_is_current_node_foreign(&parser_, true);

  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_NULL, token_.type);
  EXPECT_EQ(0, token_.v.character);

  gumbo_token_destroy(&parser_, &token_);
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_CDATA, token_.type);
  EXPECT_EQ('f', token_.v.character);
}

TEST_F(GumboTokenizerTest, StyleHasTagEmbedded) {
  SetInput("<style>/* For <head> */</style>");
  Advance(1);
  gumbo_tokenizer_set_state(&parser_, GUMBO_LEX_RCDATA);
  Advance(7);

  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_CHARACTER, token_.type);
  EXPECT_EQ('<', token_.v.character);

  gumbo_token_destroy(&parser_, &token_);
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_CHARACTER, token_.type);
  EXPECT_EQ('h', token_.v.character);

  gumbo_token_destroy(&parser_, &token_);
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_CHARACTER, token_.type);
  EXPECT_EQ('e', token_.v.character);
}

TEST_F(GumboTokenizerTest, PreWithNewlines) {
  SetInput("<!DOCTYPE html><pre>\r\na</pre>");
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  ASSERT_EQ(GUMBO_TOKEN_DOCTYPE, token_.type);
  EXPECT_EQ(0, token_.position.offset);

  gumbo_token_destroy(&parser_, &token_);
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  ASSERT_EQ(GUMBO_TOKEN_START_TAG, token_.type);
  EXPECT_EQ("<pre>", ToString(token_.original_text));
  EXPECT_EQ(15, token_.position.offset);
}

TEST_F(GumboTokenizerTest, SelfClosingStartTag) {
  SetInput("<br />");
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  ASSERT_EQ(GUMBO_TOKEN_START_TAG, token_.type);
  EXPECT_EQ(0, token_.position.offset);
  EXPECT_EQ("<br />", ToString(token_.original_text));

  GumboTokenStartTag* start_tag = &token_.v.start_tag;
  EXPECT_EQ(GUMBO_TAG_BR, start_tag->tag);
  EXPECT_EQ(0, start_tag->attributes.length);
  EXPECT_TRUE(start_tag->is_self_closing);
}

TEST_F(GumboTokenizerTest, OpenTagWithAttributes) {
  SetInput("<a href ='/search?q=foo&amp;hl=en'  id=link>");
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  ASSERT_EQ(GUMBO_TOKEN_START_TAG, token_.type);

  GumboTokenStartTag* start_tag = &token_.v.start_tag;
  EXPECT_EQ(GUMBO_TAG_A, start_tag->tag);
  EXPECT_FALSE(start_tag->is_self_closing);
  ASSERT_EQ(2, start_tag->attributes.length);

  GumboAttribute* href =
      static_cast<GumboAttribute*>(start_tag->attributes.data[0]);
  EXPECT_STREQ("href", href->name);
  EXPECT_EQ("href", ToString(href->original_name));
  EXPECT_STREQ("/search?q=foo&hl=en", href->value);
  EXPECT_EQ("'/search?q=foo&amp;hl=en'", ToString(href->original_value));

  GumboAttribute* id =
      static_cast<GumboAttribute*>(start_tag->attributes.data[1]);
  EXPECT_STREQ("id", id->name);
  EXPECT_EQ("id", ToString(id->original_name));
  EXPECT_STREQ("link", id->value);
  EXPECT_EQ("link", ToString(id->original_value));
}

TEST_F(GumboTokenizerTest, BogusComment1) {
  SetInput("<?xml is bogus-comment>Text");
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  ASSERT_EQ(GUMBO_TOKEN_COMMENT, token_.type);
  EXPECT_STREQ("?xml is bogus-comment", token_.v.text);

  gumbo_token_destroy(&parser_, &token_);
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_CHARACTER, token_.type);
  EXPECT_EQ('T', token_.v.character);

  errors_are_expected_ = true;
}

TEST_F(GumboTokenizerTest, BogusComment2) {
  SetInput("</#bogus-comment");
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  ASSERT_EQ(GUMBO_TOKEN_COMMENT, token_.type);
  EXPECT_STREQ("#bogus-comment", token_.v.text);

  gumbo_token_destroy(&parser_, &token_);
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  EXPECT_EQ(GUMBO_TOKEN_EOF, token_.type);
  errors_are_expected_ = true;
}

TEST_F(GumboTokenizerTest, MultilineAttribute) {
  SetInput(
      "<foo long_attr=\"SomeCode;\n"
      "  calls_a_big_long_function();\n"
      "  return true;\" />");
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  ASSERT_EQ(GUMBO_TOKEN_START_TAG, token_.type);

  GumboTokenStartTag* start_tag = &token_.v.start_tag;
  EXPECT_EQ(GUMBO_TAG_UNKNOWN, start_tag->tag);
  EXPECT_TRUE(start_tag->is_self_closing);
  ASSERT_EQ(1, start_tag->attributes.length);

  GumboAttribute* long_attr =
      static_cast<GumboAttribute*>(start_tag->attributes.data[0]);
  EXPECT_STREQ("long_attr", long_attr->name);
  EXPECT_EQ("long_attr", ToString(long_attr->original_name));
  EXPECT_STREQ(
      "SomeCode;\n"
      "  calls_a_big_long_function();\n"
      "  return true;",
      long_attr->value);
}

TEST_F(GumboTokenizerTest, DoubleAmpersand) {
  SetInput("<span jsif=\"foo && bar\">");
  EXPECT_TRUE(gumbo_lex(&parser_, &token_));
  ASSERT_EQ(GUMBO_TOKEN_START_TAG, token_.type);

  GumboTokenStartTag* start_tag = &token_.v.start_tag;
  EXPECT_EQ(GUMBO_TAG_SPAN, start_tag->tag);
  EXPECT_FALSE(start_tag->is_self_closing);
  ASSERT_EQ(1, start_tag->attributes.length);

  GumboAttribute* jsif =
      static_cast<GumboAttribute*>(start_tag->attributes.data[0]);
  EXPECT_STREQ("jsif", jsif->name);
  EXPECT_EQ("jsif", ToString(jsif->original_name));
  EXPECT_STREQ("foo && bar", jsif->value);
  EXPECT_EQ("\"foo && bar\"", ToString(jsif->original_value));
}

TEST_F(GumboTokenizerTest, MatchedTagPair) {
  SetInput("<div id=dash<-Dash data-test=\"bar\">a</div>");
  ASSERT_TRUE(gumbo_lex(&parser_, &token_));
  ASSERT_EQ(GUMBO_TOKEN_START_TAG, token_.type);
  EXPECT_EQ(0, token_.position.offset);

  GumboTokenStartTag* start_tag = &token_.v.start_tag;
  EXPECT_EQ(GUMBO_TAG_DIV, start_tag->tag);
  EXPECT_FALSE(start_tag->is_self_closing);
  ASSERT_EQ(2, start_tag->attributes.length);

  GumboAttribute* id =
      static_cast<GumboAttribute*>(start_tag->attributes.data[0]);
  EXPECT_STREQ("id", id->name);
  EXPECT_EQ("id", ToString(id->original_name));
  EXPECT_EQ(1, id->name_start.line);
  EXPECT_EQ(5, id->name_start.offset);
  EXPECT_EQ(6, id->name_start.column);
  EXPECT_EQ(8, id->name_end.column);
  EXPECT_STREQ("dash<-Dash", id->value);
  EXPECT_EQ("dash<-Dash", ToString(id->original_value));
  EXPECT_EQ(9, id->value_start.column);
  EXPECT_EQ(19, id->value_end.column);

  GumboAttribute* data_attr =
      static_cast<GumboAttribute*>(start_tag->attributes.data[1]);
  EXPECT_STREQ("data-test", data_attr->name);
  EXPECT_EQ("data-test", ToString(data_attr->original_name));
  EXPECT_EQ(20, data_attr->name_start.column);
  EXPECT_EQ(29, data_attr->name_end.column);
  EXPECT_STREQ("bar", data_attr->value);
  EXPECT_EQ("\"bar\"", ToString(data_attr->original_value));
  EXPECT_EQ(30, data_attr->value_start.column);
  EXPECT_EQ(35, data_attr->value_end.column);

  gumbo_token_destroy(&parser_, &token_);
  ASSERT_TRUE(gumbo_lex(&parser_, &token_));
  ASSERT_EQ(GUMBO_TOKEN_CHARACTER, token_.type);
  EXPECT_EQ(35, token_.position.offset);
  EXPECT_EQ('a', token_.v.character);

  gumbo_token_destroy(&parser_, &token_);
  ASSERT_TRUE(gumbo_lex(&parser_, &token_));
  ASSERT_EQ(GUMBO_TOKEN_END_TAG, token_.type);
  EXPECT_EQ(GUMBO_TAG_DIV, token_.v.end_tag);
  errors_are_expected_ = true;
}

TEST_F(GumboTokenizerTest, BogusEndTag) {
  // According to the spec, the correct parse of this is an end tag token for
  // "<div<>" (notice the ending bracket) with the attribute "th=th" (ignored
  // because end tags don't take attributes), with the tokenizer passing through
  // the self-closing tag state in the process.
  SetInput("</div</th>");
  ASSERT_TRUE(gumbo_lex(&parser_, &token_));
  ASSERT_EQ(GUMBO_TOKEN_END_TAG, token_.type);
  EXPECT_EQ(0, token_.position.offset);
  EXPECT_EQ(GUMBO_TAG_UNKNOWN, token_.v.end_tag);
  EXPECT_EQ("</div</th>", ToString(token_.original_text));
  errors_are_expected_ = true;
}
}  // namespace
