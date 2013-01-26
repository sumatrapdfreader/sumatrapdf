/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef CssParser_h
#define CssParser_h

#include "HtmlParserLookup.h"

#define Tag_Any ((HtmlTag)-1)

struct CssSelector {
    const char *s;
    size_t      sLen;
    // for convenience
    HtmlTag     tag;
    const char *clazz;
    size_t      clazzLen;
};

struct CssProperty {
    CssProp     type;
    const char *s;
    size_t      sLen;
};

class CssPullParser {
    const char *s;
    const char *currPos;
    const char *end;

    bool        inProps;

    const char *currSel;
    const char *selEnd;

    CssSelector sel;
    CssProperty prop;

public:
    CssPullParser(const char *s) :
        s(s), currPos(s), end(s + str::Len(s)), inProps(false), currSel(NULL) { }
    CssPullParser(const char *s, size_t len) :
        s(s), currPos(s), end(s + len), inProps(false), currSel(NULL) { }

    // call NextRule first for parsing a style element and
    // NextProperty only for parsing a single style attribute
    bool NextRule();
    const CssSelector *NextSelector();
    const CssProperty *NextProperty();
};

#endif
