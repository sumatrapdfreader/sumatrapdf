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
        if (SkipUntil(s, end, "*/"))
            SkipWs(s, end);
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

const CssProperty *CssPullParser::NextProp()
{
GetNextProp:
    SkipWsAndComment(currPos, end);
    if (currPos == end || *currPos == '}')
        return NULL;
    if (*currPos == ';') {
        currPos++;
        goto GetNextProp;
    }
    const char *name = currPos;
    // skip identifier
    while (currPos < end && !str::IsWs(*currPos) && *currPos != ':' &&
        *currPos != ';' && *currPos != '}' && *currPos != '/') {
        currPos++;
    }
    SkipWsAndComment(currPos, end);
    if (currPos == end || *currPos != ':')
        goto GetNextProp;
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
        else if (*currPos == '\\') {
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
