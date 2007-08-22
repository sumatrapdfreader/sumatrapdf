#include "PdfSearch.h"

#include <PDFDoc.h>
#include <TextOutputDev.h>
#include <Page.h>
#include <UnicodeTypeTable.h>

#define NONE    MAXLONG

void PdfSearchEngine::SetText(wchar_t *text)
{
    int n = wcslen(text) + 1;
    Unicode *data = (Unicode *)malloc(n * sizeof(Unicode));

    for (int i = 0; i < n; i++)
        data[i] = text[i];

    this->Clear();
    this->length = n - 1;
    this->text = data;
}

// Poppler
PdfSearchPoppler::PdfSearchPoppler(PdfEnginePoppler *engine): PdfSearchEngine()
{
    this->engine = engine;
    this->dev = new TextOutputDev(NULL, gTrue, gFalse, gFalse);
    this->page = NULL;
}

PdfSearchPoppler::~PdfSearchPoppler()
{
    delete dev;
}

void PdfSearchPoppler::Reset()
{
    delete page;
    page = NULL;
}

bool PdfSearchPoppler::FindStartingAtPage(int startPage)
{
    PDFDoc *doc = engine->pdfDoc();
    int pageCount = doc->getNumPages();

    for (int pageNo = startPage; pageNo <= pageCount; pageNo++) {
        Reset();

        doc->displayPage(dev, pageNo, 72.0, 72.0, 0, gFalse, gTrue, gFalse);
        page = dev->takeText();
        if (!page) continue;

        double left, right, top, bottom;
        if (page->findText((Unicode *)text, length,
                           gTrue, gTrue, gFalse, gFalse, sensitive, !forward,
                           &left, &top, &right, &bottom)) {
            result.page = pageNo;
            result.left = (int)floor(left);
            result.top = (int)floor(top);
            result.right = (int)ceil(right);
            result.bottom = (int)ceil(bottom);
            return true;
        }
    }
    return false;
}

bool PdfSearchPoppler::FindFirst(int page, wchar_t *text)
{
    SetText(text);

    return FindStartingAtPage(page);
}

bool PdfSearchPoppler::FindNext()
{
    if (!page)
        return false;

    double left, right, top, bottom;
    if (page->findText((Unicode *)text, length,
                       gFalse, gTrue, gTrue, gFalse, sensitive, !forward,
                       &left, &top, &right, &bottom)) {
        result.left = (int)floor(left);
        result.top = (int)floor(top);
        result.right = (int)ceil(right);
        result.bottom = (int)ceil(bottom);
        return true;
    }

    return FindStartingAtPage(result.page + 1);
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

bool inline PdfSearchFitz::MatchAtPosition(int n)
{
    Unicode *p = (Unicode *)text;
    result.left = current->text[n].bbox.x0;
    result.top = current->text[n].bbox.y0;
    last = n;

    while (n < current->len && *p) {
        Unicode c = current->text[n].c;
        if (*p != c && (sensitive || unicodeToUpper(*p) != unicodeToUpper(c)))
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

    return false;
}

// TODO:
// Apply Boyer-Moore algorithm here
bool PdfSearchFitz::FindTextInPage(int page)
{
    if (!text)
        return false;

    Unicode p = *(Unicode *)text;
    int start = last;

    if (forward) {
        if (NONE == start)
            start = 0;
        while (current) {
            for (int i = start; i < current->len; i++) {
                if (p == current->text[i].c) {
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
                if (p == current->text[i].c) {
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

bool PdfSearchFitz::FindStartingAtPage(int pageNo)
{
    if (!text)
        return false;

    int pageEnd, step;

    if (forward) {
        pageEnd = engine->pageCount() + 1;
        step = 1;
    } else {
        pageEnd = 0;
        step = -1;
    }

    while (pageNo != pageEnd) {
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
