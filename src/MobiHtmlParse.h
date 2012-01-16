/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef MobiHtmlParse_h
#define MobiHtmlParse_h

#include <stdint.h>
#include <Vec.h>

#include "HtmlPullParser.h"

#define IS_END_TAG_MASK  0x01
#define HAS_ATTR_MASK    0x02

enum ParsedElementType {
    ParsedElString = 0,
    ParsedElInt
};

struct ParsedElement {
    ParsedElementType type;
    // if type == ParsedElInt
    union {
        int         n;
        HtmlTag     tag;
        HtmlAttr    attr;
    };
    // if type == ParsedElString
    const uint8_t * s;
    uint32_t        sLen;
};

ParsedElement *DecodeNextParsedElement(const uint8_t* &s, const uint8_t *end);

Vec<uint8_t> *MobiHtmlToDisplay(const uint8_t *s, size_t sLen);

#endif
