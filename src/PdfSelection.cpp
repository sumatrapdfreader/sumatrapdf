/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "PdfSelection.h"
#include "Vec.h"

PdfSelection::PdfSelection(PdfEngine *engine) : engine(engine)
{
    coords = SAZA(fz_bbox *, engine->pageCount());
    text = SAZA(TCHAR *, engine->pageCount());
    lens = SAZA(int, engine->pageCount());

    result.len = 0;
    result.pages = NULL;
    result.rects = NULL;

    startPage = -1;
    startGlyph = -1;
}

PdfSelection::~PdfSelection()
{
    Reset();

    for (int i = 0; i < engine->pageCount(); i++) {
        free(coords[i]);
        coords[i] = NULL;
        free(text[i]);
        text[i] = NULL;
    }

    free(coords);
    free(text);
    free(lens);
}

void PdfSelection::Reset()
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
int PdfSelection::FindClosestGlyph(int pageNo, double x, double y)
{
    assert(1 <= pageNo && pageNo <= engine->pageCount());
    if (!text[pageNo - 1]) {
        text[pageNo - 1] = engine->ExtractPageText(pageNo, _T("\n"), &coords[pageNo - 1]);
        if (!text[pageNo - 1]) {
            text[pageNo - 1] = Str::Dup(_T(""));
            lens[pageNo - 1] = 0;
            return 0;
        }
        lens[pageNo - 1] = Str::Len(text[pageNo - 1]);
    }

    double maxDist = -1;
    int result = -1;
    fz_bbox *_coords = coords[pageNo - 1];

    for (int i = 0; i < lens[pageNo - 1]; i++) {
        if (!_coords[i].x0 && !_coords[i].x1)
            continue;

        double dist = _hypot(x - 0.5 * (_coords[i].x0 + _coords[i].x1),
                             y - 0.5 * (_coords[i].y0 + _coords[i].y1));
        if (maxDist < 0 || dist < maxDist) {
            result = i;
            maxDist = dist;
        }
    }

    if (-1 == result)
        return 0;
    assert(0 <= result && result < lens[pageNo - 1]);

    // the result indexes the first glyph to be selected in a forward selection
    fz_matrix ctm = engine->viewctm(pageNo, 1.0, 0);
    fz_bbox bbox = fz_transformbbox(ctm, _coords[result]);
    fz_point pt = { (float)x, (float)y };
    pt = fz_transformpoint(ctm, pt);
    if (pt.x > 0.5 * (bbox.x0 + bbox.x1))
        result++;

    return result;
}

void PdfSelection::FillResultRects(int pageNo, int glyph, int length, VStrList *lines)
{
    fz_bbox mediabox = fz_roundrect(engine->pageMediabox(pageNo));
    fz_bbox *c = &coords[pageNo - 1][glyph], *end = c + length;
    for (; c < end; c++) {
        // skip line breaks
        if (!c->x0 && !c->x1)
            continue;

        fz_bbox c0 = *c, *c0p = c;
        for (; c < end && (c->x0 || c->x1); c++);
        c--;
        fz_bbox c1 = *c;
        fz_bbox bbox = fz_intersectbbox(fz_unionbbox(c0, c1), mediabox);
        // skip text that's completely outside a page's mediabox
        if (fz_isemptybbox(bbox))
            continue;

        if (lines) {
            lines->Push(Str::DupN(text[pageNo - 1] + (c0p - coords[pageNo - 1]), c - c0p + 1));
            continue;
        }

        // cut the right edge, if it overlaps the next character
        if ((c[1].x0 || c[1].x1) && bbox.x0 < c[1].x0 && bbox.x1 > c[1].x0)
            bbox.x1 = c[1].x0;

        result.len++;
        result.pages = (int *)realloc(result.pages, sizeof(int) * result.len);
        result.pages[result.len - 1] = pageNo;
        result.rects = (RectI *)realloc(result.rects, sizeof(RectI) * result.len);
        result.rects[result.len - 1] = RectI::FromXY(bbox.x0, bbox.y0, bbox.x1, bbox.y1);
    }
}

bool fz_isptinbbox(fz_bbox bbox, fz_point pt)
{
    return MIN(bbox.x0, bbox.x1) <= pt.x && pt.x < MAX(bbox.x0, bbox.x1) &&
           MIN(bbox.y0, bbox.y1) <= pt.y && pt.y < MAX(bbox.y0, bbox.y1);
}

bool PdfSelection::IsOverGlyph(int pageNo, double x, double y)
{
    int glyphIx = FindClosestGlyph(pageNo, x, y);
    fz_point pt = { (float)x, (float)y };
    fz_bbox *_coords = coords[pageNo - 1];
    // when over the right half of a glyph, FindClosestGlyph returns the
    // index of the next glyph, in which case glyphIx must be decremented
    if (glyphIx == lens[pageNo - 1] || !fz_isptinbbox(_coords[glyphIx], pt))
        glyphIx--;
    if (-1 == glyphIx)
        return false;
    return fz_isptinbbox(_coords[glyphIx], pt);
}

void PdfSelection::StartAt(int pageNo, int glyphIx)
{
    startPage = pageNo;
    startGlyph = glyphIx;
    if (glyphIx < 0) {
        FindClosestGlyph(pageNo, 0, 0);
        startGlyph = lens[pageNo - 1] + glyphIx + 1;
    }
}

void PdfSelection::SelectUpTo(int pageNo, int glyphIx)
{
    if (startPage == -1 || startGlyph == -1)
        return;

    endPage = pageNo;
    endGlyph = glyphIx;
    if (glyphIx < 0) {
        FindClosestGlyph(pageNo, 0, 0);
        endGlyph = lens[pageNo - 1] + glyphIx + 1;
    }

    result.len = 0;
    int fromPage = min(startPage, endPage), toPage = max(startPage, endPage);
    int fromGlyph = (fromPage == endPage ? endGlyph : startGlyph);
    int toGlyph = (fromPage == endPage ? startGlyph : endGlyph);
    if (fromPage == toPage && fromGlyph > toGlyph)
        swap(fromGlyph, toGlyph);

    for (int page = fromPage; page <= toPage; page++) {
        // make sure that glyph coordinates and page text have been cached
        if (!coords[page - 1])
            FindClosestGlyph(page, 0, 0);

        int glyph = page == fromPage ? fromGlyph : 0;
        int length = (page == toPage ? toGlyph : lens[page - 1]) - glyph;
        if (length > 0)
            FillResultRects(page, glyph, length);
    }
}

TCHAR *PdfSelection::ExtractText(TCHAR *lineSep)
{
    VStrList lines;

    int fromPage = min(startPage, endPage), toPage = max(startPage, endPage);
    int fromGlyph = (fromPage == endPage ? endGlyph : startGlyph);
    int toGlyph = (fromPage == endPage ? startGlyph : endGlyph);
    if (fromPage == toPage && fromGlyph > toGlyph)
        swap(fromGlyph, toGlyph);

    for (int page = fromPage; page <= toPage; page++) {
        int glyph = page == fromPage ? fromGlyph : 0;
        int length = (page == toPage ? toGlyph : lens[page - 1]) - glyph;
        if (length > 0)
            FillResultRects(page, glyph, length, &lines);
    }

    return lines.Join(lineSep);
}
