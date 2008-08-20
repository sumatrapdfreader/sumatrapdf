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

/* Abstract class representing cached bitmap. Allows different implementations
   on different platforms. */
class RenderedBitmap {
public:
    virtual ~RenderedBitmap() {};
    virtual int dx() = 0;
    virtual int dy() = 0;
    virtual int rowSize() = 0;
    virtual unsigned char *data() = 0;

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

    virtual bool load(const char *fileName, WindowInfo *windowInfo, bool tryrepair) = 0;
    virtual int pageRotation(int pageNo) = 0;
    virtual SizeD pageSize(int pageNo) = 0;
    virtual RenderedBitmap *renderBitmap(int pageNo, double zoomReal, int rotation,
                         BOOL (*abortCheckCbkA)(void *data),
                         void *abortCheckCbkDataA) = 0;

    virtual bool printingAllowed() = 0;
    virtual int linkCount() = 0;
    virtual pdf_linkkind linkType(int pageNo, int linkNo) = 0;

    virtual bool hasTocTree() = 0;
    virtual PdfTocItem *getTocTree() = 0;

protected:
    const char *_fileName;
    int _pageCount;
    WindowInfo *_windowInfo;

};

class PdfEngineFitz : public  PdfEngine {
public:
    PdfEngineFitz();
    virtual ~PdfEngineFitz();
    virtual bool load(const char *fileName, WindowInfo* windowInfo, bool tryrepair);
    virtual int pageRotation(int pageNo);
    virtual SizeD pageSize(int pageNo);
    virtual RenderedBitmap *renderBitmap(int pageNo, double zoomReal, int rotation,
                         BOOL (*abortCheckCbkA)(void *data),
                         void *abortCheckCbkDataA);

    virtual bool printingAllowed();
    virtual int linkCount();
    virtual pdf_linkkind linkType(int pageNo, int linkNo);
    virtual bool hasTocTree() { return _outline != NULL; }
    virtual PdfTocItem *getTocTree();

    fz_matrix viewctm (pdf_page *page, float zoom, int rotate);

    pdf_page * getPdfPage(int pageNo);
    int        findPageNo(fz_obj *dest);
    fz_obj    *getNamedDest(const char *name);

private:
    HANDLE            _getPageSem;

    pdf_pagetree * pages() { return _pageTree; }

    void dropPdfPage(int pageNo);
    PdfTocItem *buildTocTree(pdf_outline *entry);

    pdf_xref *      _xref;
    pdf_outline *   _outline;
    pdf_pagetree *  _pageTree;
    pdf_page **     _pages;
    fz_renderer *   _rast;
};

#endif
