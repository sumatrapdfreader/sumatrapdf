#ifndef _PDF_SEARCH_H
#define _PDF_SEARCH_H

#include <windows.h>
#include "PdfEngine.h"

class PDFDoc;
class TextOutputDev;
class TextPage;

typedef struct {
    int page;
    int left;
    int top;
    int right;
    int bottom;
} PdfSearchResult;

class PdfSearchEngine
{
protected:
    void *text;
    int   length;

public:
    bool sensitive;
    bool forward;

    PdfSearchResult result;

public:
    PdfSearchEngine()
    {
        text = NULL;
        sensitive = true;
        forward = true;
    }

    virtual ~PdfSearchEngine()
    {
        Clear();
    }
    
    virtual void Reset() {}
    virtual void SetText(wchar_t *text);
    virtual bool FindFirst(int page, wchar_t *text) = 0;
    virtual bool FindNext() = 0;

protected:
    void Clear()
    {
        free(text);
        Reset();
    }
};

class PdfSearchPoppler : public PdfSearchEngine
{
private:
    PdfEnginePoppler *engine;
    TextOutputDev *dev;
    TextPage *page;
public:
    PdfSearchPoppler(PdfEnginePoppler *engine);
    virtual ~PdfSearchPoppler();

    virtual void Reset();
    virtual bool FindFirst(int page, wchar_t *text);
    virtual bool FindNext();

protected:
    bool FindStartingAtPage(int page);
};

class PdfSearchFitz : public PdfSearchEngine
{
private:
    PdfEngineFitz *engine;
    pdf_textline *line;
    pdf_textline *current;
    int last;
public:
    PdfSearchFitz(PdfEngineFitz *engine);
    virtual ~PdfSearchFitz();

    virtual void Reset();
    virtual bool FindFirst(int page, wchar_t *text);
    virtual bool FindNext();

protected:
    bool FindTextInPage(int page = 0);
    bool FindStartingAtPage(int page);
};

#endif // _PDF_SEARCH_H
