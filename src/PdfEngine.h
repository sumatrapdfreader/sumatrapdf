/* Copyright Krzysztof Kowalczyk 2006-2009
   License: GPLv3 */
#ifndef _PDF_ENGINE_H_
#define _PDF_ENGINE_H_

extern "C" {
#include <fitz.h>
#include <mupdf.h>
}

#include "base_util.h"
#include "geom_util.h"
#include "tstr_util.h"

/* Describes a link on PDF page. */
typedef struct PdfLink {
    int             pageNo;     /* on which Pdf page the link exists. 1..pageCount */
    RectD           rectPage;   /* position of the link on the page */
    RectI           rectCanvas; /* position of the link on canvas */
    pdf_link *      link;
} PdfLink;

typedef struct PdfPage {
    pdf_page *      page;
    int             num; // the page's object number
} PdfPage;

class WindowInfo;

#define INVALID_PAGE_NO     -1
#define INVALID_ROTATION    -1
/* It seems that PDF documents are encoded assuming DPI of 72.0 */
#define PDF_FILE_DPI        72

class PdfTocItem {
public:
    TCHAR *title;
    pdf_link *link;
    bool open;
    int pageNo;

    PdfTocItem *child;
    PdfTocItem *next;

    PdfTocItem(TCHAR *title, pdf_link *link)
    {
        this->title = title;
        this->link = link;
        this->child = NULL;
        this->next = NULL;
        this->open = true;
        this->pageNo = 0;
    }

    ~PdfTocItem()
    {
        delete child;
        delete next;
        free(title);
    }

    void AddChild(PdfTocItem *child)
    {
        AppendTo(&this->child, child);
    }
    
    void AddSibling(PdfTocItem *sibling)
    {
        AppendTo(&this->next, sibling);
    }

private:
    void AppendTo(PdfTocItem **root, PdfTocItem *item)
    {
        if (!root || !item)
            return;
        if (!*root) {
            *root = item;
            return;
        }
        PdfTocItem *p = *root;
        while (p->next)
            p = p->next;
        p->next = item;
    }
};

class RenderedBitmap {
public:
    RenderedBitmap(fz_pixmap *);
    ~RenderedBitmap();

    int dx() { return _bitmap->w; }
    int dy() { return _bitmap->h; }
    int rowSize();
    unsigned char *data();

    HBITMAP createDIBitmap(HDC);
    void stretchDIBits(HDC, int, int, int, int);
protected:
    fz_pixmap *_bitmap;
};

class PdfEngine {
public:
    PdfEngine();
    ~PdfEngine();
    const TCHAR *fileName(void) const { return _fileName; };

    void setFileName(const TCHAR *fileName) {
        assert(!_fileName);
        _fileName = (const TCHAR*)tstr_dup(fileName);
    }

    bool validPageNo(int pageNo) const {
        if ((pageNo >= 1) && (pageNo <= pageCount()))
            return true;
        return false;
    }

    int pageCount(void) const { 
        return _pageCount; 
    }

    bool load(const TCHAR *fileName, WindowInfo *windowInfo, bool tryrepair);
    int pageRotation(int pageNo);
    SizeD pageSize(int pageNo);
    RenderedBitmap *renderBitmap(int pageNo, double zoomReal, int rotation,
                         fz_rect *pageRect, /* if NULL: defaults to the page's mediabox */
                         BOOL (*abortCheckCbkA)(void *data),
                         void *abortCheckCbkDataA);

    bool hasPermission(int permission);
    int linkCount();
    void fillPdfLinks(PdfLink *pdfLinks, int linkCount);
    bool hasTocTree() { 
        return _outline != NULL; 
    }
    PdfTocItem *getTocTree();
    fz_matrix viewctm (pdf_page *page, float zoom, int rotate);

    pdf_page * getPdfPage(int pageNo);
    int        findPageNo(fz_obj *dest);
    fz_obj    *getNamedDest(const char *name);
    char     * getPageLayoutName(void);
    TCHAR    * PdfEngine::ExtractPageText(pdf_page *page, TCHAR *lineSep=_T(DOS_NEWLINE), fz_irect **coords_out=NULL);
    TCHAR    * PdfEngine::ExtractPageText(int pageNo, TCHAR *lineSep=_T(DOS_NEWLINE), fz_irect **coords_out=NULL) {
        return ExtractPageText(getPdfPage(pageNo), lineSep, coords_out);
    };

protected:
    const TCHAR *_fileName;
    int _pageCount;
    WindowInfo *_windowInfo;

private:
    HANDLE            _getPageSem;

    void dropPdfPage(int pageNo);
    PdfTocItem * buildTocTree(pdf_outline *entry);
    void PdfEngine::linkifyPageText(pdf_page *page);

    pdf_xref *      _xref;
    pdf_outline *   _outline;
    PdfPage *       _pages;
    fz_renderer *   _rast;
};

#endif
