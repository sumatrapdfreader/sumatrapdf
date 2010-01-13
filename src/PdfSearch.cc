#include "PdfSearch.h"
#include <shlwapi.h>

#define LINESEPLEN 1

PdfSearch::PdfSearch(PdfEngine *engine)
{
    tracker = NULL;
    text = NULL;
    pageText = NULL;
    line = NULL;
    sensitive = false;
    forward = true;
    result.page = 1;
    result.len = 0;
    result.rects = NULL;
    this->engine = engine;
}

PdfSearch::~PdfSearch()
{
    Clear();
    free(result.rects);
}

void PdfSearch::Reset()
{
    if (pageText) {
        free(pageText);
        pageText = NULL;
    }
    if (line) {
        pdf_droptextline(line);
        line = NULL;
    }
}

void PdfSearch::SetText(TCHAR *text)
{
    this->Clear();
    this->text = tstr_dup(text);
    this->length = lstrlen(text);
}

void PdfSearch::SetDirection(bool forward)
{
    if (forward == this->forward)
        return;
    this->forward = forward;
    findIndex += length * (forward ? 1 : -1);
}

void PdfSearch::FillResultRects(TCHAR *found)
{
    // skip to the line containing the match
    pdf_textline *current;
    int count = 0;
    for (current = line; current && count + current->len < findIndex; current = current->next)
        count += current->len + LINESEPLEN;
    // get the index of the first character on that line
    int n = found - pageText - count, nRest = length;
    if (n == current->len) { // skip line if the first character was lineSep
        current = current->next;
        n = 0;
        nRest--;
    }

    while (nRest > 0) {
        result.rects = (RECT *)realloc(result.rects, sizeof(RECT) * ++result.len);
        RECT *rc = &result.rects[result.len - 1];

        rc->left = current->text[n].bbox.x0;
        rc->top = current->text[n].bbox.y0;

        if (n + nRest < current->len) {
            n += nRest;
            nRest = 0;
        } else {
            nRest -= current->len - n + LINESEPLEN;
            n = current->len - 1;
        }

        rc->right = current->text[n-1].bbox.x1;
        if (current->len > n && rc->right > current->text[n].bbox.x0)
            rc->right = current->text[n].bbox.x0;
        rc->bottom = current->text[n-1].bbox.y1;

        if (rc->bottom < rc->top)
            swap_int((int *)&rc->bottom, (int *)&rc->top);
        if (rc->right < rc->left)
            swap_int((int *)&rc->right, (int *)&rc->left);

        if (nRest > 0) {
            current = current->next;
            n = 0;
        }
    }
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

    TCHAR *found = NULL;
    if (forward)
        found = (sensitive ? StrStr : StrStrI)(pageText + findIndex, text);
    else
        do { // unfortunately, there's no StrRStr...
            found = StrRStrI(pageText, pageText + findIndex, text);
            findIndex = found - pageText + length - 1;
        } while (found && sensitive && StrCmpN(text, found, length) != 0);

    if (found) {
        findIndex = found - pageText;
        FillResultRects(found);
        if (forward)
            findIndex += length;
    }

    return found != NULL;
}

bool PdfSearch::FindStartingAtPage(int pageNo)
{
    if (!text)
        return false;

    int total = engine->pageCount();
    while (1 <= pageNo && pageNo <= total && UpdateTracker(pageNo, total)) {
        Reset();

        pageText = engine->ExtractPageText(pageNo, _T(" "), &line);
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
