/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef PdfEngine_h
#define PdfEngine_h

extern "C" {
#ifdef _MSC_VER
__pragma(warning(push))
#endif

#include <fitz.h>
#include <mupdf.h>

#ifdef _MSC_VER
__pragma(warning(pop))
#endif
}

#include "BaseUtil.h"
#include "GeomUtil.h"
#include "StrUtil.h"

#define INVALID_PAGE_NO     -1
#define INVALID_ROTATION    -1
/* one PDF user space unit equals 1/72 inch */
#define PDF_FILE_DPI        72.0f
// number of page content trees to cache for quicker rendering
#define MAX_PAGE_RUN_CACHE  8
#define DOS_NEWLINE _T("\r\n")

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
    int id;

    PdfTocItem *child;
    PdfTocItem *next;

    PdfTocItem(TCHAR *title, pdf_link *link) :
        title(title), link(link), open(true), pageNo(0), id(0), child(NULL), next(NULL) { }

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

class PasswordUI {
public:
    virtual TCHAR * GetPassword(const TCHAR *fileName, unsigned char *fileDigest,
                                unsigned char decryptionKeyOut[32], bool *saveKey) = 0;
};

class BaseEngine {
public:
    virtual ~BaseEngine() { }
    virtual BaseEngine *clone() = 0;

    virtual const TCHAR *fileName() const = 0;
    virtual int pageCount() const = 0;

    bool validPageNo(int pageNo) const {
        return 1 <= pageNo && pageNo <= pageCount();
    }

    virtual fz_rect pageMediabox(int pageNo) = 0;
    virtual fz_matrix viewctm(int pageNo, float zoom, int rotate) = 0;

    virtual TCHAR * ExtractPageText(int pageNo, TCHAR *lineSep=DOS_NEWLINE, RectI **coords_out=NULL, RenderTarget target=Target_View) = 0;
};

class PdfEngine : public BaseEngine {
public:
    PdfEngine();
    virtual ~PdfEngine();
    virtual PdfEngine *clone();

    virtual const TCHAR *fileName() const { return _fileName; };
    virtual int pageCount() const { return _pageCount; }

    int pageRotation(int pageNo);
    SizeD pageSize(int pageNo);
    virtual fz_rect pageMediabox(int pageNo);
    fz_bbox pageContentBox(int pageNo, RenderTarget target=Target_View);
    RenderedBitmap *renderBitmap(int pageNo, float zoom, int rotation,
                         fz_rect *pageRect=NULL, /* if NULL: defaults to the page's mediabox */
                         RenderTarget target=Target_View, bool useGdi=false);
    bool renderPage(HDC hDC, int pageNo, RectI *screenRect,
                         fz_matrix *ctm=NULL, float zoom=0, int rotation=0,
                         fz_rect *pageRect=NULL, RenderTarget target=Target_View) {
        return renderPage(hDC, getPdfPage(pageNo), screenRect, ctm, zoom, rotation, pageRect, target);
    }

    bool hasPermission(int permission);
    int getPdfLinks(int pageNo, pdf_link **links);
    pdf_link *getLinkAtPosition(int pageNo, float x, float y);
    pdf_annot *getCommentAtPosition(int pageNo, float x, float y);
    bool hasTocTree() const { 
        return _outline != NULL || _attachments != NULL; 
    }
    void ageStore();
    PdfTocItem *getTocTree();
    virtual fz_matrix viewctm(int pageNo, float zoom, int rotate);

    int        findPageNo(fz_obj *dest);
    fz_obj   * getNamedDest(const char *name);
    char     * getPageLayoutName();
    bool       isDocumentDirectionR2L();
    virtual TCHAR * ExtractPageText(int pageNo, TCHAR *lineSep=DOS_NEWLINE, RectI **coords_out=NULL, RenderTarget target=Target_View);
    TCHAR    * getPdfInfo(char *key) const;
    int        getPdfVersion() const;
    char     * getDecryptionKey() const { return _decryptionKey ? Str::Dup(_decryptionKey) : NULL; }
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

    bool            load(const TCHAR *fileName, PasswordUI *pwdUI=NULL);
    bool            load(fz_stream *stm, TCHAR *password=NULL);
    bool            finishLoading();
    pdf_page      * getPdfPage(int pageNo, bool failIfBusy=false);
    fz_matrix       viewctm(pdf_page *page, float zoom, int rotate);
    bool            renderPage(HDC hDC, pdf_page *page, RectI *screenRect,
                               fz_matrix *ctm=NULL, float zoom=0, int rotation=0,
                               fz_rect *pageRect=NULL, RenderTarget target=Target_View);
    TCHAR         * ExtractPageText(pdf_page *page, TCHAR *lineSep=DOS_NEWLINE,
                                    fz_bbox **coords_out=NULL, RenderTarget target=Target_View,
                                    bool cacheRun=false);

    PdfPageRun    * _runCache[MAX_PAGE_RUN_CACHE];
    PdfPageRun    * getPageRun(pdf_page *page, bool tryOnly=false);
    fz_error        runPage(pdf_page *page, fz_device *dev, fz_matrix ctm,
                            RenderTarget target=Target_View, fz_rect bounds=fz_infiniterect,
                            bool cacheRun=true);
    void            dropPageRun(PdfPageRun *run, bool forceRemove=false);

    PdfTocItem    * buildTocTree(pdf_outline *entry, int *idCounter);
    void            linkifyPageText(pdf_page *page);

    pdf_outline   * _outline;
    pdf_outline   * _attachments;
    fz_obj        * _info;
    fz_glyphcache * _drawcache;

public:
    static PdfEngine *CreateFromFileName(const TCHAR *fileName, PasswordUI *pwdUI=NULL);
    static PdfEngine *CreateFromStream(fz_stream *stm, TCHAR *password=NULL);
};

namespace Str {
    namespace Conv {

inline TCHAR *FromPdf(fz_obj *obj)
{
    WCHAR *ucs2 = (WCHAR *)pdf_toucs2(obj);
    TCHAR *tstr = FromWStr(ucs2);
    fz_free(ucs2);
    return tstr;
}

// Caller needs to fz_free the result
inline char *ToPDF(TCHAR *tstr)
{
    ScopedMem<WCHAR> wstr(ToWStr(tstr));
    return pdf_fromucs2((unsigned short *)wstr.Get());
}

    }
}

static inline fz_rect fz_bboxtorect(fz_bbox bbox)
{
    fz_rect result = { (float)bbox.x0, (float)bbox.y0, (float)bbox.x1, (float)bbox.y1 };
    return result;
}

#endif
