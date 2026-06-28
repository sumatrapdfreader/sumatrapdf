/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/HtmlParserLookup.h"
#include "utils/CssParser.h"

// must be last due to assert() over-write
#include "utils/UtAssert.h"

static inline bool IsPropVal(const CssProperty* prop, Str val) {
    return str::EqNIx(prop->s, prop->s.len, val);
}
static inline bool IsSelector(const CssSelector* sel, Str val) {
    return str::EqNIx(sel->s, sel->s.len, val);
}

static void Test01() {
    Str inlineCss = "color: red; text-indent: 20px; /* comment */";
    CssPullParser parser{inlineCss};
    const CssProperty* prop = parser.NextProperty();
    utassert(prop && Css_Color == prop->type && IsPropVal(prop, "red"));
    prop = parser.NextProperty();
    utassert(prop && Css_Text_Indent == prop->type && IsPropVal(prop, "20px"));
    prop = parser.NextProperty();
    utassert(!prop);
}

static void Test02() {
    Str inlineCss = "font-family: 'Courier New', \"Times New Roman\", Arial ; font: 12pt Georgia bold";
    CssPullParser parser{inlineCss};
    const CssProperty* prop = parser.NextProperty();
    utassert(prop && Css_Font_Family == prop->type && IsPropVal(prop, "'Courier New', \"Times New Roman\", Arial"));
    prop = parser.NextProperty();
    utassert(prop && Css_Font == prop->type && IsPropVal(prop, "12pt Georgia bold"));
    prop = parser.NextProperty();
    utassert(!prop);
}

static void Test03() {
    Str simpleCss = "* { color: red }\np { color: blue }\n.green { color: green }\np.green { color: rgb(0,128,0) }\n";
    CssPullParser parser{simpleCss};
    const CssSelector* sel = parser.NextSelector();
    utassert(!sel);
    const CssProperty* prop;

    bool ok = parser.NextRule();
    utassert(ok);
    sel = parser.NextSelector();
    utassert(sel && Tag_Any == sel->tag && !sel->clazz && IsSelector(sel, "*"));
    sel = parser.NextSelector();
    utassert(!sel);
    prop = parser.NextProperty();
    utassert(prop && Css_Color == prop->type && IsPropVal(prop, "red"));
    prop = parser.NextProperty();
    utassert(!prop);

    ok = parser.NextRule();
    utassert(ok);
    sel = parser.NextSelector();
    utassert(sel && Tag_P == sel->tag && !sel->clazz && IsSelector(sel, "p"));
    prop = parser.NextProperty();
    utassert(prop && Css_Color == prop->type && IsPropVal(prop, "blue"));
    prop = parser.NextProperty();
    utassert(!prop);

    ok = parser.NextRule();
    utassert(ok);
    sel = parser.NextSelector();
    utassert(sel && Tag_Any == sel->tag && IsSelector(sel, ".green") &&
             str::EqNIx(sel->clazz, sel->clazz.len, Str("green")));
    prop = parser.NextProperty();
    utassert(prop && Css_Color == prop->type && IsPropVal(prop, "green"));
    prop = parser.NextProperty();
    utassert(!prop);

    ok = parser.NextRule();
    utassert(ok);
    sel = parser.NextSelector();
    utassert(sel && Tag_P == sel->tag && IsSelector(sel, "p.green") &&
             str::EqNIx(sel->clazz, sel->clazz.len, Str("green")));
    prop = parser.NextProperty();
    utassert(prop && Css_Color == prop->type && IsPropVal(prop, "rgb(0,128,0)"));
    prop = parser.NextProperty();
    utassert(!prop);

    ok = parser.NextRule();
    utassert(!ok);
}

static void Test04() {
    Str simpleCss = " span\n{ color: red }\n\tp /* plain paragraph */ , p#id { }";
    CssPullParser parser{simpleCss};
    const CssSelector* sel;
    const CssProperty* prop;

    bool ok = parser.NextRule();
    utassert(ok);
    prop = parser.NextProperty();
    utassert(prop && Css_Color == prop->type && IsPropVal(prop, "red"));
    prop = parser.NextProperty();
    utassert(!prop);
    sel = parser.NextSelector();
    utassert(sel && Tag_Span == sel->tag && !sel->clazz && IsSelector(sel, "span"));
    sel = parser.NextSelector();
    utassert(!sel);

    ok = parser.NextRule();
    utassert(ok);
    prop = parser.NextProperty();
    utassert(!prop);
    sel = parser.NextSelector();
    utassert(sel && Tag_P == sel->tag && !sel->clazz && IsSelector(sel, "p"));
    sel = parser.NextSelector();
    utassert(sel && Tag_NotFound == sel->tag && !sel->clazz && IsSelector(sel, "p#id"));
    sel = parser.NextSelector();
    utassert(!sel);

    ok = parser.NextRule();
    utassert(!ok);
}

static void Test05() {
    Str simpleCss = "<!-- html { ignore } @ignore this; p { } -->";
    CssPullParser parser{simpleCss};
    const CssSelector* sel;
    const CssProperty* prop;

    bool ok = parser.NextRule();
    utassert(ok);
    sel = parser.NextSelector();
    utassert(sel && Tag_Html == sel->tag && !sel->clazz && IsSelector(sel, "html"));
    sel = parser.NextSelector();
    utassert(!sel);
    prop = parser.NextProperty();
    utassert(!prop);

    ok = parser.NextRule();
    utassert(ok);
    sel = parser.NextSelector();
    utassert(sel && Tag_P == sel->tag && !sel->clazz && IsSelector(sel, "p"));
    sel = parser.NextSelector();
    utassert(!sel);
    prop = parser.NextProperty();
    utassert(!prop);

    ok = parser.NextRule();
    utassert(!ok);
}

static void Test06() {
    Str inlineCss = "block: {{ ignore this }} ; color: red; } color: blue";
    CssPullParser parser{inlineCss};
    const CssProperty* prop = parser.NextProperty();
    utassert(prop && Css_Unknown == prop->type && IsPropVal(prop, "{{ ignore this }}"));
    prop = parser.NextProperty();
    utassert(prop && Css_Color == prop->type && IsPropVal(prop, "red"));
    prop = parser.NextProperty();
    utassert(!prop);
    bool ok = parser.NextRule();
    utassert(!ok);
}

static void Test07() {
    Str simpleCss = " span\n{ color: red }\n\tp /* plain paragraph */ , p#id { }";
    CssPullParser parser{simpleCss};
    bool ok = parser.NextRule();
    utassert(ok);
    ok = parser.NextRule();
    utassert(ok);
    ok = parser.NextRule();
    utassert(!ok);
}

static void Test08() {
    Str simpleCss = "broken { brace: \"doesn't close\"; { ignore { color: red; }";
    CssPullParser parser{simpleCss};
    bool ok = parser.NextRule();
    utassert(ok);
    const CssProperty* prop = parser.NextProperty();
    utassert(Css_Unknown == prop->type && IsPropVal(prop, "\"doesn't close\""));
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