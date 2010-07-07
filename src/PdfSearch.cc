#include "PdfSearch.h"
#include <shlwapi.h>

#define SkipWhitespace(c) for (; _istspace(*(c)); (c)++)
#define islatinalnum(c) (_istalnum(c) && (unsigned short)(c) < 256)

PdfSearch::PdfSearch(PdfEngine *engine)
{
    tracker = NULL;
    text = NULL;
    anchor = NULL;
    pageText = NULL;
    coords = NULL;
    sensitive = false;
    forward = true;
    result.page = 1;
    result.len = 0;
    result.rects = NULL;
    this->engine = engine;
    findIndex = 0;
}

PdfSearch::~PdfSearch()
{
    Clear();
    free(result.rects);
}

void PdfSearch::Reset()
{
    if (pageText || coords) {
        free(pageText);
        pageText = NULL;
        free(coords);
        coords = NULL;
    }
}

void PdfSearch::SetText(TCHAR *text)
{
    this->Clear();
    this->text = tstr_dup(text);

    // extract anchor string (the first word or the first symbol) for faster searching
    TCHAR *c = this->text, *end;
    SkipWhitespace(c);
    if (islatinalnum(*c)) {
        for (end = c; islatinalnum(*end); end++);
        this->anchor = tstr_dupn(c, end - c);
    }
    else
        this->anchor = tstr_dupn(c, 1);
}

void PdfSearch::SetDirection(bool forward)
{
    if (forward == this->forward)
        return;
    this->forward = forward;
    findIndex += lstrlen(text) * (forward ? 1 : -1);
}

void PdfSearch::FillResultRects(TCHAR *found, int length)
{
    fz_bbox *c = &coords[found - pageText], *end = c + length;
    for (; c < end; c++) {
        // skip line breaks
        if (!c->x0 && !c->x1)
            continue;

        result.rects = (RECT *)realloc(result.rects, sizeof(RECT) * ++result.len);
        RECT *rc = &result.rects[result.len - 1];

        fz_bbox c0 = *c;
        for (; c < end && (c->x0 || c->x1); c++);
        c--;
        fz_bbox c1 = *c;

        rc->left = min(c0.x0, c1.x0);
        rc->top = min(c0.y0, c1.y0);
        rc->right = max(c0.x1, c1.x1);
        rc->bottom = max(c0.y1, c1.y1);
        // cut the right edge, if it overlaps the next character's
        if ((c[1].x0 || c[1].x1) && rc->left < c[1].x0 && rc->right > c[1].x0)
            rc->right = c[1].x0;
    }
}

// try to match "text" from "start" with whitespace tolerance
// (ignore all whitespace except after alphanumeric characters)
int PdfSearch::MatchLen(TCHAR *start)
{
    TCHAR *match = text, *end = start;
    SkipWhitespace(match);
    assert(!_istspace(*end));

    while (*match) {
        if (!*end)
            return 0;
        if (sensitive ? *match != *end : _totlower(*match) != _totlower(*end))
            return 0;
        match++;
        end++;
        if (!islatinalnum(*(match - 1)) || _istspace(*(match - 1)) && _istspace(*(end - 1))) {
            SkipWhitespace(match);
            SkipWhitespace(end);
        }
    }

    return end - start;
}

// TODO: use Boyer-Moore algorithm here (if it proves to be faster)
bool PdfSearch::FindTextInPage(int pageNo)
{
    if (!text)
        return false;
    if (!pageNo)
        pageNo = result.page;

    result.len = 0;
    free(result.rects);
    result.rects = NULL;
    result.page = pageNo;

    TCHAR *found;
    int length;
    do {
        if (forward)
            found = (sensitive ? StrStr : StrStrI)(pageText + findIndex, anchor);
        else
            found = StrRStrI(pageText, pageText + findIndex, anchor);
        if (!found)
            return false;
        findIndex = found - pageText + (forward ? 1 : 0);
        length = MatchLen(found);
    } while (!length);

    FillResultRects(found, length);
    findIndex = found - pageText + (forward ? length : 0);

    return true;
}

bool PdfSearch::FindStartingAtPage(int pageNo)
{
    if (!text || !anchor || !*anchor)
        return false;

    int total = engine->pageCount();
    while (1 <= pageNo && pageNo <= total && UpdateTracker(pageNo, total)) {
        Reset();

        pageText = engine->ExtractPageText(pageNo, _T(" "), &coords);
        findIndex = forward ? 0 : lstrlen(pageText);

        if (pageText && FindTextInPage(pageNo))
            return true;

        pageNo += forward ? 1 : -1;
    }
    
    // allow for the first/last page to be included in the next search
    result.page = forward ? total + 1 : 0;

    return false;
}

bool PdfSearch::FindFirst(int page, TCHAR *text)
{
    SetText(text);

    return FindStartingAtPage(page);
}

bool PdfSearch::FindNext()
{
    if (FindTextInPage())
        return true;

    int newPage = result.page + (forward ? 1 : -1);
    return FindStartingAtPage(newPage);
}
