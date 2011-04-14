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
#include "BaseEngine.h"
#include "Vec.h"

// number of page content trees to cache for quicker rendering
#define MAX_PAGE_RUN_CACHE  8

class PageDestination {
public:
    // TODO: generalize a destination
    virtual fz_obj *dest() const = 0;
};

class PageElement {
public:
    virtual ~PageElement() { }
    virtual RectD GetRect() const = 0;
    virtual TCHAR *GetValue() const = 0;
    virtual PageDestination *GetLink() = 0;
};

typedef struct {
    pdf_page *page;
    fz_display_list *list;
    int refs;
} PdfPageRun;

class PdfLink : public PageElement, public PageDestination {
    pdf_link *link;

public:
    PdfLink() : link(NULL) { }
    PdfLink(pdf_link *link) : link(link) { }

    virtual RectD GetRect() const;
    virtual TCHAR *GetValue() const;
    virtual PageDestination *GetLink() { return this; }

    pdf_link_kind kind() const { return link ? link->kind : (pdf_link_kind)-1; }
    bool isEmbeddedFile() const { return link && PDF_LINK_LAUNCH == link->kind && fz_dict_gets(link->dest, "EF"); }

    // TODO: remove this when it's no longer needed
    virtual fz_obj *dest() const { return link ? link->dest : NULL; }
};

class PdfComment : public PageElement {
    pdf_annot *annot;

public:
    PdfComment(pdf_annot *annot) : annot(annot) { }

    virtual RectD GetRect() const;
    virtual TCHAR *GetValue() const;
    virtual PageDestination *GetLink() { return NULL; }
};

class PdfTocItem {
public:
    TCHAR *title;
    PdfLink link;
    bool open;
    int pageNo;
    int id;

    PdfTocItem *child;
    PdfTocItem *next;

    PdfTocItem(TCHAR *title, PdfLink link) :
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
    virtual bool HasTextContent() { return true; }
    virtual TCHAR * ExtractPageText(int pageNo, TCHAR *lineSep, RectI **coords_out=NULL,
                                    RenderTarget target=Target_View);
    virtual bool IsImagePage(int pageNo);
    virtual PageLayoutType PreferredLayout();
    virtual TCHAR *GetProperty(char *name);

    virtual bool IsPrintingAllowed() { return hasPermission(PDF_PERM_PRINT); }
    virtual bool IsCopyingTextAllowed() { return hasPermission(PDF_PERM_COPY); }

    virtual float GetFileDPI() const { return 72.0f; }
    virtual const TCHAR *GetDefaultFileExt() const { return _T(".pdf"); }

    virtual bool BenchLoadPage(int pageNo) { return getPdfPage(pageNo) != NULL; }

    // TODO: move any of the following into BaseEngine?
    Vec<PageElement *> *GetElements(int pageNo);
    PageElement *GetElementAtPos(int pageNo, PointD pt);
    bool hasTocTree() const { 
        return _outline != NULL || _attachments != NULL; 
    }
    void ageStore();
    PdfTocItem *getTocTree();

    int        findPageNo(fz_obj *dest);
    fz_obj   * getNamedDest(const char *name);
    char     * getDecryptionKey() const;
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
    bool            load(IStream *stream, PasswordUI *pwdUI=NULL);
    bool            load(fz_stream *stm, PasswordUI *pwdUI=NULL);
    bool            load_from_stream(fz_stream *stm, PasswordUI *pwdUI=NULL);
    bool            finishLoading();
    pdf_page      * getPdfPage(int pageNo, bool failIfBusy=false);
    fz_matrix       viewctm(int pageNo, float zoom, int rotate);
    fz_matrix       viewctm(pdf_page *page, float zoom, int rotate);
    bool            renderPage(HDC hDC, pdf_page *page, RectI screenRect,
                               fz_matrix *ctm=NULL, float zoom=0, int rotation=0,
                               RectD *pageRect=NULL, RenderTarget target=Target_View);
    TCHAR         * ExtractPageText(pdf_page *page, TCHAR *lineSep, RectI **coords_out=NULL,
                                    RenderTarget target=Target_View, bool cacheRun=false);

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

    static PdfEngine *CreateFromStream(fz_stream *stm, PasswordUI *pwdUI=NULL);
public:
    static PdfEngine *CreateFromFileName(const TCHAR *fileName, PasswordUI *pwdUI=NULL);
    static PdfEngine *CreateFromStream(IStream *stream, PasswordUI *pwdUI=NULL);
};

class XpsEngine : public BaseEngine {
protected:
    virtual bool load(const TCHAR *fileName) = 0;
    virtual bool load(IStream *stream) = 0;
public:
    static XpsEngine *CreateFromFileName(const TCHAR *fileName);
    static XpsEngine *CreateFromStream(IStream *stream);
};

#endif
