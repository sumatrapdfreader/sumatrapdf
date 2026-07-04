/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Win.h"
#include "base/Dpi.h"

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
    int nWords = len(filterWords);
    if (nWords == 0) {
        WCHAR* textW = CWStrTemp(text);
        DrawTextW(hdc, textW, -1, &rc, drawFmt);
        return;
    }

    // find all match ranges in text
    int textLen = text.len;
    u8* hl = highlighted.EnsureCap(textLen);
    memset(hl, 0, textLen);
    for (int w = 0; w < nWords; w++) {
        Str word = filterWords.At(w);
        int wordLen = word.len;
        if (len(word) == 0) {
            continue;
        }
        Str rest = text;
        while (rest) {
            int idx = str::IndexOfI(rest, word);
            if (idx < 0) {
                break;
            }
            int off = (int)(rest.s - text.s) + idx;
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
            rest = Str(text.s + end, textLen - end);
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
    int textWLen = len(textW);

    // measure total string width for RTL positioning
    int strOriginX = rc.left;
    if (isRtl) {
        SIZE szTotal;
        GetTextExtentPoint32W(hdc, textW.s, textWLen, &szTotal);
        strOriginX = rc.right - szTotal.cx;
    }

    // compute pixel rectangles for each highlighted range
    RECT highlightRects[16];
    for (int i = 0; i < nRanges; i++) {
        TempWStr prefixToStart = ToWStrTemp(Str(text.s, byteRanges[i].start));
        int wStart = len(prefixToStart);
        TempWStr prefixToEnd = ToWStrTemp(Str(text.s, byteRanges[i].end));
        int wEnd = len(prefixToEnd);

        SIZE szStart, szEnd;
        GetTextExtentPoint32W(hdc, textW.s, wStart, &szStart);
        GetTextExtentPoint32W(hdc, textW.s, wEnd, &szEnd);

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
            // highlightRects are computed from the full (untruncated) string, but
            // the text is drawn clipped/ellipsized to rc. Clip to rc so a match
            // in the truncated-away tail doesn't paint a stray box outside the label.
            RECT clipped;
            if (IntersectRect(&clipped, &highlightRects[i], &rc)) {
                FillRect(hdc, &clipped, hbrHighlight);
            }
        }
        DeleteObject(hbrHighlight);
    }

    // draw the whole string at once over the highlights
    DrawTextW(hdc, textW.s, -1, &rc, drawFmt);
}

bool FilterMatches(Str str, const StrVec& words) {
    int nWords = len(words);
    for (int i = 0; i < nWords; i++) {
        Str word = words.At(i);
        if (len(word) == 0) {
            continue;
        }
        if (!str::ContainsI(str, word)) {
            return false;
        }
    }
    return true;
}

void SplitFilterToWords(Str filter, StrVec& words) {
    int i = 0;
    while (i < filter.len && filter.s[i]) {
        while (i < filter.len && str::IsWs(filter.s[i])) {
            i++;
        }
        if (i >= filter.len || !filter.s[i]) {
            break;
        }
        int start = i;
        while (i < filter.len && filter.s[i] && !str::IsWs(filter.s[i])) {
            i++;
        }
        Str word(filter.s + start, i - start);
        if (len(word) > 0) {
            AppendIfNotExists(&words, word);
        }
    }
}
