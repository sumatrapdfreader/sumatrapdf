/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "ImagesEngine.h"
#include "StrUtil.h"
#include "FileUtil.h"

// mini(un)zip
#include <ioapi.h>
#include <iowin32.h>
#include <unzip.h>

#include "../ext/unrar/dll.hpp"

extern "C" {
// needed because we compile bzip2 with #define BZ_NO_STDIO
void bz_internal_error(int errcode)
{
    // do nothing
}
}

using namespace Gdiplus;

// cf. http://stackoverflow.com/questions/4598872/creating-hbitmap-from-memory-buffer/4616394#4616394
Bitmap *BitmapFromData(void *data, size_t len)
{
    IStream *stream;
    HRESULT res = CreateStreamOnHGlobal(NULL, TRUE, &stream);
    if (FAILED(res))
        return NULL;

    Bitmap *bmp = NULL;
    res = stream->Write(data, len, NULL);
    if (SUCCEEDED(res))
        bmp = Bitmap::FromStream(stream);
    stream->Release();

    if (bmp && bmp->GetLastStatus() != Ok) {
        delete bmp;
        bmp = NULL;
    }

    return bmp;
}

static int cmpPageByName(const void *o1, const void *o2)
{
    ImagesPage *p1 = *(ImagesPage **)o1;
    ImagesPage *p2 = *(ImagesPage **)o2;
    return Str::CmpNatural(p1->fileName, p2->fileName);
}

static ImagesPage *LoadCurrentCbzPage(unzFile& uf)
{
    char            fileName[MAX_PATH];
    unz_file_info64 finfo;
    ImagesPage *    page = NULL;
    char *          bmpData = NULL;

    int err = unzGetCurrentFileInfo64(uf, &finfo, fileName, dimof(fileName), NULL, 0, NULL, 0);
    if (err != UNZ_OK)
        return NULL;

    ScopedMem<TCHAR> fileName2(Str::Conv::FromUtf8(fileName));
    if (!ImageEngine::IsSupportedFile(fileName2))
        return NULL;

    err = unzOpenCurrentFilePassword(uf, NULL);
    if (err != UNZ_OK)
        return NULL;

    unsigned len = (unsigned)finfo.uncompressed_size;
    ZPOS64_T len2 = len;
    if (len2 != finfo.uncompressed_size) // overflow check
        goto Exit;

    bmpData = SAZA(char, len);
    if (!bmpData)
        goto Exit;

    unsigned int readBytes = unzReadCurrentFile(uf, bmpData, len);
    if (readBytes != len)
        goto Exit;

    Bitmap *bmp = BitmapFromData(bmpData, len);
    if (!bmp)
        goto Exit;

    page = new ImagesPage(fileName2, bmp);

Exit:
    unzCloseCurrentFile(uf); // ignoring error code
    free(bmpData);
    return page;
}

ImagesEngine::ImagesEngine() : fileName(NULL), fileExt(NULL)
{
}

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
    rrd->currSize += bytesProcessed;        
    return 1;
}

static ImagesPage *LoadCurrentCbrPage(HANDLE hArc, RARHeaderDataEx& rarHeader)
{
    char *       bmpData = NULL;
    ImagesPage * page = NULL;

#ifdef UNICODE
    TCHAR *fileName = rarHeader.FileNameW;
#else
    TCHAR *fileName = rarHeader.FileName;
#endif
    if (!ImageEngine::IsSupportedFile(fileName))
        return NULL;

    if (rarHeader.UnpSizeHigh != 0)
        return NULL;
    if (rarHeader.UnpSize == 0)
        return NULL;

    RarDecompressData rdd = { 0, 0, 0 };
    rdd.totalSize = rarHeader.UnpSize;
    bmpData = SAZA(char, rdd.totalSize);
    if (!bmpData)
        return NULL;

    rdd.buf = bmpData;
    RARSetCallback(hArc, unrarCallback, (LPARAM)&rdd);
    int res = RARProcessFile(hArc, RAR_TEST, NULL, NULL);
    if (0 != res)
        goto Exit;

    if (rdd.totalSize != rdd.currSize)
        goto Exit;

    Bitmap *bmp = BitmapFromData(bmpData, rdd.totalSize);
    if (!bmp)
        goto Exit;

    page = new ImagesPage(fileName, bmp);

Exit:
    free(bmpData);
    return page;
 }

bool CbxEngine::LoadCbrFile(const TCHAR *file)
{
    if (!file)
        return false;
    fileName = Str::Dup(file);
    fileExt = _T(".cbr");
    assert(Str::EndsWithI(fileName, fileExt));

    RAROpenArchiveDataEx  arcData = { 0 };
    arcData.ArcNameW = (TCHAR*)file;
    arcData.OpenMode = RAR_OM_EXTRACT;

    HANDLE hArc = RAROpenArchiveEx(&arcData);
    if (arcData.OpenResult != 0)
        return false;

    for (;;) {
        RARHeaderDataEx rarHeader;
        int res = RARReadHeaderEx(hArc, &rarHeader);
        if (0 != res)
            break;

        ImagesPage *page = LoadCurrentCbrPage(hArc, rarHeader);
        if (page)
            pages.Append(page);
    }
    RARCloseArchive(hArc);

    if (pages.Count() == 0)
        return false;
    pages.Sort(cmpPageByName);
    return true;
}

ImagesPage *ImagesEngine::LoadImage(const TCHAR *fileName)
{
    size_t len = 0;
    ScopedMem<char> bmpData(File::ReadAll(fileName, &len));
    if (!bmpData)
        return NULL;

    Bitmap *bmp = BitmapFromData(bmpData, len);
    if (!bmp)
        return NULL;

    return new ImagesPage(fileName, bmp);
}

bool ImageEngine::LoadSingleFile(const TCHAR *file)
{
    assert(IsSupportedFile(file));
    ImagesPage *page = LoadImage(file);
    if (!page)
        return false;

    pages.Append(page);
    fileName = Str::Dup(file);
    fileExt = _tcsrchr(fileName, '.');
    assert(fileExt);
    return true;
}

bool CbxEngine::LoadCbzFile(const TCHAR *file)
{
    if (!file)
        return false;
    fileName = Str::Dup(file);
    fileExt = _T(".cbz");
    assert(Str::EndsWithI(fileName, fileExt));

    zlib_filefunc64_def ffunc;
    fill_win32_filefunc64(&ffunc);
    unzFile uf = unzOpen2_64(fileName, &ffunc);
    if (!uf)
        return false;

    unz_global_info64 ginfo;
    int err = unzGetGlobalInfo64(uf, &ginfo);
    if (err != UNZ_OK) {
        unzClose(uf);
        return false;
    }
    unzGoToFirstFile(uf);

    // extract all contained files one by one

    // TODO: maybe lazy loading would be beneficial (but at least I would
    // need to parse image headers to extract width/height information)
    for (int n = 0; n < ginfo.number_entry; n++) {
        ImagesPage *page = LoadCurrentCbzPage(uf);
        if (page)
            pages.Append(page);
        err = unzGoToNextFile(uf);
        if (err != UNZ_OK)
            break;
    }

    unzClose(uf);
    // TODO: any meta-information available?

    if (pages.Count() == 0)
        return false;
    pages.Sort(cmpPageByName);
    return true;
}

ImagesEngine::~ImagesEngine()
{
    DeleteVecMembers(pages);
    free((void *)fileName);
}

RenderedBitmap *ImagesEngine::RenderBitmap(int pageNo, float zoom, int rotation, RectD *pageRect, RenderTarget target, bool useGdi)
{
    RectD pageRc = pageRect ? *pageRect : PageMediabox(pageNo);
    RectI screen = Transform(pageRc, pageNo, zoom, rotation).Round();
    screen.Offset(-screen.x, -screen.y);

    HDC hDC = GetDC(NULL);
    HDC hDCMem = CreateCompatibleDC(hDC);
    HBITMAP hbmp = CreateCompatibleBitmap(hDC, screen.dx, screen.dy);
    DeleteObject(SelectObject(hDCMem, hbmp));

    bool ok = RenderPage(hDCMem, pageNo, screen, zoom, rotation, pageRect, target);
    DeleteDC(hDCMem);
    ReleaseDC(NULL, hDC);
    if (!ok) {
        DeleteObject(hbmp);
        return NULL;
    }

    return new RenderedBitmap(hbmp, screen.dx, screen.dy);
}

bool ImagesEngine::RenderPage(HDC hDC, int pageNo, RectI screenRect, float zoom, int rotation, RectD *pageRect, RenderTarget target)
{
    RectD pageRc = pageRect ? *pageRect : PageMediabox(pageNo);
    RectI screen = Transform(pageRc, pageNo, zoom, rotation).Round();

    Graphics g(hDC);
    g.SetCompositingQuality(CompositingQualityHighQuality);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPageUnit(UnitPixel);
    g.SetClip(Gdiplus::Rect(screenRect.x, screenRect.y, screenRect.dx, screenRect.dy));

    Bitmap *bmp = pages[pageNo - 1]->bmp;
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

void ImagesEngine::GetTransform(Matrix& m, int pageNo, float zoom, int rotate)
{
    SizeD size = PageMediabox(pageNo).Size();

    rotate = rotate % 360;
    if (rotate < 0) rotate = rotate + 360;
    if (90 == rotate)
        m.Translate(0, (REAL)-size.dy, MatrixOrderAppend);
    else if (180 == rotate)
        m.Translate((REAL)-size.dx, (REAL)-size.dy, MatrixOrderAppend);
    else if (270 == rotate)
        m.Translate((REAL)-size.dx, 0, MatrixOrderAppend);
    else // if (0 == rotate)
        m.Translate(0, 0, MatrixOrderAppend);

    m.Scale(zoom, zoom, MatrixOrderAppend);
    m.Rotate((REAL)rotate, MatrixOrderAppend);
}

PointD ImagesEngine::Transform(PointD pt, int pageNo, float zoom, int rotate, bool inverse)
{
    RectD rect = Transform(RectD(pt, SizeD()), pageNo, zoom, rotate, inverse);
    return PointD(rect.x, rect.y);
}

RectD ImagesEngine::Transform(RectD rect, int pageNo, float zoom, int rotate, bool inverse)
{
    PointF pts[2] = {
        PointF((REAL)rect.x, (REAL)rect.y),
        PointF((REAL)(rect.x + rect.dx), (REAL)(rect.y + rect.dy))
    };
    Matrix m;
    GetTransform(m, pageNo, zoom, rotate);
    if (inverse)
        m.Invert();
    m.TransformPoints(pts, 2);
    return RectD::FromXY(pts[0].X, pts[0].Y, pts[1].X, pts[1].Y);
}

unsigned char *ImagesEngine::GetFileData(size_t *cbCount)
{
    return (unsigned char *)File::ReadAll(fileName, cbCount);
}

ImageEngine *ImageEngine::CreateFromFileName(const TCHAR *fileName)
{
    assert(IsSupportedFile(fileName));
    ImageEngine *engine = new ImageEngine();
    if (!engine->LoadSingleFile(fileName)) {
        delete engine;
        return NULL;
    }
    return engine;    
}

CbxEngine *CbxEngine::CreateFromFileName(const TCHAR *fileName)
{
    assert(IsSupportedFile(fileName));
    CbxEngine *engine = new CbxEngine();
    bool ok = false;
    if (Str::EndsWithI(fileName, _T(".cbz")))
        ok = engine->LoadCbzFile(fileName);
    else if (Str::EndsWithI(fileName, _T(".cbr")))
        ok = engine->LoadCbrFile(fileName);
    if (!ok) {
        delete engine;
        return NULL;
    }
    return engine;
}
