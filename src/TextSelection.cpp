/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "DocController.h"
#include "gui/UIModels.h"
#include "EngineBase.h"
#ifdef DEBUG
#include "base/UtAssert.h"
#endif
#include "TextSelection.h"

uint distSq(int x, int y) {
    return (x * x) + (y * y);
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
    // called for the side effect of filling textLen and coords
    ts->engine->GetTextForPage(pageNo, &textLen, &coords);
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

        uint dist = distSq((int)x - coord.x - (coord.dx / 2), (int)y - coord.y - (coord.dy / 2));
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
    if (pt.x > bbox.x + (0.5 * bbox.dx)) {
        result++;
        // for some (DjVu) documents, all glyphs of a word share the same bbox
        while (result < textLen && coords[result - 1] == coords[result]) {
            result++;
        }
    }
    ReportIf(result > 0 && result < textLen && coords[result] == coords[result - 1]);

    return result;
}

// Dehyphenation removes both the trailing hyphen and the line-separator glyph,
// so adjacent coords can belong to different visual lines. Require at least
// half of the new glyph's height to overlap the current line box; this still
// keeps smaller subscript and superscript glyphs in the same run.
static bool IsGlyphOnVisualLine(Rect lineBox, Rect glyphBox) {
    int top = lineBox.y > glyphBox.y ? lineBox.y : glyphBox.y;
    int bottom = lineBox.y + lineBox.dy < glyphBox.y + glyphBox.dy ? lineBox.y + lineBox.dy : glyphBox.y + glyphBox.dy;
    return (bottom - top) * 2 >= glyphBox.dy;
}

static void FillSelectionRects(TextSel* result, int pageNo, Rect* coords, int textLen, int glyph, int length,
                               Rect mediabox) {
    Rect *c = &coords[glyph], *end = c + length;
    while (c < end) {
        // skip line breaks (empty boxes: hard newlines and soft-join spaces)
        for (; c < end && !c->x && !c->dx; c++) {
            // no-op
        }

        Rect bbox;
        for (; c < end && (c->x || c->dx); c++) {
            if (!bbox.IsEmpty() && !IsGlyphOnVisualLine(bbox, *c)) {
                break;
            }
            bbox = bbox.Union(*c);
        }
        bbox = bbox.Intersect(mediabox);
        // skip text that's completely outside a page's mediabox
        if (bbox.IsEmpty()) {
            continue;
        }

        // Only clip against the next glyph when it is on this visual line.
        // At a dehyphenated break the next glyph belongs to the following line.
        bool overlapsVertically = c < coords + textLen && c->y < bbox.y + bbox.dy && c->y + c->dy > bbox.y;
        if (overlapsVertically && (c->x || c->dx) && bbox.x < c->x && bbox.x + bbox.dx > c->x) {
            bbox.dx = c->x - bbox.x;
        }

        int currLen = result->len;
        int left = result->cap - currLen;
        ReportIf(left < 0);
        if (left == 0) {
            int newCap = result->cap * 2;
            newCap = std::max(newCap, 64);
            int* newPages = (int*)realloc(result->pages, sizeof(int) * newCap);
            Rect* newRects = (Rect*)realloc(result->rects, sizeof(Rect) * newCap);
            ReportIf(!newPages);
            ReportIf(!newRects);
            result->pages = newPages;
            result->rects = newRects;
            result->cap = newCap;
        }

        result->pages[currLen] = pageNo;
        result->rects[currLen] = bbox;
        result->len++;
    }
}

static void FillResultRects(TextSelection* ts, int pageNo, int glyph, int length, StrVec* lines = nullptr) {
    Rect* coords;
    int textLen = 0;
    Str text = ts->engine->GetTextForPage(pageNo, &textLen, &coords);
    // Clamp ranges that outlive their page text (stale find-match coords after
    // tab close/reload, or a multi-page match that ends past a shorter page).
    if (glyph < 0) {
        length += glyph;
        glyph = 0;
    }
    length = std::max(length, 0);
    glyph = std::min(glyph, textLen);
    if (glyph + length > textLen) {
        length = textLen - glyph;
    }
    if (length <= 0 || !coords) {
        return;
    }
    Rect mediabox = ts->engine->PageMediabox(pageNo).Round();

    // Copy path (lines != null): soft-join spaces from FzTextPageToUtf8 (#5793) have
    // empty rects like hard newlines. Treat only newline (empty-rect) glyphs as line
    // breaks; keep soft-join spaces inside the same extract line so Ctrl+C does not
    // re-split wrapped paragraphs. Highlight path (lines == null) still splits on any
    // empty rect so each visual line gets its own selection rectangle.
    if (lines) {
        int runStart = -1;
        int endGlyph = glyph + length;
        auto flushLine = [&](int runEnd) {
            if (runStart >= 0 && runEnd > runStart) {
                Str s = Utf8SliceByCodepoints(text, runStart, runEnd - runStart);
                if (len(s) > 0) {
                    lines->Append(s);
                }
            }
            runStart = -1;
        };
        for (int i = glyph; i < endGlyph; i++) {
            Rect& r = coords[i];
            bool emptyBox = !r.x && !r.dx;
            if (!emptyBox) {
                if (runStart < 0) {
                    runStart = i;
                }
                continue;
            }
            // empty box: newline ends the extract line; soft-join space stays in it
            int byteIdx = Utf8CodepointToByteIndex(text, i);
            int cp = Utf8CodepointNext(text, byteIdx);
            if (cp == '\n' || cp == '\r') {
                flushLine(i);
                continue;
            }
            if (runStart < 0) {
                runStart = i;
            }
        }
        flushLine(endGlyph);
        return;
    }

    FillSelectionRects(&ts->result, pageNo, coords, textLen, glyph, length, mediabox);
}

#ifdef DEBUG
void TextSelection_UnitTests() {
    Rect coords[] = {
        {50, 100, 12, 10}, {60, 100, 12, 10}, {70, 100, 12, 10}, {56, 115, 12, 10},
        {66, 115, 12, 10}, {76, 115, 12, 10}, {50, 130, 12, 10}, {60, 130, 12, 10},
        {70, 130, 12, 10}, {56, 145, 12, 10}, {66, 145, 12, 10}, {76, 145, 12, 10},
    };
    TextSel result;
    FillSelectionRects(&result, 1, coords, dimof(coords), 0, 10, {0, 0, 200, 200});
    utassert(result.len == 4);
    utassert(result.rects[0] == Rect(50, 100, 32, 10));
    utassert(result.rects[1] == Rect(56, 115, 32, 10));
    utassert(result.rects[2] == Rect(50, 130, 32, 10));
    utassert(result.rects[3] == Rect(56, 145, 10, 10));
    free(result.pages);
    free(result.rects);

    Rect superscript[] = {{10, 100, 12, 10}, {20, 97, 8, 6}, {28, 100, 12, 10}};
    result = {};
    FillSelectionRects(&result, 1, superscript, dimof(superscript), 0, dimof(superscript), {0, 0, 200, 200});
    utassert(result.len == 1);
    utassert(result.rects[0] == Rect(10, 97, 30, 13));
    free(result.pages);
    free(result.rects);
}
#endif

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

// index of the glyph closest to (x, y) on pageNo, without mutating the
// selection (unlike StartAt, which stores it in startGlyph)
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
        int end = page == toPage ? toGlyph : textLen;
        glyph = std::max(glyph, 0);
        end = std::min(end, textLen);
        int length = end - glyph;
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
        wordStart = std::min(maybeNumberStart, wordStart);
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

// select the whole line of text at (x, y) (triple-click; issue #694)
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

// extend the selection so it spans whole words from the anchor word (set by
// the last SelectWordAt) to the word at (x, y)
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

// Move free end (page, glyph) by one glyph in reading order. dir +1 / -1.
static bool MoveFreeEndByGlyph(EngineBase* engine, int& page, int& glyph, int dir) {
    int nPages = engine->PageCount();
    int textLen = 0;
    engine->GetTextForPage(page, &textLen);
    if (dir > 0) {
        if (glyph < textLen) {
            glyph++;
            return true;
        }
        if (page < nPages) {
            page++;
            glyph = 0;
            return true;
        }
        return false;
    }
    if (glyph > 0) {
        glyph--;
        return true;
    }
    if (page > 1) {
        page--;
        engine->GetTextForPage(page, &textLen);
        glyph = textLen;
        return true;
    }
    return false;
}

// Move free end (page, glyph) to the previous / next word boundary. dir +1 / -1.
// Steps off the current position, then over any run of non-word characters, then
// to the far side of the word it lands in - i.e. what Ctrl+Left / Ctrl+Right do
// in a text editor. Stops at a page boundary so a single step never skips a page.
static bool MoveFreeEndByWord(EngineBase* engine, int& page, int& glyph, int dir) {
    int textLen = 0;
    Str text = engine->GetTextForPage(page, &textLen);
    if (textLen <= 0) {
        return MoveFreeEndByGlyph(engine, page, glyph, dir);
    }
    auto charAt = [&](int ix) -> int {
        if (ix < 0 || ix >= textLen) {
            return 0;
        }
        int byteIdx = Utf8CodepointToByteIndex(text, ix);
        int next = byteIdx;
        return Utf8CodepointNext(text, next);
    };

    int fromPage = page;
    if (!MoveFreeEndByGlyph(engine, page, glyph, dir)) {
        return false;
    }
    if (page != fromPage) {
        return true;
    }
    // the character we are moving toward decides whether we're still in a word
    while (glyph > 0 && glyph < textLen && !isWordChar(charAt(dir < 0 ? glyph - 1 : glyph))) {
        if (!MoveFreeEndByGlyph(engine, page, glyph, dir) || page != fromPage) {
            break;
        }
    }
    while (glyph > 0 && glyph < textLen && isWordChar(charAt(dir < 0 ? glyph - 1 : glyph))) {
        if (!MoveFreeEndByGlyph(engine, page, glyph, dir) || page != fromPage) {
            break;
        }
    }
    return true;
}

// True if glyph i is a zero-width newline (line break in the page text stream).
static bool IsLineBreakAt(Str text, Rect* coords, int i, int textLen) {
    if (i < 0 || i >= textLen || !coords) {
        return false;
    }
    if (coords[i].x || coords[i].dx) {
        return false;
    }
    int byteIdx = Utf8CodepointToByteIndex(text, i);
    int nextByte = byteIdx;
    int c = Utf8CodepointNext(text, nextByte);
    return c == '\n';
}

// Move free end by one visual line (same x when possible). dir +1 = next line.
static bool MoveFreeEndByLine(EngineBase* engine, int& page, int& glyph, int dir) {
    int nPages = engine->PageCount();
    Rect* coords = nullptr;
    int textLen = 0;
    Str text = engine->GetTextForPage(page, &textLen, &coords);
    if (textLen <= 0 || !coords) {
        // empty page: step a page
        if (dir > 0 && page < nPages) {
            page++;
            glyph = 0;
            return true;
        }
        if (dir < 0 && page > 1) {
            page--;
            engine->GetTextForPage(page, &textLen);
            glyph = textLen;
            return true;
        }
        return false;
    }

    // reference point: center of the glyph left of the free end (or first glyph)
    int refIx = glyph;
    if (refIx > 0) {
        refIx--;
    }
    if (refIx >= textLen) {
        refIx = textLen - 1;
    }
    while (refIx > 0 && !coords[refIx].x && !coords[refIx].dx && !IsLineBreakAt(text, coords, refIx, textLen)) {
        refIx--;
    }
    int refX = coords[refIx].x + (coords[refIx].dx / 2);
    int refY = coords[refIx].y + (coords[refIx].dy / 2);
    int lineH = coords[refIx].dy > 0 ? coords[refIx].dy : 12;
    int ySlop = std::max(lineH / 2, 2);

    int bestIx = -1;
    int bestDist = INT_MAX;
    int targetBandY = -1;

    if (dir > 0) {
        // next line: lowest y still clearly below this line
        for (int i = 0; i < textLen; i++) {
            if (!coords[i].x && !coords[i].dx) {
                continue;
            }
            int cy = coords[i].y + (coords[i].dy / 2);
            if (cy <= refY + ySlop) {
                continue;
            }
            if (targetBandY < 0 || cy < targetBandY) {
                targetBandY = cy;
            }
        }
        if (targetBandY < 0) {
            if (page < nPages) {
                page++;
                glyph = 0;
                return true;
            }
            return false;
        }
        for (int i = 0; i < textLen; i++) {
            if (!coords[i].x && !coords[i].dx) {
                continue;
            }
            int cy = coords[i].y + (coords[i].dy / 2);
            if (std::abs(cy - targetBandY) > ySlop) {
                continue;
            }
            int cx = coords[i].x + (coords[i].dx / 2);
            int d = std::abs(cx - refX);
            if (d < bestDist) {
                bestDist = d;
                bestIx = i;
            }
        }
        if (bestIx < 0) {
            return false;
        }
        // free end is exclusive past the glyph under the caret column
        glyph = bestIx + 1;
        return true;
    }

    // previous line: highest y still clearly above this line
    for (int i = 0; i < textLen; i++) {
        if (!coords[i].x && !coords[i].dx) {
            continue;
        }
        int cy = coords[i].y + (coords[i].dy / 2);
        if (cy >= refY - ySlop) {
            continue;
        }
        if (targetBandY < 0 || cy > targetBandY) {
            targetBandY = cy;
        }
    }
    if (targetBandY < 0) {
        if (page > 1) {
            page--;
            engine->GetTextForPage(page, &textLen);
            glyph = textLen;
            return true;
        }
        return false;
    }
    for (int i = 0; i < textLen; i++) {
        if (!coords[i].x && !coords[i].dx) {
            continue;
        }
        int cy = coords[i].y + (coords[i].dy / 2);
        if (std::abs(cy - targetBandY) > ySlop) {
            continue;
        }
        int cx = coords[i].x + (coords[i].dx / 2);
        int d = std::abs(cx - refX);
        if (d < bestDist) {
            bestDist = d;
            bestIx = i;
        }
    }
    if (bestIx < 0) {
        return false;
    }
    glyph = bestIx + 1;
    return true;
}

// Move a (page, glyph) position one unit in reading order, without touching any
// selection. Keyboard selection drives its caret with this; ExtendBy() moves the
// selection's free end with the same steps.
bool TextPosMoveBy(EngineBase* engine, int& page, int& glyph, TextSelectUnit unit, int dir) {
    if (!engine || page < 1 || glyph < 0 || dir == 0) {
        return false;
    }
    int d = dir > 0 ? 1 : -1;
    if (unit == TextSelectUnit::Glyph) {
        return MoveFreeEndByGlyph(engine, page, glyph, d);
    }
    if (unit == TextSelectUnit::Word) {
        return MoveFreeEndByWord(engine, page, glyph, d);
    }
    return MoveFreeEndByLine(engine, page, glyph, d);
}

// Move the free end (endPage/endGlyph) by delta units in reading order.
// delta > 0 toward document end, delta < 0 toward document start.
// Returns true if the free end moved. Platform code maps keys to unit+delta.
bool TextSelection::ExtendBy(TextSelectUnit unit, int delta) {
    if (!engine || startPage < 1 || endPage < 1 || delta == 0) {
        return false;
    }
    if (startGlyph < 0 || endGlyph < 0) {
        return false;
    }

    int page = endPage;
    int glyph = endGlyph;
    int steps = delta > 0 ? delta : -delta;
    int dir = delta > 0 ? 1 : -1;

    for (int s = 0; s < steps; s++) {
        if (!TextPosMoveBy(engine, page, glyph, unit, dir)) {
            break;
        }
    }

    if (page == endPage && glyph == endGlyph) {
        return false;
    }
    SelectUpTo(page, glyph);
    return true;
}
