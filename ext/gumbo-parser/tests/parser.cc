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

#include "gumbo.h"

#include <string>

#include "gtest/gtest.h"
#include "test_utils.h"

namespace {

class GumboParserTest : public ::testing::Test {
 protected:
  GumboParserTest()
      : options_(kGumboDefaultOptions), output_(NULL), root_(NULL) {
    InitLeakDetection(&options_, &malloc_stats_);
  }

  virtual ~GumboParserTest() {
    if (output_) {
      gumbo_destroy_output(&options_, output_);
    }
    EXPECT_EQ(malloc_stats_.objects_allocated, malloc_stats_.objects_freed);
  }

  virtual void Parse(const char* input) {
    if (output_) {
      gumbo_destroy_output(&options_, output_);
    }

    output_ = gumbo_parse_with_options(&options_, input, strlen(input));
    // The naming inconsistency is because these tests were initially written
    // when gumbo_parse returned the document element instead of an GumboOutput
    // structure.
    root_ = output_->document;
  }

  virtual void ParseFragment(
      const char* input, GumboTag context, GumboNamespaceEnum context_ns) {
    if (output_) {
      gumbo_destroy_output(&options_, output_);
    }

    options_.fragment_context = context;
    options_.fragment_namespace = context_ns;
    output_ = gumbo_parse_with_options(&options_, input, strlen(input));
    root_ = output_->document;
  }

  virtual void Parse(const std::string& input) {
    // This overload is so we can test/demonstrate that computing offsets from
    // the .data() member of an STL string works properly.
    if (output_) {
      gumbo_destroy_output(&options_, output_);
    }

    output_ = gumbo_parse_with_options(&options_, input.data(), input.length());
    root_ = output_->document;
    SanityCheckPointers(input.data(), input.length(), output_->root, 1000);
  }

  MallocStats malloc_stats_;
  GumboOptions options_;
  GumboOutput* output_;
  GumboNode* root_;
};

TEST_F(GumboParserTest, NullDocument) {
  Parse("");
  ASSERT_TRUE(root_);
  ASSERT_EQ(GUMBO_NODE_DOCUMENT, root_->type);
  EXPECT_EQ(GUMBO_INSERTION_BY_PARSER, root_->parse_flags);

  GumboNode* body;
  GetAndAssertBody(root_, &body);
}

TEST_F(GumboParserTest, ParseTwice) {
  Parse("");
  ASSERT_TRUE(root_);
  ASSERT_EQ(GUMBO_NODE_DOCUMENT, root_->type);

  std::string second_input("");
  Parse(second_input);
  ASSERT_TRUE(root_);
  ASSERT_EQ(GUMBO_NODE_DOCUMENT, root_->type);

  GumboNode* body;
  GetAndAssertBody(root_, &body);
}

TEST_F(GumboParserTest, OneChar) {
  std::string input("T");
  Parse(input);
  ASSERT_TRUE(root_);
  ASSERT_EQ(GUMBO_NODE_DOCUMENT, root_->type);
  EXPECT_EQ(GUMBO_INSERTION_BY_PARSER, root_->parse_flags);
  ASSERT_EQ(1, GetChildCount(root_));

  GumboNode* html = GetChild(root_, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, html->type);
  EXPECT_TRUE(html->parse_flags & GUMBO_INSERTION_BY_PARSER);
  EXPECT_TRUE(html->parse_flags & GUMBO_INSERTION_IMPLICIT_END_TAG);
  EXPECT_TRUE(html->parse_flags & GUMBO_INSERTION_IMPLIED);
  EXPECT_EQ(GUMBO_TAG_HTML, html->v.element.tag);
  ASSERT_EQ(2, GetChildCount(html));

  GumboNode* head = GetChild(html, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, head->type);
  EXPECT_EQ(GUMBO_TAG_HEAD, head->v.element.tag);
  EXPECT_EQ(0, GetChildCount(head));

  GumboNode* body = GetChild(html, 1);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, body->type);
  EXPECT_EQ(GUMBO_TAG_BODY, body->v.element.tag);
  ASSERT_EQ(1, GetChildCount(body));
  EXPECT_EQ(1, body->v.element.start_pos.line);
  EXPECT_EQ(1, body->v.element.start_pos.column);
  EXPECT_EQ(0, body->v.element.start_pos.offset);
  EXPECT_EQ(1, body->v.element.end_pos.line);
  EXPECT_EQ(2, body->v.element.end_pos.column);
  EXPECT_EQ(1, body->v.element.end_pos.offset);

  GumboNode* text = GetChild(body, 0);
  ASSERT_EQ(GUMBO_NODE_TEXT, text->type);
  EXPECT_STREQ("T", text->v.text.text);
  EXPECT_EQ(1, text->v.text.start_pos.line);
  EXPECT_EQ(1, text->v.text.start_pos.column);
  EXPECT_EQ(0, text->v.text.start_pos.offset);
  EXPECT_EQ(input.data(), text->v.text.original_text.data);
  EXPECT_EQ(1, text->v.text.original_text.length);
}

TEST_F(GumboParserTest, TextOnly) {
  Parse("Test");
  EXPECT_EQ(1, output_->errors.length);  // No doctype.
  ASSERT_EQ(1, GetChildCount(root_));

  GumboNode* html = GetChild(root_, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, html->type);
  EXPECT_EQ(GUMBO_TAG_HTML, html->v.element.tag);
  ASSERT_EQ(2, GetChildCount(html));

  GumboNode* head = GetChild(html, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, head->type);
  EXPECT_EQ(GUMBO_TAG_HEAD, head->v.element.tag);
  EXPECT_EQ(0, GetChildCount(head));

  GumboNode* body = GetChild(html, 1);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, body->type);
  EXPECT_EQ(GUMBO_TAG_BODY, body->v.element.tag);
  ASSERT_EQ(1, GetChildCount(body));

  GumboNode* text = GetChild(body, 0);
  ASSERT_EQ(GUMBO_NODE_TEXT, text->type);
  EXPECT_STREQ("Test", text->v.text.text);
}

TEST_F(GumboParserTest, SelfClosingTagError) {
  Parse("<div/>");
  // TODO(jdtang): I think this is double-counting some error cases, I think we
  // may ultimately want to de-dup errors that occur on the same token.
  EXPECT_EQ(8, output_->errors.length);
}

TEST_F(GumboParserTest, UnexpectedEndBreak) {
  Parse("</br><div></div>");

  GumboNode* body;
  GetAndAssertBody(root_, &body);
  ASSERT_EQ(2, GetChildCount(body));

  GumboNode* br = GetChild(body, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, br->type);
  EXPECT_EQ(GUMBO_TAG_BR, br->v.element.tag);
  ASSERT_EQ(0, GetChildCount(br));

  GumboNode* div = GetChild(body, 1);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, div->type);
  EXPECT_EQ(GUMBO_TAG_DIV, div->v.element.tag);
  ASSERT_EQ(0, GetChildCount(div));
}

TEST_F(GumboParserTest, CaseSensitiveAttributes) {
  Parse("<div class=CamelCase>");
  GumboNode* body;
  GetAndAssertBody(root_, &body);
  ASSERT_EQ(1, GetChildCount(body));

  GumboNode* div = GetChild(body, 0);
  GumboVector* attributes = &div->v.element.attributes;
  ASSERT_EQ(1, attributes->length);

  GumboAttribute* clas = static_cast<GumboAttribute*>(attributes->data[0]);
  EXPECT_EQ(GUMBO_ATTR_NAMESPACE_NONE, clas->attr_namespace);
  EXPECT_STREQ("class", clas->name);
  EXPECT_STREQ("CamelCase", clas->value);
}

TEST_F(GumboParserTest, ExplicitHtmlStructure) {
  Parse(
      "<!doctype html>\n<html>"
      "<head><title>Foo</title></head>\n"
      "<body><div class=bar>Test</div></body></html>");
  ASSERT_EQ(1, GetChildCount(root_));
  EXPECT_EQ(0, output_->errors.length);

  ASSERT_EQ(GUMBO_NODE_DOCUMENT, root_->type);
  EXPECT_STREQ("html", root_->v.document.name);
  EXPECT_STREQ("", root_->v.document.public_identifier);
  EXPECT_STREQ("", root_->v.document.system_identifier);

  GumboNode* html = GetChild(root_, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, html->type);
  EXPECT_EQ(GUMBO_INSERTION_NORMAL, html->parse_flags);
  EXPECT_EQ(GUMBO_TAG_HTML, html->v.element.tag);
  EXPECT_EQ(2, html->v.element.start_pos.line);
  EXPECT_EQ(1, html->v.element.start_pos.column);
  EXPECT_EQ(16, html->v.element.start_pos.offset);
  EXPECT_EQ(3, html->v.element.end_pos.line);
  EXPECT_EQ(39, html->v.element.end_pos.column);
  EXPECT_EQ(92, html->v.element.end_pos.offset);
  EXPECT_EQ("<html>", ToString(html->v.element.original_tag));
  EXPECT_EQ("</html>", ToString(html->v.element.original_end_tag));
  ASSERT_EQ(3, GetChildCount(html));

  GumboNode* head = GetChild(html, 0);
  EXPECT_EQ(GUMBO_INSERTION_NORMAL, head->parse_flags);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, head->type);
  EXPECT_EQ(GUMBO_TAG_HEAD, head->v.element.tag);
  EXPECT_EQ(html, head->parent);
  EXPECT_EQ(0, head->index_within_parent);
  EXPECT_EQ(1, GetChildCount(head));

  GumboNode* body = GetChild(html, 2);
  EXPECT_EQ(GUMBO_INSERTION_NORMAL, body->parse_flags);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, body->type);
  EXPECT_EQ(GUMBO_TAG_BODY, body->v.element.tag);
  EXPECT_EQ(html, body->parent);
  EXPECT_EQ(3, body->v.element.start_pos.line);
  EXPECT_EQ(1, body->v.element.start_pos.column);
  EXPECT_EQ(54, body->v.element.start_pos.offset);
  EXPECT_EQ(3, body->v.element.end_pos.line);
  EXPECT_EQ(32, body->v.element.end_pos.column);
  EXPECT_EQ(85, body->v.element.end_pos.offset);
  EXPECT_EQ("<body>", ToString(body->v.element.original_tag));
  EXPECT_EQ("</body>", ToString(body->v.element.original_end_tag));
  EXPECT_EQ(2, body->index_within_parent);
  ASSERT_EQ(1, GetChildCount(body));

  GumboNode* div = GetChild(body, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, div->type);
  EXPECT_EQ(GUMBO_TAG_DIV, div->v.element.tag);
  EXPECT_EQ(body, div->parent);
  EXPECT_EQ(0, div->index_within_parent);
  ASSERT_EQ(1, GetChildCount(div));

  ASSERT_EQ(1, GetAttributeCount(div));
  GumboAttribute* clas = GetAttribute(div, 0);
  EXPECT_STREQ("class", clas->name);
  EXPECT_EQ("class", ToString(clas->original_name));
  EXPECT_STREQ("bar", clas->value);
  EXPECT_EQ("bar", ToString(clas->original_value));

  GumboNode* text = GetChild(div, 0);
  ASSERT_EQ(GUMBO_NODE_TEXT, text->type);
  EXPECT_STREQ("Test", text->v.text.text);
}

TEST_F(GumboParserTest, Whitespace) {
  Parse("<ul>\n  <li>Text\n</ul>");

  GumboNode* body;
  GetAndAssertBody(root_, &body);
  ASSERT_EQ(1, GetChildCount(body));

  GumboNode* ul = GetChild(body, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, ul->type);
  EXPECT_EQ(GUMBO_TAG_UL, ul->v.element.tag);
  ASSERT_EQ(2, GetChildCount(ul));

  GumboNode* whitespace = GetChild(ul, 0);
  ASSERT_EQ(GUMBO_NODE_WHITESPACE, whitespace->type);
  EXPECT_STREQ("\n  ", whitespace->v.text.text);

  GumboNode* li = GetChild(ul, 1);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, li->type);
  EXPECT_EQ(GUMBO_TAG_LI, li->v.element.tag);
  ASSERT_EQ(1, GetChildCount(li));

  GumboNode* text = GetChild(li, 0);
  ASSERT_EQ(GUMBO_NODE_TEXT, text->type);
  EXPECT_STREQ("Text\n", text->v.text.text);
}

TEST_F(GumboParserTest, DuplicateAttributes) {
  std::string text("<input checked=\"false\" checked=true id=foo id='bar'>");
  Parse(text);

  GumboNode* body;
  GetAndAssertBody(root_, &body);
  ASSERT_EQ(1, GetChildCount(body));

  GumboNode* input = GetChild(body, 0);
  EXPECT_EQ(GUMBO_INSERTION_IMPLICIT_END_TAG, input->parse_flags);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, input->type);
  EXPECT_EQ(GUMBO_TAG_INPUT, input->v.element.tag);
  EXPECT_EQ(0, GetChildCount(input));
  ASSERT_EQ(2, GetAttributeCount(input));

  GumboAttribute* checked = GetAttribute(input, 0);
  EXPECT_STREQ("checked", checked->name);
  EXPECT_STREQ("false", checked->value);
  EXPECT_EQ(1, checked->name_start.line);
  EXPECT_EQ(8, checked->name_start.column);
  EXPECT_EQ(15, checked->name_end.column);
  EXPECT_EQ(16, checked->value_start.column);
  EXPECT_EQ(23, checked->value_end.column);
  EXPECT_EQ(7, checked->original_name.data - text.data());
  EXPECT_EQ(7, checked->original_name.length);
  EXPECT_EQ(15, checked->original_value.data - text.data());
  EXPECT_EQ(7, checked->original_value.length);

  GumboAttribute* id = GetAttribute(input, 1);
  EXPECT_STREQ("id", id->name);
  EXPECT_STREQ("foo", id->value);

  // TODO(jdtang): Run some assertions on the parse error that's added.
}

TEST_F(GumboParserTest, LinkTagsInHead) {
  Parse(
      "<html>\n"
      "  <head>\n"
      "    <title>Sample title></title>\n\n"
      "    <link rel=stylesheet>\n"
      "    <link rel=author>\n"
      "  </head>\n"
      "  <body>Foo</body>");
  ASSERT_EQ(1, GetChildCount(root_));

  GumboNode* html = GetChild(root_, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, html->type);
  EXPECT_EQ(GUMBO_INSERTION_IMPLICIT_END_TAG, html->parse_flags);
  EXPECT_EQ(GUMBO_TAG_HTML, html->v.element.tag);
  ASSERT_EQ(3, GetChildCount(html));

  GumboNode* head = GetChild(html, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, head->type);
  EXPECT_EQ(GUMBO_INSERTION_NORMAL, head->parse_flags);
  EXPECT_EQ(GUMBO_TAG_HEAD, head->v.element.tag);
  EXPECT_EQ(7, GetChildCount(head));

  GumboNode* text1 = GetChild(head, 2);
  ASSERT_EQ(GUMBO_NODE_WHITESPACE, text1->type);
  EXPECT_STREQ("\n\n    ", text1->v.text.text);

  GumboNode* link1 = GetChild(head, 3);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, link1->type);
  EXPECT_EQ(GUMBO_TAG_LINK, link1->v.element.tag);
  EXPECT_EQ(GUMBO_INSERTION_IMPLICIT_END_TAG, link1->parse_flags);
  EXPECT_EQ(0, GetChildCount(link1));

  GumboNode* text2 = GetChild(head, 4);
  ASSERT_EQ(GUMBO_NODE_WHITESPACE, text2->type);
  EXPECT_STREQ("\n    ", text2->v.text.text);

  GumboNode* link2 = GetChild(head, 5);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, link2->type);
  EXPECT_EQ(GUMBO_TAG_LINK, link2->v.element.tag);
  EXPECT_EQ(GUMBO_INSERTION_IMPLICIT_END_TAG, link2->parse_flags);
  EXPECT_EQ(0, GetChildCount(link2));

  GumboNode* text3 = GetChild(head, 6);
  ASSERT_EQ(GUMBO_NODE_WHITESPACE, text3->type);
  EXPECT_STREQ("\n  ", text3->v.text.text);

  GumboNode* body = GetChild(html, 2);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, body->type);
  EXPECT_EQ(GUMBO_INSERTION_NORMAL, body->parse_flags);
  EXPECT_EQ(GUMBO_TAG_BODY, body->v.element.tag);
  ASSERT_EQ(1, GetChildCount(body));
}

TEST_F(GumboParserTest, WhitespaceBeforeHtml) {
  Parse("<!doctype html>\n<html>Test</html>");
  ASSERT_EQ(1, GetChildCount(root_));

  GumboNode* body = GetChild(GetChild(root_, 0), 1);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, body->type);
  EXPECT_EQ(GUMBO_TAG_BODY, GetTag(body));
  ASSERT_EQ(1, GetChildCount(body));

  GumboNode* text = GetChild(body, 0);
  ASSERT_EQ(GUMBO_NODE_TEXT, text->type);
  EXPECT_STREQ("Test", text->v.text.text);
}

TEST_F(GumboParserTest, TextAfterHtml) {
  Parse("<html>Test</html> after doc");
  GumboNode* body;
  GetAndAssertBody(root_, &body);

  ASSERT_EQ(GUMBO_NODE_ELEMENT, body->type);
  EXPECT_EQ(GUMBO_TAG_BODY, GetTag(body));
  ASSERT_EQ(1, GetChildCount(body));

  GumboNode* text = GetChild(body, 0);
  ASSERT_EQ(GUMBO_NODE_TEXT, text->type);
  EXPECT_STREQ("Test after doc", text->v.text.text);
}

TEST_F(GumboParserTest, WhitespaceInHead) {
  Parse("<html>  Test</html>");

  GumboNode* html = GetChild(root_, 0);
  EXPECT_EQ(GUMBO_NODE_ELEMENT, html->type);
  EXPECT_EQ(GUMBO_TAG_HTML, GetTag(html));
  EXPECT_EQ(2, GetChildCount(html));

  GumboNode* head = GetChild(html, 0);
  EXPECT_EQ(GUMBO_NODE_ELEMENT, head->type);
  EXPECT_EQ(GUMBO_TAG_HEAD, GetTag(head));
  EXPECT_EQ(0, GetChildCount(head));

  GumboNode* body = GetChild(html, 1);
  EXPECT_EQ(GUMBO_NODE_ELEMENT, body->type);
  EXPECT_EQ(GUMBO_TAG_BODY, GetTag(body));

  GumboNode* text = GetChild(body, 0);
  ASSERT_EQ(GUMBO_NODE_TEXT, text->type);
  EXPECT_STREQ("Test", text->v.text.text);
}

TEST_F(GumboParserTest, Doctype) {
  Parse("<!doctype html>Test");
  GumboDocument* doc = &root_->v.document;
  EXPECT_EQ(1, doc->children.length);
  EXPECT_EQ(GUMBO_DOCTYPE_NO_QUIRKS, doc->doc_type_quirks_mode);

  EXPECT_STREQ("html", doc->name);
  EXPECT_STREQ("", doc->public_identifier);
  EXPECT_STREQ("", doc->system_identifier);
}

TEST_F(GumboParserTest, InvalidDoctype) {
  Parse("Test<!doctype root_element SYSTEM \"DTD_location\">");

  // Default doc token; the declared one is ignored.
  GumboDocument* doc = &root_->v.document;
  EXPECT_EQ(1, doc->children.length);
  EXPECT_EQ(GUMBO_DOCTYPE_QUIRKS, doc->doc_type_quirks_mode);

  EXPECT_STREQ("", doc->name);
  EXPECT_STREQ("", doc->public_identifier);
  EXPECT_STREQ("", doc->system_identifier);

  GumboNode* body;
  GetAndAssertBody(root_, &body);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, body->type);
  EXPECT_EQ(GUMBO_TAG_BODY, GetTag(body));
  ASSERT_EQ(1, GetChildCount(body));

  GumboNode* text = GetChild(body, 0);
  ASSERT_EQ(GUMBO_NODE_TEXT, text->type);
  EXPECT_STREQ("Test", text->v.text.text);
}

TEST_F(GumboParserTest, SingleComment) {
  Parse("<!-- comment -->");
  GumboNode* comment = GetChild(root_, 0);
  ASSERT_EQ(GUMBO_NODE_COMMENT, comment->type);
  EXPECT_STREQ(" comment ", comment->v.text.text);
}

TEST_F(GumboParserTest, CommentInText) {
  Parse("Start <!-- comment --> end");
  GumboNode* body;
  GetAndAssertBody(root_, &body);
  ASSERT_EQ(3, GetChildCount(body));

  GumboNode* start = GetChild(body, 0);
  ASSERT_EQ(GUMBO_NODE_TEXT, start->type);
  EXPECT_STREQ("Start ", start->v.text.text);

  GumboNode* comment = GetChild(body, 1);
  ASSERT_EQ(GUMBO_NODE_COMMENT, comment->type);
  EXPECT_EQ(body, comment->parent);
  EXPECT_EQ(1, comment->index_within_parent);
  EXPECT_STREQ(" comment ", comment->v.text.text);

  GumboNode* end = GetChild(body, 2);
  ASSERT_EQ(GUMBO_NODE_TEXT, end->type);
  EXPECT_STREQ(" end", end->v.text.text);
}

TEST_F(GumboParserTest, CommentBeforeNode) {
  Parse("<!--This is a comment-->\n<h1>hello world!</h1>");
  GumboNode* comment = GetChild(root_, 0);
  ASSERT_EQ(GUMBO_NODE_COMMENT, comment->type);
  EXPECT_STREQ("This is a comment", comment->v.text.text);
  EXPECT_EQ(
      "<!--This is a comment-->", ToString(comment->v.text.original_text));

  // Newline is ignored per the rules for "initial" insertion mode.

  GumboNode* body;
  GetAndAssertBody(root_, &body);
  ASSERT_EQ(1, GetChildCount(body));

  GumboNode* h1 = GetChild(body, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, h1->type);
  EXPECT_EQ(GUMBO_TAG_H1, h1->v.element.tag);
}

TEST_F(GumboParserTest, CommentInVerbatimMode) {
  Parse("<body> <div id='onegoogle'>Text</div>  </body><!-- comment \n\n-->");

  GumboNode* html = GetChild(root_, 0);
  EXPECT_EQ(GUMBO_NODE_ELEMENT, html->type);
  EXPECT_EQ(GUMBO_TAG_HTML, GetTag(html));
  EXPECT_EQ(GUMBO_INSERTION_BY_PARSER | GUMBO_INSERTION_IMPLIED |
                GUMBO_INSERTION_IMPLICIT_END_TAG,
      html->parse_flags);
  EXPECT_EQ(3, GetChildCount(html));

  GumboNode* body = GetChild(html, 1);
  EXPECT_EQ(GUMBO_NODE_ELEMENT, body->type);
  EXPECT_EQ(GUMBO_TAG_BODY, GetTag(body));
  EXPECT_EQ(GUMBO_INSERTION_NORMAL, body->parse_flags);
  EXPECT_EQ(3, GetChildCount(body));

  GumboNode* comment = GetChild(html, 2);
  ASSERT_EQ(GUMBO_NODE_COMMENT, comment->type);
  EXPECT_EQ(GUMBO_INSERTION_NORMAL, comment->parse_flags);
  EXPECT_STREQ(" comment \n\n", comment->v.text.text);
}

TEST_F(GumboParserTest, UnknownTag) {
  Parse("<foo>1<p>2</FOO>");
  GumboNode* body;
  GetAndAssertBody(root_, &body);
  ASSERT_EQ(1, GetChildCount(body));

  GumboNode* foo = GetChild(body, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, foo->type);
  EXPECT_EQ(GUMBO_TAG_UNKNOWN, GetTag(foo));
  EXPECT_EQ("<foo>", ToString(foo->v.element.original_tag));
  // According to the spec, the misplaced end tag is ignored, and so we return
  // an empty original_end_tag text.  We may want to extend our error-reporting
  // a bit so that we close off the tag that it *would have closed*, had the
  // HTML been correct, along with a parse flag that says the end tag was in the
  // wrong place.
  EXPECT_EQ("", ToString(foo->v.element.original_end_tag));
}

TEST_F(GumboParserTest, UnknownTag2) {
  Parse("<div><sarcasm><div></div></sarcasm></div>");
  GumboNode* body;
  GetAndAssertBody(root_, &body);
  ASSERT_EQ(1, GetChildCount(body));

  GumboNode* div = GetChild(body, 0);
  ASSERT_EQ(1, GetChildCount(div));
  GumboNode* sarcasm = GetChild(div, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, sarcasm->type);
  EXPECT_EQ(GUMBO_TAG_UNKNOWN, GetTag(sarcasm));
  EXPECT_EQ("<sarcasm>", ToString(sarcasm->v.element.original_tag));
  EXPECT_EQ("</sarcasm>", ToString(sarcasm->v.element.original_end_tag));
}

TEST_F(GumboParserTest, InvalidEndTag) {
  Parse("<a><img src=foo.jpg></img></a>");
  GumboNode* body;
  GetAndAssertBody(root_, &body);
  ASSERT_EQ(1, GetChildCount(body));

  GumboNode* a = GetChild(body, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, a->type);
  EXPECT_EQ(GUMBO_TAG_A, GetTag(a));
  ASSERT_EQ(1, GetChildCount(a));

  GumboNode* img = GetChild(a, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, img->type);
  EXPECT_EQ(GUMBO_TAG_IMG, GetTag(img));
  ASSERT_EQ(0, GetChildCount(img));
}

TEST_F(GumboParserTest, Tables) {
  Parse(
      "<html><table>\n"
      "  <tr><br /></invalid-tag>\n"
      "    <th>One</th>\n"
      "    <td>Two</td>\n"
      "  </tr>\n"
      "  <iframe></iframe>"
      "</table><tr></tr><div></div></html>");
  GumboNode* body;
  GetAndAssertBody(root_, &body);
  ASSERT_EQ(4, GetChildCount(body));

  GumboNode* br = GetChild(body, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, br->type);
  EXPECT_EQ(GUMBO_TAG_BR, GetTag(br));
  EXPECT_EQ(body, br->parent);
  EXPECT_EQ(0, br->index_within_parent);
  ASSERT_EQ(0, GetChildCount(br));

  GumboNode* iframe = GetChild(body, 1);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, iframe->type);
  EXPECT_EQ(GUMBO_TAG_IFRAME, GetTag(iframe));
  ASSERT_EQ(0, GetChildCount(iframe));

  GumboNode* table = GetChild(body, 2);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, table->type);
  EXPECT_EQ(GUMBO_TAG_TABLE, GetTag(table));
  EXPECT_EQ(body, table->parent);
  EXPECT_EQ(2, table->index_within_parent);
  ASSERT_EQ(2, GetChildCount(table));

  GumboNode* table_text = GetChild(table, 0);
  ASSERT_EQ(GUMBO_NODE_WHITESPACE, table_text->type);
  EXPECT_STREQ("\n  ", table_text->v.text.text);

  GumboNode* tbody = GetChild(table, 1);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, tbody->type);
  EXPECT_EQ(GUMBO_TAG_TBODY, GetTag(tbody));
  ASSERT_EQ(2, GetChildCount(tbody));
  // Second node is whitespace.

  GumboNode* tr = GetChild(tbody, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, tr->type);
  EXPECT_EQ(GUMBO_TAG_TR, GetTag(tr));
  ASSERT_EQ(5, GetChildCount(tr));  // Including whitespace.

  GumboNode* tr_text = GetChild(tr, 0);
  ASSERT_EQ(GUMBO_NODE_WHITESPACE, tr_text->type);
  EXPECT_EQ(tr, tr_text->parent);
  EXPECT_EQ(0, tr_text->index_within_parent);
  EXPECT_STREQ("\n    ", tr_text->v.text.text);

  GumboNode* th = GetChild(tr, 1);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, th->type);
  EXPECT_EQ(GUMBO_TAG_TH, GetTag(th));
  EXPECT_EQ(tr, th->parent);
  EXPECT_EQ(1, th->index_within_parent);
  ASSERT_EQ(1, GetChildCount(th));

  GumboNode* th_text = GetChild(th, 0);
  ASSERT_EQ(GUMBO_NODE_TEXT, th_text->type);
  EXPECT_STREQ("One", th_text->v.text.text);

  GumboNode* td = GetChild(tr, 3);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, td->type);
  EXPECT_EQ(GUMBO_TAG_TD, GetTag(td));
  ASSERT_EQ(1, GetChildCount(td));

  GumboNode* td_text = GetChild(td, 0);
  ASSERT_EQ(GUMBO_NODE_TEXT, td_text->type);
  EXPECT_STREQ("Two", td_text->v.text.text);

  GumboNode* td2_text = GetChild(td, 0);
  ASSERT_EQ(GUMBO_NODE_TEXT, td2_text->type);
  EXPECT_STREQ("Two", td2_text->v.text.text);

  GumboNode* div = GetChild(body, 3);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, div->type);
  EXPECT_EQ(GUMBO_TAG_DIV, GetTag(div));
  ASSERT_EQ(0, GetChildCount(div));
}

TEST_F(GumboParserTest, StartParagraphInTable) {
  Parse("<table><P></tr></td>foo</table>");

  GumboNode* body;
  GetAndAssertBody(root_, &body);
  ASSERT_EQ(2, GetChildCount(body));

  GumboNode* paragraph = GetChild(body, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, paragraph->type);
  EXPECT_EQ(GUMBO_TAG_P, GetTag(paragraph));
  EXPECT_EQ(body, paragraph->parent);
  EXPECT_EQ(0, paragraph->index_within_parent);
  ASSERT_EQ(1, GetChildCount(paragraph));

  GumboNode* text = GetChild(paragraph, 0);
  ASSERT_EQ(GUMBO_NODE_TEXT, text->type);
  EXPECT_STREQ("foo", text->v.text.text);

  GumboNode* table = GetChild(body, 1);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, table->type);
  EXPECT_EQ(GUMBO_TAG_TABLE, GetTag(table));
  EXPECT_EQ(body, table->parent);
  EXPECT_EQ(1, table->index_within_parent);
  ASSERT_EQ(0, GetChildCount(table));
}

TEST_F(GumboParserTest, EndParagraphInTable) {
  Parse("<table></p></table>");

  GumboNode* body;
  GetAndAssertBody(root_, &body);
  ASSERT_EQ(2, GetChildCount(body));

  GumboNode* paragraph = GetChild(body, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, paragraph->type);
  EXPECT_EQ(GUMBO_TAG_P, GetTag(paragraph));
  EXPECT_EQ(body, paragraph->parent);
  EXPECT_EQ(0, paragraph->index_within_parent);
  ASSERT_EQ(0, GetChildCount(paragraph));

  GumboNode* table = GetChild(body, 1);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, table->type);
  EXPECT_EQ(GUMBO_TAG_TABLE, GetTag(table));
  EXPECT_EQ(body, table->parent);
  EXPECT_EQ(1, table->index_within_parent);
  ASSERT_EQ(0, GetChildCount(table));
}

TEST_F(GumboParserTest, UnknownTagInTable) {
  Parse("<table><foo>bar</table>");

  GumboNode* body;
  GetAndAssertBody(root_, &body);
  ASSERT_EQ(2, GetChildCount(body));

  GumboNode* foo = GetChild(body, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, foo->type);
  EXPECT_EQ(GUMBO_TAG_UNKNOWN, GetTag(foo));
  EXPECT_EQ("<foo>", ToString(foo->v.element.original_tag));
  EXPECT_EQ(body, foo->parent);
  EXPECT_EQ(0, foo->index_within_parent);
  ASSERT_EQ(1, GetChildCount(foo));

  GumboNode* bar = GetChild(foo, 0);
  ASSERT_EQ(GUMBO_NODE_TEXT, bar->type);
  EXPECT_STREQ("bar", bar->v.text.text);

  GumboNode* table = GetChild(body, 1);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, table->type);
  EXPECT_EQ(GUMBO_TAG_TABLE, GetTag(table));
  EXPECT_EQ(body, table->parent);
  EXPECT_EQ(1, table->index_within_parent);
  ASSERT_EQ(0, GetChildCount(table));
}

TEST_F(GumboParserTest, UnclosedTableTags) {
  Parse(
      "<html><table>\n"
      "  <tr>\n"
      "    <td>One\n"
      "    <td>Two\n"
      "  <tr><td>Row2\n"
      "  <tr><td>Row3\n"
      "</table>\n"
      "</html>");
  GumboNode* body;
  GetAndAssertBody(root_, &body);
  ASSERT_EQ(2, GetChildCount(body));

  GumboNode* table = GetChild(body, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, table->type);
  EXPECT_EQ(GUMBO_TAG_TABLE, GetTag(table));
  ASSERT_EQ(2, GetChildCount(table));

  GumboNode* table_text = GetChild(table, 0);
  ASSERT_EQ(GUMBO_NODE_WHITESPACE, table_text->type);
  EXPECT_STREQ("\n  ", table_text->v.text.text);

  GumboNode* tbody = GetChild(table, 1);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, tbody->type);
  EXPECT_EQ(GUMBO_TAG_TBODY, GetTag(tbody));
  ASSERT_EQ(3, GetChildCount(tbody));

  GumboNode* tr = GetChild(tbody, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, tr->type);
  EXPECT_EQ(GUMBO_TAG_TR, GetTag(tr));
  ASSERT_EQ(3, GetChildCount(tr));

  GumboNode* tr_text = GetChild(tr, 0);
  ASSERT_EQ(GUMBO_NODE_WHITESPACE, tr_text->type);
  EXPECT_STREQ("\n    ", tr_text->v.text.text);

  GumboNode* td1 = GetChild(tr, 1);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, td1->type);
  EXPECT_EQ(GUMBO_TAG_TD, GetTag(td1));
  ASSERT_EQ(1, GetChildCount(td1));

  GumboNode* td1_text = GetChild(td1, 0);
  ASSERT_EQ(GUMBO_NODE_TEXT, td1_text->type);
  EXPECT_STREQ("One\n    ", td1_text->v.text.text);

  GumboNode* td2 = GetChild(tr, 2);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, td2->type);
  EXPECT_EQ(GUMBO_TAG_TD, GetTag(td2));
  ASSERT_EQ(1, GetChildCount(td2));

  GumboNode* td2_text = GetChild(td2, 0);
  ASSERT_EQ(GUMBO_NODE_TEXT, td2_text->type);
  EXPECT_STREQ("Two\n  ", td2_text->v.text.text);

  GumboNode* tr3 = GetChild(tbody, 2);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, tr3->type);
  EXPECT_EQ(GUMBO_TAG_TR, GetTag(tr3));
  ASSERT_EQ(1, GetChildCount(tr3));

  GumboNode* body_text = GetChild(body, 1);
  ASSERT_EQ(GUMBO_NODE_WHITESPACE, body_text->type);
  EXPECT_STREQ("\n", body_text->v.text.text);
}

TEST_F(GumboParserTest, MisnestedTable) {
  Parse("<table><tr><div><td></div></table>");

  GumboNode* body;
  GetAndAssertBody(root_, &body);
  ASSERT_EQ(2, GetChildCount(body));

  GumboNode* div = GetChild(body, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, div->type);
  EXPECT_EQ(GUMBO_TAG_DIV, GetTag(div));
  ASSERT_EQ(0, GetChildCount(div));

  GumboNode* table = GetChild(body, 1);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, table->type);
  EXPECT_EQ(GUMBO_TAG_TABLE, GetTag(table));
  ASSERT_EQ(1, GetChildCount(table));

  GumboNode* tbody = GetChild(table, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, tbody->type);
  EXPECT_EQ(GUMBO_TAG_TBODY, GetTag(tbody));
  ASSERT_EQ(1, GetChildCount(tbody));

  GumboNode* tr = GetChild(tbody, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, tr->type);
  EXPECT_EQ(GUMBO_TAG_TR, GetTag(tr));
  ASSERT_EQ(1, GetChildCount(tr));

  GumboNode* td = GetChild(tr, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, td->type);
  EXPECT_EQ(GUMBO_TAG_TD, GetTag(td));
  ASSERT_EQ(0, GetChildCount(td));
}

TEST_F(GumboParserTest, MisnestedTable2) {
  Parse("<table><td>Cell1<table><th>Cell2<tr>Cell3</table>");

  GumboNode* body;
  GetAndAssertBody(root_, &body);
  ASSERT_EQ(1, GetChildCount(body));

  GumboNode* table1 = GetChild(body, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, table1->type);
  EXPECT_EQ(GUMBO_TAG_TABLE, GetTag(table1));
  ASSERT_EQ(1, GetChildCount(table1));

  GumboNode* tbody1 = GetChild(table1, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, tbody1->type);
  EXPECT_EQ(GUMBO_TAG_TBODY, GetTag(tbody1));
  ASSERT_EQ(1, GetChildCount(tbody1));

  GumboNode* tr1 = GetChild(tbody1, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, tr1->type);
  EXPECT_EQ(GUMBO_TAG_TR, GetTag(tr1));
  ASSERT_EQ(1, GetChildCount(tr1));

  GumboNode* td1 = GetChild(tr1, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, td1->type);
  EXPECT_EQ(GUMBO_TAG_TD, GetTag(td1));
  ASSERT_EQ(3, GetChildCount(td1));

  GumboNode* cell1 = GetChild(td1, 0);
  ASSERT_EQ(GUMBO_NODE_TEXT, cell1->type);
  EXPECT_STREQ("Cell1", cell1->v.text.text);

  // Foster-parented out of the inner <tr>
  GumboNode* cell3 = GetChild(td1, 1);
  ASSERT_EQ(GUMBO_NODE_TEXT, cell3->type);
  EXPECT_STREQ("Cell3", cell3->v.text.text);

  GumboNode* table2 = GetChild(td1, 2);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, table2->type);
  EXPECT_EQ(GUMBO_TAG_TABLE, GetTag(table2));
  ASSERT_EQ(1, GetChildCount(table2));

  GumboNode* tbody2 = GetChild(table2, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, tbody2->type);
  EXPECT_EQ(GUMBO_TAG_TBODY, GetTag(tbody2));
  ASSERT_EQ(2, GetChildCount(tbody2));

  GumboNode* tr2 = GetChild(tbody2, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, tr2->type);
  EXPECT_EQ(GUMBO_TAG_TR, GetTag(tr2));
  ASSERT_EQ(1, GetChildCount(tr2));

  GumboNode* th = GetChild(tr2, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, th->type);
  EXPECT_EQ(GUMBO_TAG_TH, GetTag(th));
  ASSERT_EQ(1, GetChildCount(th));

  GumboNode* cell2 = GetChild(th, 0);
  ASSERT_EQ(GUMBO_NODE_TEXT, cell2->type);
  EXPECT_STREQ("Cell2", cell2->v.text.text);

  GumboNode* tr3 = GetChild(tbody2, 1);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, tr3->type);
  EXPECT_EQ(GUMBO_TAG_TR, GetTag(tr3));
  ASSERT_EQ(0, GetChildCount(tr3));
}

TEST_F(GumboParserTest, Select) {
  Parse("<select><option>One<option>Two</select><div></div>");

  GumboNode* body;
  GetAndAssertBody(root_, &body);
  ASSERT_EQ(2, GetChildCount(body));

  GumboNode* select = GetChild(body, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, select->type);
  EXPECT_EQ(GUMBO_TAG_SELECT, GetTag(select));
  ASSERT_EQ(2, GetChildCount(select));

  GumboNode* option1 = GetChild(select, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, option1->type);
  EXPECT_EQ(GUMBO_TAG_OPTION, GetTag(option1));
  ASSERT_EQ(1, GetChildCount(option1));

  GumboNode* option2 = GetChild(select, 1);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, option2->type);
  EXPECT_EQ(GUMBO_TAG_OPTION, GetTag(option2));
  ASSERT_EQ(1, GetChildCount(option2));

  GumboNode* div = GetChild(body, 1);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, div->type);
  EXPECT_EQ(GUMBO_TAG_DIV, GetTag(div));
  ASSERT_EQ(0, GetChildCount(div));
}

TEST_F(GumboParserTest, ComplicatedSelect) {
  Parse(
      "<select><div class=foo></div><optgroup><option>Option"
      "</option><input></optgroup></select>");

  GumboNode* body;
  GetAndAssertBody(root_, &body);
  ASSERT_EQ(2, GetChildCount(body));

  GumboNode* select = GetChild(body, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, select->type);
  EXPECT_EQ(GUMBO_TAG_SELECT, GetTag(select));
  ASSERT_EQ(1, GetChildCount(select));

  GumboNode* optgroup = GetChild(select, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, optgroup->type);
  EXPECT_EQ(GUMBO_TAG_OPTGROUP, GetTag(optgroup));
  ASSERT_EQ(1, GetChildCount(optgroup));

  GumboNode* option = GetChild(optgroup, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, option->type);
  EXPECT_EQ(GUMBO_TAG_OPTION, GetTag(option));
  ASSERT_EQ(1, GetChildCount(option));

  GumboNode* text = GetChild(option, 0);
  ASSERT_EQ(GUMBO_NODE_TEXT, text->type);
  EXPECT_STREQ("Option", text->v.text.text);

  GumboNode* input = GetChild(body, 1);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, input->type);
  EXPECT_EQ(GUMBO_TAG_INPUT, GetTag(input));
  ASSERT_EQ(0, GetChildCount(input));
}

TEST_F(GumboParserTest, DoubleSelect) {
  Parse("<select><select><div></div>");

  GumboNode* body;
  GetAndAssertBody(root_, &body);
  ASSERT_EQ(2, GetChildCount(body));

  GumboNode* select = GetChild(body, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, select->type);
  EXPECT_EQ(GUMBO_TAG_SELECT, GetTag(select));
  ASSERT_EQ(0, GetChildCount(select));

  GumboNode* div = GetChild(body, 1);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, div->type);
  EXPECT_EQ(GUMBO_TAG_DIV, GetTag(div));
  ASSERT_EQ(0, GetChildCount(div));
}

TEST_F(GumboParserTest, InputInSelect) {
  Parse("<select><input /><div></div>");

  GumboNode* body;
  GetAndAssertBody(root_, &body);
  ASSERT_EQ(3, GetChildCount(body));

  GumboNode* select = GetChild(body, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, select->type);
  EXPECT_EQ(GUMBO_TAG_SELECT, GetTag(select));
  ASSERT_EQ(0, GetChildCount(select));

  GumboNode* input = GetChild(body, 1);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, input->type);
  EXPECT_EQ(GUMBO_TAG_INPUT, GetTag(input));
  ASSERT_EQ(0, GetChildCount(input));

  GumboNode* div = GetChild(body, 2);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, div->type);
  EXPECT_EQ(GUMBO_TAG_DIV, GetTag(div));
  ASSERT_EQ(0, GetChildCount(div));
}

TEST_F(GumboParserTest, SelectInTable) {
  Parse("<table><td><select><option value=1></table>");

  GumboNode* body;
  GetAndAssertBody(root_, &body);
  ASSERT_EQ(1, GetChildCount(body));

  GumboNode* table = GetChild(body, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, table->type);
  EXPECT_EQ(GUMBO_TAG_TABLE, GetTag(table));
  ASSERT_EQ(1, GetChildCount(table));

  GumboNode* tbody = GetChild(table, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, tbody->type);
  EXPECT_EQ(GUMBO_TAG_TBODY, GetTag(tbody));
  ASSERT_EQ(1, GetChildCount(tbody));

  GumboNode* tr = GetChild(tbody, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, tr->type);
  EXPECT_EQ(GUMBO_TAG_TR, GetTag(tr));
  ASSERT_EQ(1, GetChildCount(tr));

  GumboNode* td = GetChild(tr, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, td->type);
  EXPECT_EQ(GUMBO_TAG_TD, GetTag(td));
  ASSERT_EQ(1, GetChildCount(td));

  GumboNode* select = GetChild(td, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, select->type);
  EXPECT_EQ(GUMBO_TAG_SELECT, GetTag(select));
  ASSERT_EQ(1, GetChildCount(select));

  GumboNode* option = GetChild(select, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, option->type);
  EXPECT_EQ(GUMBO_TAG_OPTION, GetTag(option));
  ASSERT_EQ(0, GetChildCount(option));
}

TEST_F(GumboParserTest, ImplicitColgroup) {
  Parse("<table><col /><col /></table>");

  GumboNode* body;
  GetAndAssertBody(root_, &body);
  ASSERT_EQ(1, GetChildCount(body));

  GumboNode* table = GetChild(body, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, table->type);
  EXPECT_EQ(GUMBO_TAG_TABLE, GetTag(table));
  ASSERT_EQ(1, GetChildCount(table));

  GumboNode* colgroup = GetChild(table, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, colgroup->type);
  EXPECT_EQ(GUMBO_TAG_COLGROUP, GetTag(colgroup));
  ASSERT_EQ(2, GetChildCount(colgroup));

  GumboNode* col1 = GetChild(colgroup, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, col1->type);
  EXPECT_EQ(GUMBO_TAG_COL, GetTag(col1));
  ASSERT_EQ(0, GetChildCount(col1));

  GumboNode* col2 = GetChild(colgroup, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, col2->type);
  EXPECT_EQ(GUMBO_TAG_COL, GetTag(col2));
  ASSERT_EQ(0, GetChildCount(col2));
}

TEST_F(GumboParserTest, Form) {
  Parse("<form><input type=hidden /><isindex /></form>After form");

  GumboNode* body;
  GetAndAssertBody(root_, &body);
  ASSERT_EQ(2, GetChildCount(body));

  GumboNode* form = GetChild(body, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, form->type);
  EXPECT_EQ(GUMBO_TAG_FORM, GetTag(form));
  ASSERT_EQ(1, GetChildCount(form));

  GumboNode* input = GetChild(form, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, input->type);
  EXPECT_EQ(GUMBO_TAG_INPUT, GetTag(input));
  ASSERT_EQ(0, GetChildCount(input));

  GumboNode* text = GetChild(body, 1);
  ASSERT_EQ(GUMBO_NODE_TEXT, text->type);
  EXPECT_STREQ("After form", text->v.text.text);
}

TEST_F(GumboParserTest, NestedForm) {
  Parse("<form><label>Label</label><form><input id=input2></form>After form");

  GumboNode* body;
  GetAndAssertBody(root_, &body);
  ASSERT_EQ(2, GetChildCount(body));

  GumboNode* form = GetChild(body, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, form->type);
  EXPECT_EQ(GUMBO_TAG_FORM, GetTag(form));
  ASSERT_EQ(2, GetChildCount(form));

  GumboNode* label = GetChild(form, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, label->type);
  EXPECT_EQ(GUMBO_TAG_LABEL, GetTag(label));
  ASSERT_EQ(1, GetChildCount(label));

  GumboNode* input = GetChild(form, 1);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, input->type);
  EXPECT_EQ(GUMBO_TAG_INPUT, GetTag(input));
  ASSERT_EQ(0, GetChildCount(input));

  GumboNode* text = GetChild(body, 1);
  ASSERT_EQ(GUMBO_NODE_TEXT, text->type);
  EXPECT_STREQ("After form", text->v.text.text);
}

TEST_F(GumboParserTest, MisnestedFormInTable) {
  // Parse of this is somewhat weird.  The first <form> is opened outside the
  // table, so when </form> checks to see if there's a form in scope, it stops
  // at the <table> boundary and returns null.  The form pointer is nulled out
  // anyway, though, which means that the second form (parsed in the table body
  // state) ends up creating an element.  It's immediately popped off
  // the stack, but the form element pointer remains set to that node (which is
  // not on the stack of open elements).  The final </form> tag triggers the
  // "does not have node in scope" clause and is ignored.  (Note that this is
  // different from "has a form element in scope" - the first form is still in
  // scope at that point, but the form pointer does not point to it.) Then the
  // original <form> element is closed implicitly when the table cell is closed.
  Parse(
      "<table><tr><td>"
      "<form><table><tr><td></td></tr></form>"
      "<form></tr></table></form>"
      "</td></tr></table");

  GumboNode* body;
  GetAndAssertBody(root_, &body);
  ASSERT_EQ(1, GetChildCount(body));

  GumboNode* table1 = GetChild(body, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, table1->type);
  EXPECT_EQ(GUMBO_TAG_TABLE, GetTag(table1));
  ASSERT_EQ(1, GetChildCount(table1));

  GumboNode* tbody1 = GetChild(table1, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, tbody1->type);
  EXPECT_EQ(GUMBO_TAG_TBODY, GetTag(tbody1));
  ASSERT_EQ(1, GetChildCount(tbody1));

  GumboNode* tr1 = GetChild(tbody1, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, tr1->type);
  EXPECT_EQ(GUMBO_TAG_TR, GetTag(tr1));
  ASSERT_EQ(1, GetChildCount(tr1));

  GumboNode* td1 = GetChild(tr1, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, td1->type);
  EXPECT_EQ(GUMBO_TAG_TD, GetTag(td1));
  ASSERT_EQ(1, GetChildCount(td1));

  GumboNode* form1 = GetChild(td1, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, form1->type);
  EXPECT_EQ(GUMBO_TAG_FORM, GetTag(form1));
  ASSERT_EQ(1, GetChildCount(form1));

  GumboNode* table2 = GetChild(form1, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, table2->type);
  EXPECT_EQ(GUMBO_TAG_TABLE, GetTag(table2));
  ASSERT_EQ(1, GetChildCount(table2));

  GumboNode* tbody2 = GetChild(table2, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, tbody2->type);
  EXPECT_EQ(GUMBO_TAG_TBODY, GetTag(tbody2));
  ASSERT_EQ(2, GetChildCount(tbody2));

  GumboNode* tr2 = GetChild(tbody2, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, tr2->type);
  EXPECT_EQ(GUMBO_TAG_TR, GetTag(tr2));
  ASSERT_EQ(1, GetChildCount(tr2));

  GumboNode* form2 = GetChild(tbody2, 1);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, form2->type);
  EXPECT_EQ(GUMBO_TAG_FORM, GetTag(form2));
  ASSERT_EQ(0, GetChildCount(form2));
}

TEST_F(GumboParserTest, IsIndex) {
  Parse("<isindex id=form1 action='/action' prompt='Secret Message'>");
  GumboNode* body;
  GetAndAssertBody(root_, &body);
  ASSERT_EQ(1, GetChildCount(body));

  GumboNode* form = GetChild(body, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, form->type);
  EXPECT_EQ(GUMBO_TAG_FORM, GetTag(form));
  ASSERT_EQ(3, GetChildCount(form));

  GumboAttribute* action = GetAttribute(form, 0);
  EXPECT_STREQ("action", action->name);
  EXPECT_STREQ("/action", action->value);

  GumboNode* hr1 = GetChild(form, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, hr1->type);
  EXPECT_EQ(GUMBO_TAG_HR, GetTag(hr1));
  ASSERT_EQ(0, GetChildCount(hr1));

  GumboNode* label = GetChild(form, 1);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, label->type);
  EXPECT_EQ(GUMBO_TAG_LABEL, GetTag(label));
  ASSERT_EQ(2, GetChildCount(label));

  GumboNode* text = GetChild(label, 0);
  ASSERT_EQ(GUMBO_NODE_TEXT, text->type);
  EXPECT_STREQ("Secret Message", text->v.text.text);

  GumboNode* input = GetChild(label, 1);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, input->type);
  EXPECT_EQ(GUMBO_TAG_INPUT, GetTag(input));
  ASSERT_EQ(0, GetChildCount(input));
  ASSERT_EQ(2, GetAttributeCount(input));

  GumboAttribute* id = GetAttribute(input, 0);
  EXPECT_STREQ("id", id->name);
  EXPECT_STREQ("form1", id->value);

  GumboAttribute* name = GetAttribute(input, 1);
  EXPECT_STREQ("name", name->name);
  EXPECT_STREQ("isindex", name->value);

  GumboNode* hr2 = GetChild(form, 2);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, hr2->type);
  EXPECT_EQ(GUMBO_TAG_HR, GetTag(hr2));
  ASSERT_EQ(0, GetChildCount(hr2));
}

TEST_F(GumboParserTest, IsIndexDuplicateAttribute) {
  Parse("<isindex name=foo>");

  GumboNode* body;
  GetAndAssertBody(root_, &body);
  ASSERT_EQ(1, GetChildCount(body));

  GumboNode* form = GetChild(body, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, form->type);
  EXPECT_EQ(GUMBO_TAG_FORM, GetTag(form));
  ASSERT_EQ(3, GetChildCount(form));

  GumboNode* label = GetChild(form, 1);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, label->type);
  EXPECT_EQ(GUMBO_TAG_LABEL, GetTag(label));
  ASSERT_EQ(2, GetChildCount(label));

  GumboNode* input = GetChild(label, 1);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, input->type);
  EXPECT_EQ(GUMBO_TAG_INPUT, GetTag(input));
  ASSERT_EQ(0, GetChildCount(input));
  ASSERT_EQ(1, GetAttributeCount(input));

  GumboAttribute* name = GetAttribute(input, 0);
  EXPECT_STREQ("name", name->name);
  EXPECT_STREQ("isindex", name->value);
}

TEST_F(GumboParserTest, NestedRawtextTags) {
  Parse(
      "<noscript><noscript jstag=false>"
      "<style>div{text-align:center}</style></noscript>");

  GumboNode* html = GetChild(root_, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, html->type);
  EXPECT_EQ(GUMBO_TAG_HTML, GetTag(html));
  EXPECT_EQ(GUMBO_INSERTION_BY_PARSER | GUMBO_INSERTION_IMPLICIT_END_TAG |
                GUMBO_INSERTION_IMPLIED,
      html->parse_flags);
  ASSERT_EQ(2, GetChildCount(html));

  GumboNode* head = GetChild(html, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, head->type);
  EXPECT_EQ(GUMBO_TAG_HEAD, GetTag(head));
  EXPECT_EQ(GUMBO_INSERTION_BY_PARSER | GUMBO_INSERTION_IMPLICIT_END_TAG |
                GUMBO_INSERTION_IMPLIED,
      head->parse_flags);
  ASSERT_EQ(1, GetChildCount(head));

  GumboNode* noscript = GetChild(head, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, noscript->type);
  EXPECT_EQ(GUMBO_TAG_NOSCRIPT, GetTag(noscript));
  ASSERT_EQ(1, GetChildCount(noscript));

  GumboNode* style = GetChild(noscript, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, style->type);
  EXPECT_EQ(GUMBO_TAG_STYLE, GetTag(style));
  ASSERT_EQ(1, GetChildCount(style));

  GumboNode* text = GetChild(style, 0);
  ASSERT_EQ(GUMBO_NODE_TEXT, text->type);
  EXPECT_STREQ("div{text-align:center}", text->v.text.text);
}

TEST_F(GumboParserTest, RawtextInBody) {
  Parse("<body><noembed jsif=false></noembed>");

  GumboNode* body;
  GetAndAssertBody(root_, &body);
  ASSERT_EQ(1, GetChildCount(body));

  GumboNode* noembed = GetChild(body, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, noembed->type);
  EXPECT_EQ(GUMBO_TAG_NOEMBED, GetTag(noembed));
  EXPECT_EQ(1, GetAttributeCount(noembed));
}

TEST_F(GumboParserTest, MetaBeforeHead) {
  Parse(
      "<html><meta http-equiv='content-type' "
      "content='text/html; charset=UTF-8' /><head></head>");

  GumboNode* body;
  GetAndAssertBody(root_, &body);
  // Testing for a memory leak here, but
  // TODO(jdtang): Flesh out structural asserts.
}

TEST_F(GumboParserTest, NoahsArkClause) {
  Parse(
      "<p><font size=4><font color=red><font size=4><font size=4>"
      "<font size=4><font size=4><font size=4><font color=red><p>X");

  GumboNode* body;
  GetAndAssertBody(root_, &body);
  ASSERT_EQ(2, GetChildCount(body));

  GumboNode* p1 = GetChild(body, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, p1->type);
  EXPECT_EQ(GUMBO_TAG_P, p1->v.element.tag);
  ASSERT_EQ(1, GetChildCount(p1));

  GumboNode* size1 = GetChild(p1, 0);
  GumboNode* red1 = GetChild(size1, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, red1->type);
  EXPECT_EQ(GUMBO_TAG_FONT, red1->v.element.tag);
  ASSERT_EQ(1, GetAttributeCount(red1));
  GumboAttribute* red1_attr = GetAttribute(red1, 0);
  EXPECT_STREQ("color", red1_attr->name);
  EXPECT_STREQ("red", red1_attr->value);
  ASSERT_EQ(1, GetChildCount(red1));

  GumboNode* p2 = GetChild(body, 1);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, p2->type);
  EXPECT_EQ(GUMBO_TAG_P, p2->v.element.tag);
  ASSERT_EQ(1, GetChildCount(p2));

  GumboNode* red2 = GetChild(p2, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, red2->type);
  EXPECT_EQ(GUMBO_TAG_FONT, red2->v.element.tag);
  ASSERT_EQ(1, GetAttributeCount(red2));
  GumboAttribute* red2_attr = GetAttribute(red2, 0);
  EXPECT_STREQ("color", red2_attr->name);
  EXPECT_STREQ("red", red2_attr->value);
  ASSERT_EQ(1, GetChildCount(red2));
}

TEST_F(GumboParserTest, AdoptionAgency1) {
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/the-end.html#misnested-tags:-b-i-/b-/i
  Parse("<p>1<b>2<i>3</b>4</i>5</p>");
  ASSERT_EQ(1, GetChildCount(root_));

  GumboNode* html = GetChild(root_, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, html->type);
  EXPECT_EQ(GUMBO_INSERTION_BY_PARSER | GUMBO_INSERTION_IMPLICIT_END_TAG |
                GUMBO_INSERTION_IMPLIED,
      html->parse_flags);
  EXPECT_EQ(GUMBO_TAG_HTML, html->v.element.tag);
  ASSERT_EQ(2, GetChildCount(html));

  GumboNode* body = GetChild(html, 1);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, body->type);
  EXPECT_EQ(GUMBO_INSERTION_BY_PARSER | GUMBO_INSERTION_IMPLICIT_END_TAG |
                GUMBO_INSERTION_IMPLIED,
      body->parse_flags);
  EXPECT_EQ(GUMBO_TAG_BODY, body->v.element.tag);
  ASSERT_EQ(1, GetChildCount(body));

  GumboNode* p = GetChild(body, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, p->type);
  EXPECT_EQ(GUMBO_INSERTION_NORMAL, p->parse_flags);
  EXPECT_EQ(GUMBO_TAG_P, p->v.element.tag);
  ASSERT_EQ(4, GetChildCount(p));

  GumboNode* text1 = GetChild(p, 0);
  ASSERT_EQ(GUMBO_NODE_TEXT, text1->type);
  EXPECT_EQ(GUMBO_INSERTION_NORMAL, text1->parse_flags);
  EXPECT_STREQ("1", text1->v.text.text);

  GumboNode* b = GetChild(p, 1);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, b->type);
  EXPECT_EQ(GUMBO_INSERTION_NORMAL, b->parse_flags);
  EXPECT_EQ(GUMBO_TAG_B, b->v.element.tag);
  ASSERT_EQ(2, GetChildCount(b));

  GumboNode* text2 = GetChild(b, 0);
  ASSERT_EQ(GUMBO_NODE_TEXT, text2->type);
  EXPECT_EQ(GUMBO_INSERTION_NORMAL, text2->parse_flags);
  EXPECT_STREQ("2", text2->v.text.text);

  GumboNode* i = GetChild(b, 1);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, i->type);
  EXPECT_EQ(GUMBO_INSERTION_IMPLICIT_END_TAG, i->parse_flags);
  EXPECT_EQ(GUMBO_TAG_I, i->v.element.tag);
  ASSERT_EQ(1, GetChildCount(i));

  GumboNode* text3 = GetChild(i, 0);
  ASSERT_EQ(GUMBO_NODE_TEXT, text3->type);
  EXPECT_EQ(GUMBO_INSERTION_NORMAL, text2->parse_flags);
  EXPECT_STREQ("3", text3->v.text.text);

  GumboNode* i2 = GetChild(p, 2);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, i2->type);
  EXPECT_EQ(GUMBO_INSERTION_BY_PARSER |
                GUMBO_INSERTION_RECONSTRUCTED_FORMATTING_ELEMENT,
      i2->parse_flags);
  EXPECT_EQ(GUMBO_TAG_I, i2->v.element.tag);
  ASSERT_EQ(1, GetChildCount(i2));

  GumboNode* text4 = GetChild(i2, 0);
  ASSERT_EQ(GUMBO_NODE_TEXT, text4->type);
  EXPECT_EQ(GUMBO_INSERTION_NORMAL, text2->parse_flags);
  EXPECT_STREQ("4", text4->v.text.text);

  GumboNode* text5 = GetChild(p, 3);
  ASSERT_EQ(GUMBO_NODE_TEXT, text5->type);
  EXPECT_EQ(GUMBO_INSERTION_NORMAL, text2->parse_flags);
  EXPECT_STREQ("5", text5->v.text.text);
}

TEST_F(GumboParserTest, AdoptionAgency2) {
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/the-end.html#misnested-tags:-b-p-/b-/p
  Parse("<b>1<p>2</b>3</p>");
  ASSERT_EQ(1, GetChildCount(root_));

  GumboNode* html = GetChild(root_, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, html->type);
  EXPECT_EQ(GUMBO_INSERTION_BY_PARSER | GUMBO_INSERTION_IMPLICIT_END_TAG |
                GUMBO_INSERTION_IMPLIED,
      html->parse_flags);
  EXPECT_EQ(GUMBO_TAG_HTML, html->v.element.tag);
  ASSERT_EQ(2, GetChildCount(html));

  GumboNode* body = GetChild(html, 1);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, body->type);
  EXPECT_EQ(GUMBO_INSERTION_BY_PARSER | GUMBO_INSERTION_IMPLICIT_END_TAG |
                GUMBO_INSERTION_IMPLIED,
      body->parse_flags);
  EXPECT_EQ(GUMBO_TAG_BODY, body->v.element.tag);
  ASSERT_EQ(2, GetChildCount(body));

  GumboNode* b = GetChild(body, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, b->type);
  EXPECT_EQ(GUMBO_INSERTION_IMPLICIT_END_TAG, b->parse_flags);
  EXPECT_EQ(GUMBO_TAG_B, b->v.element.tag);
  ASSERT_EQ(1, GetChildCount(b));

  GumboNode* text1 = GetChild(b, 0);
  ASSERT_EQ(GUMBO_NODE_TEXT, text1->type);
  EXPECT_EQ(GUMBO_INSERTION_NORMAL, text1->parse_flags);
  EXPECT_STREQ("1", text1->v.text.text);

  GumboNode* p = GetChild(body, 1);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, p->type);
  EXPECT_EQ(GUMBO_INSERTION_ADOPTION_AGENCY_MOVED, p->parse_flags);
  EXPECT_EQ(GUMBO_TAG_P, p->v.element.tag);
  ASSERT_EQ(2, GetChildCount(p));

  GumboNode* b2 = GetChild(p, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, b2->type);
  EXPECT_EQ(GUMBO_INSERTION_ADOPTION_AGENCY_CLONED | GUMBO_INSERTION_BY_PARSER,
      b2->parse_flags);
  EXPECT_EQ(GUMBO_TAG_B, b2->v.element.tag);
  ASSERT_EQ(1, GetChildCount(b2));

  GumboNode* text2 = GetChild(b2, 0);
  ASSERT_EQ(GUMBO_NODE_TEXT, text2->type);
  EXPECT_EQ(GUMBO_INSERTION_NORMAL, text2->parse_flags);
  EXPECT_STREQ("2", text2->v.text.text);

  GumboNode* text3 = GetChild(p, 1);
  ASSERT_EQ(GUMBO_NODE_TEXT, text3->type);
  EXPECT_EQ(GUMBO_INSERTION_NORMAL, text2->parse_flags);
  EXPECT_STREQ("3", text3->v.text.text);
}

TEST_F(GumboParserTest, AdoptionAgency3) {
  Parse("<div><a><b><u><i><code><div></a>");
}

TEST_F(GumboParserTest, ImplicitlyCloseLists) {
  Parse(
      "<ul>\n"
      "  <li>First\n"
      "  <li>Second\n"
      "</ul>");

  GumboNode* body;
  GetAndAssertBody(root_, &body);
  ASSERT_EQ(1, GetChildCount(body));

  GumboNode* ul = GetChild(body, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, ul->type);
  EXPECT_EQ(GUMBO_TAG_UL, GetTag(ul));
  ASSERT_EQ(3, GetChildCount(ul));

  GumboNode* text = GetChild(ul, 0);
  ASSERT_EQ(GUMBO_NODE_WHITESPACE, text->type);
  EXPECT_EQ(GUMBO_INSERTION_NORMAL, text->parse_flags);
  EXPECT_STREQ("\n  ", text->v.text.text);

  GumboNode* li1 = GetChild(ul, 1);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, li1->type);
  EXPECT_EQ(GUMBO_TAG_LI, GetTag(li1));
  ASSERT_EQ(1, GetChildCount(li1));

  GumboNode* li2 = GetChild(ul, 2);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, li2->type);
  EXPECT_EQ(GUMBO_TAG_LI, GetTag(li2));
  ASSERT_EQ(1, GetChildCount(li2));
}

TEST_F(GumboParserTest, CData) {
  Parse("<svg><![CDATA[this is text]]></svg>");

  GumboNode* body;
  GetAndAssertBody(root_, &body);
  ASSERT_EQ(1, GetChildCount(body));

  GumboNode* svg = GetChild(body, 0);
  ASSERT_EQ(1, GetChildCount(svg));

  GumboNode* cdata = GetChild(svg, 0);
  ASSERT_EQ(GUMBO_NODE_CDATA, cdata->type);
  EXPECT_STREQ("this is text", cdata->v.text.text);
}

TEST_F(GumboParserTest, CDataUnsafe) {
  // Can't use Parse() because of the strlen
  output_ =
      gumbo_parse_with_options(&options_, "<svg><![CDATA[\0filler\0text\0]]>",
          sizeof("<svg><![CDATA[\0filler\0text\0]]>") - 1);
  root_ = output_->document;

  GumboNode* body;
  GetAndAssertBody(root_, &body);
  ASSERT_EQ(1, GetChildCount(body));

  GumboNode* svg = GetChild(body, 0);
  ASSERT_EQ(1, GetChildCount(svg));

  GumboNode* cdata = GetChild(svg, 0);
  ASSERT_EQ(GUMBO_NODE_CDATA, cdata->type);
  // \xEF\xBF\xBD = unicode replacement char
  EXPECT_STREQ(
      "\xEF\xBF\xBD"
      "filler\xEF\xBF\xBD"
      "text\xEF\xBF\xBD",
      cdata->v.text.text);
}

TEST_F(GumboParserTest, CDataInBody) {
  Parse("<div><![CDATA[this is text]]></div>");

  GumboNode* body;
  GetAndAssertBody(root_, &body);
  ASSERT_EQ(1, GetChildCount(body));

  GumboNode* div = GetChild(body, 0);
  ASSERT_EQ(1, GetChildCount(div));

  GumboNode* cdata = GetChild(div, 0);
  ASSERT_EQ(GUMBO_NODE_COMMENT, cdata->type);
  EXPECT_STREQ("[CDATA[this is text]]", cdata->v.text.text);
}

TEST_F(GumboParserTest, FormattingTagsInHeading) {
  Parse("<h2>This is <b>old</h2>text");

  GumboNode* body;
  GetAndAssertBody(root_, &body);
  ASSERT_EQ(2, GetChildCount(body));

  GumboNode* h2 = GetChild(body, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, h2->type);
  EXPECT_EQ(GUMBO_TAG_H2, GetTag(h2));
  ASSERT_EQ(2, GetChildCount(h2));

  GumboNode* text1 = GetChild(h2, 0);
  ASSERT_EQ(GUMBO_NODE_TEXT, text1->type);
  EXPECT_EQ(GUMBO_INSERTION_NORMAL, text1->parse_flags);
  EXPECT_STREQ("This is ", text1->v.text.text);

  GumboNode* b = GetChild(h2, 1);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, b->type);
  EXPECT_EQ(GUMBO_TAG_B, GetTag(b));
  EXPECT_EQ(GUMBO_INSERTION_IMPLICIT_END_TAG, b->parse_flags);
  ASSERT_EQ(1, GetChildCount(b));

  GumboNode* text2 = GetChild(b, 0);
  ASSERT_EQ(GUMBO_NODE_TEXT, text2->type);
  EXPECT_EQ(GUMBO_INSERTION_NORMAL, text2->parse_flags);
  EXPECT_STREQ("old", text2->v.text.text);

  GumboNode* b2 = GetChild(body, 1);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, b2->type);
  EXPECT_EQ(GUMBO_TAG_B, GetTag(b2));
  EXPECT_EQ(GUMBO_INSERTION_IMPLICIT_END_TAG | GUMBO_INSERTION_BY_PARSER |
                GUMBO_INSERTION_RECONSTRUCTED_FORMATTING_ELEMENT,
      b2->parse_flags);
  ASSERT_EQ(1, GetChildCount(b2));

  GumboNode* text3 = GetChild(b2, 0);
  ASSERT_EQ(GUMBO_NODE_TEXT, text3->type);
  EXPECT_EQ(GUMBO_INSERTION_NORMAL, text3->parse_flags);
  EXPECT_STREQ("text", text3->v.text.text);
}

TEST_F(GumboParserTest, ExtraReconstruction) {
  Parse("<span><b></span></p>");

  GumboNode* body;
  GetAndAssertBody(root_, &body);
  ASSERT_EQ(2, GetChildCount(body));

  EXPECT_EQ(GUMBO_TAG_SPAN, GetTag(GetChild(body, 0)));
  EXPECT_EQ(GUMBO_TAG_P, GetTag(GetChild(body, 1)));
}

TEST_F(GumboParserTest, LinkifiedHeading) {
  Parse("<li><h3><a href=#foo>Text</a></h3><div>Summary</div>");

  GumboNode* body;
  GetAndAssertBody(root_, &body);
  ASSERT_EQ(1, GetChildCount(body));

  GumboNode* li = GetChild(body, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, li->type);
  EXPECT_EQ(GUMBO_TAG_LI, GetTag(li));
  ASSERT_EQ(2, GetChildCount(li));

  GumboNode* h3 = GetChild(li, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, h3->type);
  EXPECT_EQ(GUMBO_TAG_H3, GetTag(h3));
  ASSERT_EQ(1, GetChildCount(h3));

  GumboNode* anchor = GetChild(h3, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, anchor->type);
  EXPECT_EQ(GUMBO_TAG_A, GetTag(anchor));
  ASSERT_EQ(1, GetChildCount(anchor));

  GumboNode* div = GetChild(li, 1);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, div->type);
  EXPECT_EQ(GUMBO_TAG_DIV, GetTag(div));
  ASSERT_EQ(1, GetChildCount(div));
}

TEST_F(GumboParserTest, MisnestedHeading) {
  Parse(
      "<h1>"
      "  <section>"
      "    <h2>"
      "      <dl><dt>List"
      "    </h1>"
      "  </section>"
      "  Heading1"
      "<h3>Heading3</h4>"
      "After</h3> text");
  // The parse of this is pretty weird: according to the spec, it should be:
  // <html>
  //   <head></head>
  //   <body>
  //     <h1>
  //       <section>
  //         <h2><dl><dt>List</dt></dl></h2>
  //       </section>
  //       Heading1
  //     </h1>
  //     <h3>Heading3</h3>
  //     After text
  //   </body>
  // </html>
  // Explanation:
  // <html>, <head>, and <body> tags are implied.  The opening <h1> and <section
  // tags function as expected.  Because the current node is <section>, the <h2>
  // does *not* close the existing <h1>, and then we enter a definition list.
  // The closing </h1>, even though it's misnested, causes the <dt> to be closed
  // implicitly, then also closes the <dl> and <h2> as a parse error.  <h1> is
  // still open, and so "Heading1" goes into it.  Because the current node is a
  // heading tag, <h3> closes it (as a parse error) and reopens a new <h3> node,
  // which is closed by the </h4> tag.  The remaining text goes straight into
  // the <body>; since no heading is open, the </h3> tag is ignored and the
  // second run is condensed into the first.
  // TODO(jdtang): Make sure that parse_flags are set appropriately for this.
  GumboNode* body;
  GetAndAssertBody(root_, &body);
  ASSERT_EQ(3, GetChildCount(body));

  GumboNode* h1 = GetChild(body, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, h1->type);
  EXPECT_EQ(GUMBO_TAG_H1, GetTag(h1));
  ASSERT_EQ(3, GetChildCount(h1));
  // Child 1 is whitespace, as it is for many of these nodes.

  GumboNode* section = GetChild(h1, 1);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, section->type);
  EXPECT_EQ(GUMBO_TAG_SECTION, GetTag(section));
  ASSERT_EQ(3, GetChildCount(section));

  GumboNode* h2 = GetChild(section, 1);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, h2->type);
  EXPECT_EQ(GUMBO_TAG_H2, GetTag(h2));
  ASSERT_EQ(2, GetChildCount(h2));

  GumboNode* dl = GetChild(h2, 1);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, dl->type);
  EXPECT_EQ(GUMBO_TAG_DL, GetTag(dl));
  ASSERT_EQ(1, GetChildCount(dl));

  GumboNode* dt = GetChild(dl, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, dt->type);
  EXPECT_EQ(GUMBO_TAG_DT, GetTag(dt));
  ASSERT_EQ(1, GetChildCount(dt));

  GumboNode* text1 = GetChild(dt, 0);
  ASSERT_EQ(GUMBO_NODE_TEXT, text1->type);
  EXPECT_EQ(GUMBO_INSERTION_NORMAL, text1->parse_flags);
  EXPECT_STREQ("List    ", text1->v.text.text);

  GumboNode* text2 = GetChild(h1, 2);
  ASSERT_EQ(GUMBO_NODE_TEXT, text2->type);
  EXPECT_EQ(GUMBO_INSERTION_NORMAL, text2->parse_flags);
  EXPECT_STREQ("  Heading1", text2->v.text.text);

  GumboNode* h3 = GetChild(body, 1);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, h3->type);
  EXPECT_EQ(GUMBO_TAG_H3, GetTag(h3));
  EXPECT_EQ(1, GetChildCount(h3));

  GumboNode* text3 = GetChild(h3, 0);
  ASSERT_EQ(GUMBO_NODE_TEXT, text3->type);
  EXPECT_EQ(GUMBO_INSERTION_NORMAL, text3->parse_flags);
  EXPECT_STREQ("Heading3", text3->v.text.text);

  GumboNode* text4 = GetChild(body, 2);
  ASSERT_EQ(GUMBO_NODE_TEXT, text4->type);
  EXPECT_EQ(GUMBO_INSERTION_NORMAL, text4->parse_flags);
  EXPECT_STREQ("After text", text4->v.text.text);
}

TEST_F(GumboParserTest, DoubleBody) {
  Parse("<body class=first><body class=second id=merged>Text</body></body>");

  GumboNode* body;
  GetAndAssertBody(root_, &body);
  ASSERT_EQ(1, GetChildCount(body));
  ASSERT_EQ(2, GetAttributeCount(body));

  GumboAttribute* clas = GetAttribute(body, 0);
  EXPECT_STREQ("class", clas->name);
  EXPECT_STREQ("first", clas->value);

  GumboAttribute* id = GetAttribute(body, 1);
  EXPECT_STREQ("id", id->name);
  EXPECT_STREQ("merged", id->value);

  GumboNode* text = GetChild(body, 0);
  ASSERT_EQ(GUMBO_NODE_TEXT, text->type);
  EXPECT_EQ(GUMBO_INSERTION_NORMAL, text->parse_flags);
  EXPECT_STREQ("Text", text->v.text.text);
}

TEST_F(GumboParserTest, ThInMathMl) {
  Parse("<math><th><mI><table></table><tr></table><div><tr>0");
  GumboNode* body;
  GetAndAssertBody(root_, &body);
  ASSERT_EQ(1, GetChildCount(body));

  GumboNode* math = GetChild(body, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, math->type);
  EXPECT_EQ(GUMBO_TAG_MATH, math->v.element.tag);
  EXPECT_EQ(GUMBO_NAMESPACE_MATHML, math->v.element.tag_namespace);
  ASSERT_EQ(1, GetChildCount(math));

  GumboNode* th = GetChild(math, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, th->type);
  EXPECT_EQ(GUMBO_TAG_TH, th->v.element.tag);
  EXPECT_EQ(GUMBO_NAMESPACE_MATHML, th->v.element.tag_namespace);
  ASSERT_EQ(1, GetChildCount(th));

  GumboNode* mi = GetChild(th, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, mi->type);
  EXPECT_EQ(GUMBO_TAG_MI, mi->v.element.tag);
  EXPECT_EQ(GUMBO_NAMESPACE_MATHML, mi->v.element.tag_namespace);
  ASSERT_EQ(2, GetChildCount(mi));

  GumboNode* table = GetChild(mi, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, table->type);
  EXPECT_EQ(GUMBO_TAG_TABLE, table->v.element.tag);
  EXPECT_EQ(GUMBO_NAMESPACE_HTML, table->v.element.tag_namespace);
  ASSERT_EQ(0, GetChildCount(table));

  GumboNode* div = GetChild(mi, 1);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, div->type);
  EXPECT_EQ(GUMBO_TAG_DIV, div->v.element.tag);
  EXPECT_EQ(GUMBO_NAMESPACE_HTML, div->v.element.tag_namespace);
  ASSERT_EQ(1, GetChildCount(div));

  GumboNode* text = GetChild(div, 0);
  ASSERT_EQ(GUMBO_NODE_TEXT, text->type);
  EXPECT_STREQ("0", text->v.text.text);
}

TEST_F(GumboParserTest, TdInMathml) {
  Parse("<table><th><math><td></tr>");
  GumboNode* body;
  GetAndAssertBody(root_, &body);
  ASSERT_EQ(1, GetChildCount(body));

  GumboNode* table = GetChild(body, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, table->type);
  EXPECT_EQ(GUMBO_TAG_TABLE, table->v.element.tag);
  EXPECT_EQ(GUMBO_NAMESPACE_HTML, table->v.element.tag_namespace);
  ASSERT_EQ(1, GetChildCount(table));

  GumboNode* tbody = GetChild(table, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, tbody->type);
  EXPECT_EQ(GUMBO_TAG_TBODY, tbody->v.element.tag);
  EXPECT_EQ(GUMBO_NAMESPACE_HTML, tbody->v.element.tag_namespace);
  ASSERT_EQ(1, GetChildCount(tbody));

  GumboNode* tr = GetChild(tbody, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, tr->type);
  EXPECT_EQ(GUMBO_TAG_TR, tr->v.element.tag);
  EXPECT_EQ(GUMBO_NAMESPACE_HTML, tr->v.element.tag_namespace);
  ASSERT_EQ(1, GetChildCount(tr));

  GumboNode* th = GetChild(tr, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, th->type);
  EXPECT_EQ(GUMBO_TAG_TH, th->v.element.tag);
  EXPECT_EQ(GUMBO_NAMESPACE_HTML, th->v.element.tag_namespace);
  ASSERT_EQ(1, GetChildCount(th));

  GumboNode* math = GetChild(th, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, math->type);
  EXPECT_EQ(GUMBO_TAG_MATH, math->v.element.tag);
  EXPECT_EQ(GUMBO_NAMESPACE_MATHML, math->v.element.tag_namespace);
  ASSERT_EQ(1, GetChildCount(math));

  GumboNode* td = GetChild(math, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, td->type);
  EXPECT_EQ(GUMBO_TAG_TD, td->v.element.tag);
  EXPECT_EQ(GUMBO_NAMESPACE_MATHML, td->v.element.tag_namespace);
  ASSERT_EQ(0, GetChildCount(td));
}

TEST_F(GumboParserTest, SelectInForeignContent) {
  Parse("<svg><select><foreignobject><select><select><select>");
}

TEST_F(GumboParserTest, TemplateInForeignContent) {
  Parse("<template><svg><template>");

  GumboNode* body;
  GetAndAssertBody(root_, &body);
  EXPECT_EQ(0, GetChildCount(body));

  GumboNode* html = GetChild(root_, 0);
  ASSERT_EQ(2, GetChildCount(html));

  GumboNode* head = GetChild(html, 0);
  ASSERT_EQ(1, GetChildCount(head));

  GumboNode* template_node = GetChild(head, 0);
  ASSERT_EQ(GUMBO_NODE_TEMPLATE, template_node->type);
  EXPECT_EQ(GUMBO_TAG_TEMPLATE, template_node->v.element.tag);
  ASSERT_EQ(1, GetChildCount(template_node));

  GumboNode* svg_node = GetChild(template_node, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, svg_node->type);
  EXPECT_EQ(GUMBO_TAG_SVG, svg_node->v.element.tag);
  EXPECT_EQ(GUMBO_NAMESPACE_SVG, svg_node->v.element.tag_namespace);
  ASSERT_EQ(1, GetChildCount(svg_node));

  GumboNode* svg_template = GetChild(svg_node, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, svg_template->type);
  EXPECT_EQ(GUMBO_TAG_TEMPLATE, svg_template->v.element.tag);
  EXPECT_EQ(GUMBO_NAMESPACE_SVG, svg_template->v.element.tag_namespace);
  EXPECT_EQ(0, GetChildCount(svg_template));
}

TEST_F(GumboParserTest, TemplateNull) {
  output_ = gumbo_parse_with_options(
      &options_, "<template>\0", sizeof("<template>\0") - 1);
  root_ = output_->document;

  GumboNode* body;
  GetAndAssertBody(root_, &body);
  EXPECT_EQ(0, GetChildCount(body));

  GumboNode* html = GetChild(root_, 0);
  ASSERT_EQ(2, GetChildCount(html));

  GumboNode* head = GetChild(html, 0);
  ASSERT_EQ(1, GetChildCount(head));

  GumboNode* template_node = GetChild(head, 0);
  ASSERT_EQ(GUMBO_NODE_TEMPLATE, template_node->type);
  EXPECT_EQ(GUMBO_TAG_TEMPLATE, template_node->v.element.tag);
  ASSERT_EQ(0, GetChildCount(template_node));
}

TEST_F(GumboParserTest, FragmentWithNamespace) {
  ParseFragment("<div></div>", GUMBO_TAG_TITLE, GUMBO_NAMESPACE_SVG);

  EXPECT_EQ(1, GetChildCount(root_));
  GumboNode* html = GetChild(root_, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, html->type);
  EXPECT_EQ(GUMBO_TAG_HTML, html->v.element.tag);
  EXPECT_EQ(1, GetChildCount(html));

  GumboNode* div = GetChild(html, 0);
  ASSERT_EQ(GUMBO_NODE_ELEMENT, div->type);
  EXPECT_EQ(GUMBO_TAG_DIV, div->v.element.tag);
  EXPECT_EQ(0, GetChildCount(div));
}

}  // namespace
