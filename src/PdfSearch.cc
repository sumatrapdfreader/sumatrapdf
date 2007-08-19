#include "PdfSearch.h"

#include <PDFDoc.h>
#include <TextOutputDev.h>
#include <Page.h>

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
                           gTrue, gTrue, gFalse, gFalse, gFalse, gFalse,
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
                       gFalse, gTrue, gTrue, gFalse, gFalse, gFalse,
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
    this->last = -1;
}

PdfSearchFitz::~PdfSearchFitz()
{
}

void PdfSearchFitz::Reset()
{
    if (line)
        pdf_droptextline(line);
    line = current = NULL;
    last = -1;
}

// TODO:
// Apply Boyer-Moore algorithm here
bool PdfSearchFitz::FindTextInPage(int page)
{
    Unicode *p = (Unicode *)text;
    int start = last > 0 ? last : 0;

    while (current) {
        for (int i = start; i < current->len; i++) {
            if (*p == current->text[i].c) {
                result.left = current->text[i].bbox.x0;
                result.top = current->text[i].bbox.y0;
                last = i;

                while (i < current->len && *p && *p == current->text[i].c) {
                    p++;
                    i++;
                }

                if (*p == 0) { // Found
                    result.right = current->text[i-1].bbox.x1;
                    result.bottom = current->text[i-1].bbox.y1;
                    if (page > 0)
                        result.page = page;
                    last = last + 1;
                    return true;
                }
                else {
                    i = last;
                }
                p = (Unicode *)text;
            }
        }

        current = current->next;
        start = 0;
    }
    return false;
}

bool PdfSearchFitz::FindStartingAtPage(int startPage)
{
    int pageCount = engine->pageCount();

    for (int pageNo = startPage; pageNo <= pageCount; pageNo++) {
        Reset();

        pdf_page *page = engine->getPdfPage(pageNo);
        if (!page) continue;

        pdf_textline *line;
        if (pdf_loadtextfromtree(&line, page->tree, fz_identity())) // if error
            continue;

        this->line = this->current = line;
        if (FindTextInPage(pageNo))
            return true;
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
    if (!current)
        return false;

    if (FindTextInPage())
        return true;

    return FindStartingAtPage(result.page + 1);
}
