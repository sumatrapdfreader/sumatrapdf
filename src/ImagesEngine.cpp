/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "ImagesEngine.h"

#include "BaseEngine.h"
#include "FileUtil.h"
using namespace Gdiplus;
#include "GdiPlusUtil.h"
#include "HtmlPullParser.h"
#include "JsonParser.h"
#include "WinUtil.h"
#include "ZipUtil.h"

#include "../ext/unrar/dll.hpp"

// number of decoded bitmaps to cache for quicker rendering
#define MAX_IMAGE_PAGE_CACHE    10

///// ImagesEngine methods apply to all types of engines handling full-page images /////

struct ImagePage {
    int pageNo;
    Bitmap *bmp;
    bool ownBmp;
    int refs;

    ImagePage(int pageNo, Bitmap *bmp) :
        pageNo(pageNo), bmp(bmp), ownBmp(true), refs(1) { }
};

class ImageElement;

class ImagesEngine : public BaseEngine {
    friend ImageElement;

public:
    ImagesEngine();
    virtual ~ImagesEngine();

    virtual const WCHAR *FileName() const { return fileName; };
    virtual int PageCount() const { return (int)mediaboxes.Count(); }

    virtual RectD PageMediabox(int pageNo);

    virtual RenderedBitmap *RenderBitmap(int pageNo, float zoom, int rotation,
                         RectD *pageRect=NULL, /* if NULL: defaults to the page's mediabox */
                         RenderTarget target=Target_View, AbortCookie **cookie_out=NULL);
    virtual bool RenderPage(HDC hDC, RectI screenRect, int pageNo, float zoom, int rotation,
                         RectD *pageRect=NULL, RenderTarget target=Target_View, AbortCookie **cookie_out=NULL);

    virtual PointD Transform(PointD pt, int pageNo, float zoom, int rotation, bool inverse=false);
    virtual RectD Transform(RectD rect, int pageNo, float zoom, int rotation, bool inverse=false);

    virtual unsigned char *GetFileData(size_t *cbCount);
    virtual bool SaveFileAs(const WCHAR *copyFileName, bool includeUserAnnots=false);
    virtual WCHAR * ExtractPageText(int pageNo, WCHAR *lineSep, RectI **coords_out=NULL,
                                    RenderTarget target=Target_View) { return NULL; }
    virtual bool HasClipOptimizations(int pageNo) { return false; }
    virtual PageLayoutType PreferredLayout() { return Layout_NonContinuous; }
    virtual bool IsImageCollection() const { return true; }

    virtual bool SupportsAnnotation(bool forSaving=false) const { return false; }
    virtual void UpdateUserAnnotations(Vec<PageAnnotation> *list) { }

    virtual Vec<PageElement *> *GetElements(int pageNo);
    virtual PageElement *GetElementAtPos(int pageNo, PointD pt);

    virtual bool BenchLoadPage(int pageNo) {
        ImagePage *page = GetPage(pageNo);
        if (page)
            DropPage(page);
        return page != NULL;
    }

protected:
    WCHAR *fileName;
    ScopedComPtr<IStream> fileStream;

    CRITICAL_SECTION cacheAccess;
    Vec<ImagePage *, MAX_IMAGE_PAGE_CACHE+1> pageCache;
    Vec<RectD> mediaboxes;

    void GetTransform(Matrix& m, int pageNo, float zoom, int rotation);

    virtual Bitmap *LoadBitmap(int pageNo, bool& deleteAfterUse) = 0;
    virtual RectD LoadMediabox(int pageNo) = 0;

    ImagePage *GetPage(int pageNo, bool tryOnly=false);
    void DropPage(ImagePage *page, bool forceRemove=false);
};

ImagesEngine::ImagesEngine() : fileName(NULL)
{
    InitializeCriticalSection(&cacheAccess);
}

ImagesEngine::~ImagesEngine()
{
    EnterCriticalSection(&cacheAccess);
    while (pageCache.Count() > 0) {
        CrashIf(pageCache.Last()->refs != 1);
        DropPage(pageCache.Last(), true);
    }
    LeaveCriticalSection(&cacheAccess);

    free(fileName);

    DeleteCriticalSection(&cacheAccess);
}

RectD ImagesEngine::PageMediabox(int pageNo)
{
    AssertCrash(1 <= pageNo && pageNo <= PageCount());
    if (mediaboxes.At(pageNo - 1).IsEmpty())
        mediaboxes.At(pageNo - 1) = LoadMediabox(pageNo);
    return mediaboxes.At(pageNo - 1);
}

RenderedBitmap *ImagesEngine::RenderBitmap(int pageNo, float zoom, int rotation, RectD *pageRect, RenderTarget target, AbortCookie **cookie_out)
{
    RectD pageRc = pageRect ? *pageRect : PageMediabox(pageNo);
    RectI screen = Transform(pageRc, pageNo, zoom, rotation).Round();
    screen.Offset(-screen.x, -screen.y);

    HDC hDC = GetDC(NULL);
    HDC hDCMem = CreateCompatibleDC(hDC);
    HBITMAP hbmp = CreateCompatibleBitmap(hDC, screen.dx, screen.dy);
    DeleteObject(SelectObject(hDCMem, hbmp));

    bool ok = RenderPage(hDCMem, screen, pageNo, zoom, rotation, pageRect, target, cookie_out);
    DeleteDC(hDCMem);
    ReleaseDC(NULL, hDC);
    if (!ok) {
        DeleteObject(hbmp);
        return NULL;
    }

    return new RenderedBitmap(hbmp, screen.Size());
}

bool ImagesEngine::RenderPage(HDC hDC, RectI screenRect, int pageNo, float zoom, int rotation, RectD *pageRect, RenderTarget target, AbortCookie **cookie_out)
{
    ImagePage *page = GetPage(pageNo);
    if (!page)
        return false;

    RectD pageRc = pageRect ? *pageRect : PageMediabox(pageNo);
    RectI screen = Transform(pageRc, pageNo, zoom, rotation).Round();

    Graphics g(hDC);
    g.SetCompositingQuality(CompositingQualityHighQuality);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPageUnit(UnitPixel);

    Color white(0xFF, 0xFF, 0xFF);
    Rect screenR(screenRect.ToGdipRect());
    g.SetClip(screenR);
    SolidBrush tmpBrush(white);
    g.FillRectangle(&tmpBrush, screenR);

    Matrix m;
    GetTransform(m, pageNo, zoom, rotation);
    m.Translate((REAL)(screenRect.x - screen.x), (REAL)(screenRect.y - screen.y), MatrixOrderAppend);
    g.SetTransform(&m);

    RectI pageRcI = PageMediabox(pageNo).Round();
    ImageAttributes imgAttrs;
    imgAttrs.SetWrapMode(WrapModeTileFlipXY);
    Status ok = g.DrawImage(page->bmp, pageRcI.ToGdipRect(), 0, 0, pageRcI.dx, pageRcI.dy, UnitPixel, &imgAttrs);

    DropPage(page);
    return ok == Ok;
}

void ImagesEngine::GetTransform(Matrix& m, int pageNo, float zoom, int rotation)
{
    GetBaseTransform(m, PageMediabox(pageNo).ToGdipRectF(), zoom, rotation);
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
    rect = RectD::FromXY(pts[0].X, pts[0].Y, pts[1].X, pts[1].Y);
    // try to undo rounding errors caused by a rotation
    // (necessary correction determined by experimentation)
    if (rotation != 0)
        rect.Inflate(-0.01, -0.01);
    return rect;
}

class ImageElement : public PageElement {
    ImagesEngine *engine;
    ImagePage *page;

public:
    explicit ImageElement(ImagesEngine *engine, ImagePage *page) : engine(engine), page(page) { }
    virtual ~ImageElement() { engine->DropPage(page); }

    virtual PageElementType GetType() const { return Element_Image; }
    virtual int GetPageNo() const { return page->pageNo; }
    virtual RectD GetRect() const { return RectD(0, 0, page->bmp->GetWidth(), page->bmp->GetHeight()); }
    virtual WCHAR *GetValue() const { return NULL; }

    virtual RenderedBitmap *GetImage() {
        HBITMAP hbmp;
        if (page->bmp->GetHBITMAP((ARGB)Color::White, &hbmp) != Ok)
            return NULL;
        return new RenderedBitmap(hbmp, SizeI(page->bmp->GetWidth(), page->bmp->GetHeight()));
    }
};

Vec<PageElement *> *ImagesEngine::GetElements(int pageNo)
{
    ImagePage *page = GetPage(pageNo);
    if (!page)
        return NULL;

    Vec<PageElement *> *els = new Vec<PageElement *>();
    els->Append(new ImageElement(this, page));
    return els;
}

PageElement *ImagesEngine::GetElementAtPos(int pageNo, PointD pt)
{
    if (!PageMediabox(pageNo).Contains(pt))
        return NULL;
    ImagePage *page = GetPage(pageNo);
    if (!page)
        return NULL;
    return new ImageElement(this, page);
}

unsigned char *ImagesEngine::GetFileData(size_t *cbCount)
{
    if (fileStream) {
        void *data = GetDataFromStream(fileStream, cbCount);
        if (data)
            return (unsigned char *)data;
    }
    if (fileName)
        return (unsigned char *)file::ReadAll(fileName, cbCount);
    return NULL;
}

bool ImagesEngine::SaveFileAs(const WCHAR *copyFileName, bool includeUserAnnots)
{
    if (fileName) {
        BOOL ok = CopyFile(fileName, copyFileName, FALSE);
        if (ok)
            return true;
    }
    size_t dataLen;
    ScopedMem<unsigned char> data(GetFileData(&dataLen));
    if (data)
        return file::WriteAll(copyFileName, data.Get(), dataLen);
    return false;
}

ImagePage *ImagesEngine::GetPage(int pageNo, bool tryOnly)
{
    ScopedCritSec scope(&cacheAccess);

    ImagePage *result = NULL;

    for (size_t i = 0; i < pageCache.Count(); i++) {
        if (pageCache.At(i)->pageNo == pageNo) {
            result = pageCache.At(i);
            break;
        }
    }
    if (!result && tryOnly)
        return NULL;
    if (!result) {
        // TODO: drop most memory intensive pages first
        // (i.e. formats which aren't IsGdiPlusNativeFormat)?
        if (pageCache.Count() >= MAX_IMAGE_PAGE_CACHE) {
            CrashIf(pageCache.Count() != MAX_IMAGE_PAGE_CACHE);
            DropPage(pageCache.Last(), true);
        }
        result = new ImagePage(pageNo, NULL);
        result->bmp = LoadBitmap(pageNo, result->ownBmp);
        pageCache.InsertAt(0, result);
    }
    else if (result != pageCache.At(0)) {
        // keep the list Most Recently Used first
        pageCache.Remove(result);
        pageCache.InsertAt(0, result);
    }
    // return NULL if a page failed to load
    if (result && !result->bmp)
        result = NULL;

    if (result)
        result->refs++;
    return result;
}

void ImagesEngine::DropPage(ImagePage *page, bool forceRemove)
{
    ScopedCritSec scope(&cacheAccess);
    page->refs--;

    if (0 == page->refs || forceRemove)
        pageCache.Remove(page);

    if (0 == page->refs) {
        if (page->ownBmp)
            delete page->bmp;
        delete page;
    }
}

///// ImageEngine handles a single image file /////

class ImageEngineImpl : public ImagesEngine {
public:
    ImageEngineImpl() : fileExt(NULL), image(NULL) { }
    virtual ~ImageEngineImpl() { delete image; }

    virtual BaseEngine *Clone();

    virtual WCHAR *GetProperty(DocumentProperty prop);

    virtual float GetFileDPI() const { return image->GetHorizontalResolution(); }
    virtual const WCHAR *GetDefaultFileExt() const { return fileExt; }

    static BaseEngine *CreateFromFile(const WCHAR *fileName);
    static BaseEngine *CreateFromStream(IStream *stream);

protected:
    const WCHAR *fileExt;
    Bitmap *image;

    bool LoadSingleFile(const WCHAR *fileName);
    bool LoadFromStream(IStream *stream);
    bool FinishLoading();

    virtual Bitmap *LoadBitmap(int pageNo, bool& deleteAfterUse);
    virtual RectD LoadMediabox(int pageNo);
};

BaseEngine *ImageEngineImpl::Clone()
{
    Bitmap *bmp = image->Clone(0, 0, image->GetWidth(), image->GetHeight(), PixelFormat32bppARGB);
    if (!bmp)
        return NULL;

    ImageEngineImpl *clone = new ImageEngineImpl();
    clone->fileName = str::Dup(fileName);
    clone->fileExt = fileExt;
    if (fileStream)
        fileStream->Clone(&clone->fileStream);
    clone->image = bmp;
    clone->FinishLoading();

    return clone;
}

bool ImageEngineImpl::LoadSingleFile(const WCHAR *file)
{
    if (!file)
        return false;
    fileName = str::Dup(file);

    size_t len;
    ScopedMem<char> data(file::ReadAll(file, &len));
    fileExt = GfxFileExtFromData(data, len);

    image = BitmapFromData(data, len);
    return FinishLoading();
}

bool ImageEngineImpl::LoadFromStream(IStream *stream)
{
    if (!stream)
        return false;
    fileStream = stream;
    fileStream->AddRef();

    char header[18];
    if (ReadDataFromStream(stream, header, sizeof(header)))
        fileExt = GfxFileExtFromData(header, sizeof(header));

    if (fileExt && !IsGdiPlusNativeFormat(header, sizeof(header))) {
        size_t len;
        ScopedMem<char> data((char *)GetDataFromStream(stream, &len));
        image = BitmapFromData(data, len);
    }
    else {
        image = Bitmap::FromStream(stream);
        if (!fileExt)
            fileExt = L".png";
    }

    return FinishLoading();
}

bool ImageEngineImpl::FinishLoading()
{
    if (!image || image->GetLastStatus() != Ok)
        return false;

    mediaboxes.Append(RectD(0, 0, image->GetWidth(), image->GetHeight()));
    AssertCrash(mediaboxes.Count() == 1);

    // extract all frames from multi-page TIFFs and animated GIFs
    if (str::Eq(fileExt, L".tif") || str::Eq(fileExt, L".gif")) {
        const GUID *frameDimension = str::Eq(fileExt, L".tif") ? &FrameDimensionPage : &FrameDimensionTime;
        mediaboxes.AppendBlanks(image->GetFrameCount(frameDimension) - 1);
    }

    CrashIf(!fileExt);
    return fileExt != NULL;
}

// cf. http://www.universalthread.com/ViewPageArticle.aspx?ID=831
#ifndef PropertyTagXPTitle
#define PropertyTagXPTitle      0x9c9b
#define PropertyTagXPComment    0x9c9c
#define PropertyTagXPAuthor     0x9c9d
#define PropertyTagXPKeywords   0x9c9e
#define PropertyTagXPSubject    0x9c9f
#endif

static WCHAR *GetImageProperty(Bitmap *bmp, PROPID id, PROPID altId=0)
{
    WCHAR *value = NULL;
    UINT size = bmp->GetPropertyItemSize(id);
    PropertyItem *item = (PropertyItem *)malloc(size);
    Status ok = item ? bmp->GetPropertyItem(id, size, item) : OutOfMemory;
    if (Ok != ok)
        /* property didn't exist */;
    else if (PropertyTagTypeASCII == item->type)
        value = str::conv::FromAnsi((char *)item->value);
    else if (PropertyTagTypeByte == item->type && item->length > 0 &&
        0 == (item->length % 2) && !((WCHAR *)item->value)[item->length / 2 - 1]) {
        value = str::Dup((WCHAR *)item->value);
    }
    free(item);
    if (!value && altId)
        return GetImageProperty(bmp, altId);
    return value;
}

WCHAR *ImageEngineImpl::GetProperty(DocumentProperty prop)
{
    switch (prop) {
    case Prop_Title:
        return GetImageProperty(image, PropertyTagImageDescription, PropertyTagXPTitle);
    case Prop_Subject:
        return GetImageProperty(image, PropertyTagXPSubject);
    case Prop_Author:
        return GetImageProperty(image, PropertyTagArtist, PropertyTagXPAuthor);
    case Prop_Copyright:
        return GetImageProperty(image, PropertyTagCopyright);
    case Prop_CreationDate:
        return GetImageProperty(image, PropertyTagDateTime, PropertyTagExifDTDigitized);
    case Prop_CreatorApp:
        return GetImageProperty(image, PropertyTagSoftwareUsed);
    default:
        return NULL;
    }
}

Bitmap *ImageEngineImpl::LoadBitmap(int pageNo, bool& deleteAfterUse)
{
    if (1 == pageNo) {
        deleteAfterUse = false;
        return image;
    }

    // extract other frames from multi-page TIFFs and animated GIFs
    CrashIf(!str::Eq(fileExt, L".tif") && !str::Eq(fileExt, L".gif"));
    const GUID *frameDimension = str::Eq(fileExt, L".tif") ? &FrameDimensionPage : &FrameDimensionTime;
    UINT frameCount = image->GetFrameCount(frameDimension);
    CrashIf((unsigned int)pageNo > frameCount);
    Bitmap *frame = image->Clone(0, 0, image->GetWidth(), image->GetHeight(), PixelFormat32bppARGB);
    if (!frame)
        return NULL;
    Status ok = frame->SelectActiveFrame(frameDimension, pageNo - 1);
    if (ok != Ok) {
        delete frame;
        return NULL;
    }
    deleteAfterUse = true;
    return frame;
}

RectD ImageEngineImpl::LoadMediabox(int pageNo)
{
    if (1 == pageNo)
        return RectD(0, 0, image->GetWidth(), image->GetHeight());

    // fill the cache to prevent the first few frames from being unpacked twice
    ImagePage *page = GetPage(pageNo, MAX_IMAGE_PAGE_CACHE == pageCache.Count());
    if (page) {
        RectD mbox(0, 0, page->bmp->GetWidth(), page->bmp->GetHeight());
        DropPage(page);
        return mbox;
    }

    CrashIf(!str::Eq(fileExt, L".tif") && !str::Eq(fileExt, L".gif"));
    RectD mbox = RectD(0, 0, image->GetWidth(), image->GetHeight());
    Bitmap *frame = image->Clone(0, 0, image->GetWidth(), image->GetHeight(), PixelFormat32bppARGB);
    if (!frame)
        return mbox;
    const GUID *frameDimension = str::Eq(fileExt, L".tif") ? &FrameDimensionPage : &FrameDimensionTime;
    Status ok = frame->SelectActiveFrame(frameDimension, pageNo - 1);
    if (Ok == ok)
        mbox = RectD(0, 0, frame->GetWidth(), frame->GetHeight());
    delete frame;
    return mbox;
}

BaseEngine *ImageEngineImpl::CreateFromFile(const WCHAR *fileName)
{
    ImageEngineImpl *engine = new ImageEngineImpl();
    if (!engine->LoadSingleFile(fileName)) {
        delete engine;
        return NULL;
    }
    return engine;
}

BaseEngine *ImageEngineImpl::CreateFromStream(IStream *stream)
{
    ImageEngineImpl *engine = new ImageEngineImpl();
    if (!engine->LoadFromStream(stream)) {
        delete engine;
        return NULL;
    }
    return engine;
}

namespace ImageEngine {

bool IsSupportedFile(const WCHAR *fileName, bool sniff)
{
    if (sniff) {
        char header[32] = { 0 };
        file::ReadN(fileName, header, sizeof(header));
        fileName = GfxFileExtFromData(header, sizeof(header));
    }

    return str::EndsWithI(fileName, L".png") ||
           str::EndsWithI(fileName, L".jpg") || str::EndsWithI(fileName, L".jpeg") ||
           str::EndsWithI(fileName, L".gif") ||
           str::EndsWithI(fileName, L".tif") || str::EndsWithI(fileName, L".tiff") ||
           str::EndsWithI(fileName, L".bmp") ||
           str::EndsWithI(fileName, L".tga") ||
           str::EndsWithI(fileName, L".jxr") || str::EndsWithI(fileName, L".hdp") ||
                                                str::EndsWithI(fileName, L".wdp") ||
           str::EndsWithI(fileName, L".webp")||
           str::EndsWithI(fileName, L".jp2");
}

BaseEngine *CreateFromFile(const WCHAR *fileName)
{
    AssertCrash(ImageEngine::IsSupportedFile(fileName) || ImageEngine::IsSupportedFile(fileName, true));
    return ImageEngineImpl::CreateFromFile(fileName);
}

BaseEngine *CreateFromStream(IStream *stream)
{
    return ImageEngineImpl::CreateFromStream(stream);
}

}

///// ImageDirEngine handles a directory full of image files /////

class ImageDirEngineImpl : public ImagesEngine {
public:
    ImageDirEngineImpl() : fileDPI(96.0f) { }

    virtual BaseEngine *Clone() {
        return fileName ? CreateFromFile(fileName) : NULL;
    }

    virtual unsigned char *GetFileData(size_t *cbCount) { return NULL; }
    virtual bool SaveFileAs(const WCHAR *copyFileName, bool includeUserAnnots=false);

    virtual WCHAR *GetProperty(DocumentProperty prop) { return NULL; }

    // TODO: is there a better place to expose pageFileNames than through page labels?
    virtual bool HasPageLabels() const { return true; }
    virtual WCHAR *GetPageLabel(int pageNo) const;
    virtual int GetPageByLabel(const WCHAR *label) const;

    virtual bool HasTocTree() const { return true; }
    virtual DocTocItem *GetTocTree();

    // TODO: better handle the case where images have different resolutions
    virtual float GetFileDPI() const { return fileDPI; }
    virtual const WCHAR *GetDefaultFileExt() const { return L""; }

    static BaseEngine *CreateFromFile(const WCHAR *fileName);

protected:
    bool LoadImageDir(const WCHAR *dirName);

    virtual Bitmap *LoadBitmap(int pageNo, bool& deleteAfterUse);
    virtual RectD LoadMediabox(int pageNo);

    WStrVec pageFileNames;
    float fileDPI;
};

bool ImageDirEngineImpl::LoadImageDir(const WCHAR *dirName)
{
    fileName = str::Dup(dirName);

    ScopedMem<WCHAR> pattern(path::Join(dirName, L"*"));

    WIN32_FIND_DATA fdata;
    HANDLE hfind = FindFirstFile(pattern, &fdata);
    if (INVALID_HANDLE_VALUE == hfind)
        return false;

    do {
        if (!(fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            if (ImageEngine::IsSupportedFile(fdata.cFileName))
                pageFileNames.Append(path::Join(dirName, fdata.cFileName));
        }
    } while (FindNextFile(hfind, &fdata));
    FindClose(hfind);

    if (pageFileNames.Count() == 0)
        return false;
    pageFileNames.SortNatural();

    mediaboxes.AppendBlanks(pageFileNames.Count());

    ImagePage *page = GetPage(1);
    if (page) {
        fileDPI = page->bmp->GetHorizontalResolution();
        DropPage(page);
    }

    return true;
}

WCHAR *ImageDirEngineImpl::GetPageLabel(int pageNo) const
{
    if (pageNo < 1 || PageCount() < pageNo)
        return BaseEngine::GetPageLabel(pageNo);

    const WCHAR *fileName = path::GetBaseName(pageFileNames.At(pageNo - 1));
    return str::DupN(fileName, path::GetExt(fileName) - fileName);
}

int ImageDirEngineImpl::GetPageByLabel(const WCHAR *label) const
{
    for (size_t i = 0; i < pageFileNames.Count(); i++) {
        const WCHAR *fileName = path::GetBaseName(pageFileNames.At(i));
        const WCHAR *fileExt = path::GetExt(fileName);
        if (str::StartsWithI(fileName, label) &&
            (fileName + str::Len(label) == fileExt || fileName[str::Len(label)] == '\0'))
            return (int)i + 1;
    }

    return BaseEngine::GetPageByLabel(label);
}

class ImageDirTocItem : public DocTocItem {
public:
    ImageDirTocItem(WCHAR *title, int pageNo) : DocTocItem(title, pageNo) { }

    virtual PageDestination *GetLink() { return NULL; }
};

DocTocItem *ImageDirEngineImpl::GetTocTree()
{
    DocTocItem *root = new ImageDirTocItem(GetPageLabel(1), 1);
    root->id = 1;
    for (int i = 2; i <= PageCount(); i++) {
        DocTocItem *item = new ImageDirTocItem(GetPageLabel(i), i);
        item->id = i;
        root->AddSibling(item);
    }
    return root;
}

bool ImageDirEngineImpl::SaveFileAs(const WCHAR *copyFileName, bool includeUserAnnots)
{
    // only copy the files if the target directory doesn't exist yet
    if (!CreateDirectory(copyFileName, NULL))
        return false;
    bool ok = true;
    for (size_t i = 0; i < pageFileNames.Count(); i++) {
        const WCHAR *filePathOld = pageFileNames.At(i);
        ScopedMem<WCHAR> filePathNew(path::Join(copyFileName, path::GetBaseName(filePathOld)));
        ok = ok && CopyFile(filePathOld, filePathNew, TRUE);
    }
    return ok;
}

Bitmap *ImageDirEngineImpl::LoadBitmap(int pageNo, bool& deleteAfterUse)
{
    size_t len;
    ScopedMem<char> bmpData(file::ReadAll(pageFileNames.At(pageNo - 1), &len));
    if (bmpData) {
        deleteAfterUse = true;
        return BitmapFromData(bmpData, len);
    }
    return NULL;
}

RectD ImageDirEngineImpl::LoadMediabox(int pageNo)
{
    size_t len;
    ScopedMem<char> bmpData(file::ReadAll(pageFileNames.At(pageNo - 1), &len));
    if (bmpData) {
        Size size = BitmapSizeFromData(bmpData, len);
        return RectD(0, 0, size.Width, size.Height);
    }
    return RectD();
}

BaseEngine *ImageDirEngineImpl::CreateFromFile(const WCHAR *fileName)
{
    AssertCrash(dir::Exists(fileName));
    ImageDirEngineImpl *engine = new ImageDirEngineImpl();
    if (!engine->LoadImageDir(fileName)) {
        delete engine;
        return NULL;
    }
    return engine;
}

namespace ImageDirEngine {

bool IsSupportedFile(const WCHAR *fileName, bool sniff)
{
    // whether it actually contains images will be checked in LoadImageDir
    return dir::Exists(fileName);
}

BaseEngine *CreateFromFile(const WCHAR *fileName)
{
    return ImageDirEngineImpl::CreateFromFile(fileName);
}

}

///// CbxEngine handles comic book files (either .cbz or .cbr) /////

// ComicEngine handles the common parts of CbzEngine and CbrEngine
class ComicEngine : public ImagesEngine, public json::ValueVisitor {
public:
    virtual WCHAR *GetProperty(DocumentProperty prop);

    // not using the resolution of the contained images seems to be
    // expected, cf. http://forums.fofou.org/sumatrapdf/topic?id=3183827
    // TODO: return win::GetHwndDpi(HWND_DESKTOP) instead?
    virtual float GetFileDPI() const { return 96.0f; }

    // json::ValueVisitor
    virtual bool Visit(const char *path, const char *value, json::DataType type);

protected:
    void ParseComicInfoXml(const char *xmlData);

    // extracted metadata
    ScopedMem<WCHAR> propTitle;
    WStrVec propAuthors;
    ScopedMem<WCHAR> propDate;
    ScopedMem<WCHAR> propModDate;
    ScopedMem<WCHAR> propCreator;
    ScopedMem<WCHAR> propSummary;
    // temporary state needed for extracting metadata
    ScopedMem<WCHAR> propAuthorTmp;
};

static char *GetTextContent(HtmlPullParser& parser)
{
    HtmlToken *tok = parser.Next();
    if (!tok || !tok->IsText())
        return NULL;
    return ResolveHtmlEntities(tok->s, tok->sLen);
}

// extract ComicInfo.xml metadata
// cf. http://comicrack.cyolito.com/downloads/comicrack/ComicRack/Support-Files/ComicInfoSchema.zip/
void ComicEngine::ParseComicInfoXml(const char *xmlData)
{
    // TODO: convert UTF-16 data and skip UTF-8 BOM
    HtmlPullParser parser(xmlData, str::Len(xmlData));
    HtmlToken *tok;
    while ((tok = parser.Next()) != NULL && !tok->IsError()) {
        if (!tok->IsStartTag())
            continue;
        if (tok->NameIs("Title")) {
            ScopedMem<char> value(GetTextContent(parser));
            if (value)
                Visit("/ComicBookInfo/1.0/title", value, json::Type_String);
        }
        else if (tok->NameIs("Year")) {
            ScopedMem<char> value(GetTextContent(parser));
            if (value)
                Visit("/ComicBookInfo/1.0/publicationYear", value, json::Type_Number);
        }
        else if (tok->NameIs("Month")) {
            ScopedMem<char> value(GetTextContent(parser));
            if (value)
                Visit("/ComicBookInfo/1.0/publicationMonth", value, json::Type_Number);
        }
        else if (tok->NameIs("Summary")) {
            ScopedMem<char> value(GetTextContent(parser));
            if (value)
                Visit("/X-summary", value, json::Type_String);
        }
        else if (tok->NameIs("Writer")) {
            ScopedMem<char> value(GetTextContent(parser));
            if (value) {
                Visit("/ComicBookInfo/1.0/credits[0]/person", value, json::Type_String);
                Visit("/ComicBookInfo/1.0/credits[0]/primary", "true", json::Type_Bool);
            }
        }
        else if (tok->NameIs("Penciller")) {
            ScopedMem<char> value(GetTextContent(parser));
            if (value) {
                Visit("/ComicBookInfo/1.0/credits[1]/person", value, json::Type_String);
                Visit("/ComicBookInfo/1.0/credits[1]/primary", "true", json::Type_Bool);
            }
        }
    }
}

// extract ComicBookInfo metadata
// cf. http://code.google.com/p/comicbookinfo/
bool ComicEngine::Visit(const char *path, const char *value, json::DataType type)
{
    if (json::Type_String == type && str::Eq(path, "/ComicBookInfo/1.0/title"))
        propTitle.Set(str::conv::FromUtf8(value));
    else if (json::Type_Number == type && str::Eq(path, "/ComicBookInfo/1.0/publicationYear"))
        propDate.Set(str::Format(L"%s/%d", propDate ? propDate : L"", atoi(value)));
    else if (json::Type_Number == type && str::Eq(path, "/ComicBookInfo/1.0/publicationMonth"))
        propDate.Set(str::Format(L"%d%s", atoi(value), propDate ? propDate : L""));
    else if (json::Type_String == type && str::Eq(path, "/appID"))
        propCreator.Set(str::conv::FromUtf8(value));
    else if (json::Type_String == type && str::Eq(path, "/lastModified"))
        propModDate.Set(str::conv::FromUtf8(value));
    else if (json::Type_String == type && str::Eq(path, "/X-summary"))
        propSummary.Set(str::conv::FromUtf8(value));
    else if (str::StartsWith(path, "/ComicBookInfo/1.0/credits[")) {
        int idx = -1;
        const char *prop = str::Parse(path, "/ComicBookInfo/1.0/credits[%d]/", &idx);
        if (prop) {
            if (json::Type_String == type && str::Eq(prop, "person"))
                propAuthorTmp.Set(str::conv::FromUtf8(value));
            else if (json::Type_Bool == type && str::Eq(prop, "primary") &&
                propAuthorTmp && !propAuthors.Contains(propAuthorTmp)) {
                propAuthors.Append(propAuthorTmp.StealData());
            }
        }
        return true;
    }
    // stop parsing once we have all desired information
    return !propTitle || propAuthors.Count() == 0 || !propCreator ||
           !propDate || str::FindChar(propDate, '/') <= propDate;
}

WCHAR *ComicEngine::GetProperty(DocumentProperty prop)
{
    switch (prop) {
    case Prop_Title:
        return str::Dup(propTitle);
    case Prop_Author:
        return propAuthors.Count() ? propAuthors.Join(L", ") : NULL;
    case Prop_CreationDate:
        return str::Dup(propDate);
    case Prop_ModificationDate:
        return str::Dup(propModDate);
    case Prop_CreatorApp:
        return str::Dup(propCreator);
    // TODO: replace with Prop_Summary
    case Prop_Subject:
        return str::Dup(propSummary);
    default:
        return NULL;
    }
}

class CbzEngineImpl : public ComicEngine {
public:
    CbzEngineImpl() : cbzFile(NULL) { }
    virtual ~CbzEngineImpl() { delete cbzFile; }

    virtual BaseEngine *Clone() {
        if (fileStream) {
            ScopedComPtr<IStream> stm;
            HRESULT res = fileStream->Clone(&stm);
            if (SUCCEEDED(res))
                return CreateFromStream(stm);
        }
        if (fileName)
            return CreateFromFile(fileName);
        return NULL;
    }

    virtual const WCHAR *GetDefaultFileExt() const { return L".cbz"; }

    static BaseEngine *CreateFromFile(const WCHAR *fileName);
    static BaseEngine *CreateFromStream(IStream *stream);

protected:
    ZipFile *cbzFile;
    Vec<size_t> fileIdxs;

    bool LoadFromFile(const WCHAR *fileName);
    bool LoadFromStream(IStream *stream);
    bool FinishLoading();

    char *GetImageData(int pageNo, size_t& len);

    virtual Bitmap *LoadBitmap(int pageNo, bool& deleteAfterUse);
    virtual RectD LoadMediabox(int pageNo);
};

bool CbzEngineImpl::LoadFromFile(const WCHAR *file)
{
    if (!file)
        return false;
    fileName = str::Dup(file);

    cbzFile = new ZipFile(fileName);

    return FinishLoading();
}

bool CbzEngineImpl::LoadFromStream(IStream *stream)
{
    if (!stream)
        return false;
    fileStream = stream;
    fileStream->AddRef();

    cbzFile = new ZipFile(stream);

    return FinishLoading();
}

static int cmpAscii(const void *a, const void *b)
{
    return wcscmp(*(const WCHAR **)a, *(const WCHAR **)b);
}

bool CbzEngineImpl::FinishLoading()
{
    Vec<const WCHAR *> allFileNames;
    Vec<const WCHAR *> pageFileNames;

    for (size_t idx = 0; idx < cbzFile->GetFileCount(); idx++) {
        const WCHAR *fileName = cbzFile->GetFileName(idx);
        // bail, if we accidentally try to load an XPS file
        if (fileName && str::StartsWith(fileName, L"_rels/.rels"))
            return false;
        if (fileName && ImageEngine::IsSupportedFile(fileName) &&
            // OS X occasionally leaves metadata with image extensions
            !str::StartsWith(path::GetBaseName(fileName), L".")) {
            allFileNames.Append(fileName);
            pageFileNames.Append(fileName);
        }
        else {
            allFileNames.Append(NULL);
        }
    }
    AssertCrash(allFileNames.Count() == cbzFile->GetFileCount());

    ScopedMem<char> metadata(cbzFile->GetFileDataByName(L"ComicInfo.xml"));
    if (metadata)
        ParseComicInfoXml(metadata);
    metadata.Set(cbzFile->GetComment());
    if (metadata)
        json::Parse(metadata, this);

    pageFileNames.Sort(cmpAscii);
    for (const WCHAR **fn = pageFileNames.IterStart(); fn; fn = pageFileNames.IterNext()) {
        fileIdxs.Append(allFileNames.Find(*fn));
    }
    AssertCrash(pageFileNames.Count() == fileIdxs.Count());
    if (fileIdxs.Count() == 0)
        return false;

    mediaboxes.AppendBlanks(fileIdxs.Count());

    return true;
}

Bitmap *CbzEngineImpl::LoadBitmap(int pageNo, bool& deleteAfterUse)
{
    size_t len;
    ScopedMem<char> bmpData(GetImageData(pageNo, len));
    if (bmpData) {
        deleteAfterUse = true;
        return BitmapFromData(bmpData, len);
    }
    return NULL;
}

RectD CbzEngineImpl::LoadMediabox(int pageNo)
{
    // fill the cache to prevent the first few images from being unpacked twice
    ImagePage *page = GetPage(pageNo, MAX_IMAGE_PAGE_CACHE == pageCache.Count());
    if (page) {
        RectD mbox(0, 0, page->bmp->GetWidth(), page->bmp->GetHeight());
        DropPage(page);
        return mbox;
    }

    size_t len;
    ScopedMem<char> bmpData(GetImageData(pageNo, len));
    if (bmpData) {
        Size size = BitmapSizeFromData(bmpData, len);
        return RectD(0, 0, size.Width, size.Height);
    }
    return RectD();
}

char *CbzEngineImpl::GetImageData(int pageNo, size_t& len)
{
    AssertCrash(1 <= pageNo && pageNo <= PageCount());
    ScopedCritSec scope(&cacheAccess);
    return cbzFile->GetFileDataByIdx(fileIdxs.At(pageNo - 1), &len);
}

BaseEngine *CbzEngineImpl::CreateFromFile(const WCHAR *fileName)
{
    CbzEngineImpl *engine = new CbzEngineImpl();
    if (!engine->LoadFromFile(fileName)) {
        delete engine;
        return NULL;
    }
    return engine;
}

BaseEngine *CbzEngineImpl::CreateFromStream(IStream *stream)
{
    CbzEngineImpl *engine = new CbzEngineImpl();
    if (!engine->LoadFromStream(stream)) {
        delete engine;
        return NULL;
    }
    return engine;
}

// UnRAR doesn't allow to selectively decompress files, so we store all of them
// in umcompressed form; since image decoding might be expensive, the data is only
// decoded on demand same as for CBZ and ImageDir
struct RarDecompressData {
    size_t len, cap;
    char data[1]; // data buffer has size cap (+ 2 for zero-termination)
};

class CbrEngineImpl : public ComicEngine {
public:
    virtual ~CbrEngineImpl() { FreeVecMembers(pageData); }

    virtual BaseEngine *Clone() {
        if (fileName)
            return CreateFromFile(fileName);
        return NULL;
    }

    virtual const WCHAR *GetDefaultFileExt() const { return L".cbr"; }

    static BaseEngine *CreateFromFile(const WCHAR *fileName);

protected:
    Vec<RarDecompressData *> pageData;

    bool LoadFromFile(const WCHAR *fileName);

    virtual Bitmap *LoadBitmap(int pageNo, bool& deleteAfterUse);
    virtual RectD LoadMediabox(int pageNo);
};

static int CALLBACK unrarCallback(UINT msg, LPARAM userData, LPARAM rarBuffer, LPARAM bytesProcessed)
{
    if (UCM_PROCESSDATA != msg)
        return -1;
    if (!userData)
        return -1;

    RarDecompressData *rdd = (RarDecompressData *)userData;
    if (bytesProcessed > (LPARAM)rdd->cap || rdd->len > rdd->cap - bytesProcessed)
        return -1;

    memcpy(rdd->data + rdd->len, (unsigned char *)rarBuffer, bytesProcessed);
    rdd->len += bytesProcessed;
    return 1;
}

static RarDecompressData *LoadCurrentCbrFile(HANDLE hArc, RARHeaderDataEx& rarHeader)
{
    if (rarHeader.UnpSizeHigh != 0 || rarHeader.UnpSize == 0 || (int)rarHeader.UnpSize < 0) {
        RARProcessFile(hArc, RAR_SKIP, NULL, NULL);
        return NULL;
    }

    RarDecompressData *rdd = (RarDecompressData *)malloc(sizeof(RarDecompressData) + rarHeader.UnpSize + 1);
    if (!rdd) {
        RARProcessFile(hArc, RAR_SKIP, NULL, NULL);
        return NULL;
    }
    rdd->cap = rarHeader.UnpSize;
    rdd->len = 0;

    RARSetCallback(hArc, unrarCallback, (LPARAM)rdd);
    int res = RARProcessFile(hArc, RAR_TEST, NULL, NULL);
    if (0 != res || rdd->cap != rdd->len) {
        free(rdd);
        return NULL;
    }

    // zero-terminate for convenience
    rdd->data[rdd->len] = rdd->data[rdd->len + 1] = '\0';

    return rdd;
}

struct NameToData {
    const WCHAR *fileName;
    RarDecompressData *rdd;

    explicit NameToData(const WCHAR *fileName=NULL, RarDecompressData *rdd=NULL) : fileName(fileName), rdd(rdd) { }

    static int cmpByName(const void *a, const void *b) {
        return wcscmp(((NameToData *)a)->fileName, ((NameToData *)b)->fileName);
    }
};

bool CbrEngineImpl::LoadFromFile(const WCHAR *file)
{
    if (!file)
        return false;
    fileName = str::Dup(file);

    RAROpenArchiveDataEx  arcData = { 0 };
    arcData.ArcNameW = fileName;
    arcData.OpenMode = RAR_OM_EXTRACT;

    HANDLE hArc = RAROpenArchiveEx(&arcData);
    if (!hArc || arcData.OpenResult != 0)
        return false;

    // UnRAR does not seem to support extracting a single file by name,
    // so lazy image loading doesn't seem possible

    WStrVec foundNames;
    Vec<NameToData> foundData;

    for (;;) {
        RARHeaderDataEx rarHeader;
        int res = RARReadHeaderEx(hArc, &rarHeader);
        if (0 != res)
            break;

        const WCHAR *fileName = rarHeader.FileNameW;
        if (ImageEngine::IsSupportedFile(fileName)) {
            RarDecompressData *rdd = LoadCurrentCbrFile(hArc, rarHeader);
            if (rdd && !GfxFileExtFromData(rdd->data, rdd->cap)) {
                delete rdd;
            }
            else if (rdd) {
                foundNames.Append(str::Dup(fileName));
                foundData.Append(NameToData(foundNames.Last(), rdd));
            }
        }
        else if (str::EqI(fileName, L"ComicInfo.xml")) {
            RarDecompressData *rdd = LoadCurrentCbrFile(hArc, rarHeader);
            if (rdd) {
                ParseComicInfoXml(rdd->data);
                delete rdd;
            }
        }
        else
            RARProcessFile(hArc, RAR_SKIP, NULL, NULL);

    }
    RARCloseArchive(hArc);

    if (foundData.Count() == 0)
        return false;

    foundData.Sort(NameToData::cmpByName);
    for (NameToData *item = foundData.IterStart(); item; item = foundData.IterNext()) {
        pageData.Append(item->rdd);
    }
    mediaboxes.AppendBlanks(pageData.Count());

    return true;
}

Bitmap *CbrEngineImpl::LoadBitmap(int pageNo, bool& deleteAfterUse)
{
    AssertCrash(1 <= pageNo && pageNo <= PageCount());
    RarDecompressData *rdd = pageData.At(pageNo - 1);
    deleteAfterUse = true;
    return BitmapFromData(rdd->data, rdd->len);
}

RectD CbrEngineImpl::LoadMediabox(int pageNo)
{
    // fill the cache to prevent the first few images from being unpacked twice
    ImagePage *page = GetPage(pageNo, MAX_IMAGE_PAGE_CACHE == pageCache.Count());
    if (page) {
        RectD mbox(0, 0, page->bmp->GetWidth(), page->bmp->GetHeight());
        DropPage(page);
        return mbox;
    }

    AssertCrash(1 <= pageNo && pageNo <= PageCount());
    RarDecompressData *rdd = pageData.At(pageNo - 1);
    Size size = BitmapSizeFromData(rdd->data, rdd->len);
    return RectD(0, 0, size.Width, size.Height);
}

BaseEngine *CbrEngineImpl::CreateFromFile(const WCHAR *fileName)
{
    CbrEngineImpl *engine = new CbrEngineImpl();
    if (!engine->LoadFromFile(fileName)) {
        delete engine;
        return NULL;
    }
    return engine;
}

namespace CbxEngine {

bool IsSupportedFile(const WCHAR *fileName, bool sniff)
{
    if (sniff) {
        // we don't also sniff for ZIP files, as these could also
        // be broken XPS files for which failure is expected
        return file::StartsWithN(fileName, "Rar!\x1A\x07\x00", 7);
    }

    return str::EndsWithI(fileName, L".cbz") ||
           str::EndsWithI(fileName, L".cbr") ||
           str::EndsWithI(fileName, L".zip") && !str::EndsWithI(fileName, L".fb2.zip") ||
           str::EndsWithI(fileName, L".rar");
}

BaseEngine *CreateFromFile(const WCHAR *fileName)
{
    AssertCrash(CbxEngine::IsSupportedFile(fileName) || CbxEngine::IsSupportedFile(fileName, true));
    if (str::EndsWithI(fileName, L".cbz") || str::EndsWithI(fileName, L".zip") ||
        file::StartsWith(fileName, "PK\x03\x04")) {
        BaseEngine *engine = CbzEngineImpl::CreateFromFile(fileName);
        if (engine)
            return engine;
    }
    // also try again if a .cbz or .zip file failed to load, it might
    // just have been misnamed (which apparently happens occasionally)
    if (str::EndsWithI(fileName, L".cbr") || str::EndsWithI(fileName, L".rar") ||
        file::StartsWithN(fileName, "Rar!\x1A\x07\x00", 7)) {
        BaseEngine *engine = CbrEngineImpl::CreateFromFile(fileName);
        if (engine)
            return engine;
    }
    return NULL;
}

BaseEngine *CreateFromStream(IStream *stream)
{
    // TODO: UnRAR doesn't support reading from arbitrary data streams
    return CbzEngineImpl::CreateFromStream(stream);
}

}
