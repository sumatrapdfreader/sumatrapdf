/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#define Tag_Any ((HtmlTag) - 1)

struct CssSelector {
    Str s;
    // for convenience
    HtmlTag tag = Tag_NotFound;
    Str clazz;
};

struct CssProperty {
    CssProp type = Css_Unknown;
    Str s;
};

class CssPullParser {
    Str src;
    int currOff = 0;

    bool inProps = false;
    bool inlineStyle = false;

    int currSelOff = -1;
    int selEndOff = 0;

    CssSelector sel{};
    CssProperty prop{};

  public:
    explicit CssPullParser(Str s) : src(s) {}

    bool NextRule();
    const CssSelector* NextSelector();
    const CssProperty* NextProperty();
};