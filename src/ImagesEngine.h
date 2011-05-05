/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef ImagesEngine_H
#define ImagesEngine_H

#include "BaseEngine.h"
#include "Vec.h"

class ImagesEngine : public BaseEngine {
public:
    ImagesEngine();
    virtual ~ImagesEngine();

    virtual const TCHAR *FileName() const { return fileName; };
    virtual int PageCount() const { return pages.Count(); }

    virtual RectD PageMediabox(int pageNo) {
        assert(1 <= pageNo && pageNo <= PageCount());
        return RectD(0, 0, pages[pageNo-1]->GetWidth(), pages[pageNo-1]->GetHeight());
    }

    virtual RenderedBitmap *RenderBitmap(int pageNo, float zoom, int rotation,
                         RectD *pageRect=NULL, /* if NULL: defaults to the page's mediabox */
                         RenderTarget target=Target_View);
    virtual bool RenderPage(HDC hDC, RectI screenRect, int pageNo, float zoom, int rotation,
                         RectD *pageRect=NULL, RenderTarget target=Target_View);

    virtual PointD Transform(PointD pt, int pageNo, float zoom, int rotation, bool inverse=false);
    virtual RectD Transform(RectD rect, int pageNo, float zoom, int rotation, bool inverse=false);

    virtual unsigned char *GetFileData(size_t *cbCount);
    virtual bool HasTextContent() { return false; }
    virtual TCHAR * ExtractPageText(int pageNo, TCHAR *lineSep, RectI **coords_out=NULL,
                                    RenderTarget target=Target_View) { return NULL; }
    virtual bool IsImagePage(int pageNo) { return true; }
    virtual PageLayoutType PreferredLayout() { return Layout_NonContinuous; }

    virtual const TCHAR *GetDefaultFileExt() const { return fileExt; }

    virtual bool BenchLoadPage(int pageNo) { return LoadImage(pageNo) != NULL; }

protected:
    const TCHAR *fileName;
    const TCHAR *fileExt;

    Vec<Gdiplus::Bitmap *> pages;

    void GetTransform(Gdiplus::Matrix& m, int pageNo, float zoom, int rotation);

    // override for lazily loading images
    virtual Gdiplus::Bitmap *LoadImage(int pageNo) {
        assert(1 <= pageNo && pageNo <= PageCount());
        return pages[pageNo - 1];
    }
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
        return str::EndsWithI(fileName, _T(".png"))  ||
               str::EndsWithI(fileName, _T(".jpg"))  ||
               str::EndsWithI(fileName, _T(".jpeg")) ||
               str::EndsWithI(fileName, _T(".gif"))  ||
               str::EndsWithI(fileName, _T(".tif"))  ||
               str::EndsWithI(fileName, _T(".tiff")) ||
               str::EndsWithI(fileName, _T(".bmp"));
    }
    static ImageEngine *CreateFromFileName(const TCHAR *fileName);
};

class ImageDirEngine : public ImagesEngine {
public:
    virtual ImageDirEngine *Clone() {
        return CreateFromFileName(fileName);
    }
    virtual RectD PageMediabox(int pageNo);

    virtual unsigned char *GetFileData(size_t *cbCount) { return NULL; }

protected:
    bool LoadImageDir(const TCHAR *dirName);

    virtual Gdiplus::Bitmap *LoadImage(int pageNo);

    Vec<RectD> mediaboxes;
    StrVec pageFileNames;

public:
    static bool IsSupportedFile(const TCHAR *fileName);
    static ImageDirEngine *CreateFromFileName(const TCHAR *fileName);
};

class CbxEngine : public ImagesEngine {
public:
    CbxEngine();
    virtual ~CbxEngine();

    virtual CbxEngine *Clone() {
        return CreateFromFileName(fileName);
    }
    virtual RectD PageMediabox(int pageNo);

protected:
    bool LoadCbzFile(const TCHAR *fileName);
    bool LoadCbrFile(const TCHAR *fileName);

    virtual Gdiplus::Bitmap *LoadImage(int pageNo);
    char *GetImageData(int pageNo, size_t& len);

    Vec<RectD> mediaboxes;

    // used for lazily loading page images (only supported for .cbz files)
    CRITICAL_SECTION fileAccess;
    StrVec pageFileNames;
    void *libData;

public:
    static bool IsSupportedFile(const TCHAR *fileName) {
        return str::EndsWithI(fileName, _T(".cbz")) ||
               str::EndsWithI(fileName, _T(".cbr"));
    }
    static CbxEngine *CreateFromFileName(const TCHAR *fileName);
};

RenderedBitmap *LoadRenderedBitmap(const TCHAR *filePath);
bool SaveRenderedBitmap(RenderedBitmap *bmp, const TCHAR *filePath);

#endif
