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

DocumentTextCache::DocumentTextCache(EngineBase* engine) : engine(engine) {
    nPages = engine->PageCount();
    pagesText = AllocArray<PageText>(nPages);
    debugSize = nPages * (sizeof(Rect*) + sizeof(WCHAR*) + sizeof(int));

    InitializeCriticalSection(&access);
}

DocumentTextCache::~DocumentTextCache() {
    EnterCriticalSection(&access);

    int n = engine->PageCount();
    for (int i = 0; i < n; i++) {
        PageText* pageText = &pagesText[i];
        free(pageText->coords);
        free(pageText->text);
    }
    free(pagesText);
    LeaveCriticalSection(&access);
    DeleteCriticalSection(&access);
}

bool DocumentTextCache::HasTextForPage(int pageNo) const {
    CrashIf(pageNo < 1 || pageNo > nPages);
    PageText* pageText = &pagesText[pageNo - 1];
    return pageText->text != nullptr;
}

const WCHAR* DocumentTextCache::GetTextForPage(int pageNo, int* lenOut, Rect** coordsOut) {
    CrashIf(pageNo < 1 || pageNo > nPages);

    ScopedCritSec scope(&access);
    PageText* pageText = &pagesText[pageNo - 1];

    if (!pageText->text) {
        *pageText = engine->ExtractPageText(pageNo);
        if (!pageText->text) {
            pageText->text = str::Dup(L"");
            pageText->len = 0;
        }
        debugSize += (pageText->len + 1) * (int)(sizeof(WCHAR) + sizeof(Rect));
    }

    if (lenOut) {
        *lenOut = pageText->len;
    }
    if (coordsOut) {
        *coordsOut = pageText->coords;
    }
    return pageText->text;
}

TextSelection::TextSelection(EngineBase* engine, DocumentTextCache* textCache) : engine(engine), textCache(textCache) {
}

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
    ts->textCache->GetTextForPage(pageNo, &textLen, &coords);
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
    CrashIf(result < 0 || result >= textLen);

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
    CrashIf(result > 0 && result < textLen && coords[result] == coords[result - 1]);

    return result;
}

static void FillResultRects(TextSelection* ts, int pageNo, int glyph, int length, StrVec* lines = nullptr) {
    int len;
    Rect* coords;
    const WCHAR* text = ts->textCache->GetTextForPage(pageNo, &len, &coords);
    CrashIf(len < glyph + length);
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
        CrashIf(left < 0);
        if (left == 0) {
            int newCap = ts->result.cap * 2;
            if (newCap < 64) {
                newCap = 64;
            }
            int* newPages = (int*)realloc(ts->result.pages, sizeof(int) * newCap);
            Rect* newRects = (Rect*)realloc(ts->result.rects, sizeof(Rect) * newCap);
            CrashIf(!newPages);
            CrashIf(!newRects);
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
    textCache->GetTextForPage(pageNo, &textLen, &coords);

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
        textCache->GetTextForPage(pageNo, &textLen);
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
        textCache->GetTextForPage(pageNo, &textLen);
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
        textCache->GetTextForPage(page, &textLen);

        int glyph = page == fromPage ? fromGlyph : 0;
        int length = (page == toPage ? toGlyph : textLen) - glyph;
        if (length > 0) {
            FillResultRects(this, page, glyph, length);
        }
    }
}

void TextSelection::SelectWordAt(int pageNo, double x, double y) {
    int i = FindClosestGlyph(this, pageNo, x, y);
    int textLen;
    const WCHAR* text = textCache->GetTextForPage(pageNo, &textLen);

    for (; i > 0; i--) {
        if (!isWordChar(text[i - 1])) {
            break;
        }
    }
    StartAt(pageNo, i);

    for (; i < textLen; i++) {
        if (!isWordChar(text[i])) {
            break;
        }
    }
    SelectUpTo(pageNo, i);
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
        textCache->GetTextForPage(page, &textLen);
        int glyph = page == fromPage ? fromGlyph : 0;
        int length = (page == toPage ? toGlyph : textLen) - glyph;
        if (length > 0) {
            FillResultRects(this, page, glyph, length, &lines);
        }
    }

    TempStr res = JoinTemp(lines, lineSep);
    return ToWstr(res);
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
