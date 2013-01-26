/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// note: this is only meant to be #included from CssParser.cpp,
// not compiled on its own

namespace unittests {

static void Test01()
{
    const char *inlineCss = "color: red; text-indent: 20px; /* comment */";
    CssPullParser parser(inlineCss);
    const CssProperty *prop = parser.NextProperty();
    assert(prop && Css_Color == prop->type && StrEqNIx(prop->s, prop->sLen, "red"));
    prop = parser.NextProperty();
    assert(prop && Css_Text_Indent == prop->type && StrEqNIx(prop->s, prop->sLen, "20px"));
    prop = parser.NextProperty();
    assert(!prop);
}

static void Test02()
{
    const char *inlineCss = "font-family: 'Courier New', \"Times New Roman\", Arial ; font: 12pt Georgia bold";
    CssPullParser parser(inlineCss);
    const CssProperty *prop = parser.NextProperty();
    assert(prop && Css_Font_Family == prop->type && StrEqNIx(prop->s, prop->sLen, "'Courier New', \"Times New Roman\", Arial"));
    prop = parser.NextProperty();
    assert(prop && Css_Font == prop->type && StrEqNIx(prop->s, prop->sLen, "12pt Georgia bold"));
    prop = parser.NextProperty();
    assert(!prop);
}

static void Test03()
{
    const char *simpleCss = "* { color: red }\np { color: blue }\n.green { color: green }\np.green { color: rgb(0,128,0) }\n";
    CssPullParser parser(simpleCss);
    const CssSelector *sel = parser.NextSelector();
    assert(!sel);
    const CssProperty *prop;

    bool ok = parser.NextRule();
    assert(ok);
    sel = parser.NextSelector();
    assert(sel && -1 == sel->tag && !sel->clazz && StrEqNIx(sel->s, sel->sLen, "*"));
    sel = parser.NextSelector();
    assert(!sel);
    prop = parser.NextProperty();
    assert(prop && Css_Color == prop->type && StrEqNIx(prop->s, prop->sLen, "red"));
    prop = parser.NextProperty();
    assert(!prop);

    ok = parser.NextRule();
    assert(ok);
    sel = parser.NextSelector();
    assert(sel && Tag_P == sel->tag && !sel->clazz && StrEqNIx(sel->s, sel->sLen, "p"));
    prop = parser.NextProperty();
    assert(prop && Css_Color == prop->type && StrEqNIx(prop->s, prop->sLen, "blue"));
    prop = parser.NextProperty();
    assert(!prop);

    ok = parser.NextRule();
    assert(ok);
    sel = parser.NextSelector();
    assert(sel && -1 == sel->tag && StrEqNIx(sel->clazz, sel->clazzLen, "green") && StrEqNIx(sel->s, sel->sLen, ".green"));
    prop = parser.NextProperty();
    assert(prop && Css_Color == prop->type && StrEqNIx(prop->s, prop->sLen, "green"));
    prop = parser.NextProperty();
    assert(!prop);

    ok = parser.NextRule();
    assert(ok);
    sel = parser.NextSelector();
    assert(sel && Tag_P == sel->tag && StrEqNIx(sel->clazz, sel->clazzLen, "green") && StrEqNIx(sel->s, sel->sLen, "p.green"));
    prop = parser.NextProperty();
    assert(prop && Css_Color == prop->type && StrEqNIx(prop->s, prop->sLen, "rgb(0,128,0)"));
    prop = parser.NextProperty();
    assert(!prop);

    ok = parser.NextRule();
    assert(!ok);
}

}

void CssParser_UnitTests()
{
    unittests::Test01();
    unittests::Test02();
    unittests::Test03();
}
