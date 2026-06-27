/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#define Tag_Any ((HtmlTag) - 1)

struct CssSelector {
    Str s = {};
    // for convenience
    HtmlTag tag = Tag_NotFound;
    Str clazz = {};
};

struct CssProperty {
    CssProp type = Css_Unknown;
    Str s = {};
};

class CssPullParser {
    Str src = {};
    const char* currPos = nullptr;
    const char* end = nullptr;

    bool inProps = false;
    bool inlineStyle = false;

    const char* currSel = nullptr;
    const char* selEnd = nullptr;

    CssSelector sel{};
    CssProperty prop{};

  public:
    CssPullParser(Str s) {
        this->src = s;
        currPos = s.s;
        end = s.s ? s.s + s.len : nullptr;
    }

    // call NextRule first for parsing a style element and
    // NextProperty only for parsing a single style attribute
    bool NextRule();
    const CssSelector* NextSelector();
    const CssProperty* NextProperty();
};