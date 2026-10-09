/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "gui/GpuiBridge.h"

#include "Theme.h"
#include "FilterHighlightDraw.h"
#include "FilterUtil.h"

// non-default themes: a light yellow still readable on a dark row
constexpr Color kFilterMatchBg = MkRgb(0xff, 0xee, 0x70);

// approximate "is this UTF-8 byte part of a word character?": any byte >= 0x80
// is part of a multi-byte rune (CJK / Cyrillic / accented Latin -> treat as a
// word char); ASCII bytes use the same rule as the search engine's isWordChar()
static bool IsWordByte(u8 b) {
    if (b >= 0x80) {
        return true;
    }
#if OS_WIN
    return IsCharAlphaNumericW((WCHAR)b) || b == '_';
#else
    // ng: no IsCharAlphaNumericW off Windows (as in EngineMupdf.cpp)
    return iswalnum((wint_t)b) != 0 || b == '_';
#endif
}

void MarkFilterHighlights(Str text, const StrVec& filterWords, Vec<u8>& out, bool matchWholeWord) {
    VecReset(out);
    int textLen = text.len;
    if (textLen == 0) {
        return;
    }
    // the callers index `out` up to its length
    if (!VecResize(out, textLen)) {
        return;
    }
    u8* hl = out.els;
    memset(hl, 0, (size_t)textLen);
    int nWords = len(filterWords);
    if (nWords == 0) {
        return;
    }
    for (int w = 0; w < nWords; w++) {
        Str word = filterWords[w];
        if (len(word) == 0) {
            continue;
        }
        Str rest = text;
        while (rest) {
            int wordLen = 0;
            int idx = FilterIndexOf(rest, word, &wordLen);
            if (idx < 0 || wordLen <= 0) {
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
}

gp::El* FilterHighlightText(gp::Ctx* cx, Str text, const StrVec& filterWords, gp::Rgba fg, float fontSize,
                            int boldOffset, int boldLen) {
    Vec<u8> marks;
    MarkFilterHighlights(text, filterWords, marks);
    gp::El* row = gp::Div(cx->a)->FlexRow()->ItemsCenter()->MinW(0);
    int n = len(text);
    int i = 0;
    while (i < n) {
        bool hl = len(marks) > i && marks[i] != 0;
        bool bold = boldOffset >= 0 && i >= boldOffset && i < boldOffset + boldLen;
        int j = i;
        while (j < n) {
            bool hl2 = len(marks) > j && marks[j] != 0;
            bool bold2 = boldOffset >= 0 && j >= boldOffset && j < boldOffset + boldLen;
            if (hl2 != hl || bold2 != bold) {
                break;
            }
            j++;
        }
        gp::El* span = gp::TextEl(cx->a, GpuiDup(cx->a, Str(text.s + i, j - i)))->Font(fontSize)->Fg(fg);
        if (bold) {
            span->Bold();
        }
        if (hl) {
            // default theme uses the same yellow as orig's filter underlay
            Color bg = IsCurrentThemeDefault() ? kColYellow : kFilterMatchBg;
            span->Bg(ToGpui(bg))->Fg(ToGpui(kColBlack));
        }
        row->Child(span);
        i = j;
    }
    return row;
}
