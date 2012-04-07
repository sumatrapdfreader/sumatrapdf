/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "TextSelection.h"

PageTextCache::PageTextCache(BaseEngine *engine) : engine(engine)
{
    int count = engine->PageCount();
    coords = SAZA(RectI *, count);
    text = SAZA(TCHAR *, count);
    lens = SAZA(int, count);

    InitializeCriticalSection(&access);
}

PageTextCache::~PageTextCache()
{
    EnterCriticalSection(&access);

    for (int i = 0; i < engine->PageCount(); i++) {
        delete[] coords[i];
        free(text[i]);
    }

    free(coords);
    free(text);
    free(lens);

    LeaveCriticalSection(&access);
    DeleteCriticalSection(&access);
}

bool PageTextCache::HasData(int pageNo)
{
    CrashIf(pageNo < 1 || pageNo > engine->PageCount());
    return text[pageNo - 1] != NULL;
}

const TCHAR *PageTextCache::GetData(int pageNo, int *lenOut, RectI **coordsOut)
{
    ScopedCritSec scope(&access);

    if (!text[pageNo - 1]) {
        text[pageNo - 1] = engine->ExtractPageText(pageNo, _T("\n"), &coords[pageNo - 1]);
        if (!text[pageNo - 1]) {
            text[pageNo - 1] = str::Dup(_T(""));
            lens[pageNo - 1] = 0;
        }
        else {
            lens[pageNo - 1] = (int)str::Len(text[pageNo - 1]);
        }
    }

    if (lenOut)
        *lenOut = lens[pageNo - 1];
    if (coordsOut)
        *coordsOut = coords[pageNo - 1];
    return text[pageNo - 1];
}

TextSelection::TextSelection(BaseEngine *engine, PageTextCache *textCache) :
    engine(engine), textCache(textCache), startPage(-1),
    endPage(-1), startGlyph(-1), endGlyph(-1)
{
    result.len = 0;
    result.pages = NULL;
    result.rects = NULL;
}

TextSelection::~TextSelection()
{
    Reset();
}

void TextSelection::Reset()
{
    result.len = 0;
    free(result.pages);
    result.pages = NULL;
    free(result.rects);
    result.rects = NULL;
}

// returns the index of the glyph closest to the right of the given coordinates
// (i.e. when over the right half of a glyph, the returned index will be for the
// glyph following it, which will be the first glyph (not) to be selected)
int TextSelection::FindClosestGlyph(int pageNo, double x, double y)
{
    int textLen;
    RectI *coords;
    const TCHAR *text = textCache->GetData(pageNo, &textLen, &coords);
    double maxDist = -1;
    int result = -1;

    for (int i = 0; i < textLen; i++) {
        if (!coords[i].x && !coords[i].dx)
            continue;

        double dist = _hypot(x - coords[i].x - 0.5 * coords[i].dx,
                             y - coords[i].y - 0.5 * coords[i].dy);
        if (maxDist < 0 || dist < maxDist) {
            result = i;
            maxDist = dist;
        }
    }

    if (-1 == result)
        return 0;
    CrashIf(result < 0 || result >= textLen);

    // the result indexes the first glyph to be selected in a forward selection
    RectD bbox = engine->Transform(coords[result].Convert<double>(), pageNo, 1.0, 0);
    PointD pt = engine->Transform(PointD(x, y), pageNo, 1.0, 0);
    if (pt.x > bbox.x + 0.5 * bbox.dx)
        result++;

    return result;
}

void TextSelection::FillResultRects(int pageNo, int glyph, int length, StrVec *lines)
{
    RectI *coords;
    const TCHAR *text = textCache->GetData(pageNo, NULL, &coords);
    RectI mediabox = engine->PageMediabox(pageNo).Round();
    RectI *c = &coords[glyph], *end = c + length;
    for (; c < end; c++) {
        // skip line breaks
        if (!c->x && !c->dx)
            continue;

        RectI c0 = *c, *c0p = c;
        for (; c < end && (c->x || c->dx); c++);
        c--;
        RectI c1 = *c;
        RectI bbox = c0.Union(c1).Intersect(mediabox);
        // skip text that's completely outside a page's mediabox
        if (bbox.IsEmpty())
            continue;

        if (lines) {
            lines->Push(str::DupN(text + (c0p - coords), c - c0p + 1));
            continue;
        }

        // cut the right edge, if it overlaps the next character
        if ((c[1].x || c[1].dx) && bbox.x < c[1].x && bbox.x + bbox.dx > c[1].x)
            bbox.dx = c[1].x - bbox.x;

        result.len++;
        result.pages = (int *)realloc(result.pages, sizeof(int) * result.len);
        result.pages[result.len - 1] = pageNo;
        result.rects = (RectI *)realloc(result.rects, sizeof(RectI) * result.len);
        result.rects[result.len - 1] = bbox;
    }
}

bool TextSelection::IsOverGlyph(int pageNo, double x, double y)
{
    int textLen;
    RectI *coords;
    const TCHAR *text = textCache->GetData(pageNo, &textLen, &coords);

    int glyphIx = FindClosestGlyph(pageNo, x, y);
    PointI pt = PointD(x, y).Convert<int>();
    // when over the right half of a glyph, FindClosestGlyph returns the
    // index of the next glyph, in which case glyphIx must be decremented
    if (glyphIx == textLen || !coords[glyphIx].Contains(pt))
        glyphIx--;
    if (-1 == glyphIx)
        return false;
    return coords[glyphIx].Contains(pt);
}

void TextSelection::StartAt(int pageNo, int glyphIx)
{
    startPage = pageNo;
    startGlyph = glyphIx;
    if (glyphIx < 0) {
        int textLen;
        textCache->GetData(pageNo, &textLen);
        startGlyph += textLen + 1;
    }
}

void TextSelection::SelectUpTo(int pageNo, int glyphIx)
{
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
    int fromPage = min(startPage, endPage), toPage = max(startPage, endPage);
    int fromGlyph = (fromPage == endPage ? endGlyph : startGlyph);
    int toGlyph = (fromPage == endPage ? startGlyph : endGlyph);
    if (fromPage == toPage && fromGlyph > toGlyph)
        swap(fromGlyph, toGlyph);

    for (int page = fromPage; page <= toPage; page++) {
        int textLen;
        textCache->GetData(page, &textLen);

        int glyph = page == fromPage ? fromGlyph : 0;
        int length = (page == toPage ? toGlyph : textLen) - glyph;
        if (length > 0)
            FillResultRects(page, glyph, length);
    }
}

void TextSelection::SelectWordAt(int pageNo, double x, double y)
{
    int ix = FindClosestGlyph(pageNo, x, y);
    int textLen;
    const TCHAR *text = textCache->GetData(pageNo, &textLen);

    for (; ix > 0; ix--)
        if (!iswordchar(text[ix - 1]))
            break;
    StartAt(pageNo, ix);

    for (; ix < textLen; ix++)
        if (!iswordchar(text[ix]))
            break;
    SelectUpTo(pageNo, ix);
}

void TextSelection::CopySelection(TextSelection *orig)
{
    Reset();
    StartAt(orig->startPage, orig->startGlyph);
    SelectUpTo(orig->endPage, orig->endGlyph);
}

TCHAR *TextSelection::ExtractText(TCHAR *lineSep)
{
    StrVec lines;

    int fromPage = min(startPage, endPage), toPage = max(startPage, endPage);
    int fromGlyph = (fromPage == endPage ? endGlyph : startGlyph);
    int toGlyph = (fromPage == endPage ? startGlyph : endGlyph);
    if (fromPage == toPage && fromGlyph > toGlyph)
        swap(fromGlyph, toGlyph);

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
