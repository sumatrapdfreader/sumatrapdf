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

using namespace Gdiplus;

// HGLOBAL must be allocated with GlobalAlloc(GMEM_MOVEABLE, ...)
static Bitmap *BitmapFromHGlobal(HGLOBAL mem)
{
    IStream * stream = NULL;
    Bitmap *  bmp = NULL;

    GlobalLock(mem); // not sure if needed

    if (CreateStreamOnHGlobal(mem, FALSE, &stream) != S_OK)
        goto Exit;

    bmp = Bitmap::FromStream(stream);
    stream->Release();

    if (bmp && bmp->GetLastStatus() != Ok) {
        delete bmp;
        bmp = NULL;
    }

Exit:
    GlobalUnlock(mem);
    return bmp;
}

bool IsImageFile(const WCHAR *fileName)
{
    return Str::EndsWithI(fileName, L".png")  ||
           Str::EndsWithI(fileName, L".jpg")  ||
           Str::EndsWithI(fileName, L".jpeg") ||
           Str::EndsWithI(fileName, L".gif")  ||
           Str::EndsWithI(fileName, L".bmp");
}

bool IsImageFile(const char *fileName)
{
    return Str::EndsWithI(fileName, ".png")  ||
           Str::EndsWithI(fileName, ".jpg")  ||
           Str::EndsWithI(fileName, ".jpeg") ||
           Str::EndsWithI(fileName, ".gif")  ||
           Str::EndsWithI(fileName, ".bmp");
}

static int cmpPageByName(const ImagesPage **pp1, const ImagesPage **pp2)
{
    const ImagesPage *p1 = *pp1;
    const ImagesPage *p2 = *pp2;
    const TCHAR *name1 = p1->fileName;
    const TCHAR *name2 = p2->fileName;
    return Str::CmpNatural(name1, name2);
}

static int cmpPageByName2(const void *o1, const void *o2)
{
    const ImagesPage **pp1 = (const ImagesPage**)o1;
    const ImagesPage **pp2 = (const ImagesPage**)o2;
    const ImagesPage *p1 = *pp1;
    const ImagesPage *p2 = *pp2;
    const TCHAR *name1 = p1->fileName;
    const TCHAR *name2 = p2->fileName;
    return Str::CmpNatural(name1, name2);
}

static ImagesPage *LoadCurrentCbzPage(unzFile& uf)
{
    char            fileName[MAX_PATH];
    TCHAR *         fileName2 = NULL;
    unz_file_info64 finfo;
    ImagesPage *    page = NULL;
    HGLOBAL         bmpData = NULL;
    unsigned int    readBytes;

    int err = unzGetCurrentFileInfo64(uf, &finfo, fileName, dimof(fileName), NULL, 0, NULL, 0);
    if (err != UNZ_OK)
        return NULL;

    if (!IsImageFile(fileName))
        return NULL;

    err = unzOpenCurrentFilePassword(uf, NULL);
    if (err != UNZ_OK)
        return NULL;

    unsigned len = (unsigned)finfo.uncompressed_size;
    ZPOS64_T len2 = len;
    if (len2 != finfo.uncompressed_size) // overflow check
        goto Exit;

    bmpData = GlobalAlloc(GMEM_MOVEABLE, len);
    if (!bmpData)
        goto Exit;

    void *buf = GlobalLock(bmpData);
    readBytes = unzReadCurrentFile(uf, buf, len);
    GlobalUnlock(bmpData);

    if (readBytes != len)
        goto Exit;

    // TODO: do I have to keep bmpData locked? Bitmap created from IStream
    // based on HGLOBAL memory data seems to need underlying memory bits
    // for its lifetime (it gets corrupted if I GlobalFree(bmpData)).
    // What happens if this data is moved?
    Bitmap *bmp = BitmapFromHGlobal(bmpData);
    if (!bmp)
        goto Exit;

    fileName2 = Str::Conv::FromAnsi(fileName); // Note: maybe FromUtf8?
    page = new ImagesPage(fileName2, bmpData, bmp);
    free(fileName2);
    bmpData = NULL;

Exit:
    unzCloseCurrentFile(uf); // ignoring error code
    GlobalFree(bmpData);
    return page;
}

ImagesEngine::ImagesEngine() : fileName(NULL)
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
    HGLOBAL      bmpData = NULL;
    ImagesPage * page = NULL;

    TCHAR *fileName = rarHeader.FileNameW;
    if (!IsImageFile(fileName))
        return NULL;

    if (rarHeader.UnpSizeHigh != 0)
        return NULL;
    if (rarHeader.UnpSize == 0)
        return NULL;

    RarDecompressData rdd = { 0, 0, 0 };
    rdd.totalSize = rarHeader.UnpSize;
    bmpData = GlobalAlloc(GMEM_MOVEABLE, rdd.totalSize);
    if (!bmpData)
        return NULL;

    rdd.buf = (char*)GlobalLock(bmpData);
    if (!rdd.buf)
        goto Exit;

    RARSetCallback(hArc, unrarCallback, (long)&rdd);
    int res = RARProcessFile(hArc, RAR_TEST, NULL, NULL);
    GlobalUnlock(bmpData);

    if (0 != res)
        goto Exit;

    if (rdd.totalSize != rdd.currSize)
        goto Exit;

    Bitmap *bmp = BitmapFromHGlobal(bmpData);
    if (!bmp)
        goto Exit;

    page = new ImagesPage(fileName, bmpData, bmp);
    bmpData = NULL;

Exit:
    GlobalFree(bmpData);
    return page;
 }

bool ImagesEngine::LoadCbrFile(const TCHAR *file)
{
    if (!file)
        return false;
    fileName = Str::Dup(file);

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
    // TODO: why doesn't pages.Sort(cmpPageByName); work?
    pages.Sort(cmpPageByName2);
    return true;
}

// TODO: could be more efficient by avoiding memory copy
static ImagesPage *LoadImage(const TCHAR *fileName)
{
    HGLOBAL         bmpData = NULL;
    ImagesPage *    page = NULL;
    void *          data = NULL;
    void *          buf = NULL;
    size_t          len = 0;

    if (!File::Exists(fileName))
        return NULL;

    len = 0;
    data = File::ReadAll(fileName, &len);
    if (!data || (0 == len))
        return NULL;

    bmpData = GlobalAlloc(GMEM_MOVEABLE, len);
    if (!bmpData)
        goto Exit;

    buf = GlobalLock(bmpData);
    memcpy(buf, data, len);
    GlobalUnlock(bmpData);

    // TODO: do I have to keep bmpData locked? Bitmap created from IStream
    // based on HGLOBAL memory data seems to need underlying memory bits
    // for its lifetime (it gets corrupted if I GlobalFree(bmpData)).
    // What happens if this data is moved?
    Bitmap *bmp = BitmapFromHGlobal(bmpData);
    if (!bmp)
        goto Exit;

    page = new ImagesPage(fileName, bmpData, bmp);
    bmpData = NULL;

Exit:
    free(data);
    GlobalFree(bmpData);
    return page;
}

bool ImagesEngine::LoadSingleFile(const TCHAR *file)
{
    ImagesPage *page = LoadImage(file);
    if (!page)
        return false;

    pages.Append(page);
    fileName = Str::Dup(file);
    return true;
}

bool ImagesEngine::LoadCbzFile(const TCHAR *file)
{
    if (!file)
        return false;
    fileName = Str::Dup(file);

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
    pages.Sort(cmpPageByName2);
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

    Bitmap *bmp = pages[pageNo - 1]->bmp;
    REAL scaleX = 1.0f, scaleY = 1.0f;
    if (bmp->GetHorizontalResolution() != 0.f)
        scaleX = bmp->GetHorizontalResolution() / 96.0f;
    if (bmp->GetVerticalResolution() != 0.f)
        scaleY = bmp->GetVerticalResolution() / 96.0f;

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
    Gdiplus::PointF pts[2] = {
        Gdiplus::PointF((REAL)rect.x, (REAL)rect.y),
        Gdiplus::PointF((REAL)(rect.x + rect.dx), (REAL)(rect.y + rect.dy))
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

bool ImagesEngine::IsSupportedFile(const TCHAR *fileName)
{
    if (Str::EndsWith(fileName, _T(".cbz")))
        return true;
    if (Str::EndsWith(fileName, _T(".cbr")))
        return true;
    return IsImageFile(fileName);
}

const TCHAR *ImagesEngine::GetDefaultFileExt() const
{
    if (IsSupportedFile(fileName))
        return _tcsrchr(fileName, '.');
    return NULL;
}

ImagesEngine *ImagesEngine::CreateFromFileName(const TCHAR *fileName)
{
    if (Str::EndsWith(fileName, _T(".cbz")))
        return CreateFromCbzFile(fileName);
    else if (Str::EndsWith(fileName, _T(".cbr")))
        return CreateFromCbrFile(fileName);
    else if (IsImageFile(fileName))
        return CreateFromSingleFile(fileName);
    else
        return NULL;
}

ImagesEngine *ImagesEngine::CreateFromSingleFile(const TCHAR *fileName)
{
    assert(IsImageFile(fileName));
    ImagesEngine *engine = new ImagesEngine();
    if (!engine->LoadSingleFile(fileName)) {
        delete engine;
        return NULL;
    }
    return engine;    
}

ImagesEngine *ImagesEngine::CreateFromCbzFile(const TCHAR *fileName)
{
    ImagesEngine *engine = new ImagesEngine();
    if (!engine->LoadCbzFile(fileName)) {
        delete engine;
        return NULL;
    }
    return engine;
}

ImagesEngine *ImagesEngine::CreateFromCbrFile(const TCHAR *fileName)
{
    ImagesEngine *engine = new ImagesEngine();
    if (!engine->LoadCbrFile(fileName)) {
        delete engine;
        return NULL;
    }
    return engine;
}
