/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef ImagesEngine_H
#define ImagesEngine_H

#include "BaseEngine.h"
#include "Vec.h"

// TODO: support a directory of files

class ImagesPage {
public:
    const TCHAR *       fileName; // for sorting image files
    Gdiplus::Bitmap *   bmp;

    ImagesPage(const TCHAR *fileName, Gdiplus::Bitmap *bmp) : bmp(bmp) {
        this->fileName = Str::Dup(fileName);
    }

    ~ImagesPage() {
        free((void*)fileName);
        delete bmp;
    }

    SizeI size() { return SizeI(bmp->GetWidth(), bmp->GetHeight()); }
};

class ImagesEngine : public BaseEngine {
public:
    ImagesEngine();
    virtual ~ImagesEngine();

    virtual const TCHAR *FileName() const { return fileName; };
    virtual int PageCount() const { return pages.Count(); }

    virtual RectD PageMediabox(int pageNo) {
        assert(1 <= pageNo && pageNo <= PageCount());
        ImagesPage *page = pages[pageNo - 1];
        return RectD(PointD(), page->size().Convert<double>());
    }

    virtual RenderedBitmap *RenderBitmap(int pageNo, float zoom, int rotation,
                         RectD *pageRect=NULL, /* if NULL: defaults to the page's mediabox */
                         RenderTarget target=Target_View, bool useGdi=false);
    virtual bool RenderPage(HDC hDC, int pageNo, RectI screenRect,
                         float zoom=0, int rotation=0,
                         RectD *pageRect=NULL, RenderTarget target=Target_View);

    virtual PointD Transform(PointD pt, int pageNo, float zoom, int rotate, bool inverse=false);
    virtual RectD Transform(RectD rect, int pageNo, float zoom, int rotate, bool inverse=false);

    virtual unsigned char *GetFileData(size_t *cbCount);
    virtual bool HasTextContent() { return false; }
    virtual TCHAR * ExtractPageText(int pageNo, TCHAR *lineSep, RectI **coords_out=NULL,
                                    RenderTarget target=Target_View) { return NULL; }
    virtual bool IsImagePage(int pageNo) { return true; }
    virtual PageLayoutType PreferredLayout() { return Layout_NonContinuous; }

    virtual const TCHAR *GetDefaultFileExt() const { return fileExt; }

    // we currently don't load pages lazily, so there's nothing to do here
    virtual bool BenchLoadPage(int pageNo) { return true; }

protected:
    const TCHAR *fileName;
    const TCHAR *fileExt;
    Vec<ImagesPage *> pages;

    void GetTransform(Gdiplus::Matrix& m, int pageNo, float zoom, int rotate);

    static ImagesPage *LoadImage(const TCHAR *fileName);
};

class ImageEngine : public ImagesEngine {
public:
    virtual ImageEngine *Clone() {
        return CreateFromFileName(fileName);
    }

protected:
    bool LoadSingleFile(const TCHAR *fileName);

public:
    static bool IsSupportedFile(const TCHAR *fileName) {
        return Str::EndsWithI(fileName, _T(".png"))  ||
               Str::EndsWithI(fileName, _T(".jpg"))  ||
               Str::EndsWithI(fileName, _T(".jpeg")) ||
               Str::EndsWithI(fileName, _T(".gif"))  ||
               Str::EndsWithI(fileName, _T(".bmp"));
    }
    static ImageEngine *CreateFromFileName(const TCHAR *fileName);
};

class CbxEngine : public ImagesEngine {
public:
    virtual CbxEngine *Clone() {
        return CreateFromFileName(fileName);
    }

protected:
    bool LoadCbzFile(const TCHAR *fileName);
    bool LoadCbrFile(const TCHAR *fileName);

public:
    static bool IsSupportedFile(const TCHAR *fileName) {
        return Str::EndsWithI(fileName, _T(".cbz")) ||
               Str::EndsWithI(fileName, _T(".cbr"));
    }
    static CbxEngine *CreateFromFileName(const TCHAR *fileName);
};

#endif
