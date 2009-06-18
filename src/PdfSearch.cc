#include "PdfSearch.h"

#define NONE    MAXLONG

#include "wstr_util.h"
#include <ctype.h>

PdfSearch::PdfSearch(PdfEngine *engine)
{
    tracker = NULL;
    text = NULL;
    line = NULL;
    current = NULL;
    last = 0;
    sensitive = false;
    forward = true;
    result.page = 1;
    this->engine = engine;
}

PdfSearch::~PdfSearch()
{
    Clear();
}

void PdfSearch::Reset()
{
    if (line)
        pdf_droptextline(line);
    line = current = NULL;
    last = 0;
}

void PdfSearch::SetText(wchar_t *text)
{
    this->Clear();
    this->length = wcslen(text);
    this->text = wstr_dup(text);
    this->engine = engine;
    this->line = NULL;
    this->current = NULL;
    this->last = NONE;
}

void PdfSearch::ReverseLineList()
{
    if (!line)
        return;

    pdf_textline *prev = line, *curr = line->next;
    while (curr) {
        pdf_textline *next = curr->next;
        curr->next = prev;
        prev = curr;
        curr = next;
    }
    line->next = NULL;
    line = prev;
}

void PdfSearch::SetDirection(bool forward)
{
    if (forward == this->forward)
        return;
    this->forward = forward;

    if (forward)
        last = last + 2;
    else
        last = last - 2;

    ReverseLineList();
}

#define CHR(x) (WCHAR)(x)

bool PdfSearch::MatchChars(int c1, int c2)
{
    if (c1 == c2)
        return true;
    if (sensitive)
        return false;
    if (CharUpperW((LPWSTR)MAKELONG(CHR(c1),0)) == CharUpperW((LPWSTR)MAKELONG(CHR(c2),0)))
        return true;
    return false;
}

bool PdfSearch::MatchAtPosition(int n)
{
    WCHAR *p = (WCHAR *)text;
    result.left = current->text[n].bbox.x0;
    result.top = current->text[n].bbox.y0;
    last = n;

    while (n < current->len && *p) {
        if (!MatchChars((int)*p, current->text[n].c))
            break;
        p++;
        n++;
    }

    if (*p == 0) {
        // Found
        result.right = current->text[n-1].bbox.x1;
        if (current->len > n) {
            if (result.right > current->text[n].bbox.x0)
                result.right = current->text[n].bbox.x0;
        }
        result.bottom = current->text[n-1].bbox.y1;
        if (forward)
            last = last + 1;
        else
            last = last - 1;
        return true;
    }
    return false;
}

// TODO: use Boyer-Moore algorithm here (if it proves to be faster)
bool PdfSearch::FindTextInPage(int page)
{
    if (!text)
        return false;

    WCHAR p = *(WCHAR *)text;
    int start = last;

    if (forward) {
        if (NONE == start)
            start = 0;
        while (current) {
            for (int i = start; i < current->len; i++) {
                if (MatchChars((int)p, current->text[i].c)) {
                    if (MatchAtPosition(i))
                        goto Found;
                }
            }
            current = current->next;
            start = 0;
        }
    } else {
        if (current && NONE == start)
            start = current->len - length;
        while (current) {
            for (int i = start; i >= 0; i--) {
                if (MatchChars((int)p, current->text[i].c)) {
                    if (MatchAtPosition(i))
                        goto Found;
                }
            }
            current = current->next;
            if (current)
                start = current->len - length;
        }
    }
    return false;

Found:
    if (page > 0)
        result.page = page;
    return true;
}

bool PdfSearch::FindStartingAtPage(int pageNo)
{
    if (!text)
        return false;

    int pageEnd, step;
    int total = engine->pageCount();

    if (forward) {
        pageEnd = total + 1;
        step = 1;
    } else {
        pageEnd = 0;
        step = -1;
    }

    while (pageNo != pageEnd) {
        UpdateTracker(pageNo, total);
        Reset();

        pdf_page *page = engine->getPdfPage(pageNo);
        if (!page)
            goto NextPage;

        if (pdf_loadtextfromtree(&line, page->tree, fz_identity())) // if error
            goto NextPage;
        this->line = line;
        if (!forward)
            ReverseLineList();

        this->current = this->line;
        if (FindTextInPage(pageNo))
            return true;

    NextPage:
        pageNo += step;
    }
    
    return false;
}

bool PdfSearch::FindFirst(int page, wchar_t *text)
{
    SetText(text);

    return FindStartingAtPage(page);
}

bool PdfSearch::FindNext()
{
    if (FindTextInPage())
        return true;

    int newPage;
    if (forward) {
        newPage = result.page + 1;
    } else {
        newPage = result.page - 1;
    }
    return FindStartingAtPage(newPage);
}
