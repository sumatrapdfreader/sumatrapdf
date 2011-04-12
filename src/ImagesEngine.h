/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef ImagesEngine_H
#define ImagesEngine_H

#include "BaseEngine.h"
#include "Vec.h"

// TODO: support a directory of files

class ImagesPage {
public:
    // TODO: do I need this?
    const TCHAR *       fileName;

    HGLOBAL             bmpData;
    Gdiplus::Bitmap *   bmp;
    int                 width, height;

    ImagesPage(const TCHAR *fileName, HGLOBAL bmpData, Gdiplus::Bitmap *bmp) :
        bmpData(bmpData), bmp(bmp)
    {
        this->fileName = Str::Dup(fileName);
        width = bmp->GetWidth();
        height = bmp->GetHeight();
    }

    ~ImagesPage() {
        free((void*)fileName);
        delete bmp;
        GlobalFree(bmpData);
    }

    SizeI size() { return SizeI(width, height); }
};

class ImagesEngine : public BaseEngine {
public:
    ImagesEngine();
    virtual ~ImagesEngine();
    virtual ImagesEngine *Clone() {
        return CreateFromFileName(fileName);
    }

    virtual const TCHAR *FileName() const { return fileName; };
    virtual int PageCount() const { return pages.Count(); }

    virtual RectD PageMediabox(int pageNo) {
        assert(1 <= pageNo && pageNo <= PageCount());
        ImagesPage *page = pages[pageNo - 1];
        return RectD(0, 0, page->width, page->height);
    }

    virtual RenderedBitmap *RenderBitmap(int pageNo, float zoom, int rotation,
                         RectD *pageRect=NULL, /* if NULL: defaults to the page's mediabox */
                         RenderTarget target=Target_View, bool useGdi=false);
    virtual bool RenderPage(HDC hDC, int pageNo, RectI screenRect,
                         float zoom=0, int rotation=0,
                         RectD *pageRect=NULL, RenderTarget target=Target_View);

    virtual PointD Transform(PointD pt, int pageNo, float zoom, int rotate, bool inverse=false);
    virtual RectD Transform(RectD rect, int pageNo, float zoom, int rotate, bool inverse=false);

    virtual COLORREF DefaultBackgroundColor() { return COL_BLACK; }

    virtual unsigned char *GetFileData(size_t *cbCount);
    virtual TCHAR * ExtractPageText(int pageNo, TCHAR *lineSep, RectI **coords_out=NULL,
                                    RenderTarget target=Target_View) { return NULL; }
    virtual bool IsImagePage(int pageNo) { return true; }

    // there's no text...
    virtual bool IsCopyingTextAllowed() { return false; }

    // we currently don't load pages lazily, so there's nothing to do here
    virtual bool BenchLoadPage(int pageNo) { return true; }

    virtual bool SupportsPermissions() const { return false; };

    bool LoadCbzFile(const TCHAR *fileName);
    bool LoadCbrFile(const TCHAR *fileName);
    bool LoadSingleFile(const TCHAR *fileName);

protected:
    const TCHAR *fileName;
    Vec<ImagesPage *> pages;

    void GetTransform(Gdiplus::Matrix& m, int pageNo, float zoom, int rotate);

public:
    static bool IsSupportedFile(const TCHAR *fileName);
    static ImagesEngine *CreateFromFileName(const TCHAR *fileName);
    static ImagesEngine *CreateFromCbzFile(const TCHAR *fileName);
    static ImagesEngine *CreateFromCbrFile(const TCHAR *fileName);
    static ImagesEngine *CreateFromSingleFile(const TCHAR *fileName);
};

bool IsImageFile(const WCHAR *fileName);
bool IsImageFile(const char *fileName);

#endif
