/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "CbxEngine.h"
#include "StrUtil.h"
#include "FileUtil.h"

// mini(un)zip
#include <ioapi.h>
#include <iowin32.h>
#include <unzip.h>

using namespace Gdiplus;

// HGLOBAL must be allocated with GlobalAlloc(GMEM_MOVEABLE, ...)
static Bitmap *BitmapFromHGlobal(HGLOBAL mem)
{
    IStream *stream = NULL;
    Bitmap *bmp = NULL;

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

static ComicBookPage *LoadCurrentComicBookPage(unzFile& uf)
{
    char fileName[MAX_PATH];
    unz_file_info64 finfo;
    ComicBookPage *page = NULL;
    HGLOBAL bmpData = NULL;
    unsigned int readBytes;

    int err = unzGetCurrentFileInfo64(uf, &finfo, fileName, dimof(fileName), NULL, 0, NULL, 0);
    if (err != UNZ_OK)
        return NULL;

    if (!Str::EndsWithI(fileName, ".png") &&
        !Str::EndsWithI(fileName, ".jpg") &&
        !Str::EndsWithI(fileName, ".jpeg")) {
        return NULL;
    }

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

    page = new ComicBookPage(bmpData, bmp);
    bmpData = NULL;

Exit:
    unzCloseCurrentFile(uf); // ignoring error code
    GlobalFree(bmpData);
    return page;
}

CbxEngine::CbxEngine(const TCHAR *fileName) : _fileName(Str::Dup(fileName))
{
	if (!_fileName)
		return;
	
    zlib_filefunc64_def ffunc;
    fill_win32_filefunc64(&ffunc);
    unzFile uf = unzOpen2_64(_fileName, &ffunc);
    if (!uf)
        return;

    unz_global_info64 ginfo;
    int err = unzGetGlobalInfo64(uf, &ginfo);
    if (err != UNZ_OK) {
        unzClose(uf);
        return;
    }
    unzGoToFirstFile(uf);

    // extract all contained files one by one

    // TODO: maybe lazy loading would be beneficial (but at least I would
    // need to parse image headers to extract width/height information)
    for (int n = 0; n < ginfo.number_entry; n++) {
        ComicBookPage *page = LoadCurrentComicBookPage(uf);
        if (page)
            _pages.Append(page);
        err = unzGoToNextFile(uf);
        if (err != UNZ_OK)
            break;
    }

    unzClose(uf);

    // TODO: any meta-information available?
}

CbxEngine::~CbxEngine()
{
	DeleteVecMembers(_pages);
    free((void *)_fileName);
}

RenderedBitmap *CbxEngine::RenderBitmap(int pageNo, float zoom, int rotation, RectD *pageRect, RenderTarget target, bool useGdi)
{
    RectD pageRc = pageRect ? *pageRect : PageMediabox(pageNo);
    RectI screen = Transform(pageRc, pageNo, zoom, rotation).Round();
    screen.Offset(-screen.x, -screen.y);

    HDC hDC = GetDC(NULL);
    HDC hDCMem = CreateCompatibleDC(hDC);
    HBITMAP hbmp = CreateCompatibleBitmap(hDC, screen.dx, screen.dy);
    DeleteObject(SelectObject(hDCMem, hbmp));

    bool success = RenderPage(hDCMem, pageNo, screen, zoom, rotation, pageRect, target);
    DeleteDC(hDCMem);
    ReleaseDC(NULL, hDC);
    if (!success) {
        DeleteObject(hbmp);
        return NULL;
    }

    return new RenderedBitmap(hbmp, screen.dx, screen.dy);
}

bool CbxEngine::RenderPage(HDC hDC, int pageNo, RectI screenRect, float zoom, int rotation, RectD *pageRect, RenderTarget target)
{
    RectD pageRc = pageRect ? *pageRect : PageMediabox(pageNo);
    RectI screen = Transform(pageRc, pageNo, zoom, rotation).Round();

    Graphics g(hDC);
    g.SetCompositingQuality(CompositingQualityHighQuality);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPageUnit(UnitPixel);

    Matrix m;
    GetTransform(m, pageNo, zoom, rotation);
    m.Translate((REAL)(screenRect.x - screen.x), (REAL)(screenRect.y - screen.y), MatrixOrderAppend);
    g.SetTransform(&m);

    Status ok = g.DrawImage(_pages[pageNo - 1]->bmp, 0, 0);
    return ok == Ok;
}

void CbxEngine::GetTransform(Matrix& m, int pageNo, float zoom, int rotate)
{
	SizeD size = PageSize(pageNo);

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

PointD CbxEngine::Transform(PointD pt, int pageNo, float zoom, int rotate, bool inverse)
{
    RectD rect = Transform(RectD(pt, SizeD()), pageNo, zoom, rotate, inverse);
    return PointD(rect.x, rect.y);
}

RectD CbxEngine::Transform(RectD rect, int pageNo, float zoom, int rotate, bool inverse)
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

unsigned char *CbxEngine::GetFileData(size_t *cbCount)
{
	return (unsigned char *)File::ReadAll(_fileName, cbCount);
}

CbxEngine *CbxEngine::CreateFromFileName(const TCHAR *fileName)
{
    CbxEngine *engine = new CbxEngine(fileName);
    if (!engine || engine->PageCount() == 0) {
        delete engine;
        return NULL;
    }
    return engine;
}
