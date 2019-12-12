/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
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

///// ImagesEngine methods apply to all types of engines handling full-page images /////

struct ImagePage {
    int pageNo;
    Bitmap* bmp;
    bool ownBmp;
    int refs;

    ImagePage(int pageNo, Bitmap* bmp) : pageNo(pageNo), bmp(bmp), ownBmp(true), refs(1) {
    }
};

class ImageElement;

class ImagesEngine : public BaseEngine {
    friend ImageElement;

  public:
    ImagesEngine();
    virtual ~ImagesEngine();

    int PageCount() const override {
        return (int)mediaboxes.size();
    }

    RectD PageMediabox(int pageNo) override;

    RenderedBitmap* RenderBitmap(int pageNo, float zoom, int rotation,
                                 RectD* pageRect = nullptr, /* if nullptr: defaults to the page's mediabox */
                                 RenderTarget target = RenderTarget::View, AbortCookie** cookie_out = nullptr) override;

    PointD Transform(PointD pt, int pageNo, float zoom, int rotation, bool inverse = false) override;
    RectD Transform(RectD rect, int pageNo, float zoom, int rotation, bool inverse = false) override;

    std::tuple<char*, size_t> GetFileData() override;
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

    bool SupportsAnnotation(bool forSaving = false) const override {
        UNUSED(forSaving);
        return false;
    }
    void UpdateUserAnnotations(Vec<PageAnnotation>* list) override {
        UNUSED(list);
    }

    Vec<PageElement*>* GetElements(int pageNo) override;
    PageElement* GetElementAtPos(int pageNo, PointD pt) override;

    bool BenchLoadPage(int pageNo) override {
        ImagePage* page = GetPage(pageNo);
        if (page)
            DropPage(page);
        return page != nullptr;
    }

  protected:
    ScopedComPtr<IStream> fileStream;

    CRITICAL_SECTION cacheAccess;
    Vec<ImagePage*> pageCache;
    Vec<RectD> mediaboxes;

    void GetTransform(Matrix& m, int pageNo, float zoom, int rotation);

    virtual Bitmap* LoadBitmap(int pageNo, bool& deleteAfterUse) = 0;
    virtual RectD LoadMediabox(int pageNo) = 0;

    ImagePage* GetPage(int pageNo, bool tryOnly = false);
    void DropPage(ImagePage* page, bool forceRemove = false);
};

ImagesEngine::ImagesEngine() {
    kind = kindEngineImage;
    preferredLayout = Layout_NonContinuous;
    isImageCollection = true;

    InitializeCriticalSection(&cacheAccess);
}

ImagesEngine::~ImagesEngine() {
    EnterCriticalSection(&cacheAccess);
    while (pageCache.size() > 0) {
        CrashIf(pageCache.Last()->refs != 1);
        DropPage(pageCache.Last(), true);
    }
    LeaveCriticalSection(&cacheAccess);

    DeleteCriticalSection(&cacheAccess);
}

RectD ImagesEngine::PageMediabox(int pageNo) {
    AssertCrash(1 <= pageNo && pageNo <= PageCount());
    if (mediaboxes.at(pageNo - 1).IsEmpty())
        mediaboxes.at(pageNo - 1) = LoadMediabox(pageNo);
    return mediaboxes.at(pageNo - 1);
}

RenderedBitmap* ImagesEngine::RenderBitmap(int pageNo, float zoom, int rotation, RectD* pageRect, RenderTarget target,
                                           AbortCookie** cookieOut) {
    UNUSED(target);
    UNUSED(cookieOut);
    ImagePage* page = GetPage(pageNo);
    if (!page)
        return nullptr;

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

    DropPage(page);
    DeleteDC(hDC);

    if (ok != Ok) {
        DeleteObject(hbmp);
        CloseHandle(hMap);
        return nullptr;
    }

    return new RenderedBitmap(hbmp, screen.Size(), hMap);
}

void ImagesEngine::GetTransform(Matrix& m, int pageNo, float zoom, int rotation) {
    GetBaseTransform(m, PageMediabox(pageNo).ToGdipRectF(), zoom, rotation);
}

PointD ImagesEngine::Transform(PointD pt, int pageNo, float zoom, int rotation, bool inverse) {
    RectD rect = Transform(RectD(pt, SizeD()), pageNo, zoom, rotation, inverse);
    return PointD(rect.x, rect.y);
}

RectD ImagesEngine::Transform(RectD rect, int pageNo, float zoom, int rotation, bool inverse) {
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

class ImageElement : public PageElement {
    ImagesEngine* engine;
    ImagePage* page;

  public:
    explicit ImageElement(ImagesEngine* engine, ImagePage* page) : engine(engine), page(page) {
        this->pageNo = page->pageNo;
    }
    virtual ~ImageElement() {
        engine->DropPage(page);
    }

    PageElementType GetType() const override {
        return PageElementType::Image;
    }

    RectD GetRect() const override {
        return RectD(0, 0, page->bmp->GetWidth(), page->bmp->GetHeight());
    }
    WCHAR* GetValue() const override {
        return nullptr;
    }

    RenderedBitmap* GetImage() override {
        HBITMAP hbmp;
        if (page->bmp->GetHBITMAP((ARGB)Color::White, &hbmp) != Ok)
            return nullptr;
        return new RenderedBitmap(hbmp, SizeI(page->bmp->GetWidth(), page->bmp->GetHeight()));
    }
};

Vec<PageElement*>* ImagesEngine::GetElements(int pageNo) {
    ImagePage* page = GetPage(pageNo);
    if (!page)
        return nullptr;

    Vec<PageElement*>* els = new Vec<PageElement*>();
    els->Append(new ImageElement(this, page));
    return els;
}

PageElement* ImagesEngine::GetElementAtPos(int pageNo, PointD pt) {
    if (!PageMediabox(pageNo).Contains(pt))
        return nullptr;
    ImagePage* page = GetPage(pageNo);
    if (!page)
        return nullptr;
    return new ImageElement(this, page);
}

std::tuple<char*, size_t> ImagesEngine::GetFileData() {
    return GetStreamOrFileData(fileStream.Get(), FileName());
}

bool ImagesEngine::SaveFileAs(const char* copyFileName, bool includeUserAnnots) {
    UNUSED(includeUserAnnots);
    const WCHAR* srcPath = FileName();
    AutoFreeW dstPath(str::conv::FromUtf8(copyFileName));
    if (srcPath) {
        BOOL ok = CopyFileW(srcPath, dstPath, FALSE);
        if (ok) {
            return true;
        }
    }
    auto [data, dataLen] = GetFileData();
    if (!data) {
        return false;
    }
    auto res = file::WriteFile(dstPath, data, dataLen);
    free(data);
    return res;
}

ImagePage* ImagesEngine::GetPage(int pageNo, bool tryOnly) {
    ScopedCritSec scope(&cacheAccess);

    ImagePage* result = nullptr;

    for (size_t i = 0; i < pageCache.size(); i++) {
        if (pageCache.at(i)->pageNo == pageNo) {
            result = pageCache.at(i);
            break;
        }
    }
    if (!result && tryOnly)
        return nullptr;
    if (!result) {
        // TODO: drop most memory intensive pages first
        // (i.e. formats which aren't IsGdiPlusNativeFormat)?
        if (pageCache.size() >= MAX_IMAGE_PAGE_CACHE) {
            CrashIf(pageCache.size() != MAX_IMAGE_PAGE_CACHE);
            DropPage(pageCache.Last(), true);
        }
        result = new ImagePage(pageNo, nullptr);
        result->bmp = LoadBitmap(pageNo, result->ownBmp);
        pageCache.InsertAt(0, result);
    } else if (result != pageCache.at(0)) {
        // keep the list Most Recently Used first
        pageCache.Remove(result);
        pageCache.InsertAt(0, result);
    }
    // return nullptr if a page failed to load
    if (result && !result->bmp)
        result = nullptr;

    if (result)
        result->refs++;
    return result;
}

void ImagesEngine::DropPage(ImagePage* page, bool forceRemove) {
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
    ImageEngineImpl();
    virtual ~ImageEngineImpl();

    BaseEngine* Clone() override;

    WCHAR* GetProperty(DocumentProperty prop) override;

    bool SaveFileAsPDF(const char* pdfFileName, bool includeUserAnnots = false) override;

    static BaseEngine* CreateFromFile(const WCHAR* fileName);
    static BaseEngine* CreateFromStream(IStream* stream);

  protected:
    Bitmap* image = nullptr;
    const WCHAR* fileExt = nullptr;

    bool LoadSingleFile(const WCHAR* fileName);
    bool LoadFromStream(IStream* stream);
    bool FinishLoading();

    virtual Bitmap* LoadBitmap(int pageNo, bool& deleteAfterUse);
    virtual RectD LoadMediabox(int pageNo);
};

ImageEngineImpl::ImageEngineImpl() {
    kind = kindEngineImage;
}

ImageEngineImpl::~ImageEngineImpl() {
    delete image;
}

BaseEngine* ImageEngineImpl::Clone() {
    Bitmap* bmp = image->Clone(0, 0, image->GetWidth(), image->GetHeight(), PixelFormat32bppARGB);
    if (!bmp) {
        return nullptr;
    }

    ImageEngineImpl* clone = new ImageEngineImpl();
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

bool ImageEngineImpl::LoadSingleFile(const WCHAR* file) {
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

bool ImageEngineImpl::LoadFromStream(IStream* stream) {
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

    auto [data, size] = GetDataFromStream(stream, nullptr);
    if (IsGdiPlusNativeFormat(data, size)) {
        image = Bitmap::FromStream(stream);
    } else {
        image = BitmapFromData(data, size);
    }
    free(data);

    return FinishLoading();
}

bool ImageEngineImpl::FinishLoading() {
    if (!image || image->GetLastStatus() != Ok) {
        return false;
    }
    fileDPI = image->GetHorizontalResolution();

    mediaboxes.Append(RectD(0, 0, image->GetWidth(), image->GetHeight()));
    AssertCrash(mediaboxes.size() == 1);

    // extract all frames from multi-page TIFFs and animated GIFs
    if (str::Eq(fileExt, L".tif") || str::Eq(fileExt, L".gif")) {
        const GUID* frameDimension = str::Eq(fileExt, L".tif") ? &FrameDimensionPage : &FrameDimensionTime;
        mediaboxes.AppendBlanks(image->GetFrameCount(frameDimension) - 1);
    }

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
        value = str::conv::FromAnsi((char*)item->value);
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

WCHAR* ImageEngineImpl::GetProperty(DocumentProperty prop) {
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

Bitmap* ImageEngineImpl::LoadBitmap(int pageNo, bool& deleteAfterUse) {
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

RectD ImageEngineImpl::LoadMediabox(int pageNo) {
    if (1 == pageNo) {
        return RectD(0, 0, image->GetWidth(), image->GetHeight());
    }

    // fill the cache to prevent the first few frames from being unpacked twice
    ImagePage* page = GetPage(pageNo, MAX_IMAGE_PAGE_CACHE == pageCache.size());
    if (page) {
        RectD mbox(0, 0, page->bmp->GetWidth(), page->bmp->GetHeight());
        DropPage(page);
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

bool ImageEngineImpl::SaveFileAsPDF(const char* pdfFileName, bool includeUserAnnots) {
    UNUSED(includeUserAnnots);
    bool ok = true;
    PdfCreator* c = new PdfCreator();
    if (FileName()) {
        OwnedData data(file::ReadFile(FileName()));
        ok = data.data && c->AddImagePage(data.data, data.size, GetFileDPI());
    } else {
        auto [data, size] = GetDataFromStream(fileStream, nullptr);
        ok = data && c->AddImagePage(data, size, GetFileDPI());
        free(data);
    }
    for (int i = 2; i <= PageCount() && ok; i++) {
        ImagePage* page = GetPage(i);
        ok = page && c->AddImagePage(page->bmp, GetFileDPI());
        DropPage(page);
    }
    if (ok) {
        c->CopyProperties(this);
        ok = c->SaveToFile(pdfFileName);
    }
    delete c;
    return ok;
}

BaseEngine* ImageEngineImpl::CreateFromFile(const WCHAR* fileName) {
    ImageEngineImpl* engine = new ImageEngineImpl();
    if (!engine->LoadSingleFile(fileName)) {
        delete engine;
        return nullptr;
    }
    return engine;
}

BaseEngine* ImageEngineImpl::CreateFromStream(IStream* stream) {
    ImageEngineImpl* engine = new ImageEngineImpl();
    if (!engine->LoadFromStream(stream)) {
        delete engine;
        return nullptr;
    }
    return engine;
}

bool IsImageEngineSupportedFile(const char* fileName) {
    const char* ext = path::GetExt(fileName);
    if (str::Len(ext) == 0) {
        return false;
    }
    if (str::EqI(ext, ".png")) {
        return true;
    }
    if (str::EqI(ext, ".jpg")) {
        return true;
    }
    if (str::EqI(ext, ".jpeg")) {
        return true;
    }
    if (str::EqI(ext, ".gif")) {
        return true;
    }
    if (str::EqI(ext, ".tif")) {
        return true;
    }
    if (str::EqI(ext, ".tiff")) {
        return true;
    }
    if (str::EqI(ext, ".bmp")) {
        return true;
    }
    if (str::EqI(ext, ".tga")) {
        return true;
    }
    if (str::EqI(ext, ".jxr")) {
        return true;
    }
    if (str::EqI(ext, ".hdp")) {
        return true;
    }
    if (str::EqI(ext, ".wdp")) {
        return true;
    }
    if (str::EqI(ext, ".webp")) {
        return true;
    }

    if (str::EqI(ext, ".jp2")) {
        return true;
    }
    return false;
}

bool IsImageEngineSupportedFile(const WCHAR* fileName, bool sniff) {
    const WCHAR* ext = path::GetExt(fileName);
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
    AutoFree fileNameA = str::conv::WstrToUtf8(fileName);
    return IsImageEngineSupportedFile(fileNameA);
}

BaseEngine* CreateImageEngineFromFile(const WCHAR* fileName) {
    return ImageEngineImpl::CreateFromFile(fileName);
}

BaseEngine* CreateImageEngineFromStream(IStream* stream) {
    return ImageEngineImpl::CreateFromStream(stream);
}

///// ImageDirEngine handles a directory full of image files /////

class ImageDirEngineImpl : public ImagesEngine {
  public:
    ImageDirEngineImpl() {
        fileDPI = 96.0f;
        kind = kindEngineImageDir;
        defaultFileExt = L"";
    }

    virtual ~ImageDirEngineImpl() {
        delete tocTree;
    }

    BaseEngine* Clone() override {
        if (FileName()) {
            return CreateFromFile(FileName());
        }
        return nullptr;
    }

    std::tuple<char*, size_t> GetFileData() override {
        return {};
    }
    bool SaveFileAs(const char* copyFileName, bool includeUserAnnots = false) override;

    WCHAR* GetProperty(DocumentProperty prop) override {
        UNUSED(prop);
        return nullptr;
    }

    // TODO: is there a better place to expose pageFileNames than through page labels?
    bool HasPageLabels() const override {
        return true;
    }
    WCHAR* GetPageLabel(int pageNo) const override;
    int GetPageByLabel(const WCHAR* label) const override;

    DocTocTree* GetTocTree() override;

    bool SaveFileAsPDF(const char* pdfFileName, bool includeUserAnnots = false) override;

    static BaseEngine* CreateFromFile(const WCHAR* fileName);

  protected:
    bool LoadImageDir(const WCHAR* dirName);

    virtual Bitmap* LoadBitmap(int pageNo, bool& deleteAfterUse);
    virtual RectD LoadMediabox(int pageNo);

    WStrVec pageFileNames;
    DocTocTree* tocTree = nullptr;
};

bool ImageDirEngineImpl::LoadImageDir(const WCHAR* dirName) {
    SetFileName(dirName);

    AutoFreeW pattern(path::Join(dirName, L"*"));

    WIN32_FIND_DATA fdata;
    HANDLE hfind = FindFirstFile(pattern, &fdata);
    if (INVALID_HANDLE_VALUE == hfind)
        return false;

    do {
        if (!(fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            if (IsImageEngineSupportedFile(fdata.cFileName))
                pageFileNames.Append(path::Join(dirName, fdata.cFileName));
        }
    } while (FindNextFile(hfind, &fdata));
    FindClose(hfind);

    if (pageFileNames.size() == 0) {
        return false;
    }
    pageFileNames.SortNatural();

    mediaboxes.AppendBlanks(pageFileNames.size());

    // TODO: better handle the case where images have different resolutions
    ImagePage* page = GetPage(1);
    if (page) {
        fileDPI = page->bmp->GetHorizontalResolution();
        DropPage(page);
    }

    return true;
}

WCHAR* ImageDirEngineImpl::GetPageLabel(int pageNo) const {
    if (pageNo < 1 || PageCount() < pageNo) {
        return BaseEngine::GetPageLabel(pageNo);
    }

    const WCHAR* fileName = path::GetBaseNameNoFree(pageFileNames.at(pageNo - 1));
    return str::DupN(fileName, path::GetExt(fileName) - fileName);
}

int ImageDirEngineImpl::GetPageByLabel(const WCHAR* label) const {
    for (size_t i = 0; i < pageFileNames.size(); i++) {
        const WCHAR* fileName = path::GetBaseNameNoFree(pageFileNames.at(i));
        const WCHAR* fileExt = path::GetExt(fileName);
        if (str::StartsWithI(fileName, label) &&
            (fileName + str::Len(label) == fileExt || fileName[str::Len(label)] == '\0'))
            return (int)i + 1;
    }

    return BaseEngine::GetPageByLabel(label);
}

class ImageDirTocItem : public DocTocItem {
  public:
    ImageDirTocItem(WCHAR* title, int pageNo) : DocTocItem(title, pageNo) {
    }

    PageDestination* GetLink() override {
        return nullptr;
    }
};

DocTocTree* ImageDirEngineImpl::GetTocTree() {
    if (tocTree) {
        return tocTree;
    }
    DocTocItem* root = new ImageDirTocItem(GetPageLabel(1), 1);
    root->id = 1;
    for (int i = 2; i <= PageCount(); i++) {
        DocTocItem* item = new ImageDirTocItem(GetPageLabel(i), i);
        item->id = i;
        root->AddSibling(item);
    }
    tocTree = new DocTocTree(root);
    return tocTree;
}

bool ImageDirEngineImpl::SaveFileAs(const char* copyFileName, bool includeUserAnnots) {
    UNUSED(includeUserAnnots);
    // only copy the files if the target directory doesn't exist yet
    AutoFreeW dstPath(str::conv::FromUtf8(copyFileName));
    if (!CreateDirectoryW(dstPath, nullptr)) {
        return false;
    }
    bool ok = true;
    for (size_t i = 0; i < pageFileNames.size(); i++) {
        const WCHAR* filePathOld = pageFileNames.at(i);
        AutoFreeW filePathNew(path::Join(dstPath, path::GetBaseNameNoFree(filePathOld)));
        ok = ok && CopyFileW(filePathOld, filePathNew, TRUE);
    }
    return ok;
}

Bitmap* ImageDirEngineImpl::LoadBitmap(int pageNo, bool& deleteAfterUse) {
    OwnedData bmpData(file::ReadFile(pageFileNames.at(pageNo - 1)));
    if (bmpData.data) {
        deleteAfterUse = true;
        return BitmapFromData(bmpData.data, bmpData.size);
    }
    return nullptr;
}

RectD ImageDirEngineImpl::LoadMediabox(int pageNo) {
    OwnedData bmpData(file::ReadFile(pageFileNames.at(pageNo - 1)));
    if (bmpData.data) {
        Size size = BitmapSizeFromData(bmpData.data, bmpData.size);
        return RectD(0, 0, size.Width, size.Height);
    }
    return RectD();
}

bool ImageDirEngineImpl::SaveFileAsPDF(const char* pdfFileName, bool includeUserAnnots) {
    UNUSED(includeUserAnnots);
    bool ok = true;
    PdfCreator* c = new PdfCreator();
    for (int i = 1; i <= PageCount() && ok; i++) {
        OwnedData data(file::ReadFile(pageFileNames.at(i - 1)));
        ok = data.data && c->AddImagePage(data.data, data.size, GetFileDPI());
    }
    ok = ok && c->SaveToFile(pdfFileName);
    delete c;
    return ok;
}

BaseEngine* ImageDirEngineImpl::CreateFromFile(const WCHAR* fileName) {
    AssertCrash(dir::Exists(fileName));
    ImageDirEngineImpl* engine = new ImageDirEngineImpl();
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

BaseEngine* CreateImageDirEngineFromFile(const WCHAR* fileName) {
    return ImageDirEngineImpl::CreateFromFile(fileName);
}

///// CbxEngine handles comic book files (either .cbz, .cbr, .cb7 or .cbt) /////

class CbxEngineImpl : public ImagesEngine, public json::ValueVisitor {
  public:
    CbxEngineImpl(MultiFormatArchive* arch) : cbxFile(arch) {
        kind = kindEngineComicBooks;
    }
    virtual ~CbxEngineImpl() {
        delete cbxFile;
    }

    virtual BaseEngine* Clone() override {
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

    bool SaveFileAsPDF(const char* pdfFileName, bool includeUserAnnots = false) override;

    WCHAR* GetProperty(DocumentProperty prop) override;

    const WCHAR* GetDefaultFileExt() const;

    // json::ValueVisitor
    bool Visit(const char* path, const char* value, json::DataType type) override;

    static BaseEngine* CreateFromFile(const WCHAR* fileName);
    static BaseEngine* CreateFromStream(IStream* stream);

  protected:
    virtual Bitmap* LoadBitmap(int pageNo, bool& deleteAfterUse);
    virtual RectD LoadMediabox(int pageNo);

    bool LoadFromFile(const WCHAR* fileName);
    bool LoadFromStream(IStream* stream);
    bool FinishLoading();

    OwnedData GetImageData(int pageNo);
    void ParseComicInfoXml(const char* xmlData);

    // access to cbxFile must be protected after initialization (with cacheAccess)
    MultiFormatArchive* cbxFile;
    std::vector<MultiFormatArchive::FileInfo*> files;

    // extracted metadata
    AutoFreeW propTitle;
    WStrVec propAuthors;
    AutoFreeW propDate;
    AutoFreeW propModDate;
    AutoFreeW propCreator;
    AutoFreeW propSummary;
    // temporary state needed for extracting metadata
    AutoFreeW propAuthorTmp;
};

bool CbxEngineImpl::LoadFromFile(const WCHAR* file) {
    if (!file)
        return false;
    SetFileName(file);

    return FinishLoading();
}

bool CbxEngineImpl::LoadFromStream(IStream* stream) {
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

bool CbxEngineImpl::FinishLoading() {
    CrashIf(!cbxFile);
    if (!cbxFile) {
        return false;
    }

    // not using the resolution of the contained images seems to be
    // expected, cf. http://forums.fofou.org/sumatrapdf/topic?id=3183827
    // TODO: return win::GetHwndDpi(HWND_DESKTOP) instead?
    fileDPI = 96.f;

    defaultFileExt = GetDefaultFileExt();

    std::vector<MultiFormatArchive::FileInfo*> pageFiles;

    auto& fileInfos = cbxFile->GetFileInfos();
    for (auto* fileInfo : fileInfos) {
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

    OwnedData metadata(cbxFile->GetFileDataByName("ComicInfo.xml"));
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
    return true;
}

OwnedData CbxEngineImpl::GetImageData(int pageNo) {
    CrashIf((pageNo < 1) || (pageNo > PageCount()));
    ScopedCritSec scope(&cacheAccess);
    size_t fileId = files[pageNo - 1]->fileId;
    return cbxFile->GetFileDataById(fileId);
}

static char* GetTextContent(HtmlPullParser& parser) {
    HtmlToken* tok = parser.Next();
    if (!tok || !tok->IsText())
        return nullptr;
    return ResolveHtmlEntities(tok->s, tok->sLen);
}

// extract ComicInfo.xml metadata
// cf. http://comicrack.cyolito.com/downloads/comicrack/ComicRack/Support-Files/ComicInfoSchema.zip/
void CbxEngineImpl::ParseComicInfoXml(const char* xmlData) {
    // TODO: convert UTF-16 data and skip UTF-8 BOM
    HtmlPullParser parser(xmlData, str::Len(xmlData));
    HtmlToken* tok;
    while ((tok = parser.Next()) != nullptr && !tok->IsError()) {
        if (!tok->IsStartTag())
            continue;
        if (tok->NameIs("Title")) {
            AutoFreeStr value(GetTextContent(parser));
            if (value)
                Visit("/ComicBookInfo/1.0/title", value, json::Type_String);
        } else if (tok->NameIs("Year")) {
            AutoFreeStr value(GetTextContent(parser));
            if (value)
                Visit("/ComicBookInfo/1.0/publicationYear", value, json::Type_Number);
        } else if (tok->NameIs("Month")) {
            AutoFreeStr value(GetTextContent(parser));
            if (value)
                Visit("/ComicBookInfo/1.0/publicationMonth", value, json::Type_Number);
        } else if (tok->NameIs("Summary")) {
            AutoFreeStr value(GetTextContent(parser));
            if (value)
                Visit("/X-summary", value, json::Type_String);
        } else if (tok->NameIs("Writer")) {
            AutoFreeStr value(GetTextContent(parser));
            if (value) {
                Visit("/ComicBookInfo/1.0/credits[0]/person", value, json::Type_String);
                Visit("/ComicBookInfo/1.0/credits[0]/primary", "true", json::Type_Bool);
            }
        } else if (tok->NameIs("Penciller")) {
            AutoFreeStr value(GetTextContent(parser));
            if (value) {
                Visit("/ComicBookInfo/1.0/credits[1]/person", value, json::Type_String);
                Visit("/ComicBookInfo/1.0/credits[1]/primary", "true", json::Type_Bool);
            }
        }
    }
}

// extract ComicBookInfo metadata
// cf. http://code.google.com/p/comicbookinfo/
bool CbxEngineImpl::Visit(const char* path, const char* value, json::DataType type) {
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
        const char* prop = str::Parse(path, "/ComicBookInfo/1.0/credits[%d]/", &idx);
        if (prop) {
            if (json::Type_String == type && str::Eq(prop, "person"))
                propAuthorTmp.Set(str::conv::FromUtf8(value));
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

bool CbxEngineImpl::SaveFileAsPDF(const char* pdfFileName, bool includeUserAnnots) {
    UNUSED(includeUserAnnots);
    bool ok = true;
    PdfCreator* c = new PdfCreator();
    for (int i = 1; i <= PageCount() && ok; i++) {
        OwnedData data(GetImageData(i));
        ok = data.data && c->AddImagePage(data.data, data.size, GetFileDPI());
    }
    if (ok) {
        c->CopyProperties(this);
        ok = c->SaveToFile(pdfFileName);
    }
    delete c;
    return ok;
}

WCHAR* CbxEngineImpl::GetProperty(DocumentProperty prop) {
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

const WCHAR* CbxEngineImpl::GetDefaultFileExt() const {
    switch (cbxFile->format) {
        case MultiFormatArchive::Format::Zip:
            return L".cbz";
        case MultiFormatArchive::Format::Rar:
            return L".cbr";
        case MultiFormatArchive::Format::SevenZip:
            return L".cb7";
        case MultiFormatArchive::Format::Tar:
            return L".cbt";
        default:
            CrashIf(true);
            return nullptr;
    }
}

Bitmap* CbxEngineImpl::LoadBitmap(int pageNo, bool& deleteAfterUse) {
    OwnedData bmpData(GetImageData(pageNo));
    if (bmpData.data) {
        deleteAfterUse = true;
        return BitmapFromData(bmpData.data, bmpData.size);
    }
    return nullptr;
}

RectD CbxEngineImpl::LoadMediabox(int pageNo) {
    // fill the cache to prevent the first few images from being unpacked twice
    ImagePage* page = GetPage(pageNo, MAX_IMAGE_PAGE_CACHE == pageCache.size());
    if (page) {
        RectD mbox(0, 0, page->bmp->GetWidth(), page->bmp->GetHeight());
        DropPage(page);
        return mbox;
    }

    OwnedData bmpData(GetImageData(pageNo));
    if (bmpData.data) {
        Size size = BitmapSizeFromData(bmpData.data, bmpData.size);
        return RectD(0, 0, size.Width, size.Height);
    }
    return RectD();
}

#define RAR_SIGNATURE "Rar!\x1A\x07\x00"
#define RAR_SIGNATURE_LEN 7
#define RAR5_SIGNATURE "Rar!\x1A\x07\x01\x00"
#define RAR5_SIGNATURE_LEN 8

BaseEngine* CbxEngineImpl::CreateFromFile(const WCHAR* fileName) {
    if (str::EndsWithI(fileName, L".cbz") || str::EndsWithI(fileName, L".zip") ||
        file::StartsWithN(fileName, "PK\x03\x04", 4)) {
        auto* archive = OpenZipArchive(fileName, false);
        if (!archive) {
            return nullptr;
        }
        auto* engine = new CbxEngineImpl(archive);
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
            auto* engine = new CbxEngineImpl(archive);
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
            auto* engine = new CbxEngineImpl(archive);
            if (engine->LoadFromFile(fileName)) {
                return engine;
            }
            delete engine;
        }
    }
    if (str::EndsWithI(fileName, L".cbt") || str::EndsWithI(fileName, L".tar")) {
        MultiFormatArchive* archive = OpenTarArchive(fileName);
        if (archive) {
            auto* engine = new CbxEngineImpl(archive);
            if (engine->LoadFromFile(fileName)) {
                return engine;
            }
            delete engine;
        }
    }
    return nullptr;
}

BaseEngine* CbxEngineImpl::CreateFromStream(IStream* stream) {
    auto* archive = OpenZipArchive(stream, false);
    if (archive) {
        auto* engine = new CbxEngineImpl(archive);
        if (engine->LoadFromStream(stream)) {
            return engine;
        }
        delete engine;
    }

    archive = OpenRarArchive(stream);
    if (archive) {
        auto* engine = new CbxEngineImpl(archive);
        if (engine->LoadFromStream(stream)) {
            return engine;
        }
        delete engine;
    }

    archive = Open7zArchive(stream);
    if (archive) {
        auto* engine = new CbxEngineImpl(archive);
        if (engine->LoadFromStream(stream)) {
            return engine;
        }
        delete engine;
    }

    archive = OpenTarArchive(stream);
    if (archive) {
        auto* engine = new CbxEngineImpl(archive);
        if (engine->LoadFromStream(stream)) {
            return engine;
        }
        delete engine;
    }

    return nullptr;
}

bool IsCbxEngineSupportedFile(const WCHAR* fileName, bool sniff) {
    if (sniff) {
        // we don't also sniff for ZIP files, as these could also
        // be broken XPS files for which failure is expected
        // TODO: add TAR format sniffing
        return file::StartsWithN(fileName, RAR_SIGNATURE, RAR_SIGNATURE_LEN) ||
               file::StartsWithN(fileName, RAR5_SIGNATURE, RAR5_SIGNATURE_LEN) ||
               file::StartsWith(fileName, "7z\xBC\xAF\x27\x1C");
    }

    return str::EndsWithI(fileName, L".cbz") || str::EndsWithI(fileName, L".cbr") ||
           str::EndsWithI(fileName, L".cb7") || str::EndsWithI(fileName, L".cbt") ||
           str::EndsWithI(fileName, L".zip") && !str::EndsWithI(fileName, L".fb2.zip") ||
           str::EndsWithI(fileName, L".rar") || str::EndsWithI(fileName, L".7z") || str::EndsWithI(fileName, L".tar");
}

BaseEngine* CreateCbxEngineFromFile(const WCHAR* fileName) {
    return CbxEngineImpl::CreateFromFile(fileName);
}

BaseEngine* CreateCbxEngineFromStream(IStream* stream) {
    return CbxEngineImpl::CreateFromStream(stream);
}
