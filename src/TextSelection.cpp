/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"

#include "wingui/UIModels.h"

#include "DocController.h"
#include "EngineBase.h"
#include "TextSelection.h"

uint distSq(int x, int y) {
    return x * x + y * y;
}
// underscore is mainly used for programming and is thus considered a word character
bool isWordChar(WCHAR c) {
    return IsCharAlphaNumeric(c) || c == '_';
}

static bool isDigit(WCHAR c) {
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
}

// returns the index of the glyph closest to the right of the given coordinates
// (i.e. when over the right half of a glyph, the returned index will be for the
// glyph following it, which will be the first glyph (not) to be selected)
static int FindClosestGlyph(TextSelection* ts, int pageNo, double x, double y) {
    int textLen;
    Rect* coords;
    ts->engine->GetTextForPage(pageNo, &textLen, &coords);
    PointF pt = PointF(x, y);

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
    int len;
    Rect* coords;
    const WCHAR* text = ts->engine->GetTextForPage(pageNo, &len, &coords);
    ReportIf(len < glyph + length);
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
            char* s = ToUtf8Temp(text + (c0 - coords), c - c0);
            lines->Append(s);
            continue;
        }

        // cut the right edge, if it overlaps the next character
        if (c < coords + len && (c->x || c->dx) && bbox.x < c->x && bbox.x + bbox.dx > c->x) {
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
    int textLen;
    Rect* coords;
    engine->GetTextForPage(pageNo, &textLen, &coords);

    int glyphIx = FindClosestGlyph(this, pageNo, x, y);
    Point pt = ToPoint(PointF(x, y));
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

void TextSelection::StartAt(int pageNo, int glyphIx) {
    startPage = pageNo;
    startGlyph = glyphIx;
    if (glyphIx < 0) {
        int textLen;
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
        int textLen;
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
        int textLen;
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
static int ExtendBackAcrossCommaGroups(const WCHAR* text, int curStart) {
    int pos = curStart;
    while (pos >= 2 && text[pos - 1] == ',') {
        // count digits before the comma
        int j = pos - 2;
        int nDigits = 0;
        while (j >= 0 && isDigit(text[j])) {
            nDigits++;
            j--;
        }
        if (nDigits == 0) {
            break;
        }
        pos = j + 1;
    }
    return pos;
}

// extend forward across comma-separated digit groups (e.g. ",234,567")
// returns the new end position
static int ExtendForwardAcrossCommaGroups(const WCHAR* text, int textLen, int curEnd) {
    int pos = curEnd;
    while (pos < textLen && text[pos] == ',') {
        // count digits after the comma
        int j = pos + 1;
        int nDigits = 0;
        while (j < textLen && isDigit(text[j])) {
            nDigits++;
            j++;
        }
        if (nDigits == 0) {
            break;
        }
        pos = j;
    }
    return pos;
}

void TextSelection::SelectWordAt(int pageNo, double x, double y) {
    int i = FindClosestGlyph(this, pageNo, x, y);
    int textLen;
    const WCHAR* text = engine->GetTextForPage(pageNo, &textLen);

    bool isAllDigits = true;
    WCHAR c = 0;
    for (; i > 0; i--) {
        c = text[i - 1];
        if (!isWordChar(c)) {
            break;
        }
        if (!isDigit(c)) {
            isAllDigits = false;
        }
    }
    int wordStart = i;
    int maybeNumberStart = i;
    int nDigits = 0;
    if (isAllDigits && (c == '.' || c == ',')) {
        // walk backward across a pattern like "1,234." or "1,234,567,"
        int j = i - 2;
        // first skip one group of digits (before the separator we stopped at)
        nDigits = 0;
        while (j >= 0 && isDigit(text[j])) {
            nDigits++;
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

    for (; i < textLen; i++) {
        c = text[i];
        if (!isWordChar(c)) {
            break;
        }
        if (!isDigit(c)) {
            isAllDigits = false;
        }
    }

    // try to select numbers with commas and decimal points
    // e.g. "1,234.56" or "1,234,567" or "123.45"
    int wordEnd = i;
    if (isAllDigits) {
        // extend forward across comma groups
        wordEnd = ExtendForwardAcrossCommaGroups(text, textLen, wordEnd);
        // extend forward across decimal point + digits
        if (wordEnd < textLen && text[wordEnd] == '.') {
            int j = wordEnd + 1;
            nDigits = 0;
            while (j < textLen && isDigit(text[j])) {
                nDigits++;
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
    StartAt(pageNo, wordStart);
    SelectUpTo(pageNo, wordEnd);
}

void TextSelection::CopySelection(TextSelection* orig) {
    Reset();
    StartAt(orig->startPage, orig->startGlyph);
    SelectUpTo(orig->endPage, orig->endGlyph);
}

WCHAR* TextSelection::ExtractText(const char* lineSep) {
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
    return ToWStr(res);
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
