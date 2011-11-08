/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "ImagesEngine.h"
#include "StrUtil.h"
#include "FileUtil.h"
#include "WinUtil.h"
#include "Vec.h"
#include "Scopes.h"

// mini(un)zip
#include <ioapi.h>
#include <iowin32.h>
#include <unzip.h>

#include "../ext/unrar/dll.hpp"

extern "C" {
// needed because we compile bzip2 with #define BZ_NO_STDIO
void bz_internal_error(int errcode) { /* do nothing */ }
}

// disable warning C4250 which is wrongly issued due to a compiler bug; cf.
// http://connect.microsoft.com/VisualStudio/feedback/details/101259/disable-warning-c4250-class1-inherits-class2-member-via-dominance-when-weak-member-is-a-pure-virtual-function
#pragma warning( disable: 4250 ) /* 'class1' : inherits 'class2::member' via dominance */

using namespace Gdiplus;

///// Helper methods for handling image files of the most common types /////

// cf. http://stackoverflow.com/questions/4598872/creating-hbitmap-from-memory-buffer/4616394#4616394
Bitmap *BitmapFromData(void *data, size_t len)
{
    ScopedComPtr<IStream> stream(CreateStreamFromData(data, len));
    if (!stream)
        return NULL;

    Bitmap *bmp = Bitmap::FromStream(stream);
    if (bmp && bmp->GetLastStatus() != Ok) {
        delete bmp;
        bmp = NULL;
    }

    return bmp;
}

const TCHAR *FileExtFromData(char *data, size_t len)
{
    char header[9] = { 0 };
    memcpy(header, data, min(len, sizeof(header)));

    if (str::StartsWith(header, "BM"))
        return _T(".bmp");
    if (str::StartsWith(header, "\x89PNG\x0D\x0A\x1A\x0A"))
        return _T(".png");
    if (str::StartsWith(header, "\xFF\xD8"))
        return _T(".jpg");
    if (str::StartsWith(header, "GIF87a") || str::StartsWith(header, "GIF89a"))
        return _T(".gif");
    if (!memcmp(header, "MM\x00\x2A", 4) || !memcmp(header, "II\x2A\x00", 4))
        return _T(".tif");
    return NULL;
}

// for converting between big- and little-endian values
#define SWAPWORD(x)    MAKEWORD(HIBYTE(x), LOBYTE(x))
#define SWAPLONG(x)    MAKELONG(SWAPWORD(HIWORD(x)), SWAPWORD(LOWORD(x)))

// adapted from http://cpansearch.perl.org/src/RJRAY/Image-Size-3.230/lib/Image/Size.pm
SizeI SizeFromData(char *data, size_t len)
{
    SizeI result;
    // too short to contain magic number and image dimensions
    if (len < 8) {
    }
    // Bitmap
    else if (str::StartsWith(data, "BM")) {
        if (len >= sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER)) {
            BITMAPINFOHEADER *bmi = (BITMAPINFOHEADER *)(data + sizeof(BITMAPFILEHEADER));
            result = SizeI(bmi->biWidth, bmi->biHeight);
        }
    }
    // PNG
    else if (str::StartsWith(data, "\x89PNG\x0D\x0A\x1A\x0A")) {
        if (len >= 24 && str::StartsWith(data + 12, "IHDR")) {
            DWORD width = SWAPLONG(*(DWORD *)(data + 16));
            DWORD height = SWAPLONG(*(DWORD *)(data + 20));
            result = SizeI(width, height);
        }
    }
    // JPEG
    else if (str::StartsWith(data, "\xFF\xD8")) {
        // find the last start of frame marker for non-differential Huffman coding
        for (size_t ix = 2; ix + 9 < len && data[ix] == '\xFF'; ) {
            if ('\xC0' <= data[ix + 1] && data[ix + 1] <= '\xC3') {
                WORD width = SWAPWORD(*(WORD *)(data + ix + 7));
                WORD height = SWAPWORD(*(WORD *)(data + ix + 5));
                result = SizeI(width, height);
            }
            ix += SWAPWORD(*(WORD *)(data + ix + 2)) + 2;
        }
    }
    // GIF
    else if (str::StartsWith(data, "GIF87a") || str::StartsWith(data, "GIF89a")) {
        if (len >= 13) {
            // find the first image's actual size instead of using the
            // "logical screen" size which is sometimes too large
            size_t ix = 13;
            // skip the global color table
            if ((data[10] & 0x80))
                ix += 3 * (1 << ((data[10] & 0x07) + 1));
            while (ix + 8 < len) {
                if (data[ix] == '\x2c') {
                    WORD width = *(WORD *)(data + ix + 5);
                    WORD height = *(WORD *)(data + ix + 7);
                    result = SizeI(width, height);
                    break;
                }
                else if (data[ix] == '\x21' && data[ix + 1] == '\xF9')
                    ix += 8;
                else if (data[ix] == '\x21' && data[ix + 1] == '\xFE') {
                    char *commentEnd = (char *)memchr(data + ix + 2, '\0', len - ix - 2);
                    ix = commentEnd ? commentEnd - data + 1 : len;
                }
                else if (data[ix] == '\x21' && data[ix + 1] == '\x01' && ix + 15 < len) {
                    char *textDataEnd = (char *)memchr(data + ix + 15, '\0', len - ix - 15);
                    ix = textDataEnd ? textDataEnd - data + 1 : len;
                }
                else if (data[ix] == '\x21' && data[ix + 1] == '\xFF' && ix + 14 < len) {
                    char *applicationDataEnd = (char *)memchr(data + ix + 14, '\0', len - ix - 14);
                    ix = applicationDataEnd ? applicationDataEnd - data + 1 : len;
                }
                else
                    break;
            }
        }
    }
    // TIFF
    else if (!memcmp(data, "MM\x00\x2A", 4) || !memcmp(data, "II\x2A\x00", 4)) {
        // TODO: speed this up (if necessary)
    }

    if (0 == result.dx || 0 == result.dy) {
        // let GDI+ extract the image size if we've failed
        // (currently happens for animated GIFs and for all TIFFs)
        Bitmap *bmp = BitmapFromData(data, len);
        if (bmp)
            result = SizeI(bmp->GetWidth(), bmp->GetHeight());
        delete bmp;
    }

    return result;
}

RenderedBitmap *LoadRenderedBitmap(const TCHAR *filePath)
{
    if (str::EndsWithI(filePath, _T(".bmp"))) {
        HBITMAP hbmp = (HBITMAP)LoadImage(NULL, filePath, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
        if (!hbmp)
            return NULL;
        return new RenderedBitmap(hbmp, GetBitmapSize(hbmp));
    }

    size_t len;
    ScopedMem<char> data(file::ReadAll(filePath, &len));
    if (!data)
        return NULL;
    Bitmap *bmp = BitmapFromData(data, len);
    if (!bmp)
        return NULL;

    HBITMAP hbmp;
    RenderedBitmap *rendered = NULL;
    if (bmp->GetHBITMAP(Color(0xFF, 0xFF, 0xFF), &hbmp) == Ok)
        rendered = new RenderedBitmap(hbmp, SizeI(bmp->GetWidth(), bmp->GetHeight()));
    delete bmp;

    return rendered;
}

static bool GetEncoderClsid(const TCHAR *format, CLSID& clsid)
{
    UINT numEncoders, size;
    GetImageEncodersSize(&numEncoders, &size);
    if (0 == size)
        return false;

    ScopedMem<ImageCodecInfo> codecInfo((ImageCodecInfo *)malloc(size));
    ScopedMem<WCHAR> formatW(str::conv::ToWStr(format));
    if (!codecInfo || !formatW)
        return false;

    GetImageEncoders(numEncoders, size, codecInfo);
    for (UINT j = 0; j < numEncoders; j++) {
        if (str::Eq(codecInfo[j].MimeType, formatW)) {
            clsid = codecInfo[j].Clsid;
            return true;
        }
    }

    return false;
}

bool SaveRenderedBitmap(RenderedBitmap *bmp, const TCHAR *filePath)
{
    size_t bmpDataLen;
    ScopedMem<unsigned char> bmpData(SerializeBitmap(bmp->GetBitmap(), &bmpDataLen));
    if (!bmpData)
        return false;

    const TCHAR *fileExt = path::GetExt(filePath);
    if (str::EqI(fileExt, _T(".bmp")))
        return file::WriteAll(filePath, bmpData.Get(), bmpDataLen);

    const TCHAR *encoders[] = {
        _T(".png"), _T("image/png"),
        _T(".jpg"), _T("image/jpeg"),
        _T(".jpeg"),_T("image/jpeg"),
        _T(".gif"), _T("image/gif"),
        _T(".tif"), _T("image/tiff"),
        _T(".tiff"),_T("image/tiff"),
    };
    const TCHAR *encoder = NULL;
    for (int i = 0; i < dimof(encoders) && !encoder; i += 2)
        if (str::EqI(fileExt, encoders[i]))
            encoder = encoders[i+1];

    CLSID encClsid;
    if (!encoder || !GetEncoderClsid(encoder, encClsid))
        return false;

    Bitmap *gbmp = BitmapFromData(bmpData, bmpDataLen);
    if (!gbmp)
        return false;

    ScopedMem<WCHAR> filePathW(str::conv::ToWStr(filePath));
    Status status = gbmp->Save(filePathW, &encClsid);
    delete gbmp;

    return status == Ok;
}

///// ImagesEngine methods apply to all types of engines handling full-page images /////

class ImagesEngine : public virtual BaseEngine {
public:
    ImagesEngine() : fileName(NULL), fileExt(NULL) { }
    virtual ~ImagesEngine() {
        DeleteVecMembers(pages);
        free((void *)fileName);
    }

    virtual const TCHAR *FileName() const { return fileName; };
    virtual int PageCount() const { return (int)pages.Count(); }

    virtual RectD PageMediabox(int pageNo) {
        assert(1 <= pageNo && pageNo <= PageCount());
        return RectD(0, 0, pages.At(pageNo - 1)->GetWidth(), pages.At(pageNo - 1)->GetHeight());
    }

    virtual RenderedBitmap *RenderBitmap(int pageNo, float zoom, int rotation,
                         RectD *pageRect=NULL, /* if NULL: defaults to the page's mediabox */
                         RenderTarget target=Target_View);
    virtual bool RenderPage(HDC hDC, RectI screenRect, int pageNo, float zoom, int rotation,
                         RectD *pageRect=NULL, RenderTarget target=Target_View);

    virtual PointD Transform(PointD pt, int pageNo, float zoom, int rotation, bool inverse=false);
    virtual RectD Transform(RectD rect, int pageNo, float zoom, int rotation, bool inverse=false);

    virtual unsigned char *GetFileData(size_t *cbCount) {
        return (unsigned char *)file::ReadAll(fileName, cbCount);
    }
    virtual TCHAR * ExtractPageText(int pageNo, TCHAR *lineSep, RectI **coords_out=NULL,
                                    RenderTarget target=Target_View) { return NULL; }
    virtual bool IsImagePage(int pageNo) { return true; }
    virtual PageLayoutType PreferredLayout() { return Layout_NonContinuous; }
    virtual bool IsImageCollection() { return true; }

    virtual const TCHAR *GetDefaultFileExt() const { return fileExt; }

    virtual bool BenchLoadPage(int pageNo) { return LoadImage(pageNo) != NULL; }

protected:
    const TCHAR *fileName;
    const TCHAR *fileExt;

    Vec<Bitmap *> pages;

    void GetTransform(Matrix& m, int pageNo, float zoom, int rotation);

    // override for lazily loading images
    virtual Bitmap *LoadImage(int pageNo) {
        assert(1 <= pageNo && pageNo <= PageCount());
        return pages.At(pageNo - 1);
    }
};

RenderedBitmap *ImagesEngine::RenderBitmap(int pageNo, float zoom, int rotation, RectD *pageRect, RenderTarget target)
{
    RectD pageRc = pageRect ? *pageRect : PageMediabox(pageNo);
    RectI screen = Transform(pageRc, pageNo, zoom, rotation).Round();
    screen.Offset(-screen.x, -screen.y);

    HDC hDC = GetDC(NULL);
    HDC hDCMem = CreateCompatibleDC(hDC);
    HBITMAP hbmp = CreateCompatibleBitmap(hDC, screen.dx, screen.dy);
    DeleteObject(SelectObject(hDCMem, hbmp));

    bool ok = RenderPage(hDCMem, screen, pageNo, zoom, rotation, pageRect, target);
    DeleteDC(hDCMem);
    ReleaseDC(NULL, hDC);
    if (!ok) {
        DeleteObject(hbmp);
        return NULL;
    }

    return new RenderedBitmap(hbmp, screen.Size());
}

bool ImagesEngine::RenderPage(HDC hDC, RectI screenRect, int pageNo, float zoom, int rotation, RectD *pageRect, RenderTarget target)
{
    Bitmap *bmp = LoadImage(pageNo);
    if (!bmp)
        return false;

    RectD pageRc = pageRect ? *pageRect : PageMediabox(pageNo);
    RectI screen = Transform(pageRc, pageNo, zoom, rotation).Round();

    Graphics g(hDC);
    g.SetCompositingQuality(CompositingQualityHighQuality);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPageUnit(UnitPixel);

    Color white(0xFF, 0xFF, 0xFF);
    Gdiplus::Rect screenR(screenRect.x, screenRect.y, screenRect.dx, screenRect.dy);
    g.SetClip(screenR);
    g.FillRectangle(&SolidBrush(white), screenR);

    REAL scaleX = 1.0f, scaleY = 1.0f;
    if (bmp->GetHorizontalResolution() != 0.f)
        scaleX = 1.0f * bmp->GetHorizontalResolution() / GetDeviceCaps(hDC, LOGPIXELSX);
    if (bmp->GetVerticalResolution() != 0.f)
        scaleY = 1.0f * bmp->GetVerticalResolution() / GetDeviceCaps(hDC, LOGPIXELSY);

    Matrix m;
    GetTransform(m, pageNo, zoom, rotation);
    m.Translate((REAL)(screenRect.x - screen.x), (REAL)(screenRect.y - screen.y), MatrixOrderAppend);
    if (scaleX != 1.0f || scaleY != 1.0f)
        m.Scale(scaleX, scaleY, MatrixOrderPrepend);
    g.SetTransform(&m);

    Status ok = g.DrawImage(bmp, 0, 0);
    return ok == Ok;
}

void ImagesEngine::GetTransform(Matrix& m, int pageNo, float zoom, int rotation)
{
    SizeD size = PageMediabox(pageNo).Size();

    rotation = rotation % 360;
    if (rotation < 0) rotation = rotation + 360;
    if (90 == rotation)
        m.Translate(0, (REAL)-size.dy, MatrixOrderAppend);
    else if (180 == rotation)
        m.Translate((REAL)-size.dx, (REAL)-size.dy, MatrixOrderAppend);
    else if (270 == rotation)
        m.Translate((REAL)-size.dx, 0, MatrixOrderAppend);
    else // if (0 == rotation)
        m.Translate(0, 0, MatrixOrderAppend);

    m.Scale(zoom, zoom, MatrixOrderAppend);
    m.Rotate((REAL)rotation, MatrixOrderAppend);
}

PointD ImagesEngine::Transform(PointD pt, int pageNo, float zoom, int rotation, bool inverse)
{
    RectD rect = Transform(RectD(pt, SizeD()), pageNo, zoom, rotation, inverse);
    return PointD(rect.x, rect.y);
}

RectD ImagesEngine::Transform(RectD rect, int pageNo, float zoom, int rotation, bool inverse)
{
    PointF pts[2] = {
        PointF((REAL)rect.x, (REAL)rect.y),
        PointF((REAL)(rect.x + rect.dx), (REAL)(rect.y + rect.dy))
    };
    Matrix m;
    GetTransform(m, pageNo, zoom, rotation);
    if (inverse)
        m.Invert();
    m.TransformPoints(pts, 2);
    return RectD::FromXY(pts[0].X, pts[0].Y, pts[1].X, pts[1].Y);
}

///// ImageEngine handles a single image file /////

class CImageEngine : public ImagesEngine, public ImageEngine {
    friend ImageEngine;

public:
    virtual ImageEngine *Clone();

    virtual unsigned char *GetFileData(size_t *cbCount);

protected:
    bool LoadSingleFile(const TCHAR *fileName);
    bool LoadFromStream(IStream *stream);
};

ImageEngine *CImageEngine::Clone()
{
    Bitmap *bmp = pages.At(0);
    bmp = bmp->Clone(0, 0, bmp->GetWidth(), bmp->GetHeight(), PixelFormat32bppARGB);
    if (!bmp)
        return NULL;

    CImageEngine *clone = new CImageEngine();
    clone->pages.Append(bmp);
    clone->fileName = fileName ? str::Dup(fileName) : NULL;
    clone->fileExt = clone->fileName ? path::GetExt(clone->fileName) : _T(".png");

    return clone;
}

bool CImageEngine::LoadSingleFile(const TCHAR *file)
{
    size_t len = 0;
    ScopedMem<char> bmpData(file::ReadAll(file, &len));
    if (!bmpData)
        return false;

    pages.Append(BitmapFromData(bmpData, len));

    fileName = str::Dup(file);
    fileExt = FileExtFromData(bmpData, len);
    assert(fileExt);
    if (!fileExt) fileExt = _T(".png");

    return pages.At(0) != NULL;
}

bool CImageEngine::LoadFromStream(IStream *stream)
{
    if (!stream)
        return false;

    pages.Append(Bitmap::FromStream(stream));
    // could sniff instead, but GDI+ allows us to convert the image format anyway
    fileExt = _T(".png");

    return pages.At(0) != NULL;
}

unsigned char *CImageEngine::GetFileData(size_t *cbCount) {
    if (fileName)
        return (unsigned char *)file::ReadAll(fileName, cbCount);

    // TODO: convert Bitmap to PNG and return its data (saving to/reading from a stream)
    return NULL;
}

bool ImageEngine::IsSupportedFile(const TCHAR *fileName, bool sniff)
{
    if (sniff) {
        char header[9] = { 0 };
        file::ReadAll(fileName, header, sizeof(header));
        fileName = FileExtFromData(header, sizeof(header));
    }

    return str::EndsWithI(fileName, _T(".png")) ||
           str::EndsWithI(fileName, _T(".jpg")) || str::EndsWithI(fileName, _T(".jpeg")) ||
           str::EndsWithI(fileName, _T(".gif")) ||
           str::EndsWithI(fileName, _T(".tif")) || str::EndsWithI(fileName, _T(".tiff")) ||
           str::EndsWithI(fileName, _T(".bmp"));
}

ImageEngine *ImageEngine::CreateFromFileName(const TCHAR *fileName)
{
    assert(IsSupportedFile(fileName) || IsSupportedFile(fileName, true));
    CImageEngine *engine = new CImageEngine();
    if (!engine->LoadSingleFile(fileName)) {
        delete engine;
        return NULL;
    }
    return engine;    
}

ImageEngine *ImageEngine::CreateFromStream(IStream *stream)
{
    CImageEngine *engine = new CImageEngine();
    if (!engine->LoadFromStream(stream)) {
        delete engine;
        return NULL;
    }
    return engine;
}

///// ImageDirEngine handles a directory full of image files /////

class CImageDirEngine : public ImagesEngine, public ImageDirEngine {
    friend ImageDirEngine;

public:
    virtual ImageDirEngine *Clone() { return CreateFromFileName(fileName); }
    virtual RectD PageMediabox(int pageNo);

    virtual unsigned char *GetFileData(size_t *cbCount) { return NULL; }

    // TODO: is there a better place to expose pageFileNames than through page labels?
    virtual bool HasPageLabels() { return true; }
    virtual TCHAR *GetPageLabel(int pageNo);
    virtual int GetPageByLabel(const TCHAR *label);

    virtual bool HasTocTree() const { return true; }
    virtual DocTocItem *GetTocTree();

protected:
    bool LoadImageDir(const TCHAR *dirName);

    virtual Bitmap *LoadImage(int pageNo);

    Vec<RectD> mediaboxes;
    StrVec pageFileNames;
};

bool CImageDirEngine::LoadImageDir(const TCHAR *dirName)
{
    fileName = str::Dup(dirName);
    fileExt = _T("");

    ScopedMem<TCHAR> pattern(path::Join(dirName, _T("*")));

    WIN32_FIND_DATA fdata;
    HANDLE hfind = FindFirstFile(pattern, &fdata);
    if (INVALID_HANDLE_VALUE == hfind)
        return false;

    do {
        if (!(fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            if (ImageEngine::IsSupportedFile(fdata.cFileName))
                pageFileNames.Append(path::Join(dirName, fdata.cFileName));
    } while (FindNextFile(hfind, &fdata));
    FindClose(hfind);

    if (pageFileNames.Count() == 0)
        return false;
    pageFileNames.SortNatural();

    pages.AppendBlanks(pageFileNames.Count());
    mediaboxes.AppendBlanks(pageFileNames.Count());

    return true;
}

RectD CImageDirEngine::PageMediabox(int pageNo)
{
    assert(1 <= pageNo && pageNo <= PageCount());
    if (!mediaboxes.At(pageNo - 1).IsEmpty())
        return mediaboxes.At(pageNo - 1);

    size_t len;
    ScopedMem<char> bmpData(file::ReadAll(pageFileNames.At(pageNo - 1), &len));
    if (bmpData) {
        SizeI size = SizeFromData(bmpData, len);
        mediaboxes.At(pageNo - 1) = RectI(PointI(), size).Convert<double>();
    }
    return mediaboxes.At(pageNo - 1);
}

TCHAR *CImageDirEngine::GetPageLabel(int pageNo)
{
    if (pageNo < 1 || PageCount() < pageNo)
        return BaseEngine::GetPageLabel(pageNo);

    const TCHAR *fileName = path::GetBaseName(pageFileNames.At(pageNo - 1));
    return str::DupN(fileName, path::GetExt(fileName) - fileName);
}

int CImageDirEngine::GetPageByLabel(const TCHAR *label)
{
    for (size_t i = 0; i < pageFileNames.Count(); i++) {
        const TCHAR *fileName = path::GetBaseName(pageFileNames.At(i));
        const TCHAR *fileExt = path::GetExt(fileName);
        if (str::StartsWithI(fileName, label) &&
            (fileName + str::Len(label) == fileExt || fileName[str::Len(label)] == '\0'))
            return (int)i + 1;
    }

    return BaseEngine::GetPageByLabel(label);
}

Bitmap *CImageDirEngine::LoadImage(int pageNo)
{
    assert(1 <= pageNo && pageNo <= PageCount());
    if (pages.At(pageNo - 1))
        return pages.At(pageNo - 1);

    size_t len;
    ScopedMem<char> bmpData(file::ReadAll(pageFileNames.At(pageNo - 1), &len));
    if (bmpData)
        pages.At(pageNo - 1) = BitmapFromData(bmpData, len);

    return pages.At(pageNo - 1);
}

class ImageDirTocItem : public DocTocItem {
public:
    ImageDirTocItem(TCHAR *title, int pageNo) : DocTocItem(title, pageNo) { }

    virtual PageDestination *GetLink() { return NULL; }
};

DocTocItem *CImageDirEngine::GetTocTree()
{
    DocTocItem *root = new ImageDirTocItem(GetPageLabel(1), 1);
    for (int i = 2; i <= PageCount(); i++)
        root->AddSibling(new ImageDirTocItem(GetPageLabel(i), i));
    return root;
}

bool ImageDirEngine::IsSupportedFile(const TCHAR *fileName, bool sniff)
{
    // whether it actually contains images will be checked in LoadImageDir
    return dir::Exists(fileName);
}

ImageDirEngine *ImageDirEngine::CreateFromFileName(const TCHAR *fileName)
{
    assert(dir::Exists(fileName));
    CImageDirEngine *engine = new CImageDirEngine();
    if (!engine->LoadImageDir(fileName)) {
        delete engine;
        return NULL;
    }
    return engine;
}

///// CbxEngine handles comic book files (either .cbz or .cbr) /////

struct CbzFileAccess {
    zlib_filefunc64_def ffunc;
    unzFile uf;
};

class CCbxEngine : public ImagesEngine, public CbxEngine {
    friend CbxEngine;

public:
    CCbxEngine() : cbzData(NULL) {
        InitializeCriticalSection(&fileAccess);
    }
    virtual ~CCbxEngine();

    virtual CbxEngine *Clone() { return CreateFromFileName(fileName); }
    virtual RectD PageMediabox(int pageNo);

protected:
    bool LoadCbzFile(const TCHAR *fileName);
    bool LoadCbrFile(const TCHAR *fileName);

    virtual Bitmap *LoadImage(int pageNo);
    char *GetImageData(int pageNo, size_t& len);

    Vec<RectD> mediaboxes;

    // used for lazily loading page images (only supported for .cbz files)
    CRITICAL_SECTION fileAccess;
    StrVec pageFileNames;
    CbzFileAccess *cbzData;
};

CCbxEngine::~CCbxEngine()
{
    if (cbzData) {
        unzClose(cbzData->uf);
        delete cbzData;
    }

    DeleteCriticalSection(&fileAccess);
}

RectD CCbxEngine::PageMediabox(int pageNo)
{
    assert(1 <= pageNo && pageNo <= PageCount());
    if (!mediaboxes.At(pageNo - 1).IsEmpty())
        return mediaboxes.At(pageNo - 1);

    size_t len;
    ScopedMem<char> bmpData(GetImageData(pageNo, len));
    if (bmpData) {
        SizeI size = SizeFromData(bmpData, len);
        mediaboxes.At(pageNo - 1) = RectI(PointI(), size).Convert<double>();
    }
    return mediaboxes.At(pageNo - 1);
}

Bitmap *CCbxEngine::LoadImage(int pageNo)
{
    assert(1 <= pageNo && pageNo <= PageCount());
    if (pages.At(pageNo - 1))
        return pages.At(pageNo - 1);

    size_t len;
    ScopedMem<char> bmpData(GetImageData(pageNo, len));
    if (bmpData)
        pages.At(pageNo - 1) = BitmapFromData(bmpData, len);

    return pages.At(pageNo - 1);
}

static bool SetCurrentCbzPage(unzFile& uf, const TCHAR *fileName)
{
    ScopedMem<char> fileNameUtf8(str::conv::ToUtf8(fileName));
    int err = unzLocateFile(uf, fileNameUtf8, 0);
    return err == UNZ_OK;
}

// caller must free() the result
static char *LoadCurrentCbzData(unzFile& uf, size_t& len)
{
    unz_file_info64 finfo;
    char *bmpData = NULL;

    int err = unzGetCurrentFileInfo64(uf, &finfo, NULL, 0, NULL, 0, NULL, 0);
    if (err != UNZ_OK)
        return NULL;

    err = unzOpenCurrentFilePassword(uf, NULL);
    if (err != UNZ_OK)
        return NULL;

    len = (size_t)finfo.uncompressed_size;
    ZPOS64_T len2 = len;
    if (len2 != finfo.uncompressed_size) // overflow check
        goto Exit;

    bmpData = SAZA(char, len);
    if (!bmpData)
        goto Exit;

    unsigned int readBytes = unzReadCurrentFile(uf, bmpData, (unsigned int)len);
    if (readBytes != len) {
        free(bmpData);
        bmpData = NULL;
    }

Exit:
    unzCloseCurrentFile(uf); // ignoring error code
    return bmpData;
}

bool CCbxEngine::LoadCbzFile(const TCHAR *file)
{
    if (!file)
        return false;
    fileName = str::Dup(file);
    fileExt = _T(".cbz");

    cbzData = new CbzFileAccess;

    // only extract all image filenames for now
    fill_win32_filefunc64(&cbzData->ffunc);
    cbzData->uf = unzOpen2_64(fileName, &cbzData->ffunc);
    if (!cbzData->uf)
        return false;

    unz_global_info64 ginfo;
    int err = unzGetGlobalInfo64(cbzData->uf, &ginfo);
    if (err != UNZ_OK)
        return false;
    unzGoToFirstFile(cbzData->uf);

    for (int n = 0; n < ginfo.number_entry; n++) {
        char fileName[MAX_PATH];
        int err = unzGetCurrentFileInfo64(cbzData->uf, NULL, fileName, dimof(fileName), NULL, 0, NULL, 0);
        if (err == UNZ_OK) {
            ScopedMem<TCHAR> fileName2(str::conv::FromUtf8(fileName));
            if (ImageEngine::IsSupportedFile(fileName2) &&
                // OS X occasionally leaves metadata with image extensions
                !str::StartsWith(path::GetBaseName(fileName2), _T("."))) {
                pageFileNames.Append(fileName2.StealData());
            }
        }
        // bail, if we accidentally try loading an XPS file
        if (str::StartsWith(fileName, "_rels/.rels"))
            return false;
        err = unzGoToNextFile(cbzData->uf);
        if (err != UNZ_OK)
            break;
    }

    // TODO: any meta-information available?

    if (pageFileNames.Count() == 0)
        return false;
    pageFileNames.Sort();

    pages.AppendBlanks(pageFileNames.Count());
    mediaboxes.AppendBlanks(pageFileNames.Count());

    return true;
}

class ImagesPage {
public:
    const TCHAR *   fileName; // for sorting image files
    Bitmap *        bmp;

    ImagesPage(const TCHAR *fileName, Bitmap *bmp) : bmp(bmp) {
        this->fileName = str::Dup(fileName);
    }
    ~ImagesPage() {
        free((void *)fileName);
        delete bmp;
    }

    static int cmpPageByName(const void *o1, const void *o2) {
        ImagesPage *p1 = *(ImagesPage **)o1;
        ImagesPage *p2 = *(ImagesPage **)o2;
        return _tcscmp(p1->fileName, p2->fileName);
    }
};

struct RarDecompressData {
    unsigned    totalSize;
    char *      buf;
    unsigned    currSize;
};

static int CALLBACK unrarCallback(UINT msg, LPARAM userData, LPARAM rarBuffer, LPARAM bytesProcessed)
{
    if (UCM_PROCESSDATA != msg)
        return -1;

    if (!userData)
        return -1;
    RarDecompressData *rrd = (RarDecompressData*)userData;

    if (rrd->currSize + bytesProcessed > rrd->totalSize)
        return -1;

    char *buf = rrd->buf + rrd->currSize;
    memcpy(buf, (char *)rarBuffer, bytesProcessed);
    rrd->currSize += (unsigned int)bytesProcessed;
    return 1;
}

static ImagesPage *LoadCurrentCbrPage(HANDLE hArc, RARHeaderDataEx& rarHeader)
{
    ScopedMem<char> bmpData;
#ifdef UNICODE
    TCHAR *fileName = rarHeader.FileNameW;
#else
    TCHAR *fileName = rarHeader.FileName;
#endif

    if (!ImageEngine::IsSupportedFile(fileName))
        goto SkipFile;
    if (rarHeader.UnpSizeHigh != 0)
        goto SkipFile;
    if (rarHeader.UnpSize == 0)
        goto SkipFile;

    RarDecompressData rdd;
    rdd.totalSize = rarHeader.UnpSize;
    bmpData.Set(SAZA(char, rdd.totalSize));
    if (!bmpData)
        goto SkipFile;

    rdd.buf = bmpData;
    rdd.currSize = 0;
    RARSetCallback(hArc, unrarCallback, (LPARAM)&rdd);
    int res = RARProcessFile(hArc, RAR_TEST, NULL, NULL);
    if (0 != res)
        return NULL;
    if (rdd.totalSize != rdd.currSize)
        return NULL;

    Bitmap *bmp = BitmapFromData(bmpData, rdd.totalSize);
    if (!bmp)
        return NULL;

    return new ImagesPage(fileName, bmp);

SkipFile:
    RARProcessFile(hArc, RAR_SKIP, NULL, NULL);
    return NULL;
 }

bool CCbxEngine::LoadCbrFile(const TCHAR *file)
{
    if (!file)
        return false;
    fileName = str::Dup(file);
    fileExt = _T(".cbr");

    RAROpenArchiveDataEx  arcData = { 0 };
#ifdef UNICODE
    arcData.ArcNameW = (TCHAR*)file;
#else
    arcData.ArcName = (TCHAR*)file;
#endif
    arcData.OpenMode = RAR_OM_EXTRACT;

    HANDLE hArc = RAROpenArchiveEx(&arcData);
    if (arcData.OpenResult != 0)
        return false;

    // UnRAR does not seem to support extracting a single file by name,
    // so lazy image loading doesn't seem possible

    Vec<ImagesPage *> found;
    for (;;) {
        RARHeaderDataEx rarHeader;
        int res = RARReadHeaderEx(hArc, &rarHeader);
        if (0 != res)
            break;

        ImagesPage *page = LoadCurrentCbrPage(hArc, rarHeader);
        if (page)
            found.Append(page);
    }
    RARCloseArchive(hArc);

    if (found.Count() == 0)
        return false;
    found.Sort(ImagesPage::cmpPageByName);

    for (size_t i = 0; i < found.Count(); i++) {
        pages.Append(found.At(i)->bmp);
        mediaboxes.Append(RectD(0, 0, pages.At(i)->GetWidth(), pages.At(i)->GetHeight()));
        found.At(i)->bmp = NULL;
    }

    DeleteVecMembers(found);
    return true;
}

char *CCbxEngine::GetImageData(int pageNo, size_t& len)
{
    if (cbzData) {
        ScopedCritSec scope(&fileAccess);
        if (SetCurrentCbzPage(cbzData->uf, pageFileNames.At(pageNo - 1)))
            return LoadCurrentCbzData(cbzData->uf, len);
    }
    return NULL;
}

bool CbxEngine::IsSupportedFile(const TCHAR *fileName, bool sniff)
{
    if (sniff) {
        // we don't also sniff for ZIP files, as these could also
        // be broken XPS files for which failure is expected
        return file::StartsWith(fileName, "Rar!\x1A\x07\x00", 7);
    }

    return str::EndsWithI(fileName, _T(".cbz")) ||
           str::EndsWithI(fileName, _T(".cbr")) ||
           str::EndsWithI(fileName, _T(".zip")) ||
           str::EndsWithI(fileName, _T(".rar"));
}

CbxEngine *CbxEngine::CreateFromFileName(const TCHAR *fileName)
{
    assert(IsSupportedFile(fileName) || IsSupportedFile(fileName, true));
    CCbxEngine *engine = new CCbxEngine();
    bool ok = false;
    if (str::EndsWithI(fileName, _T(".cbz")) ||
        str::EndsWithI(fileName, _T(".zip"))) {
        ok = engine->LoadCbzFile(fileName);
    }
    else if (str::EndsWithI(fileName, _T(".cbr")) ||
             str::EndsWithI(fileName, _T(".rar")) ||
             file::StartsWith(fileName, "Rar!\x1A\x07\x00", 7)) {
        ok = engine->LoadCbrFile(fileName);
    }
    if (!ok) {
        delete engine;
        return NULL;
    }
    return engine;
}
