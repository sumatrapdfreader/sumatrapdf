/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Pure-function plain-text citation detectors used by RefHoverText for PDFs
// without hyperref links. Split out of RefHoverDetect so the heuristics can be
// unit-tested with synthetic glyph arrays (see src/base/tests/RefHover_ut.cpp)
// without pulling in the engine, HWND, or rendering layers.

#include "base/Base.h"
#include "RefHoverTextDetect.h"

#include <wctype.h>

// === Plain-text citation detection ===

// Lowercase name-prefix particles that are part of a multi-word surname
// (e.g. "van der Berg", "de la Cruz"). Match case-insensitively.
static const WStr kNamePrefixes[] = {
    WStrL(L"van"), WStrL(L"von"), WStrL(L"de"),  WStrL(L"der"), WStrL(L"den"), WStrL(L"la"), WStrL(L"le"),
    WStrL(L"el"),  WStrL(L"al"),  WStrL(L"da"),  WStrL(L"du"),  WStrL(L"di"),  WStrL(L"do"), WStrL(L"bin"),
    WStrL(L"ben"), WStrL(L"te"),  WStrL(L"ten"), WStrL(L"ter"), WStrL(L"op"),  WStrL(L"'t"), WStrL(L"af"),
    WStrL(L"av"),  WStrL(L"zu"),  WStrL(L"san"), WStrL(L"st"),  WStrL(L"st."),
};

// Bounding box of glyphs [startIdx..endIdx] on one text line.
static Rect GlyphSpanBounds(const Rect* coords, int textLen, int startIdx, int endIdx) {
    int xMin = INT_MAX;
    int xMax = INT_MIN;
    int y = 0;
    int dy = 0;
    bool any = false;
    for (int i = startIdx; i <= endIdx && i < textLen; i++) {
        Rect r = coords[i];
        if (r.dx <= 0 && r.dy <= 0) {
            continue;
        }
        if (!any) {
            y = r.y;
            dy = r.dy;
            any = true;
        }
        if (r.x < xMin) {
            xMin = r.x;
        }
        if (r.x + r.dx > xMax) {
            xMax = r.x + r.dx;
        }
    }
    if (!any) {
        return {};
    }
    return Rect{xMin, y, xMax - xMin, dy};
}

// Map a chunk-text span back to glyph indices (see DetectCitationInPageText)
// and return the matched citation's horizontal extent on the page.
static Rect CitationSpanBounds(const Rect* coords, int textLen, const Vec<int>& chunkGlyphs, int spanStart,
                               int spanEnd) {
    int gStart = -1;
    int gEnd = -1;
    for (int ci = spanStart; ci < spanEnd && ci < len(chunkGlyphs); ci++) {
        int gi = chunkGlyphs[ci];
        if (gi < 0 || gi >= textLen) {
            continue;
        }
        if (gStart < 0) {
            gStart = gi;
        }
        gEnd = gi;
    }
    if (gStart < 0) {
        return {};
    }
    return GlyphSpanBounds(coords, textLen, gStart, gEnd);
}

static bool IsNamePrefix(WStr word) {
    if (!word || !iswlower(word.s[0])) {
        return false;
    }
    for (WStr prefix : kNamePrefixes) {
        if (word.len == prefix.len && _wcsnicmp(word.s, prefix.s, (size_t)prefix.len) == 0) {
            return true;
        }
    }
    return false;
}

// Detect a "(Surname et al., 2020)" / "Surname (2020)" citation pattern at
// pagePos in a page's glyph arrays. On success, returns true and fills
// *surnameOut with a freshly-allocated UTF-8 surname (caller frees) and
// *yearOut with the 4-digit year.
bool DetectCitationInPageText(WStr text, const Rect* coords, int textLen, Point pagePos, Str* surnameOut, int* yearOut,
                              Rect* srcRectOut) {
    *surnameOut = {};
    *yearOut = 0;
    if (!text || textLen <= 0 || !coords) {
        return false;
    }

    // 1. Find the glyph nearest the cursor (within ~30 pt).
    int cursorIdx = -1;
    int bestDistSq = INT_MAX;
    for (int i = 0; i < textLen; i++) {
        Rect r = coords[i];
        if (r.dx <= 0 && r.dy <= 0) {
            continue;
        }
        int cx = r.x + r.dx / 2;
        int cy = r.y + r.dy / 2;
        int ddx = cx - pagePos.x;
        int ddy = cy - pagePos.y;
        int distSq = ddx * ddx + ddy * ddy;
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            cursorIdx = i;
        }
    }
    if (cursorIdx < 0 || bestDistSq > 30 * 30) {
        return false;
    }

    // 2. Build a 3-line text band around the cursor's line. Replace
    // line-break transitions with spaces so the regex-ish matcher sees
    // wrapped citations as a single span.
    int cursorY = coords[cursorIdx].y;
    int lineH = coords[cursorIdx].dy + 4;
    if (lineH < 12) {
        lineH = 14;
    }
    int yMin = cursorY - lineH - 2;
    int yMax = cursorY + lineH * 2 + 2;

    // Word-generated PDFs frequently emit kerning-driven inter-glyph spaces
    // ("et  al .", "Geneti c  Al gorith ms"), which break naive pattern
    // matching. Collapse runs of whitespace to a single space so downstream
    // checks (the "et al." literal, walk-back stop conditions) work against
    // normalized text. Line breaks also become a single space.
    wstr::Builder chunk;
    Vec<int> chunkGlyphs;
    int cursorChunkPos = -1;
    int prevY = INT_MIN;
    bool lastWasSpace = true; // suppress leading whitespace
    for (int i = 0; i < textLen; i++) {
        Rect r = coords[i];
        if (r.y < yMin || r.y > yMax) {
            continue;
        }
        WCHAR c = text.s[i];
        bool isLineBreak = (prevY != INT_MIN && r.y > prevY + 2);
        bool isSpace = isLineBreak || c == L' ' || c == L'\t' || c == L'\n' || c == L'\r';
        if (i == cursorIdx) {
            cursorChunkPos = len(chunk);
        }
        if (isSpace) {
            if (!lastWasSpace) {
                chunk.AppendChar(L' ');
                chunkGlyphs.Append(-1);
                lastWasSpace = true;
            }
        } else {
            chunk.AppendChar(c);
            chunkGlyphs.Append(i);
            lastWasSpace = false;
        }
        prevY = r.y;
    }
    if (cursorChunkPos < 0) {
        return false;
    }

    WStr s = ToWStr(chunk);
    int slen = s.len;

    // 3. Find a 4-digit year near the cursor. Prefer a year that comes
    // *after* the cursor (within 60 chars) — for "(Bashab et al., 2023; Gu
    // et al., 2025)" with cursor on "Gu", the year after Gu (2025) is the
    // intended one even though 2023 is closer in raw chunk distance.
    auto isYearAt = [&](int i) -> int {
        if (i + 4 > slen) {
            return -1;
        }
        if (!iswdigit(s.s[i]) || !iswdigit(s.s[i + 1]) || !iswdigit(s.s[i + 2]) || !iswdigit(s.s[i + 3])) {
            return -1;
        }
        if (i + 4 < slen && iswdigit(s.s[i + 4])) {
            return -1;
        }
        if (i > 0 && iswdigit(s.s[i - 1])) {
            return -1;
        }
        int y = (s.s[i] - L'0') * 1000 + (s.s[i + 1] - L'0') * 100 + (s.s[i + 2] - L'0') * 10 + (s.s[i + 3] - L'0');
        if (y < 1900 || y > 2050) {
            return -1;
        }
        return y;
    };

    int bestYearPos = -1;
    // First pass: nearest year strictly after the cursor.
    for (int i = cursorChunkPos; i + 4 <= slen && i - cursorChunkPos <= 60; i++) {
        if (isYearAt(i) > 0) {
            bestYearPos = i;
            break;
        }
    }
    // Fallback: nearest year regardless of side.
    if (bestYearPos < 0) {
        int bestDist = INT_MAX;
        for (int i = 0; i + 4 <= slen; i++) {
            if (isYearAt(i) <= 0) {
                continue;
            }
            int dist = abs(i - cursorChunkPos);
            if (dist < bestDist) {
                bestDist = dist;
                bestYearPos = i;
            }
        }
        if (bestYearPos < 0 || abs(bestYearPos - cursorChunkPos) > 80) {
            return false;
        }
    }
    int year = (s.s[bestYearPos] - L'0') * 1000 + (s.s[bestYearPos + 1] - L'0') * 100 +
               (s.s[bestYearPos + 2] - L'0') * 10 + (s.s[bestYearPos + 3] - L'0');

    // 4. Walk back from the year through punctuation to find the surname.
    int p = bestYearPos - 1;
    // Skip spaces and citation punctuation: ", " " ( ", " "
    while (p >= 0 && (s.s[p] == L' ' || s.s[p] == L'\t' || s.s[p] == L',' || s.s[p] == L'(' || s.s[p] == L'\n' ||
                      s.s[p] == L';')) {
        p--;
    }

    // Optionally skip "et al" / "et al." / "et l" (extraction dropped 'a')
    // and space-before-period variants. The trailing period and the 'a' of
    // "al" may both be absent.
    {
        int q = p;
        if (q >= 0 && s.s[q] == L'.') {
            q--;
            while (q >= 0 && s.s[q] == L' ') {
                q--;
            }
        }
        if (q >= 4 && (s.s[q] == L'l' || s.s[q] == L'L')) {
            int r = q - 1;
            // Optional 'a' — drop tolerated.
            if (r >= 0 && (s.s[r] == L'a' || s.s[r] == L'A')) {
                r--;
            }
            // Spaces between "et" and "al"/"l".
            while (r >= 0 && s.s[r] == L' ') {
                r--;
            }
            // "et" required.
            if (r >= 1 && (s.s[r] == L't' || s.s[r] == L'T') && (s.s[r - 1] == L'e' || s.s[r - 1] == L'E')) {
                p = r - 2;
                while (p >= 0 && s.s[p] == L' ') {
                    p--;
                }
            }
        }
    }

    // 5. Walk back through name-part characters, accepting lowercase prefix
    // particles AND additional capitalized words (multi-word surnames like
    // "Oude Vrielink", "van der Berg", "El Mansouri").
    int surnameEnd = p + 1; // exclusive
    while (p >= 0) {
        WCHAR c = s.s[p];
        if (iswalpha(c) || c == L'-' || c == L'\'') {
            p--;
        } else if (c == L' ') {
            // Look at the word before this space.
            int wordEnd = p - 1;
            while (wordEnd >= 0 && s.s[wordEnd] == L' ') {
                wordEnd--;
            }
            int wordStart = wordEnd;
            while (wordStart >= 0 && (iswalpha(s.s[wordStart]) || s.s[wordStart] == L'\'' || s.s[wordStart] == L'-')) {
                wordStart--;
            }
            wordStart++;
            if (wordEnd < wordStart) {
                break;
            }
            int wordLen = wordEnd - wordStart + 1;
            WCHAR firstChar = s.s[wordStart];
            bool isLower = iswlower(firstChar);
            bool isUpper = iswupper(firstChar);
            // Stop on connectors like "and", "&", or non-name words.
            if (isLower && !IsNamePrefix(WStr(s.s + wordStart, wordLen))) {
                // Extraction artifact tolerance: a single 1-2 char lowercase
                // token between two capitalized words is likely a mangled
                // glyph from the real surname ("Oude" → "O d", "Vrielink"
                // → "Vri li k"). Peek further back; if another capitalized
                // word sits within ~6 chars, treat the short token as
                // continuation.
                bool peekCap = false;
                if (wordLen <= 2) {
                    int peek = wordStart - 1;
                    while (peek >= 0 && s.s[peek] == L' ') {
                        peek--;
                    }
                    int peekEnd = peek;
                    while (peekEnd >= 0 && (iswalpha(s.s[peekEnd]) || s.s[peekEnd] == L'\'' || s.s[peekEnd] == L'-')) {
                        peekEnd--;
                    }
                    int peekStart = peekEnd + 1;
                    int peekLen = peek - peekEnd;
                    if (peekLen > 0 && iswupper(s.s[peekStart])) {
                        peekCap = true;
                    }
                }
                if (!peekCap) {
                    break;
                }
            }
            if (!isLower && !isUpper) {
                break;
            }
            // Continue: include this word as part of the surname.
            p = wordStart - 1;
        } else {
            break;
        }
    }
    int surnameStart = p + 1;
    if (surnameEnd <= surnameStart) {
        return false;
    }

    // Sanity: must start with an uppercase letter and be at least 2 chars.
    while (surnameStart < surnameEnd && (s.s[surnameStart] == L' ' || s.s[surnameStart] == L'.')) {
        surnameStart++;
    }
    if (surnameEnd - surnameStart < 2 || !iswupper(s.s[surnameStart])) {
        return false;
    }

    // Build surname string.
    wstr::Builder surnameW;
    for (int j = surnameStart; j < surnameEnd; j++) {
        surnameW.AppendChar(s.s[j]);
    }
    while (len(surnameW) > 0) {
        WCHAR last = surnameW.LastChar();
        if (last == L' ' || last == L'.' || last == L',') {
            surnameW.RemoveLast();
        } else {
            break;
        }
    }
    if (len(surnameW) < 2) {
        return false;
    }

    if (srcRectOut) {
        // stable per-occurrence key: horizontal span of the matched citation
        // (surname through year), so two same-reference markers on one line
        // reposition the popup instead of sharing a line-only y/dy key.
        *srcRectOut = CitationSpanBounds(coords, textLen, chunkGlyphs, surnameStart, bestYearPos + 4);
    }
    *surnameOut = ToUtf8(ToWStr(surnameW));
    *yearOut = year;
    return true;
}

// Search a page's glyph arrays for a line whose first word(s) match
// `surnameW` and where `year` appears within the next ~5 lines (the entry).
// Returns true on hit and fills xOut/yOut with the entry's anchor (top-left
// of the surname's first glyph).
bool FindSurnameInPageText(WStr text, const Rect* coords, int textLen, WStr surnameW, int year, float* xOut,
                           float* yOut) {
    if (!text || textLen <= 0 || !coords || !surnameW) {
        return false;
    }
    int surnameLen = surnameW.len;

    // Determine the page's leftmost text X (= bibliography column left edge).
    int leftX = INT_MAX;
    for (int i = 0; i < textLen; i++) {
        WCHAR c = text.s[i];
        if (c == L' ' || c == L'\t' || c == L'\n' || c == L'\r') {
            continue;
        }
        if (coords[i].x < leftX) {
            leftX = coords[i].x;
        }
    }
    if (leftX == INT_MAX) {
        return false;
    }

    // Year as a wide string for searching.
    WCHAR yearStr[6];
    yearStr[0] = (WCHAR)(L'0' + (year / 1000) % 10);
    yearStr[1] = (WCHAR)(L'0' + (year / 100) % 10);
    yearStr[2] = (WCHAR)(L'0' + (year / 10) % 10);
    yearStr[3] = (WCHAR)(L'0' + year % 10);
    yearStr[4] = 0;

    int prevY = INT_MIN;
    int currentLineFirstIdx = -1;
    for (int i = 0; i < textLen; i++) {
        WCHAR c = text.s[i];
        if (c == L' ' || c == L'\t' || c == L'\n' || c == L'\r') {
            continue;
        }
        bool isNewLine = (coords[i].y > prevY + 2);
        if (isNewLine) {
            currentLineFirstIdx = i;
        }
        prevY = coords[i].y;

        // Consider line-start glyphs at (or near) the page's leftmost X —
        // the typical bibliography hanging-indent layout. Try matching the
        // surname both as a strict line-start prefix AND within the first
        // ~30 chars of the line (covers fragmented extraction where the
        // detected fragment is only part of a multi-word real surname like
        // "Vri" → "Oude Vrielink"; the line starts with "Oude" but "Vri"
        // appears a few chars in).
        if (i != currentLineFirstIdx) {
            continue;
        }
        if (coords[i].x > leftX + 20) {
            continue;
        }
        int matchAt = -1;
        // Tier 1: strict line-start prefix.
        if (i + surnameLen <= textLen && _wcsnicmp(text.s + i, surnameW.s, (size_t)surnameLen) == 0) {
            matchAt = i;
        }
        // Tier 2: fragment match within the first ~30 chars of the line.
        // Only enabled for fragments >= 3 chars to limit false positives.
        if (matchAt < 0 && surnameLen >= 3) {
            int lineEnd = (i + 30 < textLen) ? i + 30 : textLen;
            for (int k = i + 1; k + surnameLen <= lineEnd; k++) {
                if (_wcsnicmp(text.s + k, surnameW.s, (size_t)surnameLen) == 0) {
                    // Require token boundary (not preceded by another letter).
                    if (iswalpha(text.s[k - 1])) {
                        continue;
                    }
                    matchAt = k;
                    break;
                }
            }
        }
        if (matchAt < 0) {
            continue;
        }
        // Verify the year appears within the next ~6 lines worth of glyphs
        // (~600 chars, generous).
        int scanEnd = (matchAt + 600 < textLen) ? matchAt + 600 : textLen;
        bool yearFound = false;
        for (int j = matchAt + surnameLen; j + 4 <= scanEnd; j++) {
            if (text.s[j] == yearStr[0] && text.s[j + 1] == yearStr[1] && text.s[j + 2] == yearStr[2] &&
                text.s[j + 3] == yearStr[3]) {
                if (j > 0 && iswdigit(text.s[j - 1])) {
                    continue;
                }
                if (j + 4 < scanEnd && iswdigit(text.s[j + 4])) {
                    continue;
                }
                yearFound = true;
                break;
            }
        }
        if (!yearFound) {
            continue;
        }
        *xOut = (float)coords[i].x;
        *yOut = (float)coords[i].y;
        return true;
    }
    return false;
}

// === Numeric "[N]" citation detection ===

bool DetectNumericCitationInPageText(WStr text, const Rect* coords, int textLen, Point pagePos, int* numOut,
                                     Rect* srcRectOut) {
    *numOut = 0;
    if (!text || textLen <= 0 || !coords) {
        return false;
    }

    // 1. Find the glyph nearest the cursor (within ~30 pt).
    int cursorIdx = -1;
    int bestDistSq = INT_MAX;
    for (int i = 0; i < textLen; i++) {
        Rect r = coords[i];
        if (r.dx <= 0 && r.dy <= 0) {
            continue;
        }
        int cx = r.x + r.dx / 2;
        int cy = r.y + r.dy / 2;
        int ddx = cx - pagePos.x;
        int ddy = cy - pagePos.y;
        int distSq = ddx * ddx + ddy * ddy;
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            cursorIdx = i;
        }
    }
    if (cursorIdx < 0 || bestDistSq > 30 * 30) {
        return false;
    }

    int cursorY = coords[cursorIdx].y;
    int lineTol = coords[cursorIdx].dy + 4;
    if (lineTol < 12) {
        lineTol = 14;
    }
    // A numeric citation list may wrap across a line break ("[8, 17,\n18]"), so
    // the "[ ... ]" walk must span a couple of lines. It can't walk by array
    // index, though: some PDFs emit a wrapped number far from its '[' in the
    // glyph stream (observed: the glyph before "43]" sat 196pt above, not at
    // the "[42," one line up). So reconstruct *local reading order* — gather
    // glyphs in a vertical band around the cursor and order them
    // top-to-bottom / left-to-right — then walk that. "[42," (end of one line)
    // then sits immediately before "43]" (start of the next), while the
    // out-of-order stream neighbour falls outside the band.
    // Digits, separators, and dash forms used in page ranges. Beyond the ASCII
    // hyphen, accept the Unicode figure/en/em dash and minus sign, since
    // typeset ranges ("9–14") use an en-dash, not '-'.
    auto isListChar = [](WCHAR c) {
        return iswdigit(c) || c == L' ' || c == L'\t' || c == L',' || c == L'-' || c == L'\x2012' || c == L'\x2013' ||
               c == L'\x2014' || c == L'\x2212';
    };

    int blTol = (lineTol / 2 > 3) ? lineTol / 2 : 3;
    int cursorBL = coords[cursorIdx].y + coords[cursorIdx].dy;
    int cursorX = coords[cursorIdx].x;

    // Build one text line's column-limited segment: glyph indices whose baseline
    // (y+dy, stable across a line) is within blTol of targetBL, sorted
    // left-to-right, then restricted to the contiguous x-run (splits at gaps
    // wider than a column gutter) that overlaps [refLo, refHi]. This keeps the
    // neighbouring column — and the diagonal watermark, whose glyphs sit on
    // their own scattered baselines — out of the segment. A whole-Y-band
    // reconstruction can't: it merges both columns into one logical line and
    // buries a wrapped citation's "[" behind the other column's text.
    constexpr int kColGap = 16;
    auto buildSegment = [&](int targetBL, int refLo, int refHi, int* out, int cap, int* segLo, int* segHi) -> int {
        int cnt = 0;
        for (int i = 0; i < textLen && cnt < cap; i++) {
            if (abs((coords[i].y + coords[i].dy) - targetBL) <= blTol) {
                out[cnt++] = i;
            }
        }
        for (int a = 1; a < cnt; a++) { // insertion sort by x
            int v = out[a];
            int vx = coords[v].x;
            int b = a - 1;
            while (b >= 0 && coords[out[b]].x > vx) {
                out[b + 1] = out[b];
                b--;
            }
            out[b + 1] = v;
        }
        int chosenS = -1, chosenE = -1;
        int s = 0;
        while (s < cnt) {
            int e = s;
            while (e + 1 < cnt) {
                int gap = coords[out[e + 1]].x - (coords[out[e]].x + coords[out[e]].dx);
                if (gap > kColGap) {
                    break;
                }
                e++;
            }
            int runLo = coords[out[s]].x;
            int runHi = coords[out[e]].x + coords[out[e]].dx;
            if (runHi >= refLo && runLo <= refHi) {
                chosenS = s;
                chosenE = e;
                break;
            }
            s = e + 1;
        }
        if (chosenS < 0) {
            *segLo = 0;
            *segHi = -1;
            return 0;
        }
        int n = chosenE - chosenS + 1;
        for (int t = 0; t < n; t++) {
            out[t] = out[chosenS + t];
        }
        *segLo = coords[out[0]].x;
        *segHi = coords[out[n - 1]].x + coords[out[n - 1]].dx;
        return n;
    };

    constexpr int kSegCap = 512;
    int curSeg[kSegCap];
    int curLo = 0, curHi = -1;
    int curN = buildSegment(cursorBL, cursorX, cursorX, curSeg, kSegCap, &curLo, &curHi);
    if (curN <= 0) {
        return false;
    }
    // Nearest text line above / below in the same column (a glyph overlapping
    // the cursor line's x-range), so a wrapped citation's other half is
    // reachable without pulling in the neighbouring column.
    int prevBL = INT_MIN, nextBL = INT_MAX;
    for (int i = 0; i < textLen; i++) {
        int gx = coords[i].x;
        int gxr = gx + coords[i].dx;
        if (gxr < curLo - kColGap || gx > curHi + kColGap) {
            continue;
        }
        int bl = coords[i].y + coords[i].dy;
        if (bl < cursorBL - blTol && bl > prevBL) {
            prevBL = bl;
        }
        if (bl > cursorBL + blTol && bl < nextBL) {
            nextBL = bl;
        }
    }
    int prevSeg[kSegCap];
    int nextSeg[kSegCap];
    int pLo, pHi, nLo, nHi;
    int prevN = (prevBL != INT_MIN) ? buildSegment(prevBL, curLo, curHi, prevSeg, kSegCap, &pLo, &pHi) : 0;
    int nextN = (nextBL != INT_MAX) ? buildSegment(nextBL, curLo, curHi, nextSeg, kSegCap, &nLo, &nHi) : 0;

    // Reading order: previous line, cursor line, next line — each sorted by x.
    int m = prevN + curN + nextN;
    int* seq = AllocArray<int>(m);
    {
        int w = 0;
        for (int t = 0; t < prevN; t++) {
            seq[w++] = prevSeg[t];
        }
        for (int t = 0; t < curN; t++) {
            seq[w++] = curSeg[t];
        }
        for (int t = 0; t < nextN; t++) {
            seq[w++] = nextSeg[t];
        }
    }
    auto cleanup = [&]() { free(seq); };
    int cursorPos = -1;
    for (int k = 0; k < m; k++) {
        if (seq[k] == cursorIdx) {
            cursorPos = k;
            break;
        }
    }
    if (cursorPos < 0) {
        cleanup();
        return false;
    }

    // 2. Walk the reconstructed order left to '[' and right to ']', through
    // citation-list chars only (a letter ends the walk, bounding it).
    int openPos = -1;
    for (int k = cursorPos; k >= 0; k--) {
        WCHAR c = text.s[seq[k]];
        if (c == L'[') {
            openPos = k;
            break;
        }
        if (c == L']' && k == cursorPos) {
            continue; // cursor landed on the closing ']'
        }
        if (!isListChar(c)) {
            break;
        }
    }
    if (openPos < 0) {
        cleanup();
        return false;
    }
    int closePos = -1;
    for (int k = openPos + 1; k < m; k++) {
        WCHAR c = text.s[seq[k]];
        if (c == L']') {
            closePos = k;
            break;
        }
        if (!isListChar(c)) {
            break;
        }
    }
    if (closePos <= openPos + 1) {
        cleanup();
        return false;
    }

    // 3. Pick the number token nearest the cursor inside the brackets.
    int bestNum = 0;
    int bestTokDist = INT_MAX;
    int k = openPos + 1;
    while (k < closePos) {
        if (!iswdigit(text.s[seq[k]])) {
            k++;
            continue;
        }
        int start = k;
        int val = 0;
        while (k < closePos && iswdigit(text.s[seq[k]])) {
            val = val * 10 + (text.s[seq[k]] - L'0');
            if (val > 99999) {
                val = 99999; // guard against pathological runs
            }
            k++;
        }
        int end = k - 1;
        int dist;
        if (cursorPos < start) {
            dist = start - cursorPos;
        } else if (cursorPos > end) {
            dist = cursorPos - end;
        } else {
            dist = 0;
        }
        if (val >= 1 && val <= 9999 && dist < bestTokDist) {
            bestTokDist = dist;
            bestNum = val;
        }
    }
    if (bestNum <= 0) {
        cleanup();
        return false;
    }
    if (srcRectOut) {
        // stable per-occurrence key: bounds of the "[ ... ]" span (may cover
        // two lines when the list wraps).
        int xMin = INT_MAX, sMinY = INT_MAX, xMax = INT_MIN, sMaxY = INT_MIN;
        for (int p = openPos; p <= closePos; p++) {
            Rect r = coords[seq[p]];
            if (r.dx <= 0 && r.dy <= 0) {
                continue;
            }
            if (r.x < xMin) {
                xMin = r.x;
            }
            if (r.y < sMinY) {
                sMinY = r.y;
            }
            if (r.x + r.dx > xMax) {
                xMax = r.x + r.dx;
            }
            if (r.y + r.dy > sMaxY) {
                sMaxY = r.y + r.dy;
            }
        }
        *srcRectOut = (xMin != INT_MAX) ? Rect{xMin, sMinY, xMax - xMin, sMaxY - sMinY} : Rect{};
    }
    cleanup();
    *numOut = bestNum;
    return true;
}

bool FindNumericReferenceInPageText(WStr text, const Rect* coords, int textLen, int num, float* xOut, float* yOut) {
    if (!text || textLen <= 0 || !coords || num <= 0) {
        return false;
    }

    // An entry "[N]" starts at a column's left edge: there is clear horizontal
    // space immediately to its left (the page margin, or the gutter of a
    // 2-column list). Detecting this gap is order-independent and works for any
    // column, unlike a single global-leftmost-X test (which makes a 2-column
    // list's right column unreachable) or a reading-order "new line" test
    // (which misses a column whose glyph stream starts at a smaller y than the
    // previous column's tail). kGap is wider than inter-word spacing but
    // narrower than a column gutter / hanging indent.
    constexpr int kGap = 12;
    for (int i = 0; i < textLen; i++) {
        if (text.s[i] != L'[') {
            continue;
        }
        int j = i + 1;
        int val = 0;
        int nd = 0;
        while (j < textLen && iswdigit(text.s[j])) {
            val = val * 10 + (text.s[j] - L'0');
            j++;
            nd++;
        }
        if (nd == 0 || j >= textLen || text.s[j] != L']') {
            continue;
        }
        if (val != num) {
            continue;
        }
        // Reject a mid-line "[num]" (body-text citation): require clear space
        // immediately left of the "[" on its own line.
        int yi = coords[i].y;
        int xi = coords[i].x;
        int yTol = coords[i].dy > 6 ? coords[i].dy : 8;
        bool hasLeftNeighbour = false;
        for (int k = 0; k < textLen; k++) {
            WCHAR c = text.s[k];
            if (c == L' ' || c == L'\t' || c == L'\n' || c == L'\r') {
                continue;
            }
            Rect r = coords[k];
            if (abs(r.y - yi) > yTol) {
                continue;
            }
            if (r.x < xi && r.x + r.dx > xi - kGap) {
                hasLeftNeighbour = true;
                break;
            }
        }
        if (hasLeftNeighbour) {
            continue;
        }
        *xOut = (float)xi;
        *yOut = (float)yi;
        return true;
    }
    return false;
}
