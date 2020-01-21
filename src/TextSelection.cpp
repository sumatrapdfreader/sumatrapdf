/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"

#include "wingui/TreeModel.h"
#include "EngineBase.h"
#include "TextSelection.h"

PageTextCache::PageTextCache(EngineBase* engine) : engine(engine) {
    int count = engine->PageCount();
    coords = AllocArray<RectI*>(count);
    text = AllocArray<WCHAR*>(count);
    lens = AllocArray<int>(count);
#ifdef DEBUG
    debug_size = count * (sizeof(RectI*) + sizeof(WCHAR*) + sizeof(int));
#endif

    InitializeCriticalSection(&access);
}

PageTextCache::~PageTextCache() {
    EnterCriticalSection(&access);

    for (int i = 0; i < engine->PageCount(); i++) {
        free(coords[i]);
        free(text[i]);
    }

    free(coords);
    free(text);
    free(lens);

    LeaveCriticalSection(&access);
    DeleteCriticalSection(&access);
}

bool PageTextCache::HasData(int pageNo) {
    CrashIf(pageNo < 1 || pageNo > engine->PageCount());
    return text[pageNo - 1] != nullptr;
}

const WCHAR* PageTextCache::GetData(int pageNo, int* lenOut, RectI** coordsOut) {
    ScopedCritSec scope(&access);

    if (!text[pageNo - 1]) {
        text[pageNo - 1] = engine->ExtractPageText(pageNo, &coords[pageNo - 1]);
        if (!text[pageNo - 1]) {
            text[pageNo - 1] = str::Dup(L"");
            lens[pageNo - 1] = 0;
        } else {
            lens[pageNo - 1] = (int)str::Len(text[pageNo - 1]);
        }
#ifdef DEBUG
        debug_size += (lens[pageNo - 1] + 1) * (sizeof(WCHAR) + sizeof(RectI));
#endif
    }

    if (lenOut)
        *lenOut = lens[pageNo - 1];
    if (coordsOut)
        *coordsOut = coords[pageNo - 1];
    return text[pageNo - 1];
}

TextSelection::TextSelection(EngineBase* engine, PageTextCache* textCache)
    : engine(engine), textCache(textCache), startPage(-1), endPage(-1), startGlyph(-1), endGlyph(-1) {
    result.len = 0;
    result.pages = nullptr;
    result.rects = nullptr;
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
int TextSelection::FindClosestGlyph(int pageNo, double x, double y) {
    int textLen;
    RectI* coords;
    textCache->GetData(pageNo, &textLen, &coords);
    PointD pt = PointD(x, y);

    unsigned int maxDist = UINT_MAX;
    PointI pti = pt.ToInt();
    bool overGlyph = false;
    int result = -1;

    for (int i = 0; i < textLen; i++) {
        if (!coords[i].x && !coords[i].dx)
            continue;
        if (overGlyph && !coords[i].Contains(pti))
            continue;

        unsigned int dist = distSq((int)x - coords[i].x - coords[i].dx / 2, (int)y - coords[i].y - coords[i].dy / 2);
        if (dist < maxDist) {
            result = i;
            maxDist = dist;
        }
        // prefer glyphs the cursor is actually over
        if (!overGlyph && coords[i].Contains(pti)) {
            overGlyph = true;
            result = i;
            maxDist = dist;
        }
    }

    if (-1 == result)
        return 0;
    CrashIf(result < 0 || result >= textLen);

    // the result indexes the first glyph to be selected in a forward selection
    RectD bbox = engine->Transform(coords[result].Convert<double>(), pageNo, 1.0, 0);
    pt = engine->Transform(pt, pageNo, 1.0, 0);
    if (pt.x > bbox.x + 0.5 * bbox.dx) {
        result++;
        // for some (DjVu) documents, all glyphs of a word share the same bbox
        while (result < textLen && coords[result - 1] == coords[result])
            result++;
    }
    CrashIf(result > 0 && result < textLen && coords[result] == coords[result - 1]);

    return result;
}

void TextSelection::FillResultRects(int pageNo, int glyph, int length, WStrVec* lines) {
    int len;
    RectI* coords;
    const WCHAR* text = textCache->GetData(pageNo, &len, &coords);
    CrashIf(len < glyph + length);
    RectI mediabox = engine->PageMediabox(pageNo).Round();
    RectI *c = &coords[glyph], *end = c + length;
    while (c < end) {
        // skip line breaks
        for (; c < end && !c->x && !c->dx; c++) {
            // no-op
        }

        RectI bbox, *c0 = c;
        for (; c < end && (c->x || c->dx); c++) {
            bbox = bbox.Union(*c);
        }
        bbox = bbox.Intersect(mediabox);
        // skip text that's completely outside a page's mediabox
        if (bbox.IsEmpty()) {
            continue;
        }

        if (lines) {
            lines->Push(str::DupN(text + (c0 - coords), c - c0));
            continue;
        }

        // cut the right edge, if it overlaps the next character
        if (c < coords + len && (c->x || c->dx) && bbox.x < c->x && bbox.x + bbox.dx > c->x) {
            bbox.dx = c->x - bbox.x;
        }

        int currLen = result.len;
        int left = result.cap - currLen;
        CrashIf(left < 0);
        if (left == 0) {
            int newCap = result.cap * 2;
            if (newCap < 64) {
                newCap = 64;
            }
            int* newPages = (int*)realloc(result.pages, sizeof(int) * newCap);
            RectI* newRects = (RectI*)realloc(result.rects, sizeof(RectI) * newCap);
            CrashIf(!newPages);
            CrashIf(!newRects);
            result.pages = newPages;
            result.rects = newRects;
            result.cap = newCap;
        }

        result.pages[currLen] = pageNo;
        result.rects[currLen] = bbox;
        result.len++;
    }
}

bool TextSelection::IsOverGlyph(int pageNo, double x, double y) {
    int textLen;
    RectI* coords;
    textCache->GetData(pageNo, &textLen, &coords);

    int glyphIx = FindClosestGlyph(pageNo, x, y);
    PointI pt = PointD(x, y).ToInt();
    // when over the right half of a glyph, FindClosestGlyph returns the
    // index of the next glyph, in which case glyphIx must be decremented
    if (glyphIx == textLen || !coords[glyphIx].Contains(pt))
        glyphIx--;
    if (-1 == glyphIx)
        return false;
    return coords[glyphIx].Contains(pt);
}

void TextSelection::StartAt(int pageNo, int glyphIx) {
    startPage = pageNo;
    startGlyph = glyphIx;
    if (glyphIx < 0) {
        int textLen;
        textCache->GetData(pageNo, &textLen);
        startGlyph += textLen + 1;
    }
}

void TextSelection::SelectUpTo(int pageNo, int glyphIx) {
    if (startPage == -1 || startGlyph == -1)
        return;

    endPage = pageNo;
    endGlyph = glyphIx;
    if (glyphIx < 0) {
        int textLen;
        textCache->GetData(pageNo, &textLen);
        endGlyph = textLen + glyphIx + 1;
    }

    result.len = 0;
    int fromPage = std::min(startPage, endPage), toPage = std::max(startPage, endPage);
    int fromGlyph = (fromPage == endPage ? endGlyph : startGlyph);
    int toGlyph = (fromPage == endPage ? startGlyph : endGlyph);
    if (fromPage == toPage && fromGlyph > toGlyph)
        std::swap(fromGlyph, toGlyph);

    for (int page = fromPage; page <= toPage; page++) {
        int textLen;
        textCache->GetData(page, &textLen);

        int glyph = page == fromPage ? fromGlyph : 0;
        int length = (page == toPage ? toGlyph : textLen) - glyph;
        if (length > 0)
            FillResultRects(page, glyph, length);
    }
}

void TextSelection::SelectWordAt(int pageNo, double x, double y) {
    int ix = FindClosestGlyph(pageNo, x, y);
    int textLen;
    const WCHAR* text = textCache->GetData(pageNo, &textLen);

    for (; ix > 0; ix--) {
        if (!isWordChar(text[ix - 1]))
            break;
    }
    StartAt(pageNo, ix);

    for (; ix < textLen; ix++) {
        if (!isWordChar(text[ix]))
            break;
    }
    SelectUpTo(pageNo, ix);
}

void TextSelection::CopySelection(TextSelection* orig) {
    Reset();
    StartAt(orig->startPage, orig->startGlyph);
    SelectUpTo(orig->endPage, orig->endGlyph);
}

WCHAR* TextSelection::ExtractText(const WCHAR* lineSep) {
    WStrVec lines;

    int fromPage, fromGlyph, toPage, toGlyph;
    GetGlyphRange(&fromPage, &fromGlyph, &toPage, &toGlyph);

    for (int page = fromPage; page <= toPage; page++) {
        int textLen;
        textCache->GetData(page, &textLen);
        int glyph = page == fromPage ? fromGlyph : 0;
        int length = (page == toPage ? toGlyph : textLen) - glyph;
        if (length > 0)
            FillResultRects(page, glyph, length, &lines);
    }

    return lines.Join(lineSep);
}

void TextSelection::GetGlyphRange(int* fromPage, int* fromGlyph, int* toPage, int* toGlyph) const {
    *fromPage = std::min(startPage, endPage);
    *toPage = std::max(startPage, endPage);
    *fromGlyph = (*fromPage == endPage ? endGlyph : startGlyph);
    *toGlyph = (*fromPage == endPage ? startGlyph : endGlyph);
    if (*fromPage == *toPage && *fromGlyph > *toGlyph)
        std::swap(*fromGlyph, *toGlyph);
}
