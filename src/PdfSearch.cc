#include "PdfSearch.h"

#define NONE    MAXLONG

#include "wstr_util.h"

void PdfSearchEngine::SetText(wchar_t *text)
{
#if 0 // TODO: figure out what to do with it when poppler is no longer in play
    int n = wcslen(text) + 1;
    Unicode *data = (Unicode *)malloc(n * sizeof(Unicode));

    for (int i = 0; i < n; i++)
        data[i] = text[i];
#endif

    this->Clear();
    this->length = wcslen(text);
    this->text = wstr_dup(text);
}

// Fitz
PdfSearchFitz::PdfSearchFitz(PdfEngineFitz *engine) : PdfSearchEngine()
{
    this->engine = engine;
    this->line = NULL;
    this->current = NULL;
    this->last = NONE;
}

PdfSearchFitz::~PdfSearchFitz()
{
}

void PdfSearchFitz::Reset()
{
    if (line)
        pdf_droptextline(line);
    line = current = NULL;
    last = NONE;
}

void PdfSearchFitz::ReverseLineList()
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

void PdfSearchFitz::SetDirection(bool forward)
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

bool inline PdfSearchFitz::MatchChars(int c1, int c2)
{
    if (sensitive)
        return c1 == c2;
// TODO: fix this
//    return (c1 == c2) || (unicodeToUpper(c1) == unicodeToUpper(c2));
    return false;
}

bool inline PdfSearchFitz::MatchAtPosition(int n)
{
#if 0 // TODO: fix this to not depend on poppler
    Unicode *p = (Unicode *)text;
    result.left = current->text[n].bbox.x0;
    result.top = current->text[n].bbox.y0;
    last = n;

    while (n < current->len && *p) {
        if (!MatchChars(*p, current->text[n].c))
            break;
        p++;
        n++;
    }

    if (*p == 0) { // Found
        result.right = current->text[n-1].bbox.x1;
        result.bottom = current->text[n-1].bbox.y1;
        if (forward)
            last = last + 1;
        else
            last = last - 1;
        return true;
    }
#endif
    return false;
}

// TODO:
// Apply Boyer-Moore algorithm here
bool PdfSearchFitz::FindTextInPage(int page)
{
#if 0 // TODO: fix to not depend on poppler
    if (!text)
        return false;

    Unicode p = *(Unicode *)text;
    int start = last;

    if (forward) {
        if (NONE == start)
            start = 0;
        while (current) {
            for (int i = start; i < current->len; i++) {
                if (MatchChars(p, current->text[i].c)) {
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
                if (MatchChars(p, current->text[i].c)) {
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
#endif
    return false;
}

bool PdfSearchFitz::FindStartingAtPage(int pageNo)
{
#if 0 // TODO: fix to not depend on poppler
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

        pdf_textline *line;
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
#endif
    return false;
}

bool PdfSearchFitz::FindFirst(int page, wchar_t *text)
{
    SetText(text);

    return FindStartingAtPage(page);
}

bool PdfSearchFitz::FindNext()
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
