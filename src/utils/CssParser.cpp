/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/HtmlParserLookup.h"
#include "utils/CssParser.h"

// TODO: the following parser doesn't comply yet with
// http://www.w3.org/TR/CSS21/syndata.html#syntax

// return true if skipped
static bool SkipWsAndComments(Str data, int& off) {
    int start = off;
    for (; off < data.len && str::IsWs(data.s[off]); off++) {
        ;
    }
    while (off + 2 <= data.len && data.s[off] == '/' && data.s[off + 1] == '*') {
        for (off += 2; off < data.len; off++) {
            if (off + 2 <= data.len && data.s[off] == '*' && data.s[off + 1] == '/') {
                off += 2;
                break;
            }
        }
        for (; off < data.len && str::IsWs(data.s[off]); off++) {
            ;
        }
    }
    return start != off;
}

// returns false if there was no closing quotation mark
static bool SkipQuotedString(Str data, int& off) {
    if (off >= data.len) {
        return false;
    }
    char quote = data.s[off];
    off++;
    while (off < data.len && data.s[off] != quote) {
        if (data.s[off] == '\\') {
            off++;
            if (off >= data.len) {
                return false;
            }
        }
        off++;
    }
    if (off >= data.len) {
        return false;
    }
    off++;
    return true;
}

static bool SkipBlock(Str data, int& off) {
    ReportIf(off >= data.len || data.s[off] != '{');
    off++;
    while (off < data.len && data.s[off] != '}') {
        if (data.s[off] == '"' || data.s[off] == '\'') {
            if (!SkipQuotedString(data, off)) {
                return false;
            }
        } else if (data.s[off] == '{') {
            if (!SkipBlock(data, off)) {
                return false;
            }
        } else if (data.s[off] == '\\' && off < data.len - 1) {
            off += 2;
        } else if (!SkipWsAndComments(data, off)) {
            off++;
        }
    }
    if (off >= data.len) {
        return false;
    }
    off++;
    return true;
}

bool CssPullParser::NextRule() {
    if (inProps) {
        while (NextProperty()) {
            ;
        }
    }
    ReportIf(inProps && currOff < src.len);
    if (inlineStyle || currOff >= src.len) {
        return false;
    }

    if (currOff == 0) {
        SkipWsAndComments(src, currOff);
        if (currOff + 4 < src.len && str::StartsWith(Str(src.s + currOff, src.len - currOff), StrL("<!--"))) {
            currOff += 4;
        }
    }

    SkipWsAndComments(src, currOff);
    currSelOff = currOff;
    // skip selectors
    while (currOff < src.len && src.s[currOff] != '{') {
        if (src.s[currOff] == '"' || src.s[currOff] == '\'') {
            if (!SkipQuotedString(src, currOff)) {
                break;
            }
        } else if (src.s[currOff] == ';') {
            currOff++;
            SkipWsAndComments(src, currOff);
            currSelOff = currOff;
        } else if (!SkipWsAndComments(src, currOff)) {
            currOff++;
        }
    }

    if (currOff >= src.len) {
        currSelOff = -1;
        return false;
    }
    selEndOff = currOff++;
    inProps = true;
    return true;
}

const CssSelector* CssPullParser::NextSelector() {
    if (currSelOff < 0) {
        return nullptr;
    }
    int selOff = currSelOff;
    SkipWsAndComments(src, selOff);
    if (selOff >= selEndOff) {
        return nullptr;
    }

    int selStart = selOff;
    // skip single selector
    int sEnd = selOff;
    while (selOff < selEndOff && src.s[selOff] != ',') {
        if (src.s[selOff] == '"' || src.s[selOff] == '\'') {
            bool ok = SkipQuotedString(src, selOff);
            ReportIf(!ok);
            sEnd = selOff;
        } else if (src.s[selOff] == '\\' && selOff < selEndOff - 1) {
            selOff += 2;
            sEnd = selOff;
        } else if (!SkipWsAndComments(src, selOff)) {
            sEnd = ++selOff;
        }
    }
    if (selOff < selEndOff) {
        selOff++;
    }
    currSelOff = selOff;

    sel.s = Str(src.s + selStart, sEnd - selStart);
    sel.tag = Tag_NotFound;
    sel.clazz = Str();

    // parse "*", "el", ".class" and "el.class"
    int c = sEnd;
    for (; c > selStart && (isalnum((u8)src.s[c - 1]) || src.s[c - 1] == '-'); c--) {
        ;
    }
    if (c > selStart && src.s[c - 1] == '.') {
        sel.clazz = Str(src.s + c, sEnd - c);
        c--;
    }
    for (; c > selStart && (isalnum((u8)src.s[c - 1]) || src.s[c - 1] == '-'); c--) {
        ;
    }
    if (sel.clazz && sel.clazz.s == src.s + selStart + 1) {
        sel.tag = Tag_Any;
    } else if (c == (sel.clazz ? (int)(sel.clazz.s - src.s - 1) : sEnd) && c == selStart + 1 &&
               src.s[selStart] == '*') {
        sel.tag = Tag_Any;
    } else if (c == selStart) {
        size_t tagLen = sel.clazz ? (size_t)(sel.clazz.s - src.s - selStart - 1) : (size_t)sel.s.len;
        sel.tag = FindHtmlTag(Str(sel.s.s, (int)tagLen));
    }

    return &sel;
}

const CssProperty* CssPullParser::NextProperty() {
    if (currOff == 0) {
        inlineStyle = inProps = true;
    } else if (!inProps) {
        return nullptr;
    }

GetNextProperty:
    SkipWsAndComments(src, currOff);
    if (currOff >= src.len) {
        return nullptr;
    }
    if (src.s[currOff] == '}') {
        currOff++;
        inProps = false;
        return nullptr;
    }
    if (src.s[currOff] == '{') {
        if (!SkipBlock(src, currOff)) {
            return nullptr;
        }
        goto GetNextProperty;
    }
    if (src.s[currOff] == ';') {
        currOff++;
        goto GetNextProperty;
    }
    int nameOff = currOff;
    // skip identifier
    while (currOff < src.len && !str::IsWs(src.s[currOff]) && src.s[currOff] != ':' && src.s[currOff] != ';' &&
           src.s[currOff] != '{' && src.s[currOff] != '}') {
        currOff++;
    }
    SkipWsAndComments(src, currOff);
    if (currOff >= src.len || src.s[currOff] != ':') {
        goto GetNextProperty;
    }
    prop.type = FindCssProp(Str(src.s + nameOff, currOff - nameOff));
    currOff++;
    SkipWsAndComments(src, currOff);

    int valStart = currOff;
    // skip value
    int valEnd = currOff;
    while (currOff < src.len && src.s[currOff] != ';' && src.s[currOff] != '}') {
        if (src.s[currOff] == '"' || src.s[currOff] == '\'') {
            if (!SkipQuotedString(src, currOff)) {
                return nullptr;
            }
            valEnd = currOff;
        } else if (src.s[currOff] == '{') {
            if (!SkipBlock(src, currOff)) {
                return nullptr;
            }
            valEnd = currOff;
        } else if (src.s[currOff] == '\\' && currOff < src.len - 1) {
            currOff += 2;
            valEnd = currOff;
        } else if (!SkipWsAndComments(src, currOff)) {
            valEnd = ++currOff;
        }
    }
    prop.s = Str(src.s + valStart, valEnd - valStart);

    return &prop;
}