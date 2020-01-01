/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/HtmlParserLookup.h"
#include "utils/CssParser.h"

// TODO: the following parser doesn't comply yet with
// http://www.w3.org/TR/CSS21/syndata.html#syntax

// return true if skipped
static bool SkipWsAndComments(const char*& s, const char* end) {
    const char* start = s;
    for (; s < end && str::IsWs(*s); s++)
        ;
    while (s + 2 <= end && s[0] == '/' && s[1] == '*') {
        for (s += 2; s < end; s++) {
            if (s + 2 <= end && s[0] == '*' && s[1] == '/') {
                s += 2;
                break;
            }
        }
        for (; s < end && str::IsWs(*s); s++)
            ;
    }
    return start != s;
}

// returns false if there was no closing quotation mark
static bool SkipQuotedString(const char*& s, const char* end) {
    char quote = *s;
    while (++s < end && *s != quote) {
        if (*s == '\\')
            s++;
    }
    if (s == end)
        return false;
    s++;
    return true;
}

static bool SkipBlock(const char*& s, const char* end) {
    CrashIf(s >= end || *s != '{');
    s++;
    while (s < end && *s != '}') {
        if (*s == '"' || *s == '\'') {
            if (!SkipQuotedString(s, end))
                return false;
        } else if (*s == '{') {
            if (!SkipBlock(s, end))
                return false;
        } else if (*s == '\\' && s < end - 1)
            s += 2;
        else if (!SkipWsAndComments(s, end))
            s++;
    }
    if (s == end)
        return false;
    s++;
    return true;
}

bool CssPullParser::NextRule() {
    if (inProps)
        while (NextProperty())
            ;
    CrashIf(inProps && currPos < end);
    if (inlineStyle || currPos == end)
        return false;

    if (currPos == s) {
        SkipWsAndComments(currPos, end);
        if (currPos + 4 < end && str::StartsWith(currPos, "<!--"))
            currPos += 4;
    }

    SkipWsAndComments(currPos, end);
    currSel = currPos;
    // skip selectors
    while (currPos < end && *currPos != '{') {
        if (*currPos == '"' || *currPos == '\'') {
            if (!SkipQuotedString(currPos, end))
                break;
        } else if (*currPos == ';') {
            currPos++;
            SkipWsAndComments(currPos, end);
            currSel = currPos;
        } else if (!SkipWsAndComments(currPos, end)) {
            currPos++;
        }
    }

    if (currPos == end) {
        currSel = nullptr;
        return false;
    }
    selEnd = currPos++;
    inProps = true;
    return true;
}

const CssSelector* CssPullParser::NextSelector() {
    if (!currSel)
        return nullptr;
    SkipWsAndComments(currSel, selEnd);
    if (currSel == selEnd)
        return nullptr;

    sel.s = currSel;
    // skip single selector
    const char* sEnd = currSel;
    while (currSel < selEnd && *currSel != ',') {
        if (*currSel == '"' || *currSel == '\'') {
            bool ok = SkipQuotedString(currSel, selEnd);
            CrashIf(!ok);
            sEnd = currSel;
        } else if (*currSel == '\\' && currSel < selEnd - 1) {
            currSel += 2;
            sEnd = currSel;
        } else if (!SkipWsAndComments(currSel, selEnd)) {
            sEnd = ++currSel;
        }
    }
    if (currSel < selEnd)
        currSel++;

    sel.sLen = sEnd - sel.s;
    sel.tag = Tag_NotFound;
    sel.clazz = nullptr;
    sel.clazzLen = 0;

    // parse "*", "el", ".class" and "el.class"
    const char* c = sEnd;
    for (; c > sel.s && (isalnum((unsigned char)*(c - 1)) || *(c - 1) == '-'); c--)
        ;
    if (c > sel.s && *(c - 1) == '.') {
        sel.clazz = c;
        sel.clazzLen = sEnd - c;
        c--;
    }
    for (; c > sel.s && (isalnum((unsigned char)*(c - 1)) || *(c - 1) == '-'); c--)
        ;
    if (sel.clazz - 1 == sel.s) {
        sel.tag = Tag_Any;
    } else if (c == (sel.clazz ? sel.clazz - 1 : sEnd) && c == sel.s + 1 && *sel.s == '*') {
        sel.tag = Tag_Any;
    } else if (c == sel.s) {
        sel.tag = FindHtmlTag(sel.s, sel.clazz ? sel.clazz - sel.s - 1 : sel.sLen);
    }

    return &sel;
}

const CssProperty* CssPullParser::NextProperty() {
    if (currPos == s)
        inlineStyle = inProps = true;
    else if (!inProps)
        return nullptr;

GetNextProperty:
    SkipWsAndComments(currPos, end);
    if (currPos == end)
        return nullptr;
    if (*currPos == '}') {
        currPos++;
        inProps = false;
        return nullptr;
    }
    if (*currPos == '{') {
        if (!SkipBlock(currPos, end))
            return nullptr;
        goto GetNextProperty;
    }
    if (*currPos == ';') {
        currPos++;
        goto GetNextProperty;
    }
    const char* name = currPos;
    // skip identifier
    while (currPos < end && !str::IsWs(*currPos) && *currPos != ':' && *currPos != ';' && *currPos != '{' &&
           *currPos != '}') {
        currPos++;
    }
    SkipWsAndComments(currPos, end);
    if (currPos == end || *currPos != ':')
        goto GetNextProperty;
    prop.type = FindCssProp(name, currPos - name);
    currPos++;
    SkipWsAndComments(currPos, end);

    prop.s = currPos;
    // skip value
    const char* valEnd = currPos;
    while (currPos < end && *currPos != ';' && *currPos != '}') {
        if (*currPos == '"' || *currPos == '\'') {
            if (!SkipQuotedString(currPos, end))
                return nullptr;
            valEnd = currPos;
        } else if (*currPos == '{') {
            if (!SkipBlock(currPos, end))
                return nullptr;
            valEnd = currPos;
        } else if (*currPos == '\\' && currPos < end - 1) {
            currPos += 2;
            valEnd = currPos;
        } else if (!SkipWsAndComments(currPos, end)) {
            valEnd = ++currPos;
        }
    }
    prop.sLen = valEnd - prop.s;

    return &prop;
}
