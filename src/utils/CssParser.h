/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef CssParser_h
#define CssParser_h

#include "HtmlPullParser.h"

struct CssProperty {
    CssProp     type;
    const char *s;
    size_t      sLen;
};

class CssPullParser {
    const char *currPos;
    const char *end;

    CssProperty prop;

public:
    CssPullParser(const char *s) : currPos(s), end(s + str::Len(s)) { }
    CssPullParser(const char *s, size_t len) : currPos(s), end(s + len) { }

    const CssProperty *NextProp();
};

#endif
