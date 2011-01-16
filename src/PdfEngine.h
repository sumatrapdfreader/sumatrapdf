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

class WindowInfo;

#define INVALID_PAGE_NO     -1
#define INVALID_ROTATION    -1
/* one PDF user space unit equals 1/72 inch */
#define PDF_FILE_DPI        72.0
// number of page content trees to cache for quicker rendering
#define MAX_PAGE_RUN_CACHE  8

/* certain OCGs will only be rendered for some of these (e.g. watermarks) */
enum RenderTarget { Target_View, Target_Print, Target_Export };

typedef struct {
    pdf_page *page;
    fz_displaylist *list;
    int refs;
} PdfPageRun;

class PdfTocItem {
public:
    TCHAR *title;
    pdf_link *link;
    bool open;
    int pageNo;

    PdfTocItem *child;
    PdfTocItem *next;

    PdfTocItem(TCHAR *title, pdf_link *link) :
        title(title), link(link), open(true), pageNo(0), child(NULL), next(NULL) { }

    ~PdfTocItem()
    {
        delete child;
        delete next;
        free(title);
    }

    void AddSibling(PdfTocItem *sibling)
    {
        PdfTocItem *item;
        for (item = this; item->next; item = item->next);
        item->next = sibling;
    }
};

class RenderedBitmap {
public:
    bool outOfDate;

    RenderedBitmap(HBITMAP hbmp, int width, int height) :
        _hbmp(hbmp), _width(width), _height(height), outOfDate(false) { }
    RenderedBitmap(fz_pixmap *pixmap, HDC hDC);
    ~RenderedBitmap() { DeleteObject(_hbmp); }

    // callers must not delete this (use CopyImage if you have to modify it)
    HBITMAP getBitmap() const { return _hbmp; }
    int dx() const { return _width; }
    int dy() const { return _height; }

    void stretchDIBits(HDC hdc, int leftMargin, int topMargin, int pageDx, int pageDy);
    void grayOut(float alpha);
    void invertColors() { grayOut(-1); }

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

    bool load(const TCHAR *fileName, WindowInfo *windowInfo);
    bool load(fz_stream *stm, TCHAR *password=NULL);
    int pageRotation(int pageNo);
    SizeD pageSize(int pageNo);
    fz_rect pageMediabox(int pageNo);
    fz_bbox pageContentBox(int pageNo, RenderTarget target=Target_View);
    RenderedBitmap *renderBitmap(int pageNo, float zoom, int rotation,
                         fz_rect *pageRect, /* if NULL: defaults to the page's mediabox */
                         BOOL (*abortCheckCbkA)(void *data),
                         void *abortCheckCbkDataA,
                         RenderTarget target=Target_View,
                         bool useGdi=false);
    bool PdfEngine::renderPage(HDC hDC, int pageNo, RECT *screenRect,
                         fz_matrix *ctm=NULL, float zoom=0, int rotation=0,
                         fz_rect *pageRect=NULL, RenderTarget target=Target_View) {
        return renderPage(hDC, getPdfPage(pageNo), screenRect, ctm, zoom, rotation, pageRect, target);
    }

    bool hasPermission(int permission);
    int getPdfLinks(int pageNo, pdf_link **links);
    pdf_link *getLinkAtPosition(int pageNo, double x, double y);
    bool hasTocTree() const { 
        return _outline != NULL || _attachments != NULL; 
    }
    void ageStore();
    PdfTocItem *getTocTree();
    fz_matrix viewctm(int pageNo, float zoom, int rotate);

    int        findPageNo(fz_obj *dest);
    fz_obj   * getNamedDest(const char *name);
    char     * getPageLayoutName(void);
    bool       isDocumentDirectionR2L(void);
    TCHAR    * ExtractPageText(int pageNo, TCHAR *lineSep=_T(DOS_NEWLINE), fz_bbox **coords_out=NULL, RenderTarget target=Target_View) {
        return ExtractPageText(getPdfPage(pageNo), lineSep, coords_out, target);
    };
    fz_obj   * getPdfInfo(void) { return _info; };
    int        getPdfVersion(void) const { return _xref ? _xref->version : -1; }
    char     * getDecryptionKey(void) const { return _decryptionKey ? fz_strdup(_decryptionKey) : NULL; }
    fz_buffer* getStreamData(int num=0, int gen=0);
    bool       isImagePage(int pageNo);

    bool       _benchLoadPage(int pageNo) {
        return getPdfPage(pageNo) != NULL;
    }

protected:
    const TCHAR *_fileName;
    char *_decryptionKey;
    int _pageCount;

    // make sure to never ask for _pagesAccess in an _xrefAccess
    // protected critical section in order to avoid deadlocks
    CRITICAL_SECTION _xrefAccess;
    pdf_xref *      _xref;

    CRITICAL_SECTION _pagesAccess;
    pdf_page **     _pages;

    bool            finishLoading(void);
    pdf_page      * getPdfPage(int pageNo, bool failIfBusy=false);
    fz_matrix       viewctm(pdf_page *page, float zoom, int rotate);
    bool            renderPage(HDC hDC, pdf_page *page, RECT *screenRect,
                               fz_matrix *ctm=NULL, float zoom=0, int rotation=0,
                               fz_rect *pageRect=NULL, RenderTarget target=Target_View);
    TCHAR         * ExtractPageText(pdf_page *page, TCHAR *lineSep=_T(DOS_NEWLINE),
                                    fz_bbox **coords_out=NULL, RenderTarget target=Target_View,
                                    bool cacheRun=false);

    PdfPageRun    * _runCache[MAX_PAGE_RUN_CACHE];
    PdfPageRun    * getPageRun(pdf_page *page, bool tryOnly=false);
    fz_error        runPage(pdf_page *page, fz_device *dev, fz_matrix ctm,
                            RenderTarget target=Target_View, fz_rect bounds=fz_infiniterect,
                            bool cacheRun=true);
    void            dropPageRun(PdfPageRun *run, bool forceRemove=false);

    PdfTocItem    * buildTocTree(pdf_outline *entry);
    void            linkifyPageText(pdf_page *page);

    pdf_outline   * _outline;
    pdf_outline   * _attachments;
    fz_obj        * _info;
    fz_glyphcache * _drawcache;
};

static inline TCHAR *pdf_to_tstr(fz_obj *obj)
{
    WCHAR *ucs2 = (WCHAR *)pdf_toucs2(obj);
    TCHAR *tstr = wstr_to_tstr(ucs2);
    fz_free(ucs2);
    return tstr;
}

#endif
