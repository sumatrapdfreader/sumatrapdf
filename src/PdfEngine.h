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

class WindowInfo;

#define INVALID_PAGE_NO     -1
#define INVALID_ROTATION    -1
/* one PDF user space unit equals 1/72 inch */
#define PDF_FILE_DPI        72.0

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
    RenderedBitmap(HBITMAP hbmp, int width, int height) :
        _hbmp(hbmp), _width(width), _height(height) { }
    RenderedBitmap(fz_pixmap *pixmap, HDC hDC=NULL);
    ~RenderedBitmap() { DeleteObject(_hbmp); }

    // callers must not delete this (use CopyImage if you have to modify it)
    HBITMAP getBitmap() const { return _hbmp; }
    int dx() const { return _width; }
    int dy() const { return _height; }

    void stretchDIBits(HDC hdc, int leftMargin, int topMargin, int pageDx, int pageDy);
    void invertColors();

protected:
    HBITMAP _hbmp;
    int     _width;
    int     _height;
};

class PdfEngine {
public:
    PdfEngine();
    ~PdfEngine();
    const TCHAR *fileName(void) const { return _fileName; };
    int pageCount(void) const { return _pageCount; }

    bool validPageNo(int pageNo) const {
        if ((pageNo >= 1) && (pageNo <= _pageCount))
            return true;
        return false;
    }

    bool load(const TCHAR *fileName, WindowInfo *windowInfo, bool tryrepair);
    int pageRotation(int pageNo);
    SizeD pageSize(int pageNo);
    RenderedBitmap *renderBitmap(int pageNo, double zoomReal, int rotation,
                         fz_rect *pageRect, /* if NULL: defaults to the page's mediabox */
                         BOOL (*abortCheckCbkA)(void *data),
                         void *abortCheckCbkDataA,
                         bool useGdi=false);
    bool PdfEngine::renderPage(HDC hDC, pdf_page *page, RECT *screenRect,
                         fz_matrix *ctm=NULL, double zoomReal=0, int rotation=0, fz_rect *pageRect=NULL);

    bool hasPermission(int permission);
    int linkCount();
    void fillPdfLinks(PdfLink *pdfLinks, int linkCount);
    bool hasTocTree() const { 
        return _outline != NULL || _attachments != NULL; 
    }
    void ageStore();
    PdfTocItem *getTocTree();
    fz_matrix viewctm (pdf_page *page, float zoom, int rotate);

    pdf_page * getPdfPage(int pageNo);
    int        findPageNo(fz_obj *dest);
    fz_obj   * getNamedDest(const char *name);
    char     * getPageLayoutName(void);
    TCHAR    * ExtractPageText(pdf_page *page, TCHAR *lineSep=_T(DOS_NEWLINE), fz_bbox **coords_out=NULL);
    TCHAR    * ExtractPageText(int pageNo, TCHAR *lineSep=_T(DOS_NEWLINE), fz_bbox **coords_out=NULL) {
        return ExtractPageText(getPdfPage(pageNo), lineSep, coords_out);
    };
    fz_obj   * getPdfInfo(void) { return _xref ? fz_dictgets(_xref->trailer, "Info") : NULL; }
    int        getPdfVersion(void) const { return _xref->version; }
    char     * getDecryptionKey(void) const { return _decryptionKey ? fz_strdup(_decryptionKey) : NULL; }
    fz_buffer* getStreamData(int num=0, int gen=0);

protected:
    const TCHAR *_fileName;
    char *_decryptionKey;
    int _pageCount;
    WindowInfo *_windowInfo;

    CRITICAL_SECTION _xrefAccess;
    pdf_xref *      _xref;

    CRITICAL_SECTION _pagesAccess;
    pdf_page **     _pages;

    void dropPdfPage(int pageNo);
    PdfTocItem    * buildTocTree(pdf_outline *entry);
    void            linkifyPageText(pdf_page *page);

    pdf_outline   * _outline;
    pdf_outline   * _attachments;
    fz_glyphcache * _drawcache;
};

#endif
