#include "PdfSearch.h"
#include <shlwapi.h>

#define SkipWhitespace(c) for (; _istspace(*(c)); (c)++)
#define islatinalnum(c) (_istalnum(c) && (unsigned short)(c) < 256)

PdfSearch::PdfSearch(PdfEngine *engine) : PdfSelection(engine)
{
    tracker = NULL;
    text = NULL;
    anchor = NULL;
    pageText = NULL;
    sensitive = false;
    forward = true;
    findPage = 1;
    findIndex = 0;
}

PdfSearch::~PdfSearch()
{
    Clear();
}

void PdfSearch::Reset()
{
    if (pageText) {
        free(pageText);
        pageText = NULL;
    }
    PdfSelection::Reset();
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
        pageNo = findPage;
    findPage = pageNo;

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

    StartAt(pageNo, found - pageText);
    SelectUpTo(pageNo, found - pageText + length);
    findIndex = found - pageText + (forward ? length : 0);

    // try again if the found text is completely outside the page's mediabox
    if (result.len == 0)
        return FindTextInPage(pageNo);

    return true;
}

bool PdfSearch::FindStartingAtPage(int pageNo)
{
    if (!text || !anchor || !*anchor)
        return false;

    int total = engine->pageCount();
    while (1 <= pageNo && pageNo <= total && UpdateTracker(pageNo, total)) {
        Reset();

        fz_bbox **pcoords = !coords[pageNo - 1] ? &coords[pageNo - 1] : NULL;
        pageText = engine->ExtractPageText(pageNo, _T(" "), pcoords);
        findIndex = forward ? 0 : lstrlen(pageText);

        if (pageText && FindTextInPage(pageNo))
            return true;

        pageNo += forward ? 1 : -1;
    }
    
    // allow for the first/last page to be included in the next search
    findPage = forward ? total + 1 : 0;

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

    int newPage = findPage + (forward ? 1 : -1);
    return FindStartingAtPage(newPage);
}
