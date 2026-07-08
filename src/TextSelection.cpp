/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "DocController.h"
#include "TreeModel.h"
#include "EngineBase.h"
#include "TextSelection.h"

uint distSq(int x, int y) {
    return x * x + y * y;
}
// underscore is mainly used for programming and is thus considered a word character
bool isWordChar(int c) {
#if OS_WIN
    return (c > 0 && c <= 0xffff && IsCharAlphaNumericW((WCHAR)c)) || c == '_';
#else
    return (c > 0 && c <= 0xffff && iswalnum((wint_t)c)) || c == '_';
#endif
}

static bool isDigit(int c) {
    return c >= '0' && c <= '9';
}

TextSelection::TextSelection(EngineBase* engine) : engine(engine) {}

TextSelection::~TextSelection() {
    Reset();
}

void TextSelection::Reset() {
    result.len = 0;
    result.cap = 0;
    free(result.pages);
    result.pages = nullptr;
    free(result.rects);
    result.rects = nullptr;
    wordStartPage = wordStartGlyph = wordEndPage = wordEndGlyph = -1;
}

// returns the index of the glyph closest to the right of the given coordinates
// (i.e. when over the right half of a glyph, the returned index will be for the
// glyph following it, which will be the first glyph (not) to be selected)
static int FindClosestGlyph(TextSelection* ts, int pageNo, double x, double y) {
    Rect* coords;
    int textLen = 0;
    Str text = ts->engine->GetTextForPage(pageNo, &textLen, &coords);
    PointF pt = PointF((float)x, (float)y);

    unsigned int maxDist = UINT_MAX;
    Point pti = ToPoint(pt);
    bool overGlyph = false;
    int result = -1;

    for (int i = 0; i < textLen; i++) {
        Rect& coord = coords[i];
        if (!coord.x && !coord.dx) {
            continue;
        }
        if (overGlyph && !coord.Contains(pti)) {
            continue;
        }

        uint dist = distSq((int)x - coord.x - coord.dx / 2, (int)y - coord.y - coord.dy / 2);
        if (dist < maxDist) {
            result = i;
            maxDist = dist;
        }
        // prefer glyphs the cursor is actually over
        if (!overGlyph && coord.Contains(pti)) {
            overGlyph = true;
            result = i;
            maxDist = dist;
        }
    }

    if (-1 == result) {
        return 0;
    }
    ReportIf(result < 0 || result >= textLen);

    // the result indexes the first glyph to be selected in a forward selection
    RectF bbox = ts->engine->Transform(ToRectF(coords[result]), pageNo, 1.0, 0);
    pt = ts->engine->Transform(pt, pageNo, 1.0, 0);
    if (pt.x > bbox.x + 0.5 * bbox.dx) {
        result++;
        // for some (DjVu) documents, all glyphs of a word share the same bbox
        while (result < textLen && coords[result - 1] == coords[result]) {
            result++;
        }
    }
    ReportIf(result > 0 && result < textLen && coords[result] == coords[result - 1]);

    return result;
}

static void FillResultRects(TextSelection* ts, int pageNo, int glyph, int length, StrVec* lines = nullptr) {
    Rect* coords;
    int textLen = 0;
    Str text = ts->engine->GetTextForPage(pageNo, &textLen, &coords);
    ReportIf(textLen < glyph + length);
    Rect mediabox = ts->engine->PageMediabox(pageNo).Round();
    Rect *c = &coords[glyph], *end = c + length;
    while (c < end) {
        // skip line breaks
        for (; c < end && !c->x && !c->dx; c++) {
            // no-op
        }

        Rect bbox, *c0 = c;
        for (; c < end && (c->x || c->dx); c++) {
            bbox = bbox.Union(*c);
        }
        bbox = bbox.Intersect(mediabox);
        // skip text that's completely outside a page's mediabox
        if (bbox.IsEmpty()) {
            continue;
        }

        if (lines) {
            Str s = Utf8SliceByCodepoints(text, (int)(c0 - coords), (int)(c - c0));
            lines->Append(s);
            continue;
        }

        // cut the right edge, if it overlaps the next character
        if (c < coords + textLen && (c->x || c->dx) && bbox.x < c->x && bbox.x + bbox.dx > c->x) {
            bbox.dx = c->x - bbox.x;
        }

        int currLen = ts->result.len;
        int left = ts->result.cap - currLen;
        ReportIf(left < 0);
        if (left == 0) {
            int newCap = ts->result.cap * 2;
            if (newCap < 64) {
                newCap = 64;
            }
            int* newPages = (int*)realloc(ts->result.pages, sizeof(int) * newCap);
            Rect* newRects = (Rect*)realloc(ts->result.rects, sizeof(Rect) * newCap);
            ReportIf(!newPages);
            ReportIf(!newRects);
            ts->result.pages = newPages;
            ts->result.rects = newRects;
            ts->result.cap = newCap;
        }

        ts->result.pages[currLen] = pageNo;
        ts->result.rects[currLen] = bbox;
        ts->result.len++;
    }
}

bool TextSelection::IsOverGlyph(int pageNo, double x, double y) {
    Rect* coords;
    int textLen = 0;
    if (!engine->TryGetTextForPage(pageNo, &textLen, &coords)) {
        return false;
    }

    int glyphIx = FindClosestGlyph(this, pageNo, x, y);
    Point pt = ToPoint(PointF((float)x, (float)y));
    // when over the right half of a glyph, FindClosestGlyph returns the
    // index of the next glyph, in which case glyphIx must be decremented
    if (glyphIx == textLen || !coords[glyphIx].Contains(pt)) {
        glyphIx--;
    }
    if (-1 == glyphIx) {
        return false;
    }
    return coords[glyphIx].Contains(pt);
}

int TextSelection::FindClosestGlyphAt(int pageNo, double x, double y) {
    return FindClosestGlyph(this, pageNo, x, y);
}

void TextSelection::StartAt(int pageNo, int glyphIx) {
    startPage = pageNo;
    startGlyph = glyphIx;
    if (glyphIx < 0) {
        int textLen = 0;
        engine->GetTextForPage(pageNo, &textLen);
        startGlyph += textLen + 1;
    }
}

void TextSelection::StartAt(int pageNo, double x, double y) {
    StartAt(pageNo, FindClosestGlyph(this, pageNo, x, y));
}

void TextSelection::SelectUpTo(int pageNo, double x, double y) {
    SelectUpTo(pageNo, FindClosestGlyph(this, pageNo, x, y));
}

void TextSelection::SelectUpTo(int pageNo, int glyphIx) {
    if (startPage == -1 || startGlyph == -1) {
        return;
    }

    endPage = pageNo;
    endGlyph = glyphIx;
    if (glyphIx < 0) {
        int textLen = 0;
        engine->GetTextForPage(pageNo, &textLen);
        endGlyph = textLen + glyphIx + 1;
    }

    result.len = 0;
    int fromPage = std::min(startPage, endPage), toPage = std::max(startPage, endPage);
    int fromGlyph = (fromPage == endPage ? endGlyph : startGlyph);
    int toGlyph = (fromPage == endPage ? startGlyph : endGlyph);
    if (fromPage == toPage && fromGlyph > toGlyph) {
        std::swap(fromGlyph, toGlyph);
    }

    for (int page = fromPage; page <= toPage; page++) {
        int textLen = 0;
        engine->GetTextForPage(page, &textLen);

        int glyph = page == fromPage ? fromGlyph : 0;
        int length = (page == toPage ? toGlyph : textLen) - glyph;
        if (length > 0) {
            FillResultRects(this, page, glyph, length);
        }
    }
}

// extend backward across comma-separated digit groups (e.g. "1,234,567")
// returns the new start position if valid grouping found, otherwise returns curStart
static int ExtendBackAcrossCommaGroups(Str text, int curStart) {
    int pos = curStart;
    int posByte = Utf8CodepointToByteIndex(text, pos);
    while (pos >= 2) {
        int commaByte = posByte;
        int c = Utf8CodepointPrev(text, commaByte);
        if (c != ',') {
            break;
        }
        // count digits before the comma
        int j = pos - 2;
        int jByte = commaByte;
        int nDigits = 0;
        while (j >= 0) {
            int prevByte = jByte;
            int digit = Utf8CodepointPrev(text, prevByte);
            if (!isDigit(digit)) {
                break;
            }
            nDigits++;
            jByte = prevByte;
            j--;
        }
        if (nDigits == 0) {
            break;
        }
        pos = j + 1;
        posByte = jByte;
    }
    return pos;
}

// extend forward across comma-separated digit groups (e.g. ",234,567")
// returns the new end position
static int ExtendForwardAcrossCommaGroups(Str text, int textLen, int curEnd) {
    int pos = curEnd;
    int posByte = Utf8CodepointToByteIndex(text, pos);
    while (pos < textLen) {
        int commaEndByte = posByte;
        int c = Utf8CodepointNext(text, commaEndByte);
        if (c != ',') {
            break;
        }
        // count digits after the comma
        int j = pos + 1;
        int jByte = commaEndByte;
        int nDigits = 0;
        while (j < textLen) {
            int nextByte = jByte;
            int digit = Utf8CodepointNext(text, nextByte);
            if (!isDigit(digit)) {
                break;
            }
            nDigits++;
            jByte = nextByte;
            j++;
        }
        if (nDigits == 0) {
            break;
        }
        pos = j;
        posByte = jByte;
    }
    return pos;
}

void TextSelection::GetWordBoundsAt(int pageNo, double x, double y, int* wordStartOut, int* wordEndOut) {
    int i = FindClosestGlyph(this, pageNo, x, y);
    int textLen = 0;
    Str text = engine->GetTextForPage(pageNo, &textLen);

    bool isAllDigits = true;
    int c = 0;
    int iByte = Utf8CodepointToByteIndex(text, i);
    int cByte = iByte;
    for (; i > 0;) {
        int prevByte = iByte;
        c = Utf8CodepointPrev(text, prevByte);
        if (!isWordChar(c)) {
            cByte = prevByte;
            break;
        }
        if (!isDigit(c)) {
            isAllDigits = false;
        }
        iByte = prevByte;
        i--;
    }
    int wordStart = i;
    int maybeNumberStart = i;
    int nDigits = 0;
    if (isAllDigits && (c == '.' || c == ',')) {
        // walk backward across a pattern like "1,234." or "1,234,567,"
        int j = i - 2;
        int jByte = cByte;
        // first skip one group of digits (before the separator we stopped at)
        nDigits = 0;
        while (j >= 0) {
            int prevByte = jByte;
            int digit = Utf8CodepointPrev(text, prevByte);
            if (!isDigit(digit)) {
                break;
            }
            nDigits++;
            jByte = prevByte;
            j--;
        }
        if (nDigits > 0) {
            maybeNumberStart = j + 1;
            // continue backward across comma-separated groups
            maybeNumberStart = ExtendBackAcrossCommaGroups(text, maybeNumberStart);
        } else {
            isAllDigits = false;
        }
    }

    for (; i < textLen;) {
        int nextByte = iByte;
        c = Utf8CodepointNext(text, nextByte);
        if (!isWordChar(c)) {
            break;
        }
        if (!isDigit(c)) {
            isAllDigits = false;
        }
        iByte = nextByte;
        i++;
    }

    // try to select numbers with commas and decimal points
    // e.g. "1,234.56" or "1,234,567" or "123.45"
    int wordEnd = i;
    if (isAllDigits) {
        // extend forward across comma groups
        wordEnd = ExtendForwardAcrossCommaGroups(text, textLen, wordEnd);
        // extend forward across decimal point + digits
        int wordEndByte = Utf8CodepointToByteIndex(text, wordEnd);
        int dotEndByte = wordEndByte;
        if (wordEnd < textLen && Utf8CodepointNext(text, dotEndByte) == '.') {
            int j = wordEnd + 1;
            int jByte = dotEndByte;
            nDigits = 0;
            while (j < textLen) {
                int nextByte = jByte;
                int digit = Utf8CodepointNext(text, nextByte);
                if (!isDigit(digit)) {
                    break;
                }
                nDigits++;
                jByte = nextByte;
                j++;
            }
            if (nDigits > 0) {
                wordEnd = j;
            }
        }
        // extend backward across comma groups
        wordStart = ExtendBackAcrossCommaGroups(text, wordStart);
        if (maybeNumberStart < wordStart) {
            wordStart = maybeNumberStart;
        }
    }
    *wordStartOut = wordStart;
    *wordEndOut = wordEnd;
}

void TextSelection::SelectWordAt(int pageNo, double x, double y) {
    int wordStart = 0, wordEnd = 0;
    GetWordBoundsAt(pageNo, x, y, &wordStart, &wordEnd);
    // remember the word as the anchor for word-granular drag extension
    wordStartPage = pageNo;
    wordStartGlyph = wordStart;
    wordEndPage = pageNo;
    wordEndGlyph = wordEnd;
    StartAt(pageNo, wordStart);
    SelectUpTo(pageNo, wordEnd);
}

void TextSelection::SelectLineAt(int pageNo, double x, double y) {
    int i = FindClosestGlyph(this, pageNo, x, y);
    if (i < 0) {
        return;
    }
    Rect* coords;
    int textLen = 0;
    Str text = engine->GetTextForPage(pageNo, &textLen, &coords);
    // line breaks are newline glyphs with zero-size coords. Some whitespace (e.g.
    // spaces with FZ_STEXT_ACCURATE_BBOXES) can also have empty boxes and must not
    // be treated as line ends (issue #5712).
    int lineStart = i;
    int lineStartByte = Utf8CodepointToByteIndex(text, lineStart);
    while (lineStart > 0) {
        int prevByte = lineStartByte;
        int c = Utf8CodepointPrev(text, prevByte);
        int prevGlyph = lineStart - 1;
        if (c == '\n' && !coords[prevGlyph].x && !coords[prevGlyph].dx) {
            break;
        }
        lineStart--;
        lineStartByte = prevByte;
    }
    int lineEnd = i;
    int lineEndByte = Utf8CodepointToByteIndex(text, lineEnd);
    while (lineEnd < textLen) {
        int nextByte = lineEndByte;
        int c = Utf8CodepointNext(text, nextByte);
        if (c == '\n' && !coords[lineEnd].x && !coords[lineEnd].dx) {
            break;
        }
        lineEnd++;
        lineEndByte = nextByte;
    }
    StartAt(pageNo, lineStart);
    SelectUpTo(pageNo, lineEnd);
}

// (pageA, glyphA) is before (pageB, glyphB) in reading order
static bool PosBefore(int pageA, int glyphA, int pageB, int glyphB) {
    if (pageA != pageB) {
        return pageA < pageB;
    }
    return glyphA < glyphB;
}

void TextSelection::SelectWordsUpTo(int pageNo, double x, double y) {
    // no anchor word yet (shouldn't happen) - fall back to glyph selection
    if (wordStartGlyph == -1) {
        SelectUpTo(pageNo, x, y);
        return;
    }
    int cursorStart = 0, cursorEnd = 0;
    GetWordBoundsAt(pageNo, x, y, &cursorStart, &cursorEnd);

    // union the anchor word with the word under the cursor, so the selection
    // always covers whole words from the lower to the upper of the two
    int startPg = wordStartPage, startGl = wordStartGlyph;
    if (PosBefore(pageNo, cursorStart, startPg, startGl)) {
        startPg = pageNo;
        startGl = cursorStart;
    }
    int endPg = wordEndPage, endGl = wordEndGlyph;
    if (PosBefore(endPg, endGl, pageNo, cursorEnd)) {
        endPg = pageNo;
        endGl = cursorEnd;
    }
    StartAt(startPg, startGl);
    SelectUpTo(endPg, endGl);
}

void TextSelection::CopySelection(TextSelection* orig) {
    Reset();
    StartAt(orig->startPage, orig->startGlyph);
    SelectUpTo(orig->endPage, orig->endGlyph);
}

Str TextSelection::ExtractText(Str lineSep) {
    StrVec lines;

    int fromPage, fromGlyph, toPage, toGlyph;
    GetGlyphRange(&fromPage, &fromGlyph, &toPage, &toGlyph);

    for (int page = fromPage; page <= toPage; page++) {
        int textLen;
        engine->GetTextForPage(page, &textLen);
        int glyph = page == fromPage ? fromGlyph : 0;
        int length = (page == toPage ? toGlyph : textLen) - glyph;
        if (length > 0) {
            FillResultRects(this, page, glyph, length, &lines);
        }
    }

    TempStr res = JoinTemp(&lines, lineSep);
    return str::Dup(res);
}

void TextSelection::GetGlyphRange(int* fromPage, int* fromGlyph, int* toPage, int* toGlyph) const {
    *fromPage = std::min(startPage, endPage);
    *toPage = std::max(startPage, endPage);
    *fromGlyph = (*fromPage == endPage ? endGlyph : startGlyph);
    *toGlyph = (*fromPage == endPage ? startGlyph : endGlyph);
    if (*fromPage == *toPage && *fromGlyph > *toGlyph) {
        std::swap(*fromGlyph, *toGlyph);
    }
}
