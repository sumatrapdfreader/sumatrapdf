/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "CssParser.h"

// TODO: the following parser doesn't comply yet with
// http://www.w3.org/TR/CSS21/syndata.html#syntax

inline bool StrEqNIx(const char *s, size_t len, const char *s2)
{
    return str::Len(s2) == len && str::StartsWithI(s, s2);
}

// return true if skipped
static bool SkipWsAndComment(const char*& s, const char *end)
{
    const char *start = s;
    SkipWs(s, end);
    while (s + 2 < end && str::StartsWith(s, "/*")) {
        s += 2;
        if (SkipUntil(s, end, "*/")) {
            s += 2;
            SkipWs(s, end);
        }
    }
    return start != s;
}

static bool SkipQuotedString(const char*& s, const char *end)
{
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

bool CssPullParser::NextRule()
{
    if (inProps || currPos == end)
        return false;

    SkipWsAndComment(currPos, end);
    currSel = currPos;
    // skip selectors
    while (currPos < end && *currPos != '{') {
        if (*currPos == '"' || *currPos == '\'') {
            if (!SkipQuotedString(currPos, end))
                break;
        }
        else if (!SkipWsAndComment(currPos, end)) {
            currPos++;
        }
    }

    if (currPos == end) {
        currSel = NULL;
        return false;
    }
    selEnd = currPos++;
    inProps = true;
    return true;
}

const CssSelector *CssPullParser::NextSelector()
{
    if (!currSel)
        return NULL;
    SkipWs(currSel, selEnd);
    if (currSel == selEnd)
        return NULL;

    sel.s = currSel;
    // skip single selector
    const char *sEnd = currSel;
    while (currSel < selEnd && *currSel != ',') {
        if (*currSel == '"' || *currSel == '\'') {
            bool ok = SkipQuotedString(currSel, selEnd);
            CrashIf(!ok);
            sEnd = currSel;
        }
        else if (*currSel == '\\' && currSel < selEnd - 1) {
            currSel += 2;
            sEnd = currSel;
        }
        else if (!SkipWsAndComment(currSel, selEnd)) {
            sEnd = ++currSel;
        }
    }
    if (currSel < selEnd)
        currSel++;

    sel.sLen = sEnd - sel.s;
    sel.tag = Tag_NotFound;
    sel.clazz = NULL;
    sel.clazzLen = 0;

    // parse "*", "el", ".class" and "el.class"
    const char *c = sEnd;
    for (; c > sel.s && (isalnum((wint_t)*(c - 1)) || *(c - 1) == '-'); c--);
    if (c > sel.s && *(c - 1) == '.') {
        sel.clazz = c;
        sel.clazzLen = sEnd - c;
        c--;
    }
    for (; c > sel.s && (isalnum((wint_t)*(c - 1)) || *(c - 1) == '-'); c--);
    if (sel.clazz - 1 == sel.s) {
        sel.tag = Tag_Any;
    }
    else if (c == (sel.clazz ? sel.clazz - 1 : sEnd) && c == sel.s + 1 && *sel.s == '*') {
        sel.tag = Tag_Any;
    }
    else if (c == sel.s) {
        sel.tag = FindHtmlTag(sel.s, sel.clazz ? sel.clazz - sel.s - 1 : sel.sLen);
    }

    return &sel;
}

const CssProperty *CssPullParser::NextProperty()
{
    if (currPos == s)
        inProps = true;
    else if (!inProps)
        return NULL;

GetNextProperty:
    SkipWsAndComment(currPos, end);
    if (currPos == end)
        return NULL;
    if (*currPos == '}') {
        currPos++;
        inProps = false;
        return NULL;
    }
    if (*currPos == ';') {
        currPos++;
        goto GetNextProperty;
    }
    const char *name = currPos;
    // skip identifier
    while (currPos < end && !str::IsWs(*currPos) && *currPos != ':' &&
        *currPos != ';' && *currPos != '}' && *currPos != '/') {
        currPos++;
    }
    SkipWsAndComment(currPos, end);
    if (currPos == end || *currPos != ':')
        goto GetNextProperty;
    prop.type = FindCssProp(name, currPos - name);
    currPos++;
    SkipWsAndComment(currPos, end);

    prop.s = currPos;
    // skip value
    const char *valEnd = currPos;
    while (currPos < end && *currPos != ';' && *currPos != '}') {
        if (*currPos == '"' || *currPos == '\'') {
            if (!SkipQuotedString(currPos, end))
                return NULL;
            valEnd = currPos;
        }
        else if (*currPos == '\\' && currPos < end - 1) {
            currPos += 2;
            valEnd = currPos;
        }
        else if (!SkipWsAndComment(currPos, end)) {
            valEnd = ++currPos;
        }
    }
    prop.sLen = valEnd - prop.s;

    return &prop;
}

#ifdef DEBUG
#include "CssParser_ut.cpp"
#endif
