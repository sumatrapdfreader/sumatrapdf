/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/HtmlParserLookup.h"
#include "utils/HtmlPullParser.h"

// must be last due to assert() over-write
#include "utils/UtAssert.h"

static void Test00(Str s, HtmlToken::TokenType expectedType) {
    HtmlPullParser parser{s};
    HtmlToken* t = parser.Next();
    utassert(t->type == expectedType);
    utassert(t->NameIs("p"));
    utassert(Tag_P == t->tag);
    AttrInfo* a = t->GetAttrByName("a1");
    utassert(a->NameIs("a1"));
    utassert(a->ValIs(">"));

    a = t->GetAttrByName("foo");
    utassert(a->NameIs("foo"));
    utassert(a->ValIs("bar"));

    a = t->GetAttrByName("nope");
    utassert(!a);

    t = parser.Next();
    utassert(!t);
}

static void HtmlEntities() {
    struct {
        Str s;
        int rune;
    } entities[] = {{"&Uuml;", 220}, {"&uuml;", 252},     {"&times;", 215}, {"&AElig;", 198},    {"&zwnj;", 8204},
                    {"&#58;", 58},   {"&#32783;", 32783}, {"&#x20;", 32},   {"&#xAf34;", 44852}, {"&Auml;", 196},
                    {"&a3;", -1},    {"&#xz312;", -1},    {"&aer;", -1}};
    for (size_t i = 0; i < dimof(entities); i++) {
        Str s = entities[i].s;
        int got;
        Str entEnd = ResolveHtmlEntity(Str(s.s + 1, len(s) - 1), got);
        utassert(got == entities[i].rune);
        utassert((-1 == got) == str::IsNull(entEnd));
    }
    Arena* ta = GetTempArena();
    Str unchanged[] = {"foo", "", " as;d "};
    for (size_t i = 0; i < dimof(unchanged); i++) {
        Str s = unchanged[i];
        Str res = ResolveHtmlEntities(s, ta);
        utassert(res.s == s.s);
    }

    struct {
        Str s;
        Str res;
    } changed[] = {
        // implementation detail: if there is '&' in the string
        // we always allocate, even if it isn't a valid entity
        {"a&12", "a&12"},
        {"a&x#30", "a&x#30"},

        {"&#32;b", " b"},
        {"&#x20;ra", " ra"},
        {"&lt;", "<"},
        {"a&amp; &#32;to&#x20;end", "a&  to end"},
        {"&nbsp test&auml ;&ouml;&#64&#x50go", "\xC2\xA0 test\xC3\xA4 ;\xC3\xB6@Pgo"},
    };
    for (size_t i = 0; i < dimof(changed); i++) {
        Str s = changed[i].s;
        Str res = ResolveHtmlEntities(Str(s), ta);
        utassert(str::Eq(res, changed[i].res));
    }
}

static void Test01() {
    utassert(IsInlineTag(Tag_A));
    utassert(IsInlineTag(Tag_U));
    utassert(IsInlineTag(Tag_Span));
    utassert(!IsInlineTag(Tag_P));
    utassert(IsTagSelfClosing(Tag_Area));
    utassert(IsTagSelfClosing(Tag_Link));
    utassert(IsTagSelfClosing(Tag_Param));
    utassert(!IsTagSelfClosing(Tag_P));
}

static void Test02() {
    Str s = "<p>Last paragraph";
    HtmlPullParser parser{s};
    HtmlToken* t = parser.Next();
    utassert(t && t->IsTag() && t->IsStartTag() && Tag_P == t->tag);
    t = parser.Next();
    utassert(t && t->IsText() && str::EqI(t->s, "Last paragraph"));
}

static void Test03() {
    Str s = "a < b > c <> d <";
    HtmlPullParser parser{s};
    HtmlToken* t = parser.Next();
    utassert(t && t->IsText() && str::EqI(t->s, "a "));
    t = parser.Next();
    utassert(t && t->IsText() && str::EqI(t->s, "< b > c "));
    t = parser.Next();
    utassert(t && t->IsText() && str::EqI(t->s, "<> d "));
    t = parser.Next();
    utassert(t && t->IsError() && HtmlToken::UnclosedTag == t->error);
    t = parser.Next();
    utassert(!t);
}

void HtmlPullParser_UnitTests() {
    Test00("<p a1='>' foo=bar />", HtmlToken::EmptyElementTag);
    Test00("<p a1 ='>'     foo=\"bar\"/>", HtmlToken::EmptyElementTag);
    Test00("<p a1=  '>' foo=bar>", HtmlToken::StartTag);
    Test00("</><!-- < skip > --><p a1=\">\" foo=bar>", HtmlToken::StartTag);
    Test00("<P A1='>' FOO=bar />", HtmlToken::EmptyElementTag);
    HtmlEntities();
    Test01();
    Test02();
    Test03();
}
