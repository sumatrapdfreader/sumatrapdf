/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/Archive.h"
#include "utils/ScopedWin.h"

#include "utils/FileUtil.h"
#include "utils/GuessFileType.h"
#include "utils/GdiPlusUtil.h"
#include "utils/HtmlParserLookup.h"
#include "utils/HtmlPullParser.h"
#include "utils/JsonParser.h"
#include "utils/WinUtil.h"
#include "utils/Timer.h"
#include "utils/DirIter.h"
#include "utils/Log.h"

#include "wingui/TreeModel.h"

#include "Annotation.h"
#include "EngineBase.h"
#include "EngineImages.h"
#include "PdfCreator.h"

// using namespace Gdiplus;

using Gdiplus::ARGB;
using Gdiplus::Bitmap;
using Gdiplus::Brush;
using Gdiplus::Color;
using Gdiplus::CombineModeReplace;
using Gdiplus::CompositingQualityHighQuality;
using Gdiplus::Font;
using Gdiplus::FontFamily;
using Gdiplus::FontStyle;
using Gdiplus::FontStyleBold;
using Gdiplus::FontStyleRegular;
using Gdiplus::FontStyleStrikeout;
using Gdiplus::FontStyleUnderline;
using Gdiplus::FrameDimensionPage;
using Gdiplus::FrameDimensionTime;
using Gdiplus::Graphics;
using Gdiplus::GraphicsPath;
using Gdiplus::Image;
using Gdiplus::ImageAttributes;
using Gdiplus::LinearGradientBrush;
using Gdiplus::LinearGradientMode;
using Gdiplus::Matrix;
using Gdiplus::MatrixOrderAppend;
using Gdiplus::Ok;
using Gdiplus::OutOfMemory;
using Gdiplus::Pen;
using Gdiplus::PenAlignmentInset;
using Gdiplus::PropertyItem;
using Gdiplus::Region;
using Gdiplus::SmoothingModeAntiAlias;
using Gdiplus::SolidBrush;
using Gdiplus::Status;
using Gdiplus::StringFormat;
using Gdiplus::StringFormatFlagsDirectionRightToLeft;
using Gdiplus::TextRenderingHintClearTypeGridFit;
using Gdiplus::UnitPixel;
using Gdiplus::Win32Error;
using Gdiplus::WrapModeTileFlipXY;

Kind kindEngineImage = "engineImage";
Kind kindEngineImageDir = "engineImageDir";
Kind kindEngineComicBooks = "engineComicBooks";

// number of decoded bitmaps to cache for quicker rendering
#define MAX_IMAGE_PAGE_CACHE 10

///// EngineImages methods apply to all types of engines handling full-page images /////

struct ImagePage {
    int pageNo = 0;
    Bitmap* bmp = nullptr;
    bool ownBmp = true;
    int refs = 1;

    ImagePage(int pageNo, Bitmap* bmp) {
        this->pageNo = pageNo;
        this->bmp = bmp;
    }
};

class EngineImages : public EngineBase {
  public:
    EngineImages();
    virtual ~EngineImages();

    RectF PageMediabox(int pageNo) override;

    RenderedBitmap* RenderPage(RenderPageArgs& args) override;

    RectF Transform(const RectF& rect, int pageNo, float zoom, int rotation, bool inverse = false) override;

    std::span<u8> GetFileData() override;
    bool SaveFileAs(const char* copyFileName, bool includeUserAnnots = false) override;
    PageText ExtractPageText([[maybe_unused]] int pageNo) override {
        return {};
    }
    bool HasClipOptimizations([[maybe_unused]] int pageNo) override {
        return false;
    }

    Vec<IPageElement*>* GetElements(int pageNo) override;
    IPageElement* GetElementAtPos(int pageNo, PointF pt) override;

    RenderedBitmap* GetImageForPageElement(IPageElement*) override;

    bool BenchLoadPage(int pageNo) override {
        ImagePage* page = GetPage(pageNo);
        if (page) {
            DropPage(page, false);
        }
        return page != nullptr;
    }

    // protected:
    ScopedComPtr<IStream> fileStream;

    CRITICAL_SECTION cacheAccess;
    Vec<ImagePage*> pageCache;
    Vec<RectF> mediaboxes;

    void GetTransform(Matrix& m, int pageNo, float zoom, int rotation);

    virtual Bitmap* LoadBitmapForPage(int pageNo, bool& deleteAfterUse) = 0;
    virtual RectF LoadMediabox(int pageNo) = 0;

    ImagePage* GetPage(int pageNo, bool tryOnly = false);
    void DropPage(ImagePage* page, bool forceRemove);
};

EngineImages::EngineImages() {
    kind = kindEngineImage;

    preferredLayout = Layout_NonContinuous;
    isImageCollection = true;

    InitializeCriticalSection(&cacheAccess);
}

EngineImages::~EngineImages() {
    EnterCriticalSection(&cacheAccess);
    while (pageCache.size() > 0) {
        ImagePage* lastPage = pageCache.Last();
        CrashIf(lastPage->refs != 1);
        DropPage(lastPage, true);
    }
    LeaveCriticalSection(&cacheAccess);
    DeleteCriticalSection(&cacheAccess);
}

RectF EngineImages::PageMediabox(int pageNo) {
    CrashIf((pageNo < 1) || (pageNo > pageCount));
    int n = pageNo - 1;
    if (mediaboxes.at(n).IsEmpty()) {
        mediaboxes.at(n) = LoadMediabox(pageNo);
    }
    return mediaboxes.at(n);
}

RenderedBitmap* EngineImages::RenderPage(RenderPageArgs& args) {
    auto pageNo = args.pageNo;
    auto pageRect = args.pageRect;
    auto zoom = args.zoom;
    auto rotation = args.rotation;

    ImagePage* page = GetPage(pageNo);
    if (!page) {
        return nullptr;
    }

    auto timeStart = TimeGet();
    defer {
        auto dur = TimeSinceInMs(timeStart);
        logf("EngineImages::RenderPage() in %.2f\n", dur);
    };

    RectF pageRc = pageRect ? *pageRect : PageMediabox(pageNo);
    Rect screen = Transform(pageRc, pageNo, zoom, rotation).Round();
    Point screenTL = screen.TL();
    screen.Offset(-screen.x, -screen.y);

    HANDLE hMap = nullptr;
    HBITMAP hbmp = CreateMemoryBitmap(screen.Size(), &hMap);
    HDC hDC = CreateCompatibleDC(nullptr);
    DeleteObject(SelectObject(hDC, hbmp));

    Graphics g(hDC);
    g.SetCompositingQuality(CompositingQualityHighQuality);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPageUnit(UnitPixel);

    Color white(0xFF, 0xFF, 0xFF);
    SolidBrush tmpBrush(white);
    Gdiplus::Rect screenR = ToGdipRect(screen);
    screenR.Inflate(1, 1);
    g.FillRectangle(&tmpBrush, screenR);

    Matrix m;
    GetTransform(m, pageNo, zoom, rotation);
    m.Translate((float)-screenTL.x, (float)-screenTL.y, MatrixOrderAppend);
    g.SetTransform(&m);

    Rect pageRcI = PageMediabox(pageNo).Round();
    ImageAttributes imgAttrs;
    imgAttrs.SetWrapMode(WrapModeTileFlipXY);
    Status ok = g.DrawImage(page->bmp, ToGdipRect(pageRcI), 0, 0, pageRcI.dx, pageRcI.dy, UnitPixel, &imgAttrs);

    DropPage(page, false);
    DeleteDC(hDC);

    if (ok != Ok) {
        DeleteObject(hbmp);
        CloseHandle(hMap);
        return nullptr;
    }

    return new RenderedBitmap(hbmp, screen.Size(), hMap);
}

void EngineImages::GetTransform(Matrix& m, int pageNo, float zoom, int rotation) {
    GetBaseTransform(m, ToGdipRectF(PageMediabox(pageNo)), zoom, rotation);
}

RectF EngineImages::Transform(const RectF& rect, int pageNo, float zoom, int rotation, bool inverse) {
    Gdiplus::PointF pts[2] = {Gdiplus::PointF((float)rect.x, (float)rect.y),
                              Gdiplus::PointF((float)(rect.x + rect.dx), (float)(rect.y + rect.dy))};
    Matrix m;
    GetTransform(m, pageNo, zoom, rotation);
    if (inverse) {
        m.Invert();
    }
    m.TransformPoints(pts, 2);
    RectF res = RectF::FromXY(pts[0].X, pts[0].Y, pts[1].X, pts[1].Y);
    // try to undo rounding errors caused by a rotation
    // (necessary correction determined by experimentation)
    if (rotation != 0) {
        res.Inflate(-0.01f, -0.01f);
    }
    return res;
}

static PageElement* newImageElement(ImagePage* page) {
    auto res = new PageElement();
    res->kind_ = kindPageElementImage;
    res->pageNo = page->pageNo;
    int dx = page->bmp->GetWidth();
    int dy = page->bmp->GetHeight();
    res->rect = RectF(0, 0, (float)dx, (float)dy);
    res->imageID = page->pageNo;
    return res;
}

Vec<IPageElement*>* EngineImages::GetElements(int pageNo) {
    // TODO: this is inefficient because we don't need to
    // decompress the image. just need to know the size
    ImagePage* page = GetPage(pageNo);
    if (!page) {
        return nullptr;
    }

    auto els = new Vec<IPageElement*>();
    auto el = newImageElement(page);
    els->Append(el);
    DropPage(page, false);
    return els;
}

IPageElement* EngineImages::GetElementAtPos(int pageNo, PointF pt) {
    if (!PageMediabox(pageNo).Contains(pt)) {
        return nullptr;
    }
    ImagePage* page = GetPage(pageNo);
    if (!page) {
        return nullptr;
    }
    auto res = newImageElement(page);
    DropPage(page, false);
    return res;
}

RenderedBitmap* EngineImages::GetImageForPageElement(IPageElement* ipel) {
    PageElement* pel = (PageElement*)ipel;
    int pageNo = pel->imageID;
    auto page = GetPage(pageNo);

    HBITMAP hbmp;
    auto bmp = page->bmp;
    int dx = bmp->GetWidth();
    int dy = bmp->GetHeight();
    Size s{dx, dy};
    auto status = bmp->GetHBITMAP((ARGB)Color::White, &hbmp);
    DropPage(page, false);
    if (status != Ok) {
        return nullptr;
    }
    return new RenderedBitmap(hbmp, s);
}

std::span<u8> EngineImages::GetFileData() {
    return GetStreamOrFileData(fileStream.Get(), FileName());
}

bool EngineImages::SaveFileAs(const char* copyFileName, [[maybe_unused]] bool includeUserAnnots) {
    const WCHAR* srcPath = FileName();
    AutoFreeWstr dstPath = strconv::Utf8ToWstr(copyFileName);
    if (srcPath) {
        BOOL ok = CopyFileW(srcPath, dstPath, FALSE);
        if (ok) {
            return true;
        }
    }
    AutoFree d = GetFileData();
    if (d.empty()) {
        return false;
    }
    return file::WriteFile(dstPath, d.AsSpan());
}

ImagePage* EngineImages::GetPage(int pageNo, bool tryOnly) {
    ScopedCritSec scope(&cacheAccess);

    ImagePage* result = nullptr;

    for (size_t i = 0; i < pageCache.size(); i++) {
        if (pageCache.at(i)->pageNo == pageNo) {
            result = pageCache.at(i);
            break;
        }
    }
    if (!result && tryOnly) {
        return nullptr;
    }

    if (!result) {
        // TODO: drop most memory intensive pages first
        // (i.e. formats which aren't IsGdiPlusNativeFormat)?
        if (pageCache.size() >= MAX_IMAGE_PAGE_CACHE) {
            CrashIf(pageCache.size() != MAX_IMAGE_PAGE_CACHE);
            DropPage(pageCache.Last(), true);
        }
        result = new ImagePage(pageNo, nullptr);
        result->bmp = LoadBitmapForPage(pageNo, result->ownBmp);
        pageCache.InsertAt(0, result);
    } else if (result != pageCache.at(0)) {
        // keep the list Most Recently Used first
        pageCache.Remove(result);
        pageCache.InsertAt(0, result);
    }
    // return nullptr if a page failed to load
    if (result && !result->bmp) {
        return nullptr;
    }
    if (!result) {
        return nullptr;
    }

    result->refs++;
    return result;
}

void EngineImages::DropPage(ImagePage* page, bool forceRemove) {
    ScopedCritSec scope(&cacheAccess);
    page->refs--;
    CrashIf(page->refs < 0);

    if (0 == page->refs || forceRemove) {
        pageCache.Remove(page);
    }

    if (0 == page->refs) {
        if (page->ownBmp) {
            delete page->bmp;
        }
        delete page;
    }
}

///// ImageEngine handles a single image file /////

class EngineImage : public EngineImages {
  public:
    EngineImage();
    virtual ~EngineImage();

    EngineBase* Clone() override;

    WCHAR* GetProperty(DocumentProperty prop) override;

    bool SaveFileAsPDF(const char* pdfFileName, bool includeUserAnnots = false) override;

    static EngineBase* CreateFromFile(const WCHAR* fileName);
    static EngineBase* CreateFromStream(IStream* stream);

  protected:
    Bitmap* image = nullptr;
    const WCHAR* fileExt = nullptr;

    bool LoadSingleFile(const WCHAR* fileName);
    bool LoadFromStream(IStream* stream);
    bool FinishLoading();

    Bitmap* LoadBitmapForPage(int pageNo, bool& deleteAfterUse) override;
    RectF LoadMediabox(int pageNo) override;
};

EngineImage::EngineImage() {
    kind = kindEngineImage;
}

EngineImage::~EngineImage() {
    delete image;
}

EngineBase* EngineImage::Clone() {
    Bitmap* bmp = image->Clone(0, 0, image->GetWidth(), image->GetHeight(), PixelFormat32bppARGB);
    if (!bmp) {
        return nullptr;
    }

    EngineImage* clone = new EngineImage();
    clone->SetFileName(FileName());
    clone->defaultFileExt = defaultFileExt;
    clone->fileExt = fileExt;
    clone->fileDPI = fileDPI;
    if (fileStream) {
        fileStream->Clone(&clone->fileStream);
    }
    clone->image = bmp;
    clone->FinishLoading();

    return clone;
}

bool EngineImage::LoadSingleFile(const WCHAR* file) {
    if (!file) {
        return false;
    }
    SetFileName(file);

    AutoFree data = file::ReadFile(file);
    fileExt = GfxFileExtFromData(data.AsSpan());
    defaultFileExt = fileExt;
    image = BitmapFromData(data.AsSpan());
    return FinishLoading();
}

bool EngineImage::LoadFromStream(IStream* stream) {
    if (!stream) {
        return false;
    }
    fileStream = stream;
    fileStream->AddRef();

    u8 header[18];
    if (ReadDataFromStream(stream, header, sizeof(header))) {
        fileExt = GfxFileExtFromData({header, sizeof(header)});
    }
    if (!fileExt) {
        return false;
    }

    defaultFileExt = fileExt;

    AutoFree data = GetDataFromStream(stream, nullptr);
    if (IsGdiPlusNativeFormat(data.AsSpan())) {
        image = Bitmap::FromStream(stream);
    } else {
        image = BitmapFromData(data.AsSpan());
    }

    return FinishLoading();
}

bool EngineImage::FinishLoading() {
    if (!image || image->GetLastStatus() != Ok) {
        return false;
    }
    fileDPI = image->GetHorizontalResolution();

    mediaboxes.Append(RectF(0, 0, (float)image->GetWidth(), (float)image->GetHeight()));
    CrashIf(mediaboxes.size() != 1);

    // extract all frames from multi-page TIFFs and animated GIFs
    if (str::Eq(fileExt, L".tif") || str::Eq(fileExt, L".gif")) {
        const GUID* frameDimension = str::Eq(fileExt, L".tif") ? &FrameDimensionPage : &FrameDimensionTime;
        mediaboxes.AppendBlanks(image->GetFrameCount(frameDimension) - 1);
    }
    pageCount = (int)mediaboxes.size();

    CrashIf(!fileExt);
    return fileExt != nullptr;
}

// cf. http://www.universalthread.com/ViewPageArticle.aspx?ID=831
#ifndef PropertyTagXPTitle
#define PropertyTagXPTitle 0x9c9b
#define PropertyTagXPComment 0x9c9c
#define PropertyTagXPAuthor 0x9c9d
#define PropertyTagXPKeywords 0x9c9e
#define PropertyTagXPSubject 0x9c9f
#endif

static WCHAR* GetImageProperty(Bitmap* bmp, PROPID id, PROPID altId = 0) {
    WCHAR* value = nullptr;
    uint size = bmp->GetPropertyItemSize(id);
    PropertyItem* item = (PropertyItem*)malloc(size);
    Status ok = item ? bmp->GetPropertyItem(id, size, item) : OutOfMemory;
    if (Ok != ok) {
        /* property didn't exist */;
    } else if (PropertyTagTypeASCII == item->type) {
        value = strconv::FromAnsi((char*)item->value);
    } else if (PropertyTagTypeByte == item->type && item->length > 0 && 0 == (item->length % 2) &&
               !((WCHAR*)item->value)[item->length / 2 - 1]) {
        value = str::Dup((WCHAR*)item->value);
    }
    free(item);
    if (!value && altId) {
        return GetImageProperty(bmp, altId);
    }
    return value;
}

WCHAR* EngineImage::GetProperty(DocumentProperty prop) {
    switch (prop) {
        case DocumentProperty::Title:
            return GetImageProperty(image, PropertyTagImageDescription, PropertyTagXPTitle);
        case DocumentProperty::Subject:
            return GetImageProperty(image, PropertyTagXPSubject);
        case DocumentProperty::Author:
            return GetImageProperty(image, PropertyTagArtist, PropertyTagXPAuthor);
        case DocumentProperty::Copyright:
            return GetImageProperty(image, PropertyTagCopyright);
        case DocumentProperty::CreationDate:
            return GetImageProperty(image, PropertyTagDateTime, PropertyTagExifDTDigitized);
        case DocumentProperty::CreatorApp:
            return GetImageProperty(image, PropertyTagSoftwareUsed);
        default:
            return nullptr;
    }
}

Bitmap* EngineImage::LoadBitmapForPage(int pageNo, bool& deleteAfterUse) {
    if (1 == pageNo) {
        deleteAfterUse = false;
        return image;
    }

    // extract other frames from multi-page TIFFs and animated GIFs
    CrashIf(!str::Eq(fileExt, L".tif") && !str::Eq(fileExt, L".gif"));
    const GUID* frameDimension = str::Eq(fileExt, L".tif") ? &FrameDimensionPage : &FrameDimensionTime;
    uint frameCount = image->GetFrameCount(frameDimension);
    CrashIf((unsigned int)pageNo > frameCount);
    Bitmap* frame = image->Clone(0, 0, image->GetWidth(), image->GetHeight(), PixelFormat32bppARGB);
    if (!frame) {
        return nullptr;
    }
    Status ok = frame->SelectActiveFrame(frameDimension, pageNo - 1);
    if (ok != Ok) {
        delete frame;
        return nullptr;
    }
    deleteAfterUse = true;
    return frame;
}

RectF EngineImage::LoadMediabox(int pageNo) {
    if (1 == pageNo) {
        return RectF(0, 0, (float)image->GetWidth(), (float)image->GetHeight());
    }

    // fill the cache to prevent the first few frames from being unpacked twice
    ImagePage* page = GetPage(pageNo, MAX_IMAGE_PAGE_CACHE == pageCache.size());
    if (page) {
        RectF mbox(0, 0, (float)page->bmp->GetWidth(), (float)page->bmp->GetHeight());
        DropPage(page, false);
        return mbox;
    }

    CrashIf(!str::Eq(fileExt, L".tif") && !str::Eq(fileExt, L".gif"));
    RectF mbox = RectF(0, 0, (float)image->GetWidth(), (float)image->GetHeight());
    Bitmap* frame = image->Clone(0, 0, image->GetWidth(), image->GetHeight(), PixelFormat32bppARGB);
    if (!frame) {
        return mbox;
    }
    const GUID* frameDimension = str::Eq(fileExt, L".tif") ? &FrameDimensionPage : &FrameDimensionTime;
    Status ok = frame->SelectActiveFrame(frameDimension, pageNo - 1);
    if (Ok == ok) {
        mbox = RectF(0, 0, (float)frame->GetWidth(), (float)frame->GetHeight());
    }
    delete frame;
    return mbox;
}

bool EngineImage::SaveFileAsPDF(const char* pdfFileName, [[maybe_unused]] bool includeUserAnnots) {
    bool ok = true;
    PdfCreator* c = new PdfCreator();
    auto dpi = GetFileDPI();
    if (FileName()) {
        AutoFree data = file::ReadFile(FileName());
        ok = c->AddPageFromImageData(data.data, data.size(), dpi);
    } else {
        AutoFree data = GetDataFromStream(fileStream, nullptr);
        ok = c->AddPageFromImageData(data.data, data.size(), dpi);
    }
    for (int i = 2; i <= PageCount() && ok; i++) {
        ImagePage* page = GetPage(i);
        ok = page && c->AddPageFromGdiplusBitmap(page->bmp, dpi);
        DropPage(page, false);
    }
    if (ok) {
        c->CopyProperties(this);
        ok = c->SaveToFile(pdfFileName);
    }
    delete c;
    return ok;
}

EngineBase* EngineImage::CreateFromFile(const WCHAR* fileName) {
    EngineImage* engine = new EngineImage();
    if (!engine->LoadSingleFile(fileName)) {
        delete engine;
        return nullptr;
    }
    return engine;
}

EngineBase* EngineImage::CreateFromStream(IStream* stream) {
    EngineImage* engine = new EngineImage();
    if (!engine->LoadFromStream(stream)) {
        delete engine;
        return nullptr;
    }
    return engine;
}

static Kind imageEngineKinds[] = {
    kindFilePng, kindFileJpeg, kindFileGif, kindFileTiff, kindFileBmp, kindFileTga,
    kindFileJxr, kindFileHdp,  kindFileWdp, kindFileWebp, kindFileJp2,
};

bool IsImageEngineSupportedFileType(Kind kind) {
    int n = dimof(imageEngineKinds);
    return KindInArray(imageEngineKinds, n, kind);
}

EngineBase* CreateImageEngineFromFile(const WCHAR* fileName) {
    return EngineImage::CreateFromFile(fileName);
}

EngineBase* CreateImageEngineFromStream(IStream* stream) {
    return EngineImage::CreateFromStream(stream);
}

///// ImageDirEngine handles a directory full of image files /////

class EngineImageDir : public EngineImages {
  public:
    EngineImageDir() {
        fileDPI = 96.0f;
        kind = kindEngineImageDir;
        defaultFileExt = L"";
        // TODO: is there a better place to expose pageFileNames
        // than through page labels?
        hasPageLabels = true;
    }

    virtual ~EngineImageDir() {
        delete tocTree;
    }

    EngineBase* Clone() override {
        if (FileName()) {
            return CreateFromFile(FileName());
        }
        return nullptr;
    }

    std::span<u8> GetFileData() override {
        return {};
    }
    bool SaveFileAs(const char* copyFileName, bool includeUserAnnots = false) override;

    WCHAR* GetProperty([[maybe_unused]] DocumentProperty prop) override {
        return nullptr;
    }

    WCHAR* GetPageLabel(int pageNo) const override;
    int GetPageByLabel(const WCHAR* label) const override;

    TocTree* GetToc() override;

    bool SaveFileAsPDF(const char* pdfFileName, bool includeUserAnnots = false) override;

    static EngineBase* CreateFromFile(const WCHAR* fileName);

    // protected:

    Bitmap* LoadBitmapForPage(int pageNo, bool& deleteAfterUse) override;
    RectF LoadMediabox(int pageNo) override;

    WStrVec pageFileNames;
    TocTree* tocTree = nullptr;
};

static bool LoadImageDir(EngineImageDir* e, const WCHAR* dir) {
    e->SetFileName(dir);

    DirIter di(dir, false);
    for (const WCHAR* path = di.First(); path; path = di.Next()) {
        Kind kind = GuessFileTypeFromName(path);
        if (IsImageEngineSupportedFileType(kind)) {
            WCHAR* pathCopy = str::Dup(path);
            e->pageFileNames.Append(pathCopy);
        }
    }

    if (e->pageFileNames.size() == 0) {
        return false;
    }
    e->pageFileNames.SortNatural();

    e->mediaboxes.AppendBlanks(e->pageFileNames.size());
    e->pageCount = (int)e->mediaboxes.size();

    // TODO: better handle the case where images have different resolutions
    ImagePage* page = e->GetPage(1);
    if (page) {
        e->fileDPI = page->bmp->GetHorizontalResolution();
        e->DropPage(page, false);
    }
    return true;
}

WCHAR* EngineImageDir::GetPageLabel(int pageNo) const {
    if (pageNo < 1 || PageCount() < pageNo) {
        return EngineBase::GetPageLabel(pageNo);
    }

    const WCHAR* path = pageFileNames.at(pageNo - 1);
    const WCHAR* fileName = path::GetBaseNameNoFree(path);
    size_t n = path::GetExtNoFree(fileName) - fileName;
    return str::DupN(fileName, n);
}

int EngineImageDir::GetPageByLabel(const WCHAR* label) const {
    for (size_t i = 0; i < pageFileNames.size(); i++) {
        const WCHAR* fileName = path::GetBaseNameNoFree(pageFileNames.at(i));
        const WCHAR* fileExt = path::GetExtNoFree(fileName);
        if (str::StartsWithI(fileName, label) &&
            (fileName + str::Len(label) == fileExt || fileName[str::Len(label)] == '\0')) {
            return (int)i + 1;
        }
    }

    return EngineBase::GetPageByLabel(label);
}

static TocItem* newImageDirTocItem(TocItem* parent, WCHAR* title, int pageNo) {
    return new TocItem(parent, title, pageNo);
};

TocTree* EngineImageDir::GetToc() {
    if (tocTree) {
        return tocTree;
    }
    AutoFreeWstr ws = GetPageLabel(1);
    TocItem* root = newImageDirTocItem(nullptr, ws, 1);
    root->id = 1;
    for (int i = 2; i <= PageCount(); i++) {
        ws = GetPageLabel(i);
        TocItem* item = newImageDirTocItem(root, ws, i);
        item->id = i;
        root->AddSiblingAtEnd(item);
    }
    tocTree = new TocTree(root);
    return tocTree;
}

bool EngineImageDir::SaveFileAs(const char* copyFileName, [[maybe_unused]] bool includeUserAnnots) {
    // only copy the files if the target directory doesn't exist yet
    AutoFreeWstr dstPath = strconv::Utf8ToWstr(copyFileName);
    if (!CreateDirectoryW(dstPath, nullptr)) {
        return false;
    }
    bool ok = true;
    for (size_t i = 0; i < pageFileNames.size(); i++) {
        const WCHAR* filePathOld = pageFileNames.at(i);
        AutoFreeWstr filePathNew(path::Join(dstPath, path::GetBaseNameNoFree(filePathOld)));
        ok = ok && CopyFileW(filePathOld, filePathNew, TRUE);
    }
    return ok;
}

Bitmap* EngineImageDir::LoadBitmapForPage(int pageNo, bool& deleteAfterUse) {
    AutoFree bmpData = file::ReadFile(pageFileNames.at(pageNo - 1));
    if (bmpData.data) {
        deleteAfterUse = true;
        return BitmapFromData(bmpData.AsSpan());
    }
    return nullptr;
}

RectF EngineImageDir::LoadMediabox(int pageNo) {
    AutoFree bmpData = file::ReadFile(pageFileNames.at(pageNo - 1));
    if (bmpData.data) {
        std::span<u8> sp{(u8*)bmpData.data, bmpData.size()};
        Size size = BitmapSizeFromData(sp);
        return RectF(0, 0, (float)size.dx, (float)size.dy);
    }
    return RectF();
}

bool EngineImageDir::SaveFileAsPDF(const char* pdfFileName, [[maybe_unused]] bool includeUserAnnots) {
    bool ok = true;
    PdfCreator* c = new PdfCreator();
    for (int i = 1; i <= PageCount() && ok; i++) {
        AutoFree data = file::ReadFile(pageFileNames.at(i - 1));
        ok = c->AddPageFromImageData(data.data, data.size(), GetFileDPI());
    }
    if (ok) {
        ok = c->SaveToFile(pdfFileName);
    }
    delete c;
    return ok;
}

EngineBase* EngineImageDir::CreateFromFile(const WCHAR* fileName) {
    CrashIf(!dir::Exists(fileName));
    EngineImageDir* engine = new EngineImageDir();
    if (!LoadImageDir(engine, fileName)) {
        delete engine;
        return nullptr;
    }
    return engine;
}

bool IsImageDirEngineSupportedFile(const WCHAR* fileName, [[maybe_unused]] bool sniff) {
    // whether it actually contains images will be checked in LoadImageDir
    return dir::Exists(fileName);
}

EngineBase* CreateImageDirEngineFromFile(const WCHAR* fileName) {
    return EngineImageDir::CreateFromFile(fileName);
}

///// CbxEngine handles comic book files (either .cbz, .cbr, .cb7 or .cbt) /////

class EngineCbx : public EngineImages, public json::ValueVisitor {
  public:
    EngineCbx(MultiFormatArchive* arch);
    ~EngineCbx() override;

    EngineBase* Clone() override;

    bool SaveFileAsPDF(const char* pdfFileName, bool includeUserAnnots = false) override;

    WCHAR* GetProperty(DocumentProperty prop) override;

    const WCHAR* GetDefaultFileExt() const;

    TocTree* GetToc() override;

    // json::ValueVisitor
    bool Visit(const char* path, const char* value, json::Type type) override;

    static EngineBase* CreateFromFile(const WCHAR* path);
    static EngineBase* CreateFromStream(IStream* stream);

    // an image for each page
    Vec<ImageData> images;

  protected:
    Bitmap* LoadBitmapForPage(int pageNo, bool& deleteAfterUse) override;
    RectF LoadMediabox(int pageNo) override;

    bool LoadFromFile(const WCHAR* fileName);
    bool LoadFromStream(IStream* stream);
    bool FinishLoading();

    ImageData GetImageData(int pageNo);
    void ParseComicInfoXml(std::span<u8> xmlData);

    // access to cbxFile must be protected after initialization (with cacheAccess)
    MultiFormatArchive* cbxFile = nullptr;
    Vec<MultiFormatArchive::FileInfo*> files;
    TocTree* tocTree = nullptr;

    // not owned
    const WCHAR* defaultExt = nullptr;

    // extracted metadata
    AutoFreeWstr propTitle;
    WStrVec propAuthors;
    AutoFreeWstr propDate;
    AutoFreeWstr propModDate;
    AutoFreeWstr propCreator;
    AutoFreeWstr propSummary;
    // temporary state needed for extracting metadata
    AutoFreeWstr propAuthorTmp;
};

// TODO: refactor so that doesn't have to keep <arch>
EngineCbx::EngineCbx(MultiFormatArchive* arch) {
    cbxFile = arch;
    kind = kindEngineComicBooks;
}

EngineCbx::~EngineCbx() {
    delete tocTree;

    // can be set in error conditions but generally is
    // deleted in FinishLoading
    delete cbxFile;

    for (auto&& img : images) {
        free(img.data);
    }
}

EngineBase* EngineCbx::Clone() {
    if (fileStream) {
        ScopedComPtr<IStream> stm;
        HRESULT res = fileStream->Clone(&stm);
        if (SUCCEEDED(res)) {
            return CreateFromStream(stm);
        }
    }
    if (FileName()) {
        return CreateFromFile(FileName());
    }
    return nullptr;
}

bool EngineCbx::LoadFromFile(const WCHAR* file) {
    if (!file) {
        return false;
    }
    SetFileName(file);
    return FinishLoading();
}

bool EngineCbx::LoadFromStream(IStream* stream) {
    if (!stream) {
        return false;
    }
    fileStream = stream;
    fileStream->AddRef();

    return FinishLoading();
}

static bool cmpArchFileInfoByName(MultiFormatArchive::FileInfo* f1, MultiFormatArchive::FileInfo* f2) {
    const char* s1 = f1->name.data();
    const char* s2 = f2->name.data();
    int res = str::CmpNatural(s1, s2);
    return res < 0;
}

static const WCHAR* GetExtFromArchiveType(MultiFormatArchive* cbxFile) {
    switch (cbxFile->format) {
        case MultiFormatArchive::Format::Zip:
            return L".cbz";
        case MultiFormatArchive::Format::Rar:
            return L".cbr";
        case MultiFormatArchive::Format::SevenZip:
            return L".cb7";
        case MultiFormatArchive::Format::Tar:
            return L".cbt";
    }
    CrashIf(true);
    return nullptr;
}

bool EngineCbx::FinishLoading() {
    CrashIf(!cbxFile);
    if (!cbxFile) {
        return false;
    }

    auto timeStart = TimeGet();
    defer {
        auto dur = TimeSinceInMs(timeStart);
        logf("EngineCbx::FinisHLoading() in %.2f\n", dur);
    };

    // not using the resolution of the contained images seems to be
    // expected, cf. http://forums.fofou.org/sumatrapdf/topic?id=3183827
    // TODO: return DpiGetForHwnd(HWND_DESKTOP) instead?
    fileDPI = 96.f;

    defaultFileExt = GetExtFromArchiveType(cbxFile);

    Vec<MultiFormatArchive::FileInfo*> pageFiles;

    auto& fileInfos = cbxFile->GetFileInfos();
    size_t n = fileInfos.size();
    for (size_t i = 0; i < n; i++) {
        auto* fileInfo = fileInfos[i];
        const char* fileName = fileInfo->name.data();
        if (str::Len(fileName) == 0) {
            continue;
        }
        if (MultiFormatArchive::Format::Zip == cbxFile->format && str::StartsWithI(fileName, "_rels/.rels")) {
            // bail, if we accidentally try to load an XPS file
            return false;
        }

        AutoFreeWstr fileNameW = strconv::Utf8ToWstr(fileName);
        Kind kind = GuessFileTypeFromName(fileNameW);
        if (IsImageEngineSupportedFileType(kind) &&
            // OS X occasionally leaves metadata with image extensions
            !str::StartsWith(path::GetBaseNameNoFree(fileName), ".")) {
            pageFiles.Append(fileInfo);
        }
    }

    AutoFree metadata(cbxFile->GetFileDataByName("ComicInfo.xml"));
    if (metadata.data) {
        ParseComicInfoXml(metadata.AsSpan());
    }
    std::string_view comment = cbxFile->GetComment();
    if (comment.data()) {
        json::Parse(comment.data(), this);
    }

    size_t nFiles = pageFiles.size();
    if (nFiles == 0) {
        return false;
    }

    std::sort(pageFiles.begin(), pageFiles.end(), cmpArchFileInfoByName);

    mediaboxes.AppendBlanks(nFiles);
    files = std::move(pageFiles);
    pageCount = (int)nFiles;
    if (pageCount == 0) {
        delete cbxFile;
        cbxFile = nullptr;
        return false;
    }

    TocItem* root = nullptr;
    TocItem* curr = nullptr;
    for (int i = 0; i < pageCount; i++) {
        std::string_view fname = pageFiles[i]->name;
        AutoFreeWstr name = strconv::Utf8ToWstr(fname);
        const WCHAR* baseName = path::GetBaseNameNoFree(name.Get());
        TocItem* ti = new TocItem(nullptr, baseName, i + 1);
        if (root == nullptr) {
            root = ti;
            curr = ti;
        } else {
            curr->next = ti;
            curr = ti;
        }
    }
    tocTree = new TocTree(root);

    for (int i = 0; i < pageCount; i++) {
        size_t fileId = files[i]->fileId;
        std::span<u8> sv = cbxFile->GetFileDataById(fileId);
        ImageData img;
        img.data = (char*)sv.data();
        img.len = sv.size();
        images.Append(img);
    }

    delete cbxFile;
    cbxFile = nullptr;

    return true;
}

TocTree* EngineCbx::GetToc() {
    return tocTree;
}

ImageData EngineCbx::GetImageData(int pageNo) {
    CrashIf((pageNo < 1) || (pageNo > PageCount()));
    return images[pageNo - 1];
}

static char* GetTextContent(HtmlPullParser& parser) {
    HtmlToken* tok = parser.Next();
    if (!tok || !tok->IsText()) {
        return nullptr;
    }
    return ResolveHtmlEntities(tok->s, tok->sLen);
}

// extract ComicInfo.xml metadata
// cf. http://comicrack.cyolito.com/downloads/comicrack/ComicRack/Support-Files/ComicInfoSchema.zip/
void EngineCbx::ParseComicInfoXml(std::span<u8> xmlData) {
    // TODO: convert UTF-16 data and skip UTF-8 BOM
    HtmlPullParser parser(xmlData);
    HtmlToken* tok;
    while ((tok = parser.Next()) != nullptr && !tok->IsError()) {
        if (!tok->IsStartTag()) {
            continue;
        }
        if (tok->NameIs("Title")) {
            AutoFree value(GetTextContent(parser));
            if (value) {
                Visit("/ComicBookInfo/1.0/title", value, json::Type::String);
            }
        } else if (tok->NameIs("Year")) {
            AutoFree value(GetTextContent(parser));
            if (value) {
                Visit("/ComicBookInfo/1.0/publicationYear", value, json::Type::Number);
            }
        } else if (tok->NameIs("Month")) {
            AutoFree value(GetTextContent(parser));
            if (value) {
                Visit("/ComicBookInfo/1.0/publicationMonth", value, json::Type::Number);
            }
        } else if (tok->NameIs("Summary")) {
            AutoFree value(GetTextContent(parser));
            if (value) {
                Visit("/X-summary", value, json::Type::String);
            }
        } else if (tok->NameIs("Writer")) {
            AutoFree value(GetTextContent(parser));
            if (value) {
                Visit("/ComicBookInfo/1.0/credits[0]/person", value, json::Type::String);
                Visit("/ComicBookInfo/1.0/credits[0]/primary", "true", json::Type::Bool);
            }
        } else if (tok->NameIs("Penciller")) {
            AutoFree value(GetTextContent(parser));
            if (value) {
                Visit("/ComicBookInfo/1.0/credits[1]/person", value, json::Type::String);
                Visit("/ComicBookInfo/1.0/credits[1]/primary", "true", json::Type::Bool);
            }
        }
    }
}

// extract ComicBookInfo metadata
// cf. http://code.google.com/p/comicbookinfo/
bool EngineCbx::Visit(const char* path, const char* value, json::Type type) {
    if (json::Type::String == type && str::Eq(path, "/ComicBookInfo/1.0/title")) {
        propTitle.Set(strconv::Utf8ToWstr(value));
    } else if (json::Type::Number == type && str::Eq(path, "/ComicBookInfo/1.0/publicationYear")) {
        propDate.Set(str::Format(L"%s/%d", propDate ? propDate.Get() : L"", atoi(value)));
    } else if (json::Type::Number == type && str::Eq(path, "/ComicBookInfo/1.0/publicationMonth")) {
        propDate.Set(str::Format(L"%d%s", atoi(value), propDate ? propDate.Get() : L""));
    } else if (json::Type::String == type && str::Eq(path, "/appID")) {
        propCreator.Set(strconv::Utf8ToWstr(value));
    } else if (json::Type::String == type && str::Eq(path, "/lastModified")) {
        propModDate.Set(strconv::Utf8ToWstr(value));
    } else if (json::Type::String == type && str::Eq(path, "/X-summary")) {
        propSummary.Set(strconv::Utf8ToWstr(value));
    } else if (str::StartsWith(path, "/ComicBookInfo/1.0/credits[")) {
        int idx = -1;
        const char* prop = str::Parse(path, "/ComicBookInfo/1.0/credits[%d]/", &idx);
        if (prop) {
            if (json::Type::String == type && str::Eq(prop, "person")) {
                propAuthorTmp.Set(strconv::Utf8ToWstr(value));
            } else if (json::Type::Bool == type && str::Eq(prop, "primary") && propAuthorTmp &&
                       !propAuthors.Contains(propAuthorTmp)) {
                propAuthors.Append(propAuthorTmp.StealData());
            }
        }
        return true;
    }
    // stop parsing once we have all desired information
    return !propTitle || propAuthors.size() == 0 || !propCreator || !propDate ||
           str::FindChar(propDate, '/') <= propDate;
}

bool EngineCbx::SaveFileAsPDF(const char* pdfFileName, [[maybe_unused]] bool includeUserAnnots) {
    bool ok = true;
    PdfCreator* c = new PdfCreator();
    for (int i = 1; i <= PageCount() && ok; i++) {
        ImageData img = GetImageData(i);
        ok = c->AddPageFromImageData(img.data, img.size(), GetFileDPI());
    }
    if (ok) {
        c->CopyProperties(this);
        ok = c->SaveToFile(pdfFileName);
    }
    delete c;
    return ok;
}

WCHAR* EngineCbx::GetProperty(DocumentProperty prop) {
    switch (prop) {
        case DocumentProperty::Title:
            return str::Dup(propTitle);
        case DocumentProperty::Author:
            return propAuthors.size() ? propAuthors.Join(L", ") : nullptr;
        case DocumentProperty::CreationDate:
            return str::Dup(propDate);
        case DocumentProperty::ModificationDate:
            return str::Dup(propModDate);
        case DocumentProperty::CreatorApp:
            return str::Dup(propCreator);
        // TODO: replace with Prop_Summary
        case DocumentProperty::Subject:
            return str::Dup(propSummary);
        default:
            return nullptr;
    }
}

const WCHAR* EngineCbx::GetDefaultFileExt() const {
    return defaultExt;
}

Bitmap* EngineCbx::LoadBitmapForPage(int pageNo, bool& deleteAfterUse) {
    auto timeStart = TimeGet();
    defer {
        auto dur = TimeSinceInMs(timeStart);
        logf("EngineCbx::LoadBitmapForPage(page: %d) took %.2f\n", pageNo, dur);
    };
    ImageData img = GetImageData(pageNo);
    if (img.data) {
        deleteAfterUse = true;
        return BitmapFromData(img.AsSpan());
    }
    return nullptr;
}

RectF EngineCbx::LoadMediabox(int pageNo) {
    // fill the cache to prevent the first few images from being unpacked twice
    ImagePage* page = GetPage(pageNo, MAX_IMAGE_PAGE_CACHE == pageCache.size());
    if (page) {
        RectF mbox(0, 0, (float)page->bmp->GetWidth(), (float)page->bmp->GetHeight());
        DropPage(page, false);
        return mbox;
    }

    ImageData img = GetImageData(pageNo);
    if (img.data) {
        Size size = BitmapSizeFromData(img.AsSpan());
        return RectF(0, 0, (float)size.dx, (float)size.dy);
    }
    return RectF();
}

EngineBase* EngineCbx::CreateFromFile(const WCHAR* path) {
    // we sniff the type from content first because the
    // files can be mis-named e.g. .cbr archive with .cbz ext
    Kind kind = GuessFileTypeFromContent(path);
    MultiFormatArchive* archive = nullptr;
    if (kind == kindFileZip) {
        archive = OpenZipArchive(path, false);
    } else if (kind == kindFileRar) {
        archive = OpenRarArchive(path);
    } else if (kind == kindFile7Z) {
        archive = Open7zArchive(path);
    }

    if (!archive) {
        kind = GuessFileTypeFromName(path);
        if (kind == kindFileCbt || kind == kindFileTar) {
            archive = OpenTarArchive(path);
        }
    }
    if (!archive) {
        return nullptr;
    }

    auto* engine = new EngineCbx(archive);
    if (engine->LoadFromFile(path)) {
        return engine;
    }
    delete engine;
    return nullptr;
}

EngineBase* EngineCbx::CreateFromStream(IStream* stream) {
    auto* archive = OpenZipArchive(stream, false);
    if (archive) {
        auto* engine = new EngineCbx(archive);
        if (engine->LoadFromStream(stream)) {
            return engine;
        }
        delete engine;
    }

    archive = OpenRarArchive(stream);
    if (archive) {
        auto* engine = new EngineCbx(archive);
        if (engine->LoadFromStream(stream)) {
            return engine;
        }
        delete engine;
    }

    archive = Open7zArchive(stream);
    if (archive) {
        auto* engine = new EngineCbx(archive);
        if (engine->LoadFromStream(stream)) {
            return engine;
        }
        delete engine;
    }

    archive = OpenTarArchive(stream);
    if (archive) {
        auto* engine = new EngineCbx(archive);
        if (engine->LoadFromStream(stream)) {
            return engine;
        }
        delete engine;
    }

    return nullptr;
}

static Kind cbxKinds[] = {
    kindFileCbz, kindFileCbr, kindFileCb7, kindFileCbt, kindFileZip, kindFileRar, kindFile7Z, kindFileTar,
};

bool IsCbxEngineSupportedFileType(Kind kind) {
    int n = dimof(cbxKinds);
    return KindInArray(cbxKinds, n, kind);
}

EngineBase* CreateCbxEngineFromFile(const WCHAR* path) {
    return EngineCbx::CreateFromFile(path);
}

EngineBase* CreateCbxEngineFromStream(IStream* stream) {
    return EngineCbx::CreateFromStream(stream);
}
