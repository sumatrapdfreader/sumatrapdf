/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// note: this is only meant to be #included from HtmlPullParser.cpp,
// not compiled on its own

#include <assert.h>

namespace unittests {

static void Test00(const char *s, HtmlToken::TokenType expectedType) {
    HtmlPullParser parser(s, str::Len(s));
    HtmlToken *t = parser.Next();
    assert(t->type == expectedType);
    assert(t->NameIs("p"));
    assert(Tag_P == t->tag);
    AttrInfo *a = t->GetAttrByName("a1");
    assert(a->NameIs("a1"));
    assert(a->ValIs(">"));

    a = t->GetAttrByName("foo");
    assert(a->NameIs("foo"));
    assert(a->ValIs("bar"));

    a = t->GetAttrByName("nope");
    assert(!a);

    a = t->GetAttrByValue(">");
    assert(a->NameIs("a1"));
    a = t->GetAttrByValue("bar");
    assert(a->NameIs("foo"));

    t = parser.Next();
    assert(!t);
}

static void HtmlEntities()
{
    struct {
        const char *s; int rune;
    } entities[] = {
        { "&Uuml;", 220 },
        { "&uuml;", 252 },
        { "&times;", 215 },
        { "&AElig;", 198 },
        { "&zwnj;", 8204 },
        { "&#58;", 58 },
        { "&#32783;", 32783 },
        { "&#x20;", 32 },
        { "&#xAf34;", 44852 },
        { "&Auml;", 196 },
        { "&a3;", -1 },
        { "&#xz312;", -1 },
        { "&aer;", -1 }
    };
    for (size_t i = 0; i < dimof(entities); i++ ) {
        const char *s = entities[i].s;
        int got;
        const char *entEnd = ResolveHtmlEntity(s + 1, str::Len(s) - 1, got);
        assert(got == entities[i].rune);
        assert((-1 == got) == !entEnd);
    }
    const char *unchanged[] = {
        "foo", "", " as;d "
    };
    for (size_t i = 0; i < dimof(unchanged); i++) {
        const char *s = unchanged[i];
        const char *res = ResolveHtmlEntities(s, s + str::Len(s), NULL);
        assert(res == s);
    }

    struct {
        const char *s; const char *res;
    } changed[] = {
        // implementation detail: if there is '&' in the string
        // we always allocate, even if it isn't a valid entity
        { "a&12", "a&12" },
        { "a&x#30", "a&x#30" },

        { "&#32;b", " b" },
        { "&#x20;ra", " ra" },
        { "&lt;", "<" },
        { "a&amp; &#32;to&#x20;end", "a&  to end" },
        { "&nbsp test&auml ;&ouml;&#64&#x50go", "\xC2\xA0 test\xC3\xA4 ;\xC3\xB6@Pgo" },
    };
    for (size_t i = 0; i < dimof(changed); i++) {
        const char *s = changed[i].s;
        const char *res = ResolveHtmlEntities(s, s + str::Len(s), NULL);
        assert(str::Eq(res, changed[i].res));
        free((void*)res);
    }
}

static void Test01()
{
    assert(IsInlineTag(Tag_A));
    assert(IsInlineTag(Tag_U));
    assert(IsInlineTag(Tag_Span));
    assert(!IsInlineTag(Tag_P));
    assert(IsTagSelfClosing(Tag_Area));
    assert(IsTagSelfClosing(Tag_Link));
    assert(IsTagSelfClosing(Tag_Param));
    assert(!IsTagSelfClosing(Tag_P));
    assert(IsTagSelfClosing("area"));
    assert(IsTagSelfClosing("link"));
    assert(IsTagSelfClosing("param"));
    assert(!IsTagSelfClosing("p"));
}

}

void HtmlPullParser_UnitTests()
{
    unittests::Test00("<p a1='>' foo=bar />", HtmlToken::EmptyElementTag);
    unittests::Test00("<p a1 ='>'     foo=\"bar\"/>", HtmlToken::EmptyElementTag);
    unittests::Test00("<p a1=  '>' foo=bar>", HtmlToken::StartTag);
    unittests::Test00("<></><><!-- < skip > --><p a1=\">\" foo=bar>", HtmlToken::StartTag);
    unittests::Test00("<P A1='>' FOO=bar />", HtmlToken::EmptyElementTag);
    unittests::HtmlEntities();
    unittests::Test01();
}
