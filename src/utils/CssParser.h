/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#define Tag_Any ((HtmlTag)-1)

struct CssSelector {
    const char* s;
    size_t sLen;
    // for convenience
    HtmlTag tag;
    const char* clazz;
    size_t clazzLen;
};

struct CssProperty {
    CssProp type;
    const char* s;
    size_t sLen;
};

class CssPullParser {
    const char* s;
    const char* currPos;
    const char* end;

    bool inProps;
    bool inlineStyle;

    const char* currSel;
    const char* selEnd;

    CssSelector sel;
    CssProperty prop;

  public:
    CssPullParser(const char* s, size_t len)
        : s(s), currPos(s), end(s + len), inProps(false), inlineStyle(false), currSel(nullptr) {
    }

    // call NextRule first for parsing a style element and
    // NextProperty only for parsing a single style attribute
    bool NextRule();
    const CssSelector* NextSelector();
    const CssProperty* NextProperty();
};
