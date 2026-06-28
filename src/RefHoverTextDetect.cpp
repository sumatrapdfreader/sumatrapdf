/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Pure-function plain-text citation detectors used by RefHoverText for PDFs
// without hyperref links. Split out of RefHoverDetect so the heuristics can be
// unit-tested with synthetic glyph arrays (see src/utils/tests/RefHover_ut.cpp)
// without pulling in the engine, HWND, or rendering layers.

#include "utils/BaseUtil.h"
#include "RefHoverTextDetect.h"

#include <wctype.h>

// === Plain-text citation detection ===

// Lowercase name-prefix particles that are part of a multi-word surname
// (e.g. "van der Berg", "de la Cruz"). Match case-insensitively.
static const WCHAR* kNamePrefixes[] = {L"van", L"von", L"de", L"der", L"den", L"la",  L"le", L"el",  L"al",
                                       L"da",  L"du",  L"di", L"do",  L"bin", L"ben", L"te", L"ten", L"ter",
                                       L"op",  L"'t",  L"af", L"av",  L"zu",  L"san", L"st", L"st.", nullptr};

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
    for (int ci = spanStart; ci < spanEnd && ci < (int)chunkGlyphs.size(); ci++) {
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

static bool IsNamePrefix(const WCHAR* word, int wordLen) {
    if (wordLen == 0 || !iswlower(word[0])) {
        return false;
    }
    for (int i = 0; kNamePrefixes[i]; i++) {
        int plen = (int)wcslen(kNamePrefixes[i]);
        if (wordLen == plen && _wcsnicmp(word, kNamePrefixes[i], plen) == 0) {
            return true;
        }
    }
    return false;
}

// Detect a "(Surname et al., 2020)" / "Surname (2020)" citation pattern at
// pagePos in a page's glyph arrays. On success, returns true and fills
// *surnameOut with a freshly-allocated UTF-8 surname (caller frees) and
// *yearOut with the 4-digit year.
bool DetectCitationInPageText(const WCHAR* text, const Rect* coords, int textLen, Point pagePos, char** surnameOut,
                              int* yearOut, Rect* srcRectOut) {
    *surnameOut = nullptr;
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
    WStrBuilder chunk;
    Vec<int> chunkGlyphs;
    int cursorChunkPos = -1;
    int prevY = INT_MIN;
    bool lastWasSpace = true; // suppress leading whitespace
    for (int i = 0; i < textLen; i++) {
        Rect r = coords[i];
        if (r.y < yMin || r.y > yMax) {
            continue;
        }
        WCHAR c = text[i];
        bool isLineBreak = (prevY != INT_MIN && r.y > prevY + 2);
        bool isSpace = isLineBreak || c == L' ' || c == L'\t' || c == L'\n' || c == L'\r';
        if (i == cursorIdx) {
            cursorChunkPos = (int)chunk.size();
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

    const WCHAR* s = chunk.Get();
    int slen = (int)chunk.size();

    // 3. Find a 4-digit year near the cursor. Prefer a year that comes
    // *after* the cursor (within 60 chars) — for "(Bashab et al., 2023; Gu
    // et al., 2025)" with cursor on "Gu", the year after Gu (2025) is the
    // intended one even though 2023 is closer in raw chunk distance.
    auto isYearAt = [&](int i) -> int {
        if (i + 4 > slen) {
            return -1;
        }
        if (!iswdigit(s[i]) || !iswdigit(s[i + 1]) || !iswdigit(s[i + 2]) || !iswdigit(s[i + 3])) {
            return -1;
        }
        if (i + 4 < slen && iswdigit(s[i + 4])) {
            return -1;
        }
        if (i > 0 && iswdigit(s[i - 1])) {
            return -1;
        }
        int y = (s[i] - L'0') * 1000 + (s[i + 1] - L'0') * 100 + (s[i + 2] - L'0') * 10 + (s[i + 3] - L'0');
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
    int year = (s[bestYearPos] - L'0') * 1000 + (s[bestYearPos + 1] - L'0') * 100 + (s[bestYearPos + 2] - L'0') * 10 +
               (s[bestYearPos + 3] - L'0');

    // 4. Walk back from the year through punctuation to find the surname.
    int p = bestYearPos - 1;
    // Skip spaces and citation punctuation: ", " " ( ", " "
    while (p >= 0 && (s[p] == L' ' || s[p] == L'\t' || s[p] == L',' || s[p] == L'(' || s[p] == L'\n' || s[p] == L';')) {
        p--;
    }

    // Optionally skip "et al" / "et al." / "et l" (extraction dropped 'a')
    // and space-before-period variants. The trailing period and the 'a' of
    // "al" may both be absent.
    {
        int q = p;
        if (q >= 0 && s[q] == L'.') {
            q--;
            while (q >= 0 && s[q] == L' ') {
                q--;
            }
        }
        if (q >= 4 && (s[q] == L'l' || s[q] == L'L')) {
            int r = q - 1;
            // Optional 'a' — drop tolerated.
            if (r >= 0 && (s[r] == L'a' || s[r] == L'A')) {
                r--;
            }
            // Spaces between "et" and "al"/"l".
            while (r >= 0 && s[r] == L' ') {
                r--;
            }
            // "et" required.
            if (r >= 1 && (s[r] == L't' || s[r] == L'T') && (s[r - 1] == L'e' || s[r - 1] == L'E')) {
                p = r - 2;
                while (p >= 0 && s[p] == L' ') {
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
        WCHAR c = s[p];
        if (iswalpha(c) || c == L'-' || c == L'\'') {
            p--;
        } else if (c == L' ') {
            // Look at the word before this space.
            int wordEnd = p - 1;
            while (wordEnd >= 0 && s[wordEnd] == L' ') {
                wordEnd--;
            }
            int wordStart = wordEnd;
            while (wordStart >= 0 && (iswalpha(s[wordStart]) || s[wordStart] == L'\'' || s[wordStart] == L'-')) {
                wordStart--;
            }
            wordStart++;
            if (wordEnd < wordStart) {
                break;
            }
            int wordLen = wordEnd - wordStart + 1;
            WCHAR firstChar = s[wordStart];
            bool isLower = iswlower(firstChar);
            bool isUpper = iswupper(firstChar);
            // Stop on connectors like "and", "&", or non-name words.
            if (isLower && !IsNamePrefix(s + wordStart, wordLen)) {
                // Extraction artifact tolerance: a single 1-2 char lowercase
                // token between two capitalized words is likely a mangled
                // glyph from the real surname ("Oude" → "O d", "Vrielink"
                // → "Vri li k"). Peek further back; if another capitalized
                // word sits within ~6 chars, treat the short token as
                // continuation.
                bool peekCap = false;
                if (wordLen <= 2) {
                    int peek = wordStart - 1;
                    while (peek >= 0 && s[peek] == L' ') {
                        peek--;
                    }
                    int peekEnd = peek;
                    while (peekEnd >= 0 && (iswalpha(s[peekEnd]) || s[peekEnd] == L'\'' || s[peekEnd] == L'-')) {
                        peekEnd--;
                    }
                    int peekStart = peekEnd + 1;
                    int peekLen = peek - peekEnd;
                    if (peekLen > 0 && iswupper(s[peekStart])) {
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
    while (surnameStart < surnameEnd && (s[surnameStart] == L' ' || s[surnameStart] == L'.')) {
        surnameStart++;
    }
    if (surnameEnd - surnameStart < 2 || !iswupper(s[surnameStart])) {
        return false;
    }

    // Build surname string.
    WStrBuilder surnameW;
    for (int j = surnameStart; j < surnameEnd; j++) {
        surnameW.AppendChar(s[j]);
    }
    while (surnameW.size() > 0) {
        WCHAR last = surnameW.LastChar();
        if (last == L' ' || last == L'.' || last == L',') {
            surnameW.RemoveLast();
        } else {
            break;
        }
    }
    if (surnameW.size() < 2) {
        return false;
    }

    if (srcRectOut) {
        // stable per-occurrence key: horizontal span of the matched citation
        // (surname through year), so two same-reference markers on one line
        // reposition the popup instead of sharing a line-only y/dy key.
        *srcRectOut = CitationSpanBounds(coords, textLen, chunkGlyphs, surnameStart, bestYearPos + 4);
    }
    *surnameOut = ToUtf8(surnameW.Get());
    *yearOut = year;
    return true;
}

// Search a page's glyph arrays for a line whose first word(s) match
// `surnameW` and where `year` appears within the next ~5 lines (the entry).
// Returns true on hit and fills xOut/yOut with the entry's anchor (top-left
// of the surname's first glyph).
bool FindSurnameInPageText(const WCHAR* text, const Rect* coords, int textLen, const WCHAR* surnameW, int surnameLen,
                           int year, float* xOut, float* yOut) {
    if (!text || textLen <= 0 || !coords) {
        return false;
    }

    // Determine the page's leftmost text X (= bibliography column left edge).
    int leftX = INT_MAX;
    for (int i = 0; i < textLen; i++) {
        WCHAR c = text[i];
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
        WCHAR c = text[i];
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
        if (i + surnameLen <= textLen && _wcsnicmp(text + i, surnameW, (size_t)surnameLen) == 0) {
            matchAt = i;
        }
        // Tier 2: fragment match within the first ~30 chars of the line.
        // Only enabled for fragments >= 3 chars to limit false positives.
        if (matchAt < 0 && surnameLen >= 3) {
            int lineEnd = (i + 30 < textLen) ? i + 30 : textLen;
            for (int k = i + 1; k + surnameLen <= lineEnd; k++) {
                if (_wcsnicmp(text + k, surnameW, (size_t)surnameLen) == 0) {
                    // Require token boundary (not preceded by another letter).
                    if (iswalpha(text[k - 1])) {
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
            if (text[j] == yearStr[0] && text[j + 1] == yearStr[1] && text[j + 2] == yearStr[2] &&
                text[j + 3] == yearStr[3]) {
                if (j > 0 && iswdigit(text[j - 1])) {
                    continue;
                }
                if (j + 4 < scanEnd && iswdigit(text[j + 4])) {
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

bool DetectNumericCitationInPageText(const WCHAR* text, const Rect* coords, int textLen, Point pagePos, int* numOut,
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

    // 2. The cursor must sit inside a "[ ... ]" bracket on its own line. Walk
    // left to '[' and right to ']', staying on the cursor's text line and
    // allowing only the chars that make up a numeric citation list
    // ("[1]", "[1, 2]", "[3-5]").
    int cursorY = coords[cursorIdx].y;
    int lineTol = coords[cursorIdx].dy + 4;
    if (lineTol < 12) {
        lineTol = 14;
    }
    auto sameLine = [&](int i) { return abs(coords[i].y - cursorY) <= lineTol; };
    auto isListChar = [](WCHAR c) { return iswdigit(c) || c == L' ' || c == L'\t' || c == L',' || c == L'-'; };

    int open = -1;
    for (int i = cursorIdx; i >= 0 && sameLine(i); i--) {
        WCHAR c = text[i];
        if (c == L'[') {
            open = i;
            break;
        }
        // Tolerate the cursor landing on the closing ']' itself.
        if (c == L']' && i == cursorIdx) {
            continue;
        }
        if (!isListChar(c)) {
            break;
        }
    }
    if (open < 0) {
        return false;
    }
    int close = -1;
    for (int i = open + 1; i < textLen && sameLine(i); i++) {
        WCHAR c = text[i];
        if (c == L']') {
            close = i;
            break;
        }
        if (!isListChar(c)) {
            break;
        }
    }
    if (close <= open + 1) {
        return false;
    }

    // 3. Pick the number token nearest the cursor inside the brackets.
    int bestNum = 0;
    int bestTokDist = INT_MAX;
    int i = open + 1;
    while (i < close) {
        if (!iswdigit(text[i])) {
            i++;
            continue;
        }
        int start = i;
        int val = 0;
        while (i < close && iswdigit(text[i])) {
            val = val * 10 + (text[i] - L'0');
            if (val > 99999) {
                val = 99999; // guard against pathological runs
            }
            i++;
        }
        int end = i - 1;
        int dist;
        if (cursorIdx < start) {
            dist = start - cursorIdx;
        } else if (cursorIdx > end) {
            dist = cursorIdx - end;
        } else {
            dist = 0;
        }
        if (val >= 1 && val <= 9999 && dist < bestTokDist) {
            bestTokDist = dist;
            bestNum = val;
        }
    }
    if (bestNum <= 0) {
        return false;
    }
    if (srcRectOut) {
        // stable per-occurrence key: the "[N]" bracket span on this line
        *srcRectOut = GlyphSpanBounds(coords, textLen, open, close);
    }
    *numOut = bestNum;
    return true;
}

bool FindNumericReferenceInPageText(const WCHAR* text, const Rect* coords, int textLen, int num, float* xOut,
                                    float* yOut) {
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
        if (text[i] != L'[') {
            continue;
        }
        int j = i + 1;
        int val = 0;
        int nd = 0;
        while (j < textLen && iswdigit(text[j])) {
            val = val * 10 + (text[j] - L'0');
            j++;
            nd++;
        }
        if (nd == 0 || j >= textLen || text[j] != L']') {
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
            WCHAR c = text[k];
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
