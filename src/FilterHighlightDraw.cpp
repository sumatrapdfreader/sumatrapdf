/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/ScopedWin.h"
#include "base/Win.h"
#include "gui/Dpi.h"

#include "gui/PlatformFont.h"
#include "gui/Gfx.h"

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

void DrawMaybeHighlightedText(Gfx* gfx, Rect rc, Str text, const StrVec& filterWords, Vec<u8>& highlighted, Color colBg,
                              bool isRtl, bool matchWholeWord, u32 drawFlags, PlatformFont* font, Color colText) {
    int nWords = len(filterWords);
    if (nWords == 0) {
        gfx->DrawText(text, rc, drawFlags, font, colText);
        return;
    }

    // find all match ranges in text
    int textLen = text.len;
    u8* hl = VecReserve(highlighted, textLen);
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

    // measure total string width for RTL positioning
    int strOriginX = rc.x;
    if (isRtl) {
        Size sizeTotal = gfx->MeasureText(text, font);
        strOriginX = rc.x + rc.dx - sizeTotal.dx;
    }

    // compute pixel rectangles for each highlighted range
    Rect highlightRects[16];
    for (int i = 0; i < nRanges; i++) {
        Size sizeStart = gfx->MeasureText(Str(text.s, byteRanges[i].start), font);
        Size sizeEnd = gfx->MeasureText(Str(text.s, byteRanges[i].end), font);
        highlightRects[i] = {strOriginX + sizeStart.dx, rc.y, sizeEnd.dx - sizeStart.dx, rc.dy};
    }

    // draw highlight background rectangles for matches
    {
        Color highlightCol;
        if (IsCurrentThemeDefault()) {
            highlightCol = kColYellow; // yellow for default theme
        } else {
            highlightCol = AccentColor(colBg, 40);
        }
        for (int i = 0; i < nRanges; i++) {
            // highlightRects are computed from the full (untruncated) string, but
            // the text is drawn clipped/ellipsized to rc. Clip to rc so a match
            // in the truncated-away tail doesn't paint a stray box outside the label.
            Rect clipped = highlightRects[i].Intersect(rc);
            if (!clipped.IsEmpty()) {
                gfx->FillRect(clipped, highlightCol);
            }
        }
    }

    // draw the whole string at once over the highlights
    gfx->DrawText(text, rc, drawFlags, font, colText);
}

// Ink that stays readable on a solid highlight underlay (black on yellow).
static Color TextColorContrasting(Color bg) {
    int lum = (GetRValue(bg) * 299 + GetGValue(bg) * 587 + GetBValue(bg) * 114) / 1000;
    return lum >= 140 ? kColBlack : kColWhite;
}

// Sample the row background the TreeView already painted (indent/icon strip).
// Falls back to kColorUnset if GetPixel fails.
static Color SamplePaintedRowBackground(HDC hdc, Rect itemRc) {
    if (itemRc.IsEmpty()) {
        return kColorUnset;
    }
    int x = itemRc.x + 2;
    if (x >= itemRc.x + itemRc.dx) {
        x = itemRc.x;
    }
    int y = itemRc.y + (itemRc.dy / 2);
    Color c = GetPixel(hdc, x, y);
    if (c == CLR_INVALID) {
        return kColorUnset;
    }
    return c;
}

// Colors for clearing/redrawing a TreeView label after default paint.
// Selected+focus: system highlight. Selected unfocused: themed accent of
// treeBg (not COLOR_BTNFACE — unreadable with light text in dark mode).
// Non-selected: sample the painted row / treeBg / theme control bg.
// treeBg/treeTxt are TreeView::bgColor/textColor (may be unset).
// itemRc is the full row rect (TreeView_GetItemRect with textOnly=FALSE).
void ResolveTreeFilterItemColors(HDC hdc, Rect itemRc, Color treeBg, Color treeTxt, bool isSelected, bool hasFocus,
                                 Color* bgOut, Color* txtOut) {
    if (!bgOut || !txtOut) {
        ReportIf(true);
        return;
    }
    if (isSelected && hasFocus) {
        *bgOut = GetSysColor(COLOR_HIGHLIGHT);
        *txtOut = GetSysColor(COLOR_HIGHLIGHTTEXT);
        return;
    }
    if (isSelected) {
        // Selected but unfocused (or multi-match "current page" rows): use a
        // subtle accent of the themed tree background. Explorer-themed TreeView
        // inactive selection is often a light gray even in dark mode; combined
        // with light theme text that made the initial bookmark highlight
        // unreadable (issue #5848). COLOR_BTNFACE has the same problem when
        // system colors are not fully remapped.
        Color base = !IsSpecialColor(treeBg) ? treeBg : ThemeControlBackgroundColor();
        *bgOut = AccentColor(base, 40);
        *txtOut = IsSpecialColor(treeTxt) ? ThemeWindowTextColor() : treeTxt;
        return;
    }

    // Non-selected: match the already-painted themed row, not COLOR_WINDOW
    // (white), which washed out dark/blue-ish sidebar backgrounds.
    Color sampled = SamplePaintedRowBackground(hdc, itemRc);
    if (!IsSpecialColor(sampled)) {
        *bgOut = sampled;
    } else if (!IsSpecialColor(treeBg)) {
        *bgOut = treeBg;
    } else {
        *bgOut = ThemeControlBackgroundColor();
    }
    *txtOut = IsSpecialColor(treeTxt) ? ThemeWindowTextColor() : treeTxt;
}

// TreeView post-paint: repaint the label with multi-word match underlays
// (command-palette style). `font` should be the tree's font (WM_GETFONT) so
// extents match the control's text; pass nullptr to keep the HDC font.
void DrawTreeItemFilterHighlight(Gfx* gfx, Rect labelRect, Str text, const StrVec& filterWords, Color bgCol,
                                 Color txtCol, PlatformFont* font) {
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
        return;
    }

    Size sizeFull = gfx->MeasureText(text, font);
    // center underlay height on the glyph height (labelRect can be taller than
    // the font, which made yellow bars spill into neighboring rows)
    int textTop = labelRect.y + ((labelRect.dy - sizeFull.dy) / 2);
    textTop = std::max(textTop, labelRect.y);
    int textBottom = textTop + sizeFull.dy;
    if (textBottom > labelRect.y + labelRect.dy) {
        textBottom = labelRect.y + labelRect.dy;
        textTop = textBottom - sizeFull.dy;
        textTop = std::max(textTop, labelRect.y);
    }

    // clear label so we do not stack on top of the control's text
    gfx->FillRect(labelRect, bgCol);

    Color highlightCol;
    if (IsCurrentThemeDefault()) {
        highlightCol = kColYellow;
    } else {
        highlightCol = AccentColor(bgCol, 40);
    }
    for (int i = 0; i < nRanges; i++) {
        Size sizeStart = gfx->MeasureText(Str(text.s, byteRanges[i].start), font);
        Size sizeEnd = gfx->MeasureText(Str(text.s, byteRanges[i].end), font);
        Rect hr{labelRect.x + sizeStart.dx, textTop, sizeEnd.dx - sizeStart.dx, textBottom - textTop};
        Rect clipped = hr.Intersect(labelRect);
        if (!clipped.IsEmpty()) {
            gfx->FillRect(clipped, highlightCol);
        }
    }

    // Draw non-match runs in the row text color (white when selected+focused);
    // match runs use ink that contrasts with the underlay so yellow+white does
    // not wash out. Prefix extents keep run x positions aligned with underlays.
    Color matchTxtCol = TextColorContrasting(highlightCol);
    int pos = 0;
    while (pos < textLen) {
        bool isHl = hl[pos] != 0;
        int start = pos;
        while (pos < textLen && (hl[pos] != 0) == isHl) {
            pos++;
        }
        Size sizeStart = gfx->MeasureText(Str(text.s, start), font);
        Str run(text.s + start, pos - start);
        if (len(run) == 0) {
            continue;
        }
        Rect runRect = labelRect;
        runRect.x = labelRect.x + sizeStart.dx;
        runRect.y = textTop;
        runRect.dy = textBottom - textTop;
        gfx->DrawText(run, runRect, gfxTextSingleLine | gfxTextNoClip, font, isHl ? matchTxtCol : txtCol);
    }
}
