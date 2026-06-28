/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"
#include "utils/Dpi.h"

#include "Theme.h"
#include "FilterHighlightDraw.h"

// approximate "is this UTF-8 byte part of a word character?": any byte >= 0x80
// is part of a multi-byte rune (CJK / Cyrillic / accented Latin -> treat as a
// word char); ASCII bytes use the same rule as the search engine's isWordChar()
static bool IsWordByte(u8 b) {
    if (b >= 0x80) {
        return true;
    }
    return IsCharAlphaNumericW((WCHAR)b) || b == '_';
}

void DrawMaybeHighlightedText(HDC hdc, RECT rc, Str text, const StrVec& filterWords, Vec<u8>& highlighted,
                              COLORREF colBg, bool isRtl, bool matchWholeWord, uint drawFmt) {
    int nWords = filterWords.Size();
    if (nWords == 0) {
        TempWStr textW = ToWStrTemp(text);
        DrawTextW(hdc, textW, -1, &rc, drawFmt);
        return;
    }

    // find all match ranges in text
    int textLen = text.len;
    u8* hl = highlighted.EnsureCap((size_t)textLen);
    memset(hl, 0, textLen);
    for (int w = 0; w < nWords; w++) {
        Str word = filterWords.At(w);
        int wordLen = word.len;
        if (wordLen == 0) {
            continue;
        }
        const char* p = text.s;
        while ((p = str::FindI(Str((char*)p, textLen - (int)(p - text.s)), word).s) != nullptr) {
            int off = (int)(p - text.s);
            int end = off + wordLen;
            // with "match whole word", skip occurrences that sit inside a larger
            // word so the snippet doesn't highlight non-matching substrings (e.g.
            // "cat" inside "category"). Mirrors TextSearch::MatchEnd's boundary
            // rule: a boundary is only required when both sides are word chars.
            bool wholeWordOk = true;
            if (matchWholeWord) {
                bool leftViolation = off > 0 && IsWordByte((u8)text.s[off - 1]) && IsWordByte((u8)text.s[off]);
                bool rightViolation = end < textLen && IsWordByte((u8)text.s[end - 1]) && IsWordByte((u8)text.s[end]);
                wholeWordOk = !leftViolation && !rightViolation;
            }
            if (wholeWordOk) {
                for (int k = 0; k < wordLen && off + k < textLen; k++) {
                    hl[off + k] = 1;
                }
            }
            p += wordLen;
        }
    }

    // collect contiguous highlighted ranges (up to 16)
    struct ByteRange {
        int start;
        int end;
    };
    ByteRange byteRanges[16];
    int nRanges = 0;
    {
        int pos = 0;
        while (pos < textLen && nRanges < 16) {
            if (hl[pos]) {
                int start = pos;
                while (pos < textLen && hl[pos]) {
                    pos++;
                }
                byteRanges[nRanges++] = {start, pos};
            } else {
                pos++;
            }
        }
    }

    TempWStr textW = ToWStrTemp(text);
    int textWLen = str::Leni(textW);

    // measure total string width for RTL positioning
    int strOriginX = rc.left;
    if (isRtl) {
        SIZE szTotal;
        GetTextExtentPoint32W(hdc, textW, textWLen, &szTotal);
        strOriginX = rc.right - szTotal.cx;
    }

    // compute pixel rectangles for each highlighted range
    RECT highlightRects[16];
    for (int i = 0; i < nRanges; i++) {
        TempWStr prefixToStart = ToWStrTemp(Str(text.s, byteRanges[i].start));
        int wStart = str::Leni(prefixToStart);
        TempWStr prefixToEnd = ToWStrTemp(Str(text.s, byteRanges[i].end));
        int wEnd = str::Leni(prefixToEnd);

        SIZE szStart, szEnd;
        GetTextExtentPoint32W(hdc, textW, wStart, &szStart);
        GetTextExtentPoint32W(hdc, textW, wEnd, &szEnd);

        highlightRects[i].top = rc.top;
        highlightRects[i].bottom = rc.bottom;
        highlightRects[i].left = strOriginX + szStart.cx;
        highlightRects[i].right = strOriginX + szEnd.cx;
    }

    // draw highlight background rectangles for matches
    {
        COLORREF highlightCol;
        if (IsCurrentThemeDefault()) {
            highlightCol = RGB(255, 255, 0); // yellow for default theme
        } else {
            highlightCol = AccentColor(colBg, 40);
        }
        HBRUSH hbrHighlight = CreateSolidBrush(highlightCol);
        for (int i = 0; i < nRanges; i++) {
            FillRect(hdc, &highlightRects[i], hbrHighlight);
        }
        DeleteObject(hbrHighlight);
    }

    // draw the whole string at once over the highlights
    DrawTextW(hdc, textW, -1, &rc, drawFmt);
}

bool FilterMatches(Str str, const StrVec& words) {
    int nWords = words.Size();
    for (int i = 0; i < nWords; i++) {
        Str word = words.At(i);
        if (!str::ContainsI(str, word)) {
            return false;
        }
    }
    return true;
}

void SplitFilterToWords(Str filter, StrVec& words) {
    char* s = str::DupTemp(filter);
    char* wordStart = s;
    bool wasWs = false;
    while (*s) {
        if (str::IsWs(*s)) {
            *s = 0;
            if (!wasWs) {
                AppendIfNotExists(&words, wordStart);
                wasWs = true;
            }
            wordStart = s + 1;
        }
        s++;
    }
    if (str::Leni(wordStart) > 0) {
        AppendIfNotExists(&words, wordStart);
    }
}