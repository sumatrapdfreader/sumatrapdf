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
        Str word = filterWords[w];
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

// Ink that stays readable on a solid highlight underlay (black on yellow).
static COLORREF TextColorContrasting(COLORREF bg) {
    int lum = (GetRValue(bg) * 299 + GetGValue(bg) * 587 + GetBValue(bg) * 114) / 1000;
    return lum >= 140 ? RGB(0, 0, 0) : RGB(255, 255, 255);
}

// Sample the row background the TreeView already painted (indent/icon strip).
// Falls back to kColorUnset if GetPixel fails.
static COLORREF SamplePaintedRowBackground(HDC hdc, RECT itemRc) {
    if (itemRc.right <= itemRc.left || itemRc.bottom <= itemRc.top) {
        return kColorUnset;
    }
    int x = itemRc.left + 2;
    if (x >= itemRc.right) {
        x = itemRc.left;
    }
    int y = (itemRc.top + itemRc.bottom) / 2;
    COLORREF c = GetPixel(hdc, x, y);
    if (c == CLR_INVALID) {
        return kColorUnset;
    }
    return c;
}

void ResolveTreeFilterItemColors(HDC hdc, RECT itemRc, COLORREF treeBg, COLORREF treeTxt, bool isSelected,
                                 bool hasFocus, COLORREF* bgOut, COLORREF* txtOut) {
    ReportIf(!bgOut || !txtOut);
    if (isSelected && hasFocus) {
        *bgOut = GetSysColor(COLOR_HIGHLIGHT);
        *txtOut = GetSysColor(COLOR_HIGHLIGHTTEXT);
        return;
    }
    if (isSelected) {
        // Selected but unfocused: keep system inactive-selection face; text from
        // the tree/theme so dark themes don't fall back to pure black on gray.
        *bgOut = GetSysColor(COLOR_BTNFACE);
        *txtOut = IsSpecialColor(treeTxt) ? ThemeWindowTextColor() : treeTxt;
        return;
    }

    // Non-selected: match the already-painted themed row, not COLOR_WINDOW
    // (white), which washed out dark/blue-ish sidebar backgrounds.
    COLORREF sampled = SamplePaintedRowBackground(hdc, itemRc);
    if (!IsSpecialColor(sampled)) {
        *bgOut = sampled;
    } else if (!IsSpecialColor(treeBg)) {
        *bgOut = treeBg;
    } else {
        *bgOut = ThemeControlBackgroundColor();
    }
    *txtOut = IsSpecialColor(treeTxt) ? ThemeWindowTextColor() : treeTxt;
}

void DrawTreeItemFilterHighlight(HDC hdc, RECT labelRect, Str text, const StrVec& filterWords, COLORREF bgCol,
                                 COLORREF txtCol, HFONT font) {
    // TreeView has already painted the row. We repaint only the text label:
    // solid bg (selection or window) so themed double-draw artifacts go away,
    // yellow/accent underlays for each match word, then the string in runs so
    // match glyphs use ink that contrasts with the underlay (black on yellow).
    // Drawing the whole label in selection white over yellow made matches
    // disappear on the focused selected row.
    // Use the tree's font for GetTextExtentPoint32 / DrawText or the bars
    // misalign and look oversized relative to the control's text.
    if (!text || len(text) == 0 || len(filterWords) == 0) {
        return;
    }

    HFONT oldFont = nullptr;
    if (font) {
        oldFont = (HFONT)SelectObject(hdc, font);
    }

    int textLen = text.len;
    u8* hl = AllocArrayTemp<u8>(textLen);
    for (int w = 0; w < len(filterWords); w++) {
        Str word = filterWords[w];
        int wordLen = word.len;
        if (wordLen == 0) {
            continue;
        }
        Str rest = text;
        while (len(rest) > 0) {
            int idx = str::IndexOfI(rest, word);
            if (idx < 0) {
                break;
            }
            int off = (int)(rest.s - text.s) + idx;
            for (int k = 0; k < wordLen && off + k < textLen; k++) {
                hl[off + k] = 1;
            }
            int skip = idx + wordLen;
            rest.s += skip;
            rest.len -= skip;
        }
    }

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
    if (nRanges == 0) {
        if (oldFont) {
            SelectObject(hdc, oldFont);
        }
        return;
    }

    TempWStr textW = ToWStrTemp(text);
    SIZE szFull{};
    GetTextExtentPoint32W(hdc, textW.s, textW.len, &szFull);
    // center underlay height on the glyph height (labelRect can be taller than
    // the font, which made yellow bars spill into neighboring rows)
    int textTop = labelRect.top + ((labelRect.bottom - labelRect.top) - szFull.cy) / 2;
    if (textTop < labelRect.top) {
        textTop = labelRect.top;
    }
    int textBottom = textTop + szFull.cy;
    if (textBottom > labelRect.bottom) {
        textBottom = labelRect.bottom;
        textTop = textBottom - szFull.cy;
        if (textTop < labelRect.top) {
            textTop = labelRect.top;
        }
    }

    // clear label so we do not stack on top of the control's text
    HBRUSH hbrBg = CreateSolidBrush(bgCol);
    FillRect(hdc, &labelRect, hbrBg);
    DeleteObject(hbrBg);

    COLORREF highlightCol;
    if (IsCurrentThemeDefault()) {
        highlightCol = RGB(255, 255, 0);
    } else {
        highlightCol = AccentColor(bgCol, 40);
    }
    HBRUSH hbrHighlight = CreateSolidBrush(highlightCol);
    for (int i = 0; i < nRanges; i++) {
        TempWStr prefixToStart = ToWStrTemp(Str(text.s, byteRanges[i].start));
        TempWStr prefixToEnd = ToWStrTemp(Str(text.s, byteRanges[i].end));
        SIZE szStart, szEnd;
        GetTextExtentPoint32W(hdc, textW.s, len(prefixToStart), &szStart);
        GetTextExtentPoint32W(hdc, textW.s, len(prefixToEnd), &szEnd);
        RECT hr{labelRect.left + szStart.cx, textTop, labelRect.left + szEnd.cx, textBottom};
        RECT clipped;
        if (IntersectRect(&clipped, &hr, &labelRect)) {
            FillRect(hdc, &clipped, hbrHighlight);
        }
    }
    DeleteObject(hbrHighlight);

    // Draw non-match runs in the row text color (white when selected+focused);
    // match runs use ink that contrasts with the underlay so yellow+white does
    // not wash out. Prefix extents keep run x positions aligned with underlays.
    COLORREF matchTxtCol = TextColorContrasting(highlightCol);
    int oldBkMode = SetBkMode(hdc, TRANSPARENT);
    COLORREF oldTxtCol = SetTextColor(hdc, txtCol);
    int pos = 0;
    while (pos < textLen) {
        bool isHl = hl[pos] != 0;
        int start = pos;
        while (pos < textLen && (hl[pos] != 0) == isHl) {
            pos++;
        }
        TempWStr prefixToStart = ToWStrTemp(Str(text.s, start));
        SIZE szStart{};
        GetTextExtentPoint32W(hdc, textW.s, len(prefixToStart), &szStart);
        TempWStr runW = ToWStrTemp(Str(text.s + start, pos - start));
        if (len(runW) == 0) {
            continue;
        }
        RECT runRc = labelRect;
        runRc.left = labelRect.left + szStart.cx;
        runRc.top = textTop;
        runRc.bottom = textBottom;
        SetTextColor(hdc, isHl ? matchTxtCol : txtCol);
        DrawTextW(hdc, runW.s, -1, &runRc, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP);
    }
    SetBkMode(hdc, oldBkMode);
    SetTextColor(hdc, oldTxtCol);

    if (oldFont) {
        SelectObject(hdc, oldFont);
    }
}

bool FilterMatches(Str str, const StrVec& words) {
    int nWords = len(words);
    for (int i = 0; i < nWords; i++) {
        Str word = words[i];
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
