/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#define Tag_Any ((HtmlTag) - 1)

struct CssSelector {
    const char* s = nullptr;
    size_t sLen = 0;
    // for convenience
    HtmlTag tag = Tag_NotFound;
    const char* clazz = nullptr;
    size_t clazzLen = 0;
};

struct CssProperty {
    CssProp type = Css_Unknown;
    const char* s = nullptr;
    size_t sLen = 0;
};

class CssPullParser {
    const char* s = nullptr;
    const char* currPos = nullptr;
    const char* end = nullptr;

    bool inProps = false;
    bool inlineStyle = false;

    const char* currSel = nullptr;
    const char* selEnd = nullptr;

    CssSelector sel{};
    CssProperty prop{};

  public:
    CssPullParser(const char* s, size_t len) {
        this->s = s;
        currPos = s;
        end = s + len;
    }

    // call NextRule first for parsing a style element and
    // NextProperty only for parsing a single style attribute
    bool NextRule();
    const CssSelector* NextSelector();
    const CssProperty* NextProperty();
};
