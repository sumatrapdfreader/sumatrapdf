/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/HtmlTags.h"
#include "base/CssParser.h"

// must be last due to assert() over-write
#include "base/UtAssert.h"

static inline bool IsPropVal(const CssProperty* prop, Str val) {
    return str::EqNIx(prop->s, prop->s.len, val);
}
static inline bool IsSelector(const CssSelector* sel, Str val) {
    return str::EqNIx(sel->s, sel->s.len, val);
}

static void Test01() {
    Str inlineCss = StrL("color: red; text-indent: 20px; /* comment */");
    CssPullParser parser{inlineCss};
    const CssProperty* prop = parser.NextProperty();
    utassert(prop && Css_Color == prop->type && IsPropVal(prop, StrL("red")));
    prop = parser.NextProperty();
    utassert(prop && Css_Text_Indent == prop->type && IsPropVal(prop, StrL("20px")));
    prop = parser.NextProperty();
    utassert(!prop);
}

static void Test02() {
    Str inlineCss = StrL("font-family: 'Courier New', \"Times New Roman\", Arial ; font: 12pt Georgia bold");
    CssPullParser parser{inlineCss};
    const CssProperty* prop = parser.NextProperty();
    utassert(prop && Css_Font_Family == prop->type &&
             IsPropVal(prop, StrL("'Courier New', \"Times New Roman\", Arial")));
    prop = parser.NextProperty();
    utassert(prop && Css_Font == prop->type && IsPropVal(prop, StrL("12pt Georgia bold")));
    prop = parser.NextProperty();
    utassert(!prop);
}

static void Test03() {
    Str simpleCss =
        StrL("* { color: red }\np { color: blue }\n.green { color: green }\np.green { color: rgb(0,128,0) }\n");
    CssPullParser parser{simpleCss};
    const CssSelector* sel = parser.NextSelector();
    utassert(!sel);
    const CssProperty* prop;

    bool ok = parser.NextRule();
    utassert(ok);
    sel = parser.NextSelector();
    utassert(sel && kTagAny == sel->tag && len(sel->clazz) == 0 && IsSelector(sel, StrL("*")));
    sel = parser.NextSelector();
    utassert(!sel);
    prop = parser.NextProperty();
    utassert(prop && Css_Color == prop->type && IsPropVal(prop, StrL("red")));
    prop = parser.NextProperty();
    utassert(!prop);

    ok = parser.NextRule();
    utassert(ok);
    sel = parser.NextSelector();
    utassert(sel && Tag_P == sel->tag && len(sel->clazz) == 0 && IsSelector(sel, StrL("p")));
    prop = parser.NextProperty();
    utassert(prop && Css_Color == prop->type && IsPropVal(prop, StrL("blue")));
    prop = parser.NextProperty();
    utassert(!prop);

    ok = parser.NextRule();
    utassert(ok);
    sel = parser.NextSelector();
    utassert(sel && kTagAny == sel->tag && IsSelector(sel, StrL(".green")) &&
             str::EqNIx(sel->clazz, sel->clazz.len, StrL("green")));
    prop = parser.NextProperty();
    utassert(prop && Css_Color == prop->type && IsPropVal(prop, StrL("green")));
    prop = parser.NextProperty();
    utassert(!prop);

    ok = parser.NextRule();
    utassert(ok);
    sel = parser.NextSelector();
    utassert(sel && Tag_P == sel->tag && IsSelector(sel, StrL("p.green")) &&
             str::EqNIx(sel->clazz, sel->clazz.len, StrL("green")));
    prop = parser.NextProperty();
    utassert(prop && Css_Color == prop->type && IsPropVal(prop, StrL("rgb(0,128,0)")));
    prop = parser.NextProperty();
    utassert(!prop);

    ok = parser.NextRule();
    utassert(!ok);
}

static void Test04() {
    Str simpleCss = StrL(" span\n{ color: red }\n\tp /* plain paragraph */ , p#id { }");
    CssPullParser parser{simpleCss};
    const CssSelector* sel;
    const CssProperty* prop;

    bool ok = parser.NextRule();
    utassert(ok);
    prop = parser.NextProperty();
    utassert(prop && Css_Color == prop->type && IsPropVal(prop, StrL("red")));
    prop = parser.NextProperty();
    utassert(!prop);
    sel = parser.NextSelector();
    utassert(sel && Tag_Span == sel->tag && len(sel->clazz) == 0 && IsSelector(sel, StrL("span")));
    sel = parser.NextSelector();
    utassert(!sel);

    ok = parser.NextRule();
    utassert(ok);
    prop = parser.NextProperty();
    utassert(!prop);
    sel = parser.NextSelector();
    utassert(sel && Tag_P == sel->tag && len(sel->clazz) == 0 && IsSelector(sel, StrL("p")));
    sel = parser.NextSelector();
    utassert(sel && Tag_NotFound == sel->tag && len(sel->clazz) == 0 && IsSelector(sel, StrL("p#id")));
    sel = parser.NextSelector();
    utassert(!sel);

    ok = parser.NextRule();
    utassert(!ok);
}

static void Test05() {
    Str simpleCss = StrL("<!-- html { ignore } @ignore this; p { } -->");
    CssPullParser parser{simpleCss};
    const CssSelector* sel;
    const CssProperty* prop;

    bool ok = parser.NextRule();
    utassert(ok);
    sel = parser.NextSelector();
    utassert(sel && Tag_Html == sel->tag && len(sel->clazz) == 0 && IsSelector(sel, StrL("html")));
    sel = parser.NextSelector();
    utassert(!sel);
    prop = parser.NextProperty();
    utassert(!prop);

    ok = parser.NextRule();
    utassert(ok);
    sel = parser.NextSelector();
    utassert(sel && Tag_P == sel->tag && len(sel->clazz) == 0 && IsSelector(sel, StrL("p")));
    sel = parser.NextSelector();
    utassert(!sel);
    prop = parser.NextProperty();
    utassert(!prop);

    ok = parser.NextRule();
    utassert(!ok);
}

static void Test06() {
    Str inlineCss = StrL("block: {{ ignore this }} ; color: red; } color: blue");
    CssPullParser parser{inlineCss};
    const CssProperty* prop = parser.NextProperty();
    utassert(prop && Css_Unknown == prop->type && IsPropVal(prop, StrL("{{ ignore this }}")));
    prop = parser.NextProperty();
    utassert(prop && Css_Color == prop->type && IsPropVal(prop, StrL("red")));
    prop = parser.NextProperty();
    utassert(!prop);
    bool ok = parser.NextRule();
    utassert(!ok);
}

static void Test07() {
    Str simpleCss = StrL(" span\n{ color: red }\n\tp /* plain paragraph */ , p#id { }");
    CssPullParser parser{simpleCss};
    bool ok = parser.NextRule();
    utassert(ok);
    ok = parser.NextRule();
    utassert(ok);
    ok = parser.NextRule();
    utassert(!ok);
}

static void Test08() {
    Str simpleCss = StrL("broken { brace: \"doesn't close\"; { ignore { color: red; }");
    CssPullParser parser{simpleCss};
    bool ok = parser.NextRule();
    utassert(ok);
    const CssProperty* prop = parser.NextProperty();
    utassert(Css_Unknown == prop->type && IsPropVal(prop, StrL("\"doesn't close\"")));
    prop = parser.NextProperty();
    utassert(!prop);
    ok = parser.NextRule();
    utassert(!ok);
}

void CssParser_UnitTests() {
    Test01();
    Test02();
    Test03();
    Test04();
    Test05();
    Test06();
    Test07();
    Test08();
}
