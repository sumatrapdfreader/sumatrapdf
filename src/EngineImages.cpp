/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/Archive.h"
#include "utils/ScopedWin.h"

#include "utils/FileUtil.h"
#include "utils/GdiPlusUtil.h"
#include "utils/HtmlParserLookup.h"
#include "utils/HtmlPullParser.h"
#include "utils/JsonParser.h"
#include "utils/WinUtil.h"
#include "utils/Timer.h"
#include "utils/Log.h"

#include "wingui/TreeModel.h"
#include "EngineBase.h"
#include "EngineImages.h"
#include "PdfCreator.h"

using namespace Gdiplus;

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

    RectD PageMediabox(int pageNo) override;

    RenderedBitmap* RenderPage(RenderPageArgs& args) override;

    RectD Transform(RectD rect, int pageNo, float zoom, int rotation, bool inverse = false) override;

    std::string_view GetFileData() override;
    bool SaveFileAs(const char* copyFileName, bool includeUserAnnots = false) override;
    WCHAR* ExtractPageText(int pageNo, RectI** coordsOut = nullptr) override {
        UNUSED(pageNo);
        UNUSED(coordsOut);
        return nullptr;
    }
    bool HasClipOptimizations(int pageNo) override {
        UNUSED(pageNo);
        return false;
    }

    void UpdateUserAnnotations(Vec<PageAnnotation>* list) override {
        UNUSED(list);
    }

    Vec<PageElement*>* GetElements(int pageNo) override;
    PageElement* GetElementAtPos(int pageNo, PointD pt) override;

    RenderedBitmap* GetImageForPageElement(PageElement*) override;

    bool BenchLoadPage(int pageNo) override {
        ImagePage* page = GetPage(pageNo);
        if (page) {
            DropPage(page, false);
        }
        return page != nullptr;
    }

  protected:
    ScopedComPtr<IStream> fileStream;

    CRITICAL_SECTION cacheAccess;
    Vec<ImagePage*> pageCache;
    Vec<RectD> mediaboxes;

    void GetTransform(Matrix& m, int pageNo, float zoom, int rotation);

    virtual Bitmap* LoadBitmapForPage(int pageNo, bool& deleteAfterUse) = 0;
    virtual RectD LoadMediabox(int pageNo) = 0;

    ImagePage* GetPage(int pageNo, bool tryOnly = false);
    void DropPage(ImagePage* page, bool forceRemove);
};

EngineImages::EngineImages() {
    kind = kindEngineImage;

    supportsAnnotations = false;
    supportsAnnotationsForSaving = false;
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

RectD EngineImages::PageMediabox(int pageNo) {
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

    RectD pageRc = pageRect ? *pageRect : PageMediabox(pageNo);
    RectI screen = Transform(pageRc, pageNo, zoom, rotation).Round();
    PointI screenTL = screen.TL();
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
    Rect screenR(screen.ToGdipRect());
    screenR.Inflate(1, 1);
    g.FillRectangle(&tmpBrush, screenR);

    Matrix m;
    GetTransform(m, pageNo, zoom, rotation);
    m.Translate((REAL)-screenTL.x, (REAL)-screenTL.y, MatrixOrderAppend);
    g.SetTransform(&m);

    RectI pageRcI = PageMediabox(pageNo).Round();
    ImageAttributes imgAttrs;
    imgAttrs.SetWrapMode(WrapModeTileFlipXY);
    Status ok = g.DrawImage(page->bmp, pageRcI.ToGdipRect(), 0, 0, pageRcI.dx, pageRcI.dy, UnitPixel, &imgAttrs);

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
    GetBaseTransform(m, PageMediabox(pageNo).ToGdipRectF(), zoom, rotation);
}

RectD EngineImages::Transform(RectD rect, int pageNo, float zoom, int rotation, bool inverse) {
    PointF pts[2] = {PointF((REAL)rect.x, (REAL)rect.y), PointF((REAL)(rect.x + rect.dx), (REAL)(rect.y + rect.dy))};
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

static PageElement* newImageElement(ImagePage* page) {
    auto res = new PageElement();
    res->kind = kindPageElementImage;
    res->pageNo = page->pageNo;
    int dx = page->bmp->GetWidth();
    int dy = page->bmp->GetHeight();
    res->rect = RectD(0, 0, dx, dy);
    res->imageID = page->pageNo;
    return res;
}

Vec<PageElement*>* EngineImages::GetElements(int pageNo) {
    // TODO: this is inefficient because we don't need to
    // decompress the image. just need to know the size
    ImagePage* page = GetPage(pageNo);
    if (!page) {
        return nullptr;
    }

    Vec<PageElement*>* els = new Vec<PageElement*>();
    auto el = newImageElement(page);
    els->Append(el);
    DropPage(page, false);
    return els;
}

PageElement* EngineImages::GetElementAtPos(int pageNo, PointD pt) {
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

RenderedBitmap* EngineImages::GetImageForPageElement(PageElement* pel) {
    int pageNo = pel->imageID;
    auto page = GetPage(pageNo);

    HBITMAP hbmp;
    auto bmp = page->bmp;
    int dx = bmp->GetWidth();
    int dy = bmp->GetHeight();
    SizeI s{dx, dy};
    auto status = bmp->GetHBITMAP((ARGB)Color::White, &hbmp);
    DropPage(page, false);
    if (status != Ok) {
        return nullptr;
    }
    return new RenderedBitmap(hbmp, s);
}

std::string_view EngineImages::GetFileData() {
    return GetStreamOrFileData(fileStream.Get(), FileName());
}

bool EngineImages::SaveFileAs(const char* copyFileName, bool includeUserAnnots) {
    UNUSED(includeUserAnnots);
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
    return file::WriteFile(dstPath, d.as_view());
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
    RectD LoadMediabox(int pageNo) override;
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
    fileExt = GfxFileExtFromData(data.data, data.size());
    defaultFileExt = fileExt;
    image = BitmapFromData(data.data, data.size());
    return FinishLoading();
}

bool EngineImage::LoadFromStream(IStream* stream) {
    if (!stream) {
        return false;
    }
    fileStream = stream;
    fileStream->AddRef();

    char header[18];
    if (ReadDataFromStream(stream, header, sizeof(header))) {
        fileExt = GfxFileExtFromData(header, sizeof(header));
    }
    if (!fileExt) {
        return false;
    }

    defaultFileExt = fileExt;

    AutoFree data = GetDataFromStream(stream, nullptr);
    if (IsGdiPlusNativeFormat(data.data, data.size())) {
        image = Bitmap::FromStream(stream);
    } else {
        image = BitmapFromData(data.data, data.size());
    }

    return FinishLoading();
}

bool EngineImage::FinishLoading() {
    if (!image || image->GetLastStatus() != Ok) {
        return false;
    }
    fileDPI = image->GetHorizontalResolution();

    mediaboxes.Append(RectD(0, 0, image->GetWidth(), image->GetHeight()));
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
    UINT size = bmp->GetPropertyItemSize(id);
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
    UINT frameCount = image->GetFrameCount(frameDimension);
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

RectD EngineImage::LoadMediabox(int pageNo) {
    if (1 == pageNo) {
        return RectD(0, 0, image->GetWidth(), image->GetHeight());
    }

    // fill the cache to prevent the first few frames from being unpacked twice
    ImagePage* page = GetPage(pageNo, MAX_IMAGE_PAGE_CACHE == pageCache.size());
    if (page) {
        RectD mbox(0, 0, page->bmp->GetWidth(), page->bmp->GetHeight());
        DropPage(page, false);
        return mbox;
    }

    CrashIf(!str::Eq(fileExt, L".tif") && !str::Eq(fileExt, L".gif"));
    RectD mbox = RectD(0, 0, image->GetWidth(), image->GetHeight());
    Bitmap* frame = image->Clone(0, 0, image->GetWidth(), image->GetHeight(), PixelFormat32bppARGB);
    if (!frame) {
        return mbox;
    }
    const GUID* frameDimension = str::Eq(fileExt, L".tif") ? &FrameDimensionPage : &FrameDimensionTime;
    Status ok = frame->SelectActiveFrame(frameDimension, pageNo - 1);
    if (Ok == ok) {
        mbox = RectD(0, 0, frame->GetWidth(), frame->GetHeight());
    }
    delete frame;
    return mbox;
}

bool EngineImage::SaveFileAsPDF(const char* pdfFileName, bool includeUserAnnots) {
    UNUSED(includeUserAnnots);
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

static const char* imageExtensions =
    ".png\0.jpg\0.jpeg\0.gif\0.tif\0.tiff\0.bmp\0.tga\0.jxr\0.hdp\0.wdp\0.webp\0.jp2\0";

bool IsImageEngineSupportedFile(const char* fileName) {
    const char* ext = path::GetExtNoFree(fileName);
    AutoFree extLower = str::ToLower(ext);
    int idx = seqstrings::StrToIdx(imageExtensions, extLower);
    return idx >= 0;
}

bool IsImageEngineSupportedFile(const WCHAR* fileName, bool sniff) {
    const WCHAR* ext = path::GetExtNoFree(fileName);
    if (sniff) {
        char header[32] = {0};
        file::ReadN(fileName, header, sizeof(header));
        const WCHAR* ext2 = GfxFileExtFromData(header, sizeof(header));
        if (ext2 != nullptr) {
            ext = ext2;
        }
    }
    if (str::Len(ext) == 0) {
        return false;
    }
    AutoFree fileNameA = strconv::WstrToUtf8(fileName);
    return IsImageEngineSupportedFile(fileNameA);
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

    std::string_view GetFileData() override {
        return {};
    }
    bool SaveFileAs(const char* copyFileName, bool includeUserAnnots = false) override;

    WCHAR* GetProperty(DocumentProperty prop) override {
        UNUSED(prop);
        return nullptr;
    }

    WCHAR* GetPageLabel(int pageNo) const override;
    int GetPageByLabel(const WCHAR* label) const override;

    TocTree* GetToc() override;

    bool SaveFileAsPDF(const char* pdfFileName, bool includeUserAnnots = false) override;

    static EngineBase* CreateFromFile(const WCHAR* fileName);

  protected:
    bool LoadImageDir(const WCHAR* dirName);

    Bitmap* LoadBitmapForPage(int pageNo, bool& deleteAfterUse) override;
    RectD LoadMediabox(int pageNo) override;

    WStrVec pageFileNames;
    TocTree* tocTree = nullptr;
};

bool EngineImageDir::LoadImageDir(const WCHAR* dirName) {
    SetFileName(dirName);

    AutoFreeWstr pattern(path::Join(dirName, L"*"));

    WIN32_FIND_DATA fdata;
    HANDLE hfind = FindFirstFile(pattern, &fdata);
    if (INVALID_HANDLE_VALUE == hfind) {
        return false;
    }

    do {
        if (!(fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            if (IsImageEngineSupportedFile(fdata.cFileName)) {
                pageFileNames.Append(path::Join(dirName, fdata.cFileName));
            }
        }
    } while (FindNextFile(hfind, &fdata));
    FindClose(hfind);

    if (pageFileNames.size() == 0) {
        return false;
    }
    pageFileNames.SortNatural();

    mediaboxes.AppendBlanks(pageFileNames.size());
    pageCount = (int)mediaboxes.size();

    // TODO: better handle the case where images have different resolutions
    ImagePage* page = GetPage(1);
    if (page) {
        fileDPI = page->bmp->GetHorizontalResolution();
        DropPage(page, false);
    }
    return true;
}

WCHAR* EngineImageDir::GetPageLabel(int pageNo) const {
    if (pageNo < 1 || PageCount() < pageNo) {
        return EngineBase::GetPageLabel(pageNo);
    }

    const WCHAR* fileName = path::GetBaseNameNoFree(pageFileNames.at(pageNo - 1));
    return str::DupN(fileName, path::GetExtNoFree(fileName) - fileName);
}

int EngineImageDir::GetPageByLabel(const WCHAR* label) const {
    for (size_t i = 0; i < pageFileNames.size(); i++) {
        const WCHAR* fileName = path::GetBaseNameNoFree(pageFileNames.at(i));
        const WCHAR* fileExt = path::GetExtNoFree(fileName);
        if (str::StartsWithI(fileName, label) &&
            (fileName + str::Len(label) == fileExt || fileName[str::Len(label)] == '\0'))
            return (int)i + 1;
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
        root->AddSibling(item);
    }
    tocTree = new TocTree(root);
    return tocTree;
}

bool EngineImageDir::SaveFileAs(const char* copyFileName, bool includeUserAnnots) {
    UNUSED(includeUserAnnots);
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
    AutoFree bmpData(file::ReadFile(pageFileNames.at(pageNo - 1)));
    if (bmpData.data) {
        deleteAfterUse = true;
        return BitmapFromData(bmpData.data, bmpData.size());
    }
    return nullptr;
}

RectD EngineImageDir::LoadMediabox(int pageNo) {
    AutoFree bmpData(file::ReadFile(pageFileNames.at(pageNo - 1)));
    if (bmpData.data) {
        Size size = BitmapSizeFromData(bmpData.data, bmpData.size());
        return RectD(0, 0, size.Width, size.Height);
    }
    return RectD();
}

bool EngineImageDir::SaveFileAsPDF(const char* pdfFileName, bool includeUserAnnots) {
    UNUSED(includeUserAnnots);
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
    AssertCrash(dir::Exists(fileName));
    EngineImageDir* engine = new EngineImageDir();
    if (!engine->LoadImageDir(fileName)) {
        delete engine;
        return nullptr;
    }
    return engine;
}

bool IsImageDirEngineSupportedFile(const WCHAR* fileName, bool sniff) {
    UNUSED(sniff);
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
    bool Visit(const char* path, const char* value, json::DataType type) override;

    static EngineBase* CreateFromFile(const WCHAR* fileName);
    static EngineBase* CreateFromStream(IStream* stream);

    // an image for each page
    Vec<ImageData> images;

  protected:
    Bitmap* LoadBitmapForPage(int pageNo, bool& deleteAfterUse) override;
    RectD LoadMediabox(int pageNo) override;

    bool LoadFromFile(const WCHAR* fileName);
    bool LoadFromStream(IStream* stream);
    bool FinishLoading();

    ImageData GetImageData(int pageNo);
    void ParseComicInfoXml(const char* xmlData);

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

        if (IsImageEngineSupportedFile(fileName) &&
            // OS X occasionally leaves metadata with image extensions
            !str::StartsWith(path::GetBaseNameNoFree(fileName), ".")) {
            pageFiles.push_back(fileInfo);
        }
    }

    AutoFree metadata(cbxFile->GetFileDataByName("ComicInfo.xml"));
    if (metadata.data) {
        ParseComicInfoXml(metadata.data);
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
        const WCHAR* baseName = path::GetBaseNameNoFree(name.get());
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
        std::string_view sv = cbxFile->GetFileDataById(fileId);
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
void EngineCbx::ParseComicInfoXml(const char* xmlData) {
    // TODO: convert UTF-16 data and skip UTF-8 BOM
    HtmlPullParser parser(xmlData, str::Len(xmlData));
    HtmlToken* tok;
    while ((tok = parser.Next()) != nullptr && !tok->IsError()) {
        if (!tok->IsStartTag()) {
            continue;
        }
        if (tok->NameIs("Title")) {
            AutoFree value(GetTextContent(parser));
            if (value) {
                Visit("/ComicBookInfo/1.0/title", value, json::Type_String);
            }
        } else if (tok->NameIs("Year")) {
            AutoFree value(GetTextContent(parser));
            if (value) {
                Visit("/ComicBookInfo/1.0/publicationYear", value, json::Type_Number);
            }
        } else if (tok->NameIs("Month")) {
            AutoFree value(GetTextContent(parser));
            if (value) {
                Visit("/ComicBookInfo/1.0/publicationMonth", value, json::Type_Number);
            }
        } else if (tok->NameIs("Summary")) {
            AutoFree value(GetTextContent(parser));
            if (value) {
                Visit("/X-summary", value, json::Type_String);
            }
        } else if (tok->NameIs("Writer")) {
            AutoFree value(GetTextContent(parser));
            if (value) {
                Visit("/ComicBookInfo/1.0/credits[0]/person", value, json::Type_String);
                Visit("/ComicBookInfo/1.0/credits[0]/primary", "true", json::Type_Bool);
            }
        } else if (tok->NameIs("Penciller")) {
            AutoFree value(GetTextContent(parser));
            if (value) {
                Visit("/ComicBookInfo/1.0/credits[1]/person", value, json::Type_String);
                Visit("/ComicBookInfo/1.0/credits[1]/primary", "true", json::Type_Bool);
            }
        }
    }
}

// extract ComicBookInfo metadata
// cf. http://code.google.com/p/comicbookinfo/
bool EngineCbx::Visit(const char* path, const char* value, json::DataType type) {
    if (json::Type_String == type && str::Eq(path, "/ComicBookInfo/1.0/title"))
        propTitle.Set(strconv::Utf8ToWstr(value));
    else if (json::Type_Number == type && str::Eq(path, "/ComicBookInfo/1.0/publicationYear"))
        propDate.Set(str::Format(L"%s/%d", propDate ? propDate.get() : L"", atoi(value)));
    else if (json::Type_Number == type && str::Eq(path, "/ComicBookInfo/1.0/publicationMonth"))
        propDate.Set(str::Format(L"%d%s", atoi(value), propDate ? propDate.get() : L""));
    else if (json::Type_String == type && str::Eq(path, "/appID"))
        propCreator.Set(strconv::Utf8ToWstr(value));
    else if (json::Type_String == type && str::Eq(path, "/lastModified"))
        propModDate.Set(strconv::Utf8ToWstr(value));
    else if (json::Type_String == type && str::Eq(path, "/X-summary"))
        propSummary.Set(strconv::Utf8ToWstr(value));
    else if (str::StartsWith(path, "/ComicBookInfo/1.0/credits[")) {
        int idx = -1;
        const char* prop = str::Parse(path, "/ComicBookInfo/1.0/credits[%d]/", &idx);
        if (prop) {
            if (json::Type_String == type && str::Eq(prop, "person"))
                propAuthorTmp.Set(strconv::Utf8ToWstr(value));
            else if (json::Type_Bool == type && str::Eq(prop, "primary") && propAuthorTmp &&
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

bool EngineCbx::SaveFileAsPDF(const char* pdfFileName, bool includeUserAnnots) {
    UNUSED(includeUserAnnots);
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
        return BitmapFromData(img.data, img.size());
    }
    return nullptr;
}

RectD EngineCbx::LoadMediabox(int pageNo) {
    // fill the cache to prevent the first few images from being unpacked twice
    ImagePage* page = GetPage(pageNo, MAX_IMAGE_PAGE_CACHE == pageCache.size());
    if (page) {
        RectD mbox(0, 0, page->bmp->GetWidth(), page->bmp->GetHeight());
        DropPage(page, false);
        return mbox;
    }

    ImageData img = GetImageData(pageNo);
    if (img.data) {
        Size size = BitmapSizeFromData(img.data, img.size());
        return RectD(0, 0, size.Width, size.Height);
    }
    return RectD();
}

#define RAR_SIGNATURE "Rar!\x1A\x07\x00"
#define RAR_SIGNATURE_LEN 7
#define RAR5_SIGNATURE "Rar!\x1A\x07\x01\x00"
#define RAR5_SIGNATURE_LEN 8

EngineBase* EngineCbx::CreateFromFile(const WCHAR* fileName) {
    if (str::EndsWithI(fileName, L".cbz") || str::EndsWithI(fileName, L".zip") ||
        file::StartsWithN(fileName, "PK\x03\x04", 4)) {
        auto* archive = OpenZipArchive(fileName, false);
        if (!archive) {
            return nullptr;
        }
        auto* engine = new EngineCbx(archive);
        if (engine->LoadFromFile(fileName)) {
            return engine;
        }
        delete engine;
    }
    // also try again if a .cbz or .zip file failed to load, it might
    // just have been misnamed (which apparently happens occasionally)
    if (str::EndsWithI(fileName, L".cbr") || str::EndsWithI(fileName, L".rar") ||
        file::StartsWithN(fileName, RAR_SIGNATURE, RAR_SIGNATURE_LEN) ||
        file::StartsWithN(fileName, RAR5_SIGNATURE, RAR5_SIGNATURE_LEN)) {
        auto* archive = OpenRarArchive(fileName);
        if (archive) {
            auto* engine = new EngineCbx(archive);
            if (engine->LoadFromFile(fileName)) {
                return engine;
            }
            delete engine;
        }
    }
    if (str::EndsWithI(fileName, L".cb7") || str::EndsWithI(fileName, L".7z") ||
        file::StartsWith(fileName, "7z\xBC\xAF\x27\x1C")) {
        MultiFormatArchive* archive = Open7zArchive(fileName);
        if (archive) {
            auto* engine = new EngineCbx(archive);
            if (engine->LoadFromFile(fileName)) {
                return engine;
            }
            delete engine;
        }
    }
    if (str::EndsWithI(fileName, L".cbt") || str::EndsWithI(fileName, L".tar")) {
        MultiFormatArchive* archive = OpenTarArchive(fileName);
        if (archive) {
            auto* engine = new EngineCbx(archive);
            if (engine->LoadFromFile(fileName)) {
                return engine;
            }
            delete engine;
        }
    }
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

static const char* cbxExts = ".cbz\0.cbr\0.cb7\0.cbt\0.zip\0.rar\0.7z\0.tar\0";

bool IsCbxEngineSupportedFile(const WCHAR* fileName, bool sniff) {
    if (sniff) {
        // we don't also sniff for ZIP files, as these could also
        // be broken XPS files for which failure is expected
        // TODO: add TAR format sniffing
        return file::StartsWithN(fileName, RAR_SIGNATURE, RAR_SIGNATURE_LEN) ||
               file::StartsWithN(fileName, RAR5_SIGNATURE, RAR5_SIGNATURE_LEN) ||
               file::StartsWith(fileName, "7z\xBC\xAF\x27\x1C");
    }
    if (str::EndsWithI(fileName, L".fb2.zip")) {
        return false;
    }
    const WCHAR* ext = path::GetExtNoFree(fileName);
    AutoFreeWstr extLower = str::ToLower(ext);
    int idx = seqstrings::StrToIdx(cbxExts, extLower);
    return idx >= 0;
}

EngineBase* CreateCbxEngineFromFile(const WCHAR* fileName) {
    return EngineCbx::CreateFromFile(fileName);
}

EngineBase* CreateCbxEngineFromStream(IStream* stream) {
    return EngineCbx::CreateFromStream(stream);
}
