/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// note: this is only meant to be #included from CssParser.cpp,
// not compiled on its own

namespace unittests {

static void Test01()
{
    const char *inlineCss = "color: red; text-indent: 20px; /* comment */";
    CssPullParser parser(inlineCss);
    const CssProperty *prop = parser.NextProp();
    assert(prop && Css_Color == prop->type && StrEqNIx(prop->s, prop->sLen, "red"));
    prop = parser.NextProp();
    assert(prop && Css_Text_Indent == prop->type && StrEqNIx(prop->s, prop->sLen, "20px"));
    prop = parser.NextProp();
    assert(!prop);
}

static void Test02()
{
    const char *inlineCss = "font-family: 'Courier New', \"Times New Roman\", Arial ; font: 12pt Georgia bold";
    CssPullParser parser(inlineCss);
    const CssProperty *prop = parser.NextProp();
    assert(prop && Css_Font_Family == prop->type && StrEqNIx(prop->s, prop->sLen, "'Courier New', \"Times New Roman\", Arial"));
    prop = parser.NextProp();
    assert(prop && Css_Font == prop->type && StrEqNIx(prop->s, prop->sLen, "12pt Georgia bold"));
    prop = parser.NextProp();
    assert(!prop);
}

}

void CssParser_UnitTests()
{
    unittests::Test01();
    unittests::Test02();
}
