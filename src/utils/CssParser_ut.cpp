/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// note: this is only meant to be #included from CssParser.cpp,
// not compiled on its own

namespace unittests {

static inline bool StrEqNIx(const char *s, size_t len, const char *s2) {
    return str::Len(s2) == len && str::StartsWithI(s, s2);
}
static inline bool IsPropVal(const CssProperty *prop, const char *val) {
    return StrEqNIx(prop->s, prop->sLen, val);
}
static inline bool IsSelector(const CssSelector *sel, const char *val) {
    return StrEqNIx(sel->s, sel->sLen, val);
}

static void Test01()
{
    const char *inlineCss = "color: red; text-indent: 20px; /* comment */";
    CssPullParser parser(inlineCss, str::Len(inlineCss));
    const CssProperty *prop = parser.NextProperty();
    assert(prop && Css_Color == prop->type && IsPropVal(prop, "red"));
    prop = parser.NextProperty();
    assert(prop && Css_Text_Indent == prop->type && IsPropVal(prop, "20px"));
    prop = parser.NextProperty();
    assert(!prop);
}

static void Test02()
{
    const char *inlineCss = "font-family: 'Courier New', \"Times New Roman\", Arial ; font: 12pt Georgia bold";
    CssPullParser parser(inlineCss, str::Len(inlineCss));
    const CssProperty *prop = parser.NextProperty();
    assert(prop && Css_Font_Family == prop->type && IsPropVal(prop, "'Courier New', \"Times New Roman\", Arial"));
    prop = parser.NextProperty();
    assert(prop && Css_Font == prop->type && IsPropVal(prop, "12pt Georgia bold"));
    prop = parser.NextProperty();
    assert(!prop);
}

static void Test03()
{
    const char *simpleCss = "* { color: red }\np { color: blue }\n.green { color: green }\np.green { color: rgb(0,128,0) }\n";
    CssPullParser parser(simpleCss, str::Len(simpleCss));
    const CssSelector *sel = parser.NextSelector();
    assert(!sel);
    const CssProperty *prop;

    bool ok = parser.NextRule();
    assert(ok);
    sel = parser.NextSelector();
    assert(sel && Tag_Any == sel->tag && !sel->clazz && IsSelector(sel, "*"));
    sel = parser.NextSelector();
    assert(!sel);
    prop = parser.NextProperty();
    assert(prop && Css_Color == prop->type && IsPropVal(prop, "red"));
    prop = parser.NextProperty();
    assert(!prop);

    ok = parser.NextRule();
    assert(ok);
    sel = parser.NextSelector();
    assert(sel && Tag_P == sel->tag && !sel->clazz && IsSelector(sel, "p"));
    prop = parser.NextProperty();
    assert(prop && Css_Color == prop->type && IsPropVal(prop, "blue"));
    prop = parser.NextProperty();
    assert(!prop);

    ok = parser.NextRule();
    assert(ok);
    sel = parser.NextSelector();
    assert(sel && Tag_Any == sel->tag && IsSelector(sel, ".green") && StrEqNIx(sel->clazz, sel->clazzLen, "green"));
    prop = parser.NextProperty();
    assert(prop && Css_Color == prop->type && IsPropVal(prop, "green"));
    prop = parser.NextProperty();
    assert(!prop);

    ok = parser.NextRule();
    assert(ok);
    sel = parser.NextSelector();
    assert(sel && Tag_P == sel->tag && IsSelector(sel, "p.green") && StrEqNIx(sel->clazz, sel->clazzLen, "green"));
    prop = parser.NextProperty();
    assert(prop && Css_Color == prop->type && IsPropVal(prop, "rgb(0,128,0)"));
    prop = parser.NextProperty();
    assert(!prop);

    ok = parser.NextRule();
    assert(!ok);
}

static void Test04()
{
    const char *simpleCss = " span\n{ color: red }\n\tp /* plain paragraph */ , p#id { }";
    CssPullParser parser(simpleCss, str::Len(simpleCss));
    const CssSelector *sel;
    const CssProperty *prop;

    bool ok = parser.NextRule();
    assert(ok);
    prop = parser.NextProperty();
    assert(prop && Css_Color == prop->type && IsPropVal(prop, "red"));
    prop = parser.NextProperty();
    assert(!prop);
    sel = parser.NextSelector();
    assert(sel && Tag_Span == sel->tag && !sel->clazz && IsSelector(sel, "span"));
    sel = parser.NextSelector();
    assert(!sel);

    ok = parser.NextRule();
    assert(ok);
    prop = parser.NextProperty();
    assert(!prop);
    sel = parser.NextSelector();
    assert(sel && Tag_P == sel->tag && !sel->clazz && IsSelector(sel, "p"));
    sel = parser.NextSelector();
    assert(sel && Tag_NotFound == sel->tag && !sel->clazz && IsSelector(sel, "p#id"));
    sel = parser.NextSelector();
    assert(!sel);

    ok = parser.NextRule();
    assert(!ok);
}

static void Test05()
{
    const char *simpleCss = "<!-- html { ignore } @ignore this; p { } -->";
    CssPullParser parser(simpleCss, str::Len(simpleCss));
    const CssSelector *sel;
    const CssProperty *prop;

    bool ok = parser.NextRule();
    assert(ok);
    sel = parser.NextSelector();
    assert(sel && Tag_Html == sel->tag && !sel->clazz && IsSelector(sel, "html"));
    sel = parser.NextSelector();
    assert(!sel);
    prop = parser.NextProperty();
    assert(!prop);

    ok = parser.NextRule();
    assert(ok);
    sel = parser.NextSelector();
    assert(sel && Tag_P == sel->tag && !sel->clazz && IsSelector(sel, "p"));
    sel = parser.NextSelector();
    assert(!sel);
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
    unittests::Test04();
    unittests::Test05();
}
