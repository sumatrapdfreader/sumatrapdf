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
#include <muxps.h>

#ifdef _MSC_VER
__pragma(warning(pop))
#endif
}

#include "BaseUtil.h"
#include "GeomUtil.h"
#include "StrUtil.h"
#include "BaseEngine.h"

// number of page content trees to cache for quicker rendering
#define MAX_PAGE_RUN_CACHE  8

typedef struct {
    pdf_page *page;
    fz_display_list *list;
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

class PasswordUI {
public:
    virtual TCHAR * GetPassword(const TCHAR *fileName, unsigned char *fileDigest,
                                unsigned char decryptionKeyOut[32], bool *saveKey) = 0;
};

class PdfEngine : public BaseEngine {
public:
    PdfEngine();
    virtual ~PdfEngine();
    virtual PdfEngine *Clone();

    virtual const TCHAR *FileName() const { return _fileName; };
    virtual int PageCount() const {
        return _xref ? pdf_count_pages(_xref) : 0;
    }

    virtual int PageRotation(int pageNo);
    virtual RectD PageMediabox(int pageNo);
    virtual RectI PageContentBox(int pageNo, RenderTarget target=Target_View);

    virtual RenderedBitmap *RenderBitmap(int pageNo, float zoom, int rotation,
                         RectD *pageRect=NULL, /* if NULL: defaults to the page's mediabox */
                         RenderTarget target=Target_View, bool useGdi=false);
    virtual bool RenderPage(HDC hDC, int pageNo, RectI screenRect,
                         float zoom=0, int rotation=0,
                         RectD *pageRect=NULL, RenderTarget target=Target_View) {
        return renderPage(hDC, getPdfPage(pageNo), screenRect, NULL, zoom, rotation, pageRect, target);
    }

    virtual PointD Transform(PointD pt, int pageNo, float zoom, int rotate, bool inverse=false);
    virtual RectD Transform(RectD rect, int pageNo, float zoom, int rotate, bool inverse=false);

    virtual unsigned char *GetFileData(size_t *cbCount);
    virtual TCHAR * ExtractPageText(int pageNo, TCHAR *lineSep=DOS_NEWLINE, RectI **coords_out=NULL, RenderTarget target=Target_View);
    virtual bool IsImagePage(int pageNo);
    virtual PageLayoutType PreferredLayout();

    virtual bool IsPrintingAllowed() { return hasPermission(PDF_PERM_PRINT); }
    virtual bool IsCopyingTextAllowed() { return hasPermission(PDF_PERM_COPY); }

    virtual float GetFileDPI() const { return 72.0f; }

    virtual bool BenchLoadPage(int pageNo) { return getPdfPage(pageNo) != NULL; }

    // TODO: move any of these into BaseEngine?
    int getPdfLinks(int pageNo, pdf_link **links);
    pdf_link *getLinkAtPosition(int pageNo, float x, float y);
    pdf_annot *getCommentAtPosition(int pageNo, float x, float y);
    TCHAR *getLinkPath(pdf_link *link);
    bool hasTocTree() const { 
        return _outline != NULL || _attachments != NULL; 
    }
    void ageStore();
    PdfTocItem *getTocTree();

    int        findPageNo(fz_obj *dest);
    fz_obj   * getNamedDest(const char *name);
    TCHAR    * getPdfInfo(char *key) const;
    int        getPdfVersion() const;
    char     * getDecryptionKey() const { return _decryptionKey ? Str::Dup(_decryptionKey) : NULL; }
    fz_buffer* getStreamData(int num, int gen);

protected:
    const TCHAR *_fileName;
    char *_decryptionKey;

    // make sure to never ask for _pagesAccess in an _xrefAccess
    // protected critical section in order to avoid deadlocks
    CRITICAL_SECTION _xrefAccess;
    pdf_xref *      _xref;

    CRITICAL_SECTION _pagesAccess;
    pdf_page **     _pages;

    bool            load(const TCHAR *fileName, PasswordUI *pwdUI=NULL);
    bool            load(fz_stream *stm, PasswordUI *pwdUI=NULL);
    bool            load_from_stream(fz_stream *stm, PasswordUI *pwdUI=NULL);
    bool            finishLoading();
    pdf_page      * getPdfPage(int pageNo, bool failIfBusy=false);
    fz_matrix       viewctm(int pageNo, float zoom, int rotate);
    fz_matrix       viewctm(pdf_page *page, float zoom, int rotate);
    bool            renderPage(HDC hDC, pdf_page *page, RectI screenRect,
                               fz_matrix *ctm=NULL, float zoom=0, int rotation=0,
                               RectD *pageRect=NULL, RenderTarget target=Target_View);
    TCHAR         * ExtractPageText(pdf_page *page, TCHAR *lineSep=DOS_NEWLINE,
                                    RectI **coords_out=NULL, RenderTarget target=Target_View,
                                    bool cacheRun=false);

    PdfPageRun    * _runCache[MAX_PAGE_RUN_CACHE];
    PdfPageRun    * getPageRun(pdf_page *page, bool tryOnly=false);
    fz_error        runPage(pdf_page *page, fz_device *dev, fz_matrix ctm,
                            RenderTarget target=Target_View,
                            fz_bbox clipbox=fz_infinite_bbox, bool cacheRun=true);
    void            dropPageRun(PdfPageRun *run, bool forceRemove=false);

    PdfTocItem    * buildTocTree(pdf_outline *entry, int *idCounter);
    void            linkifyPageText(pdf_page *page);
    bool            hasPermission(int permission);

    pdf_outline   * _outline;
    pdf_outline   * _attachments;
    fz_obj        * _info;
    fz_glyph_cache* _drawcache;

public:
    static PdfEngine *CreateFromFileName(const TCHAR *fileName, PasswordUI *pwdUI=NULL);
    static PdfEngine *CreateFromStream(fz_stream *stm, PasswordUI *pwdUI=NULL);
};

typedef struct {
    xps_page *page;
    fz_display_list *list;
    int refs;
} XpsPageRun;

class XpsEngine : public BaseEngine {
public:
    XpsEngine();
    virtual ~XpsEngine();
    virtual XpsEngine *Clone() {
        return CreateFromFileName(_fileName);
    }

    virtual const TCHAR *FileName() const { return _fileName; };
    virtual int PageCount() const {
        return _ctx ? xps_count_pages(_ctx) : 0;
    }

    virtual RectD PageMediabox(int pageNo);
    virtual RectI PageContentBox(int pageNo, RenderTarget target=Target_View);

    virtual RenderedBitmap *RenderBitmap(int pageNo, float zoom, int rotation,
                         RectD *pageRect=NULL, /* if NULL: defaults to the page's mediabox */
                         RenderTarget target=Target_View, bool useGdi=false);
    virtual bool RenderPage(HDC hDC, int pageNo, RectI screenRect,
                         float zoom=0, int rotation=0,
                         RectD *pageRect=NULL, RenderTarget target=Target_View) {
        return renderPage(hDC, getXpsPage(pageNo), screenRect, NULL, zoom, rotation, pageRect);
    }

    virtual PointD Transform(PointD pt, int pageNo, float zoom, int rotate, bool inverse=false);
    virtual RectD Transform(RectD rect, int pageNo, float zoom, int rotate, bool inverse=false);

    virtual unsigned char *GetFileData(size_t *cbCount);
    virtual TCHAR * ExtractPageText(int pageNo, TCHAR *lineSep=DOS_NEWLINE, RectI **coords_out=NULL, RenderTarget target=Target_View);
    virtual bool IsImagePage(int pageNo) { return false; }

    virtual float GetFileDPI() const { return 96.0f; }

    virtual bool BenchLoadPage(int pageNo) { return getXpsPage(pageNo) != NULL; }

protected:
    const TCHAR *_fileName;

    // make sure to never ask for _pagesAccess in an _ctxAccess
    // protected critical section in order to avoid deadlocks
    CRITICAL_SECTION _ctxAccess;
    xps_context *    _ctx;

    CRITICAL_SECTION _pagesAccess;
    xps_page **     _pages;

    bool            load(const TCHAR *fileName);
    bool            load(fz_stream *stm);
    xps_page      * getXpsPage(int pageNo, bool failIfBusy=false);
    fz_matrix       viewctm(int pageNo, float zoom, int rotate) {
        return viewctm(getXpsPage(pageNo), zoom, rotate);
    }
    fz_matrix       viewctm(xps_page *page, float zoom, int rotate);
    bool            renderPage(HDC hDC, xps_page *page, RectI screenRect,
                               fz_matrix *ctm=NULL, float zoom=0, int rotation=0,
                               RectD *pageRect=NULL);
    TCHAR         * ExtractPageText(xps_page *page, TCHAR *lineSep=DOS_NEWLINE,
                                    RectI **coords_out=NULL, bool cacheRun=false);

    XpsPageRun    * _runCache[MAX_PAGE_RUN_CACHE];
    XpsPageRun    * getPageRun(xps_page *page, bool tryOnly=false);
    void            runPage(xps_page *page, fz_device *dev, fz_matrix ctm,
                            fz_bbox clipbox=fz_infinite_bbox, bool cacheRun=true);
    void            dropPageRun(XpsPageRun *run, bool forceRemove=false);

    fz_glyph_cache* _drawcache;

public:
    static XpsEngine *CreateFromFileName(const TCHAR *fileName);
    static XpsEngine *CreateFromStream(fz_stream *stm);
};

// TODO: move inside PdfEngine.cpp once we've gotten rid of fz_* outside

inline RectD fz_rect_to_RectD(fz_rect rect)
{
    return RectD::FromXY(rect.x0, rect.y0, rect.x1, rect.y1);
}

inline fz_rect fz_RectD_to_rect(RectD rect)
{
    fz_rect result = { (float)rect.x, (float)rect.y, (float)(rect.x + rect.dx), (float)(rect.y + rect.dy) };
    return result;
}

#endif
