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

class ImagesPage {
public:
    const TCHAR *       fileName; // for sorting image files
    Gdiplus::Bitmap *   bmp;

    ImagesPage(const TCHAR *fileName, Gdiplus::Bitmap *bmp) : bmp(bmp) {
        this->fileName = str::Dup(fileName);
    }
    ~ImagesPage() {
        free((void *)fileName);
        delete bmp;
    }

    static int cmpPageByName(const void *o1, const void *o2) {
        ImagesPage *p1 = *(ImagesPage **)o1;
        ImagesPage *p2 = *(ImagesPage **)o2;
        return str::CmpNatural(p1->fileName, p2->fileName);
    }
};

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

// for converting between big- and little-endian values
#define SWAPWORD(x)    MAKEWORD(HIBYTE(x), LOBYTE(x))
#define SWAPLONG(x)    MAKELONG(SWAPWORD(HIWORD(x)), SWAPWORD(LOWORD(x)))

#define PNG_SIGNATURE "\x89PNG\x0D\x0A\x1A\x0A"

struct PngImageHeader {
    // width and height are big-endian values, use SWAPLONG to convert
    DWORD width;
    DWORD height;
    byte bitDepth;
    byte colorType;
    byte compression;
    byte filter;
    byte interlace;
};

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
    else if (str::StartsWith(data, PNG_SIGNATURE)) {
        if (len >= 16 + sizeof(PngImageHeader) && str::StartsWith(data + 12, "IHDR")) {
            PngImageHeader *hdr = (PngImageHeader *)data + 16;
            result = SizeI(SWAPLONG(hdr->width), SWAPLONG(hdr->height));
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
        Bitmap *bmp = BitmapFromData(data, len);
        if (bmp)
            result = SizeI(bmp->GetWidth(), bmp->GetHeight());
        delete bmp;
    }

    return result;
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

    unsigned int readBytes = unzReadCurrentFile(uf, bmpData, len);
    if (readBytes != len) {
        free(bmpData);
        bmpData = NULL;
    }

Exit:
    unzCloseCurrentFile(uf); // ignoring error code
    return bmpData;
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
    fileName = str::Dup(file);
    fileExt = _T(".cbr");
    assert(str::EndsWithI(fileName, fileExt));

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
        pages.Append(found[i]->bmp);
        mediaboxes.Append(RectD(0, 0, pages[i]->GetWidth(),  pages[i]->GetHeight()));
        found[i]->bmp = NULL;
    }

    DeleteVecMembers(found);
    return true;
}

bool ImageEngine::LoadSingleFile(const TCHAR *file)
{
    assert(IsSupportedFile(file));

    size_t len = 0;
    ScopedMem<char> bmpData(File::ReadAll(file, &len));
    if (!bmpData)
        return false;

    pages.Append(BitmapFromData(bmpData, len));

    fileName = str::Dup(file);
    fileExt = Path::GetExt(fileName);
    assert(fileExt && *fileExt == '.');

    return pages[0] != NULL;
}

struct CbzFileAccess {
    zlib_filefunc64_def ffunc;
    unzFile uf;
};

bool CbxEngine::LoadCbzFile(const TCHAR *file)
{
    if (!file)
        return false;
    fileName = str::Dup(file);
    fileExt = _T(".cbz");
    assert(str::EndsWithI(fileName, fileExt));

    CbzFileAccess *fa = new CbzFileAccess;
    libData = fa;

    // only extract all image filenames for now
    fill_win32_filefunc64(&fa->ffunc);
    fa->uf = unzOpen2_64(fileName, &fa->ffunc);
    if (!fa->uf)
        return false;

    unz_global_info64 ginfo;
    int err = unzGetGlobalInfo64(fa->uf, &ginfo);
    if (err != UNZ_OK)
        return false;
    unzGoToFirstFile(fa->uf);

    for (int n = 0; n < ginfo.number_entry; n++) {
        char fileName[MAX_PATH];
        int err = unzGetCurrentFileInfo64(fa->uf, NULL, fileName, dimof(fileName), NULL, 0, NULL, 0);
        if (err == UNZ_OK) {
            ScopedMem<TCHAR> fileName2(str::conv::FromUtf8(fileName));
            if (ImageEngine::IsSupportedFile(fileName2) &&
                // OS X occasionally leaves metadata with image extensions
                !str::StartsWith(Path::GetBaseName(fileName2), _T("."))) {
                pageFileNames.Append(fileName2.StealData());
            }
        }
        err = unzGoToNextFile(fa->uf);
        if (err != UNZ_OK)
            break;
    }

    // TODO: any meta-information available?

    if (pageFileNames.Count() == 0)
        return false;
    pageFileNames.Sort();

    pages.MakeSpaceAt(0, pageFileNames.Count());
    mediaboxes.MakeSpaceAt(0, pageFileNames.Count());

    return true;
}

ImagesEngine::ImagesEngine() : fileName(NULL), fileExt(NULL), pageCount(0), pages(NULL)
{
}

ImagesEngine::~ImagesEngine()
{
    for (int i = 0; i < pageCount; i++)
        delete pages[i];
    DeleteVecMembers(pages);
    free((void *)fileName);
}

CbxEngine::CbxEngine() : mediaboxes(NULL), libData(NULL)
{
    InitializeCriticalSection(&fileAccess);
}

CbxEngine::~CbxEngine()
{
    if (str::EqI(fileExt, _T(".cbz"))) {
        CbzFileAccess *fa = (CbzFileAccess *)libData;
        unzClose(fa->uf);
        delete fa;
    }

    DeleteCriticalSection(&fileAccess);
}

RectD CbxEngine::PageMediabox(int pageNo)
{
    assert(1 <= pageNo && pageNo <= PageCount());
    if (!mediaboxes[pageNo-1].IsEmpty())
        return mediaboxes[pageNo-1];

    size_t len;
    ScopedMem<char> bmpData;
    if (str::EqI(fileExt, _T(".cbz"))) {
        ScopedCritSec scope(&fileAccess);
        CbzFileAccess *fa = (CbzFileAccess *)libData;
        if (SetCurrentCbzPage(fa->uf, pageFileNames[pageNo - 1]))
            bmpData.Set(LoadCurrentCbzData(fa->uf, len));
    }
    if (bmpData) {
        SizeI size = SizeFromData(bmpData, len);
        mediaboxes[pageNo-1] = RectI(PointI(), size).Convert<double>();
    }
    return mediaboxes[pageNo-1];
}

Bitmap *CbxEngine::LoadImage(int pageNo)
{
    assert(1 <= pageNo && pageNo <= PageCount());
    if (pages[pageNo-1])
        return pages[pageNo-1];

    size_t len;
    ScopedMem<char> bmpData;
    if (str::EqI(fileExt, _T(".cbz"))) {
        ScopedCritSec scope(&fileAccess);
        CbzFileAccess *fa = (CbzFileAccess *)libData;
        if (SetCurrentCbzPage(fa->uf, pageFileNames[pageNo-1]))
            bmpData.Set(LoadCurrentCbzData(fa->uf, len));
    }
    if (bmpData)
        pages[pageNo-1] = BitmapFromData(bmpData, len);

    return pages[pageNo-1];
}

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

    return new RenderedBitmap(hbmp, screen.dx, screen.dy);
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
    g.SetClip(Gdiplus::Rect(screenRect.x, screenRect.y, screenRect.dx, screenRect.dy));

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
    if (str::EndsWithI(fileName, _T(".cbz")))
        ok = engine->LoadCbzFile(fileName);
    else if (str::EndsWithI(fileName, _T(".cbr")))
        ok = engine->LoadCbrFile(fileName);
    if (!ok) {
        delete engine;
        return NULL;
    }
    return engine;
}

RenderedBitmap *LoadRenderedBitmap(const TCHAR *filePath)
{
    if (str::EndsWithI(filePath, _T(".bmp"))) {
        HBITMAP hbmp = (HBITMAP)LoadImage(NULL, filePath, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
        if (!hbmp)
            return NULL;

        BITMAP bmp;
        GetObject(hbmp, sizeof(BITMAP), &bmp);
        return new RenderedBitmap(hbmp, bmp.bmWidth, bmp.bmHeight);
    }

    size_t len;
    ScopedMem<char> data(File::ReadAll(filePath, &len));
    if (!data)
        return NULL;
    Bitmap *bmp = BitmapFromData(data, len);
    if (!bmp)
        return NULL;

    HBITMAP hbmp;
    RenderedBitmap *rendered = NULL;
    if (bmp->GetHBITMAP(Color(0xFF, 0xFF, 0xFF), &hbmp) == Ok)
        rendered = new RenderedBitmap(hbmp, bmp->GetWidth(), bmp->GetHeight());
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
    ScopedMem<unsigned char> bmpData(bmp->Serialize(&bmpDataLen));
    if (!bmpData)
        return false;

    const TCHAR *fileExt = Path::GetExt(filePath);
    if (str::EqI(fileExt, _T(".bmp")))
        return File::WriteAll(filePath, bmpData.Get(), bmpDataLen);

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

    ScopedMem<TCHAR> filePathW(str::conv::ToWStr(filePath));
    Status status = gbmp->Save(filePathW, &encClsid);
    delete gbmp;

    return status == Ok;
}
