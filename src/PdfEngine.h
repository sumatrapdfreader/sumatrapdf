/* Copyright Krzysztof Kowalczyk 2006-2007
   License: GPLv2 */
#ifndef _PDF_ENGINE_H_
#define _PDF_ENGINE_H_

#include "geom_util.h"

#ifndef _FITZ_H_
#include <fitz.h>
#include <mupdf.h>
#endif

class WindowInfo;

extern const char* const LINK_ACTION_GOTO;
extern const char* const LINK_ACTION_GOTOR;
extern const char* const LINK_ACTION_LAUNCH;
extern const char* const LINK_ACTION_URI;
extern const char* const LINK_ACTION_NAMED;
extern const char* const LINK_ACTION_MOVIE;

/* For simplicity, all in one file. Would be cleaner if they were
   in separate files PdfEngineFitz.h and PdfEnginePoppler.h */

#define INVALID_PAGE_NO     -1
#define INVALID_ROTATION    -1
/* It seems that PDF documents are encoded assuming DPI of 72.0 */
#define PDF_FILE_DPI        72

class PdfTocItem {
public:
    wchar_t *title;
    void *link;

    PdfTocItem *child;
    PdfTocItem *next;

    PdfTocItem(wchar_t *title, void *link)
    {
        this->title = title;
        this->link = link;
        this->child = NULL;
        this->next = NULL;
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

void SplashColorsInit(void);

/* Abstract class representing cached bitmap. Allows different implementations
   on different platforms. */
class RenderedBitmap {
public:
    virtual ~RenderedBitmap() {};
    virtual int dx() = 0;
    virtual int dy() = 0;
    virtual int rowSize() = 0;
    virtual unsigned char *data() = 0;

    // TODO: this is for WINDOWS only
    virtual HBITMAP createDIBitmap(HDC) = 0;
    virtual void stretchDIBits(HDC, int, int, int, int) = 0;
};

class RenderedBitmapFitz : public RenderedBitmap {
public:
    RenderedBitmapFitz(fz_pixmap *);
    virtual ~RenderedBitmapFitz();

    virtual int dx() { return _bitmap->w; }
    virtual int dy() { return _bitmap->h; }
    virtual int rowSize();
    virtual unsigned char *data();

    virtual HBITMAP createDIBitmap(HDC);
    virtual void stretchDIBits(HDC, int, int, int, int);
protected:
    fz_pixmap *_bitmap;
};

class PdfEngine {
public:
    PdfEngine() : 
        _fileName(0)
        , _pageCount(INVALID_PAGE_NO) 
    { }

    virtual ~PdfEngine() { free((void*)_fileName); }

    const char *fileName(void) const { return _fileName; };

    void setFileName(const char *fileName) {
        assert(!_fileName);
        _fileName = (const char*)strdup(fileName);
    }

    bool validPageNo(int pageNo) const {
        if ((pageNo >= 1) && (pageNo <= pageCount()))
            return true;
        return false;
    }

    int pageCount(void) const { return _pageCount; }

    virtual bool load(const char *fileName, WindowInfo *windowInfo) = 0;
    virtual int pageRotation(int pageNo) = 0;
    virtual SizeD pageSize(int pageNo) = 0;
    virtual RenderedBitmap *renderBitmap(int pageNo, double zoomReal, int rotation,
                         BOOL (*abortCheckCbkA)(void *data),
                         void *abortCheckCbkDataA) = 0;

    virtual bool printingAllowed() = 0;
    virtual int linkCount(int pageNo) = 0;
    virtual const char* linkType(int pageNo, int linkNo) = 0;

    virtual bool hasTocTree() = 0;
    virtual PdfTocItem *getTocTree() = 0;

    bool enableEngineSwitch() { return _enableEngineSwitch; }
    void setEnableEngineSwitch(bool enable) { _enableEngineSwitch = enable; }

protected:
    const char *_fileName;
    int _pageCount;
    WindowInfo *_windowInfo;

    /* true by default. If true, if fitz engine fails to load PDF file, it'll
       transparently switch to using poppler engine */
    bool _enableEngineSwitch;
};

class PdfEngineFitz : public  PdfEngine {
public:
    PdfEngineFitz();
    virtual ~PdfEngineFitz();
    virtual bool load(const char *fileName, WindowInfo* windowInfo);
    virtual int pageRotation(int pageNo);
    virtual SizeD pageSize(int pageNo);
    virtual RenderedBitmap *renderBitmap(int pageNo, double zoomReal, int rotation,
                         BOOL (*abortCheckCbkA)(void *data),
                         void *abortCheckCbkDataA);

    virtual bool printingAllowed();
    virtual int linkCount(int pageNo);
    virtual const char* linkType(int pageNo, int linkNo);
    virtual bool hasTocTree() { return _outline != NULL; }
    virtual PdfTocItem *getTocTree();

    fz_matrix viewctm (pdf_page *page, float zoom, int rotate);

    pdf_page * getPdfPage(int pageNo);
    int        findPageNo(fz_obj *dest);

private:
    HANDLE            _getPageSem;

    pdf_xref * xref() { return _xref; }
    pdf_pagetree * pages() { return _pageTree; }

    void dropPdfPage(int pageNo);
    PdfTocItem *buildTocTree(pdf_outline *entry);

    pdf_xref *      _xref;
    pdf_outline *   _outline;
    pdf_pagetree *  _pageTree;
    pdf_page **     _pages;
#ifdef FITZ_HEAD
    fz_graphics *   _rast;
#else
    fz_renderer *   _rast;
#endif
};

#endif
