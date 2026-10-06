/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Plain-text citation detection for PDFs without link annotations.

#include "base/Base.h"
#include "RefHover.h"

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
        xMin = std::min(r.x, xMin);
        xMax = std::max(r.x + r.dx, xMax);
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
    if (len(word) == 0 || !iswlower(word.s[0])) {
        return false;
    }
    for (WStr prefix : kNamePrefixes) {
        if (wstr::EqI(word, prefix)) {
            return true;
        }
    }
    return false;
}

static constexpr int kCitationMaxDistance = 30;

static int FindCursorGlyph(const Rect* coords, int textLen, Point pagePos) {
    int cursorIdx = -1;
    int bestDistSq = INT_MAX;
    for (int i = 0; i < textLen; i++) {
        Rect r = coords[i];
        if (r.dx <= 0 && r.dy <= 0) {
            continue;
        }
        int cx = r.x + (r.dx / 2);
        int cy = r.y + (r.dy / 2);
        int ddx = cx - pagePos.x;
        int ddy = cy - pagePos.y;
        int distSq = (ddx * ddx) + (ddy * ddy);
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            cursorIdx = i;
        }
    }
    return bestDistSq <= kCitationMaxDistance * kCitationMaxDistance ? cursorIdx : -1;
}

// Detect an author/year citation at pagePos; the caller frees surnameOut.
// text and coords are parallel views, one WCHAR/Rect per engine codepoint.
bool DetectCitationInPageText(WStr text, const Rect* coords, int textLen, Point pagePos, Str* surnameOut, int* yearOut,
                              Rect* srcRectOut) {
    *surnameOut = {};
    *yearOut = 0;
    if (len(text) == 0 || textLen <= 0 || !coords) {
        return false;
    }

    int cursorIdx = FindCursorGlyph(coords, textLen, pagePos);
    if (cursorIdx < 0) {
        return false;
    }

    // A three-line band includes wrapped citations near the cursor.
    int cursorY = coords[cursorIdx].y;
    int lineH = coords[cursorIdx].dy + 4;
    if (lineH < 12) {
        lineH = 14;
    }
    int yMin = cursorY - lineH - 2;
    int yMax = cursorY + (lineH * 2) + 2;

    // Collapse whitespace and line breaks so extraction artifacts such as
    // "et  al ." still match.
    WCHAR chunkScratch[512]{};
    wstr::Builder chunk;
    wstr::BuilderUseExternalBuffer(chunk, WStr(chunkScratch, dimofi(chunkScratch)));
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
                VecAppend(chunkGlyphs, -1);
                lastWasSpace = true;
            }
        } else {
            chunk.AppendChar(c);
            VecAppend(chunkGlyphs, i);
            lastWasSpace = false;
        }
        prevY = r.y;
    }
    if (cursorChunkPos < 0) {
        return false;
    }

    WStr s = ToWStr(chunk);
    int slen = s.len;

    // Prefer a year after the cursor, so hovering "Gu" in
    // "(Bashab, 2023; Gu, 2025)" chooses 2025 despite the closer 2023.
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
        int y =
            ((s.s[i] - L'0') * 1000) + ((s.s[i + 1] - L'0') * 100) + ((s.s[i + 2] - L'0') * 10) + (s.s[i + 3] - L'0');
        if (y < 1900 || y > 2050) {
            return -1;
        }
        return y;
    };

    constexpr int kYearAheadChars = 60;
    constexpr int kYearNearbyChars = 80;
    int bestYearPos = -1;
    int bestDist = INT_MAX;
    for (int i = 0; i + 4 <= slen; i++) {
        if (isYearAt(i) <= 0) {
            continue;
        }
        int dist = abs(i - cursorChunkPos);
        // A nearby following year wins over any preceding year.
        if (i >= cursorChunkPos && dist <= kYearAheadChars) {
            bestYearPos = i;
            break;
        }
        if (dist < bestDist) {
            bestDist = dist;
            bestYearPos = i;
        }
    }
    if (bestYearPos < 0 || abs(bestYearPos - cursorChunkPos) > kYearNearbyChars) {
        return false;
    }
    int year = isYearAt(bestYearPos);

    // Walk back from the year through punctuation to find the surname.
    int p = bestYearPos - 1;
    // Skip spaces and citation punctuation: ", " " ( ", " "
    while (p >= 0 && (s.s[p] == L' ' || s.s[p] == L'\t' || s.s[p] == L',' || s.s[p] == L'(' || s.s[p] == L'\n' ||
                      s.s[p] == L';')) {
        p--;
    }

    // Skip "et al" with optional spaces, period and an extraction-dropped 'a'.
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

    // Include lowercase prefixes and capitalized words in multi-word surnames,
    // e.g. "van der Berg" or "Oude Vrielink".
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
                // Keep short lowercase fragments between capitalized words:
                // extraction can split "Oude Vrielink" into "O d Vri li k".
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

    WStr surname(s.s + surnameStart, surnameEnd - surnameStart);
    while (len(surname) > 0 && wstr::ContainsChar(WStrL(L" .,"), surname.s[len(surname) - 1])) {
        surname.len--;
    }
    if (len(surname) < 2) {
        return false;
    }

    if (srcRectOut) {
        // The citation's horizontal span distinguishes occurrences on one line.
        *srcRectOut = CitationSpanBounds(coords, textLen, chunkGlyphs, surnameStart, bestYearPos + 4);
    }
    *surnameOut = ToUtf8(surname);
    *yearOut = year;
    return true;
}

// Find a bibliography entry with surnameW near its line start and year nearby.
// Return the matching line's first glyph position in xOut/yOut.
bool FindSurnameInPageText(WStr text, const Rect* coords, int textLen, WStr surnameW, int year, float* xOut,
                           float* yOut) {
    if (len(text) == 0 || textLen <= 0 || !coords || len(surnameW) == 0) {
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
        leftX = std::min(coords[i].x, leftX);
    }
    if (leftX == INT_MAX) {
        return false;
    }

    // Year as a wide string for searching.
    WCHAR yearStr[6];
    yearStr[0] = (WCHAR)(L'0' + ((year / 1000) % 10));
    yearStr[1] = (WCHAR)(L'0' + ((year / 100) % 10));
    yearStr[2] = (WCHAR)(L'0' + ((year / 10) % 10));
    yearStr[3] = (WCHAR)(L'0' + (year % 10));
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

        // Match at the column's left edge or within the first 30 characters,
        // allowing a detected fragment such as "Vri" in "Oude Vrielink".
        if (i != currentLineFirstIdx) {
            continue;
        }
        if (coords[i].x > leftX + 20) {
            continue;
        }
        int matchAt = -1;
        // Tier 1: strict line-start prefix.
        if (i + surnameLen <= textLen && wstr::StartsWithI(WStr(text.s + i, textLen - i), surnameW)) {
            matchAt = i;
        }
        // Tier 2: fragment match within the first ~30 chars of the line.
        // Only enabled for fragments >= 3 chars to limit false positives.
        if (matchAt < 0 && surnameLen >= 3) {
            int lineEnd = (i + 30 < textLen) ? i + 30 : textLen;
            for (int k = i + 1; k + surnameLen <= lineEnd; k++) {
                if (wstr::StartsWithI(WStr(text.s + k, textLen - k), surnameW)) {
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

// Pick the nearest number in a citation list/range, e.g. "[1, 2]" or "[3-5]".
// On success, set numOut and the optional bracket-span srcRectOut.
bool DetectNumericCitationInPageText(WStr text, const Rect* coords, int textLen, Point pagePos, int* numOut,
                                     Rect* srcRectOut) {
    *numOut = 0;
    if (len(text) == 0 || textLen <= 0 || !coords) {
        return false;
    }

    int cursorIdx = FindCursorGlyph(coords, textLen, pagePos);
    if (cursorIdx < 0) {
        return false;
    }

    // Reconstruct local reading order for wrapped lists: glyph stream order
    // can place "43]" far from the preceding "[42,".
    int lineTol = coords[cursorIdx].dy + 4;
    if (lineTol < 12) {
        lineTol = 14;
    }
    // Accept typographic dashes and minus signs in ranges such as "9–14".
    auto isListChar = [](WCHAR c) {
        return iswdigit(c) || c == L' ' || c == L'\t' || c == L',' || c == L'-' || c == L'\x2012' || c == L'\x2013' ||
               c == L'\x2014' || c == L'\x2212';
    };

    int blTol = (lineTol / 2 > 3) ? lineTol / 2 : 3;
    int cursorBL = coords[cursorIdx].y + coords[cursorIdx].dy;
    int cursorX = coords[cursorIdx].x;

    // Group by baseline and sort by x; keep the run overlapping [refLo, refHi].
    // Column gutters and scattered watermark baselines split unrelated text.
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
                int n = e - s + 1;
                memmove(out, out + s, n * sizeof(*out));
                *segLo = coords[out[0]].x;
                *segHi = coords[out[n - 1]].x + coords[out[n - 1]].dx;
                return n;
            }
            s = e + 1;
        }
        *segLo = 0;
        *segHi = -1;
        return 0;
    };

    constexpr int kSegCap = 512;
    int curSeg[kSegCap];
    int curLo = 0, curHi = -1;
    int curN = buildSegment(cursorBL, cursorX, cursorX, curSeg, kSegCap, &curLo, &curHi);
    if (curN <= 0) {
        return false;
    }
    // Adjacent lines must overlap the cursor line's x-range to stay in its column.
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
    int* seq = AllocArrayTemp<int>(m);
    memcpy(seq, prevSeg, prevN * sizeof(int));
    memcpy(seq + prevN, curSeg, curN * sizeof(int));
    memcpy(seq + prevN + curN, nextSeg, nextN * sizeof(int));
    int cursorPos = -1;
    for (int k = 0; k < m; k++) {
        if (seq[k] == cursorIdx) {
            cursorPos = k;
            break;
        }
    }
    if (cursorPos < 0) {
        return false;
    }

    // Walk the reconstructed order left to '[' and right to ']', through
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
        return false;
    }

    // Pick the number token nearest the cursor inside the brackets.
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
            val = (val * 10) + (text.s[seq[k]] - L'0');
            val = std::min(val, 99999); // guard against pathological runs
            k++;
        }
        int nearest = std::min(std::max(cursorPos, start), k - 1);
        int dist = abs(cursorPos - nearest);
        if (val >= 1 && val <= 9999 && dist < bestTokDist) {
            bestTokDist = dist;
            bestNum = val;
        }
    }
    if (bestNum <= 0) {
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
            xMin = std::min(r.x, xMin);
            sMinY = std::min(r.y, sMinY);
            xMax = std::max(r.x + r.dx, xMax);
            sMaxY = std::max(r.y + r.dy, sMaxY);
        }
        *srcRectOut = (xMin != INT_MAX) ? Rect{xMin, sMinY, xMax - xMin, sMaxY - sMinY} : Rect{};
    }
    *numOut = bestNum;
    return true;
}

// Find a "[num]" bibliography entry at a column's left edge.
// Return its opening bracket position in xOut/yOut.
bool FindNumericReferenceInPageText(WStr text, const Rect* coords, int textLen, int num, float* xOut, float* yOut) {
    if (len(text) == 0 || textLen <= 0 || !coords || num <= 0) {
        return false;
    }

    // Clear space left of "[N]" identifies entries in any column, regardless
    // of glyph stream order. kGap exceeds word spacing but fits column gutters.
    constexpr int kGap = 12;
    for (int i = 0; i < textLen; i++) {
        if (text.s[i] != L'[') {
            continue;
        }
        int j = i + 1;
        int val = 0;
        int nd = 0;
        while (j < textLen && iswdigit(text.s[j])) {
            val = (val * 10) + (text.s[j] - L'0');
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
