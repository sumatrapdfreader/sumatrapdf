/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/Archive.h"
#include "utils/ScopedWin.h"
#include "utils/FileUtil.h"
#include "utils/GuessFileType.h"
#include "utils/GdiPlusUtil.h"
#include "GumboHelpers.h"
#include "utils/JsonParser.h"
#include "utils/WinUtil.h"
#include "utils/Timer.h"
#include "utils/DirIter.h"

#include "wingui/UIModels.h"

extern "C" {
#include <mupdf/fitz.h>
}

#include "FzImgReader.h"
#include "DocProperties.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "PdfCreator.h"

#include "utils/Log.h"

using Gdiplus::ARGB;
using Gdiplus::Bitmap;
using Gdiplus::Color;
using Gdiplus::CompositingQualityHighQuality;
using Gdiplus::FrameDimensionPage;
using Gdiplus::FrameDimensionTime;
using Gdiplus::Graphics;
using Gdiplus::ImageAttributes;
using Gdiplus::InterpolationModeHighQualityBilinear;
using Gdiplus::Matrix;
using Gdiplus::MatrixOrderAppend;
using Gdiplus::Ok;
using Gdiplus::OutOfMemory;
using Gdiplus::PropertyItem;
using Gdiplus::SmoothingModeAntiAlias;
using Gdiplus::SolidBrush;
using Gdiplus::Status;
using Gdiplus::UnitPixel;
using Gdiplus::WrapModeTileFlipXY;

Kind kindEngineImage = "engineImage";
Kind kindEngineImageDir = "engineImageDir";
Kind kindEngineComicBooks = "engineComicBooks";

// number of decoded bitmaps to cache for quicker rendering.
// Sized for multi-threaded prefetch: enough to hold a few visible pages,
// the worker pool's in-flight pages, and a few prefetch slots without
// thrashing. Each cached entry holds the decoded GDI+ Bitmap, so the
// memory cost scales with image dimensions -- bump cautiously.
#define MAX_IMAGE_PAGE_CACHE 32

///// EngineImages methods apply to all types of engines handling full-page images /////

struct ImagePage {
    int pageNo = 0;
    // Decoded forms; at most one is non-null. img (mupdf, lazy) is preferred
    // for RenderPage when available: each render decodes the JPEG at near-
    // target scale (DCT-domain 1/2, 1/4, 1/8 downsampling), so big images
    // displayed at small zooms cost only a fraction of a full-res decode.
    // bmp (GDI+) is the fallback for paths that need GDI+ (multi-frame TIFF,
    // or rotation/tile cases the mupdf render path doesn't handle yet).
    Bitmap* bmp = nullptr;
    fz_image* img = nullptr;
    bool ownBmp = true;
    bool failedToLoad = false;
    // true while LoadBitmapForPage / LoadFzPixmapForPage is running on a worker;
    // concurrent GetPage callers wait on loadedEvent instead of serializing on cacheLock
    bool loading = false;

    // refcount: cache holds 1, every successful GetPage adds 1.
    // mutated atomically so DropPage's common case (refs > 0 after decrement)
    // doesn't need to acquire cacheLock. ++ stays under cacheLock in GetPage
    // because we need exclusion against eviction-in-progress.
    AtomicInt refs = 1;

    // manual-reset event, signaled when loading transitions to false
    HANDLE loadedEvent = nullptr;

    // serializes GDI+ DrawImage calls against this->bmp -- a single Bitmap*
    // is not safe to draw from multiple threads concurrently. Different pages
    // have different drawLocks so they render in parallel.
    // Not needed for the pix path: fz_pixmap is immutable after load and
    // fz_scale_pixmap is safe to call concurrently.
    CRITICAL_SECTION drawLock;

    ImagePage(int pageNo, Bitmap* bmp) {
        this->pageNo = pageNo;
        this->bmp = bmp;
        InitializeCriticalSection(&drawLock);
        loadedEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    }
    ~ImagePage() {
        DeleteCriticalSection(&drawLock);
        if (loadedEvent) {
            CloseHandle(loadedEvent);
        }
        // img is dropped by DropPage before delete (it needs the engine's
        // per-thread fz_context to call into mupdf safely).
        ReportIf(img);
    }
};

struct ImagePageInfo {
    Vec<IPageElement*> allElements;
    RectF mediabox{};
    PageInfoState state = PageInfoState::Unknown;
    // raw image bytes; populated lazily by GetImageData for file-backed
    // engines (EngineImage, EngineImageDir). Unused by EngineCbx (which
    // returns a view into the archive's cache).
    ByteSlice rawData;
    ImagePageInfo() = default;
    ~ImagePageInfo() { rawData.Free(); }
};

class EngineImages : public EngineBase {
  public:
    EngineImages();
    ~EngineImages() override;

    RectF PageMediabox(int pageNo) override;

    RenderedBitmap* RenderPage(RenderPageArgs& args) override;

    RectF Transform(const RectF& rect, int pageNo, float zoom, int rotation, bool inverse = false) override;

    ByteSlice GetFileData() override;
    bool SaveFileAs(const char* copyFileName) override;
    PageText ExtractPageText(int) override { return {}; }
    bool HasClipOptimizations(int) override { return false; }

    Vec<IPageElement*> GetElements(int pageNo) override;
    IPageElement* GetElementAtPos(int pageNo, PointF pt) override;

    RenderedBitmap* GetImageForPageElement(IPageElement*) override;

    bool BenchLoadPage(int pageNo) override {
        ImagePage* page = GetPage(pageNo);
        if (page) {
            DropPage(page, false);
        }
        return page != nullptr;
    }

    ScopedComPtr<IStream> fileStream;

    CRITICAL_SECTION cacheLock;
    Vec<ImagePage*> pageCache;
    Vec<ImagePageInfo*> pageInfos;

    // root mupdf context. Each thread that calls into mupdf gets its own
    // cloned context via Ctx() -- mupdf's per-context setjmp/error stack
    // is NOT thread-safe (concurrent fz_try would clobber each other's
    // jmp_buf and fz_throw would longjmp into a stale stack frame). Cloned
    // contexts share the underlying allocator/store/etc via refcounts.
    fz_context* fz_ctx = nullptr;
    struct ThreadCtx {
        DWORD threadID;
        fz_context* ctx;
    };
    Vec<ThreadCtx> threadCtxs;
    CRITICAL_SECTION threadCtxsLock;

    fz_context* Ctx();

    void GetTransform(Matrix& m, int pageNo, float zoom, int rotation);

    virtual Bitmap* LoadBitmapForPage(int pageNo, bool& deleteAfterUse) = 0;
    // Optional: load the page as an fz_image (encoded form, lazy decode).
    // RenderPage then asks mupdf to decode the JPEG at near-target scale on
    // each render -- much cheaper than decoding at full resolution and
    // scaling down. Default impl uses GetImageData + fz_new_image_from_buffer,
    // which works for any single-frame format mupdf understands (JPEG, PNG,
    // BMP, etc.). Subclasses can override to return nullptr (forcing the
    // GDI+ path) for cases mupdf can't handle -- e.g. multi-frame TIFFs.
    virtual fz_image* LoadFzImageForPage(fz_context* ctx, int pageNo);
    virtual RectF LoadMediabox(int pageNo) = 0;
    // Returns a non-owning view into engine-owned storage; the caller must
    // not free. Bytes stay valid until the engine is destroyed.
    virtual ByteSlice GetImageData(int pageNo) = 0;
    virtual TempStr GetImagePathTemp(int pageNo) { return nullptr; }

    ImagePage* GetPage(int pageNo, bool tryOnly = false);
    void DropPage(ImagePage* page, bool forceRemove);

    RectF PageContentBox(int pageNo, RenderTarget) override;
    void GetImageProperties(int pageNo, StrVec& keyValOut);
};

EngineImages::EngineImages() {
    kind = kindEngineImage;

    preferredLayout = PageLayout();
    preferredLayout.nonContinuous = true;
    isImageCollection = true;

    InitializeCriticalSection(&cacheLock);
    InitializeCriticalSection(&threadCtxsLock);
    fz_ctx = fz_new_context_windows();
}

fz_context* EngineImages::Ctx() {
    DWORD tid = GetCurrentThreadId();
    {
        ScopedCritSec scope(&threadCtxsLock);
        for (auto& tc : threadCtxs) {
            if (tc.threadID == tid) {
                return tc.ctx;
            }
        }
    }
    // clone outside the lock to avoid blocking other threads.
    // safe because only the current thread can register an entry for its own tid.
    fz_context* newCtx = fz_clone_context(fz_ctx);
    if (!newCtx) {
        return fz_ctx; // last-resort fallback; caller will serialize on the root
    }
    {
        ScopedCritSec scope(&threadCtxsLock);
        threadCtxs.Append({tid, newCtx});
    }
    return newCtx;
}

EngineImages::~EngineImages() {
    // drop per-thread cloned contexts BEFORE pages: workers are no longer
    // running by the time we destruct, so this just releases their refcounts
    // on the shared mupdf state. Pages then drop their pixmaps via the root
    // ctx in DropPage.
    for (auto& tc : threadCtxs) {
        fz_drop_context(tc.ctx);
    }
    threadCtxs.Reset();

    EnterCriticalSection(&cacheLock);
    while (pageCache.size() > 0) {
        ImagePage* lastPage = pageCache.Last();
        ReportIf(lastPage->refs != 1);
        DropPage(lastPage, true);
    }
    DeleteVecMembers(pageInfos);
    LeaveCriticalSection(&cacheLock);
    DeleteCriticalSection(&cacheLock);
    if (fz_ctx) {
        fz_drop_context_windows(fz_ctx);
    }
    DeleteCriticalSection(&threadCtxsLock);
}

// Wrap the page's raw image bytes in an fz_image for lazy mupdf decoding.
// The actual JPEG/PNG decode happens later in RenderPage at near-target
// scale, much cheaper than decoding at full resolution up front.
fz_image* EngineImages::LoadFzImageForPage(fz_context* ctx, int pageNo) {
    ByteSlice data = GetImageData(pageNo);
    if (data.empty()) {
        return nullptr;
    }
    fz_image* img = nullptr;
    fz_buffer* buf = nullptr;
    fz_var(buf);
    fz_var(img);
    fz_try(ctx) {
        // fz_new_buffer_from_copied_data takes ownership of the copy; the
        // resulting fz_image keeps a ref to the buffer, so it's safe to drop
        // the local buffer ref in fz_always.
        buf = fz_new_buffer_from_copied_data(ctx, data.data(), data.size());
        img = fz_new_image_from_buffer(ctx, buf);
    }
    fz_always(ctx) {
        fz_drop_buffer(ctx, buf);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        img = nullptr;
    }
    return img;
}

RectF EngineImages::PageMediabox(int pageNo) {
    ReportIf((pageNo < 1) || (pageNo > pageCount));
    int n = pageNo - 1;
    ImagePageInfo* pi = pageInfos[n];
    if (pi->state == PageInfoState::Unknown) {
        pi->mediabox = LoadMediabox(pageNo);
        pi->state = PageInfoState::Known;
    }
    return pi->mediabox;
}

// Wrap a fresh fz_pixmap into a RenderedBitmap (DIB section). Converts to
// 32bpp BGRA which is the GDI-compatible layout. The pixmap argument is
// consumed (dropped) on success; on failure it's also dropped.
static RenderedBitmap* FzPixmapToRenderedBitmap(fz_context* ctx, fz_pixmap* pixmap) {
    fz_pixmap* bgr = nullptr;
    fz_var(bgr);
    fz_try(ctx) {
        bgr = fz_convert_pixmap(ctx, pixmap, fz_device_bgr(ctx), nullptr, nullptr, fz_default_color_params, 1);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        return nullptr;
    }
    if (!bgr || !bgr->samples) {
        if (bgr) {
            fz_drop_pixmap(ctx, bgr);
        }
        return nullptr;
    }

    int w = bgr->w;
    int h = bgr->h;
    int n = bgr->n;
    int bitsCount = n * 8;
    // DIB rows are DWORD-aligned. mupdf pixmap rows are tightly packed
    // (stride = n * w). For 24bpp these often differ -- e.g. w=4001, n=3:
    // dibStride = 12004, pixmap stride = 12003. A bulk memcpy then offsets
    // every subsequent row by 1 byte and the image renders skewed.
    int dibStride = ((w * bitsCount + 31) / 32) * 4;
    int srcStride = (int)bgr->stride;
    int rowBytes = w * n;
    int imgSize = dibStride * h;

    BITMAPINFO bmi{};
    BITMAPINFOHEADER* bmih = &bmi.bmiHeader;
    bmih->biSize = sizeof(*bmih);
    bmih->biWidth = w;
    bmih->biHeight = -h;
    bmih->biPlanes = 1;
    bmih->biCompression = BI_RGB;
    bmih->biBitCount = bitsCount;
    bmih->biSizeImage = imgSize;

    void* data = nullptr;
    HANDLE hMap = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, imgSize, nullptr);
    HBITMAP hbmp = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &data, hMap, 0);
    if (data) {
        u8* dst = (u8*)data;
        u8* src = bgr->samples;
        if (srcStride == dibStride) {
            memcpy(dst, src, imgSize);
        } else {
            for (int y = 0; y < h; y++) {
                memcpy(dst + (size_t)y * dibStride, src + (size_t)y * srcStride, rowBytes);
            }
        }
    }
    fz_drop_pixmap(ctx, bgr);
    if (!hbmp) {
        if (hMap) {
            CloseHandle(hMap);
        }
        return nullptr;
    }
    return new RenderedBitmap(hbmp, Size(w, h), hMap);
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
        if (dur > 300.f) {
            logf("EngineImages::RenderPage() in %.2f ms\n", dur);
        }
    };

    RectF mediabox = PageMediabox(pageNo);
    RectF pageRc = pageRect ? *pageRect : mediabox;
    Rect screen = Transform(pageRc, pageNo, zoom, rotation).Round();

    // Mupdf fast path: rotation 0 + full-page render + cached fz_image available.
    // Tiles (pageRect != mediabox) and rotations fall through to the GDI+ path.
    if (page->img && rotation == 0 && !page->failedToLoad) {
        Rect mediaScreen = Transform(mediabox, pageNo, zoom, rotation).Round();
        bool isFullPage = (mediaScreen.dx == screen.dx && mediaScreen.dy == screen.dy);
        if (isFullPage) {
            // Per-thread cloned context lets multiple workers decode/scale concurrently.
            fz_context* ctx = Ctx();
            RenderedBitmap* result = nullptr;
            fz_pixmap* decoded = nullptr;
            fz_pixmap* scaled = nullptr;
            fz_var(decoded);
            fz_var(scaled);
            // Build a CTM mapping the fz_image unit box (1x1) to target
            // screen dimensions. mupdf reads |ctm| as the output pixel size
            // (w = sqrt(ctm.a^2 + ctm.b^2)) and picks a JPEG decode scale
            // (1, 1/2, 1/4, 1/8) so huge images at small zooms decode
            // dramatically faster.
            fz_matrix ctm = fz_scale((float)screen.dx, (float)screen.dy);
            fz_try(ctx) {
                int dw = 0, dh = 0;
                decoded = fz_get_pixmap_from_image(ctx, page->img, nullptr, &ctm, &dw, &dh);
                if (decoded && (decoded->w != screen.dx || decoded->h != screen.dy)) {
                    // mupdf decoded at a JPEG-friendly scale that's >= target;
                    // do the final exact-size scale on the much smaller pixmap.
                    scaled = fz_scale_pixmap(ctx, decoded, 0, 0, (float)screen.dx, (float)screen.dy, nullptr);
                }
            }
            fz_catch(ctx) {
                fz_report_error(ctx);
            }
            fz_pixmap* final = scaled ? scaled : decoded;
            if (final) {
                result = FzPixmapToRenderedBitmap(ctx, final);
            }
            if (scaled) {
                fz_drop_pixmap(ctx, scaled);
            }
            if (decoded) {
                fz_drop_pixmap(ctx, decoded);
            }
            if (result) {
                DropPage(page, false);
                return result;
            }
            // fall through to GDI+ on failure
        }
    }

    // GDI+ path: needs page->bmp. If we only have img (subclass loaded via
    // mupdf), lazy-load the GDI+ Bitmap on demand for this rare path
    // (rotation, sub-rect tile, or mupdf decode/scale failure).
    if (!page->bmp && !page->failedToLoad) {
        ScopedCritSec scope(&page->drawLock);
        if (!page->bmp) {
            bool ownBmp = true;
            page->bmp = LoadBitmapForPage(pageNo, ownBmp);
            page->ownBmp = ownBmp;
        }
    }

    Point screenTL = screen.TL();
    screen.Offset(-screen.x, -screen.y);

    HANDLE hMap = nullptr;
    HBITMAP hbmp = CreateMemoryBitmap(screen.Size(), &hMap);
    if (!hbmp) {
        DropPage(page, false);
        return nullptr;
    }
    HDC hDC = CreateCompatibleDC(nullptr);
    DeleteObject(SelectObject(hDC, hbmp));

    Graphics g(hDC);
    g.SetCompositingQuality(CompositingQualityHighQuality);
    if (this->disableAntiAlias) {
        g.SetSmoothingMode(Gdiplus::SmoothingModeNone);
        g.SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);
    } else {
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        // HighQualityBilinear is several times faster than HighQualityBicubic
        // and visually indistinguishable for typical photographic content,
        // especially when downscaling (the common case for image viewing).
        g.SetInterpolationMode(InterpolationModeHighQualityBilinear);
    }
    g.SetPageUnit(UnitPixel);

    Color white(0xFF, 0xFF, 0xFF);
    SolidBrush tmpBrush(white);
    Gdiplus::Rect screenR = ToGdipRect(screen);
    screenR.Inflate(1, 1);
    g.FillRectangle(&tmpBrush, screenR);

    if (page->failedToLoad) {
        // draw error message for pages that failed to load
        Gdiplus::Font font(L"Arial", 14.0f * zoom);
        Color red(0xFF, 0xCC, 0x00, 0x00);
        SolidBrush textBrush(red);
        Gdiplus::StringFormat sf;
        sf.SetAlignment(Gdiplus::StringAlignmentCenter);
        sf.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        Gdiplus::RectF layoutRect(0, 0, (float)screen.dx, (float)screen.dy);
        TempStr msg = str::FormatTemp("Failed to load page %d", pageNo);
        TempWStr msgW = ToWStrTemp(msg);
        g.DrawString(msgW, -1, &font, layoutRect, &sf, &textBrush);
        DropPage(page, false);
        DeleteDC(hDC);
        return new RenderedBitmap(hbmp, screen.Size(), hMap);
    }

    Matrix m;
    GetTransform(m, pageNo, zoom, rotation);
    m.Translate((float)-screenTL.x, (float)-screenTL.y, MatrixOrderAppend);
    g.SetTransform(&m);

    Rect pageRcI = PageMediabox(pageNo).Round();
    ImageAttributes imgAttrs;
    imgAttrs.SetWrapMode(WrapModeTileFlipXY);
    Status ok;
    {
        // GDI+ Bitmap is not thread-safe; concurrent DrawImage on the same Bitmap
        // from multiple threads causes InsufficientBuffer (status 4) errors.
        // Per-page lock: different pages render in parallel, only repeated draws
        // of the same page serialize.
        ScopedCritSec scope(&page->drawLock);
        ok = g.DrawImage(page->bmp, ToGdipRect(pageRcI), pageRcI.x, pageRcI.y, pageRcI.dx, pageRcI.dy, UnitPixel,
                         &imgAttrs);
    }

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

static IPageElement* NewImageElement(int pageNo, float dx, float dy) {
    auto res = new PageElementImage();
    res->pageNo = pageNo;
    res->rect = RectF(0, 0, dx, dy);
    res->imageID = pageNo;
    return res;
}

// don't delete the result
Vec<IPageElement*> EngineImages::GetElements(int pageNo) {
    ReportIf(pageNo < 1 || pageNo > pageCount);
    auto* pi = pageInfos[pageNo - 1];
    if (pi->allElements.size() > 0) {
        return pi->allElements;
    }
    auto mbox = PageMediabox(pageNo);

    float dx = mbox.dx;
    float dy = mbox.dy;
    auto el = NewImageElement(pageNo, dx, dy);
    pi->allElements.Append(el);
    return pi->allElements;
}

// don't delete the result
IPageElement* EngineImages::GetElementAtPos(int pageNo, PointF pt) {
    if (!PageMediabox(pageNo).Contains(pt)) {
        return nullptr;
    }
    auto els = GetElements(pageNo);
    if (els.size() == 0) {
        return nullptr;
    }
    IPageElement* el = els[0];
    return el;
}

RenderedBitmap* EngineImages::GetImageForPageElement(IPageElement* pel) {
    ReportIf(pel->GetKind() != kindPageElementImage);
    auto ipel = (PageElementImage*)pel;
    int pageNo = ipel->pageNo;
    auto page = GetPage(pageNo);
    if (!page || page->failedToLoad) {
        if (page) {
            DropPage(page, false);
        }
        return nullptr;
    }

    // mupdf fz_image path leaves page->bmp null; lazy-load the GDI+ Bitmap
    if (!page->bmp && !page->failedToLoad) {
        ScopedCritSec scope(&page->drawLock);
        if (!page->bmp) {
            bool ownBmp = true;
            page->bmp = LoadBitmapForPage(pageNo, ownBmp);
            page->ownBmp = ownBmp;
        }
    }
    if (!page->bmp) {
        DropPage(page, false);
        return nullptr;
    }

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

ByteSlice EngineImages::GetFileData() {
    return GetStreamOrFileData(fileStream.Get(), FilePath());
}

bool EngineImages::SaveFileAs(const char* dstPath) {
    const char* srcPath = FilePath();
    if (srcPath) {
        bool ok = file::Copy(dstPath, srcPath, false);
        if (ok) {
            return true;
        }
    }
    ByteSlice d = GetFileData();
    if (d.empty()) {
        return false;
    }
    return file::WriteFile(dstPath, d);
}

ImagePage* EngineImages::GetPage(int pageNo, bool tryOnly) {
    ImagePage* result = nullptr;
    bool isLoader = false;
    bool waitForLoad = false;

    {
        ScopedCritSec scope(&cacheLock);

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
            if (pageCache.size() >= MAX_IMAGE_PAGE_CACHE) {
                ReportIf(pageCache.size() != MAX_IMAGE_PAGE_CACHE);
                DropPage(pageCache.Last(), true);
            }
            // insert a loading placeholder; do the actual decode without
            // holding cacheLock so other threads can keep using the cache
            result = new ImagePage(pageNo, nullptr);
            result->loading = true;
            pageCache.InsertAt(0, result);
            isLoader = true;
        } else if (result != pageCache.at(0)) {
            // keep the list Most Recently Used first
            pageCache.Remove(result);
            pageCache.InsertAt(0, result);
        }

        if (!isLoader && result->loading) {
            waitForLoad = true;
        }
        // ++ under lock: prevents racing with eviction that would otherwise
        // delete the page between our lookup and our ref bump.
        AtomicIntInc(&result->refs);
    }

    if (isLoader) {
        // Slow path: load without cacheLock held. The page is pinned
        // (refs >= 2) so it can't be deleted under us, even if some other
        // thread evicts it from the cache while we're working.
        // Try the mupdf image path first (lazy decode at render time at near-
        // target scale); fall back to GDI+ Bitmap if the subclass opts out
        // or mupdf can't handle the format.
        fz_image* img = LoadFzImageForPage(Ctx(), pageNo);
        Bitmap* bmp = nullptr;
        bool ownBmp = true;
        if (!img) {
            bmp = LoadBitmapForPage(pageNo, ownBmp);
        }
        {
            ScopedCritSec scope(&cacheLock);
            result->img = img;
            result->bmp = bmp;
            result->ownBmp = ownBmp;
            if (!img && !bmp) {
                result->failedToLoad = true;
            }
            result->loading = false;
        }
        SetEvent(result->loadedEvent);
    } else if (waitForLoad) {
        // Another thread is decoding this same page; wait for it to finish.
        WaitForSingleObject(result->loadedEvent, INFINITE);
    }

    return result;
}

void EngineImages::DropPage(ImagePage* page, bool forceRemove) {
    int newRefs = AtomicIntDec(&page->refs);
    ReportIf(newRefs < 0);

    // common case: ref still held by someone (typically the cache) -- no lock,
    // no removal, just return. This is the hot path during render workloads.
    if (newRefs > 0 && !forceRemove) {
        return;
    }

    {
        ScopedCritSec scope(&cacheLock);
        // pageCache.Remove is a no-op if the page was already evicted earlier
        pageCache.Remove(page);
    }

    if (newRefs == 0) {
        if (page->ownBmp) {
            delete page->bmp;
        }
        if (page->img) {
            // safe across threads: fz_drop_image uses our per-thread cloned
            // ctx for atomic refcount + dealloc via mupdf's locks callbacks.
            fz_drop_image(Ctx(), page->img);
            page->img = nullptr;
        }
        delete page;
    }
}

// Get content box for image by cropping out margins of similar color
RectF EngineImages::PageContentBox(int pageNo, RenderTarget target) {
    // try to load bitmap for the image
    auto page = GetPage(pageNo, true);
    if (!page) return RectF{};
    defer {
        DropPage(page, false);
    };

    auto bmp = page->bmp;
    if (!bmp) return RectF{};

    const int w = bmp->GetWidth(), h = bmp->GetHeight();

    // Handle degenerate cases where the image is too small for margin detection
    // Minimum sensible dimension for margin cropping is about 10 pixels
    if (w < 10 || h < 10) {
        return RectF(0, 0, (float)w, (float)h);
    }

    // don't need pixel-perfect margin, so scan 200 points at most
    const int deltaX = std::max(1, w / 200), deltaY = std::max(1, h / 200);

    Rect r(0, 0, w, h);

    auto fmt = bmp->GetPixelFormat();
    // getPixel can work with the following formats, otherwise convert it to 24bppRGB
    switch (fmt) {
        case PixelFormat24bppRGB:
        case PixelFormat32bppRGB:
        case PixelFormat32bppARGB:
        case PixelFormat32bppPARGB:
            break;
        default:
            fmt = PixelFormat24bppRGB;
    }
    const int bytesPerPixel = ((fmt >> 8) & 0xff) / 8; // either 3 or 4

    Gdiplus::BitmapData bmpData;
    // lock bitmap
    {
        Gdiplus::Rect bmpRect(0, 0, w, h);
        Gdiplus::Status lock = bmp->LockBits(&bmpRect, Gdiplus::ImageLockModeRead, fmt, &bmpData);
        if (lock != Gdiplus::Ok) return RectF{};
    }

    auto getPixel = [&bmpData, bytesPerPixel](int x, int y) -> uint32_t {
        ReportIf(x < 0 || x >= (int)bmpData.Width || y < 0 || y >= (int)bmpData.Height);
        auto data = static_cast<const uint8_t*>(bmpData.Scan0);
        unsigned idx = bytesPerPixel * x + bmpData.Stride * y;
        uint32_t rgb = (data[idx + 2] << 16) | (data[idx + 1] << 8) | data[idx];
        // ignore the lowest 3 bits (7=0b111) of each color component
        return rgb & (~0x070707U);
    };

    uint32_t marginColor;
    // crop the page, but no more than 25% from each side

    // left margin
    marginColor = getPixel(0, h / 2);
    for (; r.x < w / 4 && r.dx > w / 2; r.x += deltaX, r.dx -= deltaX) {
        bool ok = true;
        for (int y = 0; y <= h - deltaY; y += deltaY) {
            ok = getPixel(r.x + deltaX, y) == marginColor;
            if (!ok) break;
        }
        if (!ok) break;
    }

    // right margin
    marginColor = getPixel(w - 1, h / 2);
    for (; r.dx > w / 2; r.dx -= deltaX) {
        bool ok = true;
        for (int y = 0; y <= h - deltaY; y += deltaY) {
            ok = getPixel((r.x + r.dx) - 1 - deltaX, y) == marginColor;
            if (!ok) break;
        }
        if (!ok) break;
    }

    // top margin
    marginColor = getPixel(w / 2, 0);
    for (; r.y < h / 4 && r.dy > h / 2; r.y += deltaY, r.dy -= deltaY) {
        bool ok = true;
        for (int x = r.x; x <= r.x + r.dx - deltaX; x += deltaX) {
            ok = getPixel(x, r.y + deltaY) == marginColor;
            if (!ok) break;
        }
        if (!ok) break;
    }

    // bottom margin
    marginColor = getPixel(w / 2, h - 1);
    for (; r.dy > h / 2; r.dy -= deltaY) {
        bool ok = true;
        for (int x = r.x; x <= r.x + r.dx - deltaX; x += deltaX) {
            ok = getPixel(x, (r.y + r.dy) - 1 - deltaY) == marginColor;
            if (!ok) break;
        }
        if (!ok) break;
    }
    bmp->UnlockBits(&bmpData);

    return ToRectF(r);
}

///// ImageEngine handles a single image file /////

class EngineImage : public EngineImages {
  public:
    EngineImage();
    ~EngineImage() override;

    EngineBase* Clone() override;

    TempStr GetPropertyTemp(const char* name) override;
    void GetProperties(StrVec& keyValOut) override;
    void GetImageProperties(int pageNo, StrVec& keyValOut);

    static EngineBase* CreateFromFile(const char* fileName);
    static EngineBase* CreateFromStream(IStream* stream);

    Bitmap* image = nullptr;
    Kind imageFormat = nullptr;

    bool LoadSingleFile(const char* fileName);
    bool LoadFromStream(IStream* stream);
    bool FinishLoading();

    Bitmap* LoadBitmapForPage(int pageNo, bool& deleteAfterUse) override;
    fz_image* LoadFzImageForPage(fz_context* ctx, int pageNo) override;
    RectF LoadMediabox(int pageNo) override;
    ByteSlice GetImageData(int pageNo) override;
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
        logf("EngineImage::Clone() failed: Bitmap::Clone() failed for '%s'\n", FilePath() ? FilePath() : "(null)");
        return nullptr;
    }

    EngineImage* clone = new EngineImage();
    clone->SetFilePath(FilePath());
    clone->defaultExt = str::Dup(defaultExt);
    clone->imageFormat = imageFormat;
    clone->fileDPI = fileDPI;
    if (fileStream) {
        fileStream->Clone(&clone->fileStream);
    }
    clone->image = bmp;
    clone->FinishLoading();

    return clone;
}

bool EngineImage::LoadSingleFile(const char* path) {
    if (!path) {
        return false;
    }
    SetFilePath(path);

    ByteSlice data = file::ReadFile(path);
    imageFormat = GuessFileTypeFromContent(data);
    if (imageFormat == nullptr) {
        imageFormat = GuessFileTypeFromName(path);
    }
    if (imageFormat == nullptr) {
        logfa("EngineImage::LoadSingleFile: '%s'\n", path);
        ReportIf(imageFormat == nullptr);
    }

    // TODO: maybe default to file extension and only use detected from content
    // if no extension?
    const char* fileExt = GfxFileExtFromData(data);
    if (fileExt == nullptr) {
        // imageFormat already holds the Kind we resolved above; skip the
        // redundant GuessFileTypeFromName call.
        fileExt = GfxFileExtFromKind(imageFormat);
    }
    if (fileExt == nullptr) {
        fileExt = path::GetExtTemp(path);
    }
    if (fileExt == nullptr) {
        fileExt = "";
    }
    str::ReplaceWithCopy(&defaultExt, fileExt);
    image = BitmapFromData(data);
    bool ok = FinishLoading();
    if (ok) {
        pageInfos[0]->rawData = data;
    } else {
        data.Free();
    }
    return ok;
}

bool EngineImage::LoadFromStream(IStream* stream) {
    if (!stream) {
        return false;
    }
    fileStream = stream;
    fileStream->AddRef();

    const char* fileExtA = nullptr;
    u8 header[18];
    if (ReadDataFromStream(stream, header, sizeof(header))) {
        ByteSlice d = {header, sizeof(header)};
        fileExtA = GfxFileExtFromData(d);
    }
    if (!fileExtA) {
        return false;
    }
    str::ReplaceWithCopy(&defaultExt, path::GetExtTemp(fileExtA));

    ByteSlice data = GetDataFromStream(stream, nullptr);
    image = BitmapFromData(data);
    bool ok = FinishLoading();
    if (ok) {
        pageInfos[0]->rawData = data;
    } else {
        data.Free();
    }
    return ok;
}

static bool IsMultiImage(Kind fmt) {
    return (fmt == kindFileTiff) || (fmt == kindFileGif);
}

static void ReportIfNotMultiImage(EngineImage* e) {
    Kind fmt = e->imageFormat;
    if (IsMultiImage(fmt)) {
        return;
    }
    logfa("EngineImage::LoadBitmapForPage: trying for non-multi image, %s, path: '%s'\n", fmt, e->FilePath());
    ReportIf(true);
}

bool EngineImage::FinishLoading() {
    if (!image || image->GetLastStatus() != Ok) {
        return false;
    }
    fileDPI = image->GetHorizontalResolution();

    auto pi = new ImagePageInfo();
    pi->mediabox = RectF(0, 0, (float)image->GetWidth(), (float)image->GetHeight());
    pageInfos.Append(pi);
    pi->state = PageInfoState::Known;
    ReportIf(pageInfos.size() != 1);

    // extract all frames from multi-page TIFFs and animated GIFs
    // TODO: do the same for .avif and .heic formats
    if (IsMultiImage(imageFormat)) {
        const GUID* dim = imageFormat == kindFileTiff ? &FrameDimensionPage : &FrameDimensionTime;
        int nFrames = image->GetFrameCount(dim) - 1;
        for (int i = 0; i < nFrames; i++) {
            pi = new ImagePageInfo();
            pageInfos.Append(pi);
        }
    }
    pageCount = pageInfos.Size();

    return pageCount > 0;
}

#ifndef PropertyTagXPTitle
#define PropertyTagXPTitle 0x9c9b
#define PropertyTagXPComment 0x9c9c
#define PropertyTagXPAuthor 0x9c9d
#define PropertyTagXPKeywords 0x9c9e
#define PropertyTagXPSubject 0x9c9f
#endif

static bool GetImagePropertyItem(Bitmap* bmp, PROPID id, PropertyItem** itemOut) {
    uint size = bmp->GetPropertyItemSize(id);
    if (size == 0) {
        return false;
    }
    PropertyItem* item = (PropertyItem*)malloc(size);
    if (!item) {
        return false;
    }
    Status ok = bmp->GetPropertyItem(id, size, item);
    if (ok != Ok) {
        free(item);
        return false;
    }
    *itemOut = item;
    return true;
}

// get a rational property as numerator/denominator
static bool GetImagePropertyRational(Bitmap* bmp, PROPID id, ULONG& num, ULONG& den) {
    PropertyItem* item = nullptr;
    if (!GetImagePropertyItem(bmp, id, &item)) {
        return false;
    }
    bool ok = (item->type == PropertyTagTypeRational) && (item->length >= 8);
    if (ok) {
        num = ((ULONG*)item->value)[0];
        den = ((ULONG*)item->value)[1];
    }
    free(item);
    return ok;
}

// get a short/long integer property
static bool GetImagePropertyLong(Bitmap* bmp, PROPID id, ULONG& val) {
    PropertyItem* item = nullptr;
    if (!GetImagePropertyItem(bmp, id, &item)) {
        return false;
    }
    bool ok = false;
    if (item->type == PropertyTagTypeShort && item->length >= 2) {
        val = *(USHORT*)item->value;
        ok = true;
    } else if (item->type == PropertyTagTypeLong && item->length >= 4) {
        val = *(ULONG*)item->value;
        ok = true;
    }
    free(item);
    return ok;
}

static TempStr GetImagePropertyTemp(Bitmap* bmp, PROPID id, PROPID altId = 0) {
    TempStr value = nullptr;
    uint size = bmp->GetPropertyItemSize(id);
    if (size == 0) {
        return altId == 0 ? nullptr : GetImagePropertyTemp(bmp, altId);
    }
    PropertyItem* item = (PropertyItem*)malloc(size);
    if (!item) return nullptr;
    AutoFree freeItem((const char*)item);
    Status ok = bmp->GetPropertyItem(id, size, item);
    if (Ok != ok) {
        /* property didn't exist */;
        return altId == 0 ? nullptr : GetImagePropertyTemp(bmp, altId);
    } else if (PropertyTagTypeASCII == item->type) {
        value = strconv::AnsiToUtf8Temp((char*)item->value, (size_t)size);
    } else if (PropertyTagTypeByte == item->type && item->length > 0 && 0 == (item->length % 2) &&
               !((WCHAR*)item->value)[item->length / 2 - 1]) {
        value = ToUtf8Temp((WCHAR*)item->value);
    }
    if (str::IsEmptyOrWhiteSpace(value)) {
        return altId == 0 ? nullptr : GetImagePropertyTemp(bmp, altId);
    }
    return value;
}

// load bitmap using GDI+ Bitmap::FromStream which preserves EXIF metadata
// BitmapFromData() uses WIC which decodes to raw pixels, losing EXIF
static Bitmap* BitmapWithExifFromData(const ByteSlice& data) {
    if (data.empty()) {
        return nullptr;
    }
    IStream* strm = CreateStreamFromData(data);
    if (!strm) {
        return nullptr;
    }
    Bitmap* bmp = Gdiplus::Bitmap::FromStream(strm);
    strm->Release();
    if (bmp && bmp->GetLastStatus() != Ok) {
        delete bmp;
        return nullptr;
    }
    return bmp;
}

static Bitmap* BitmapWithExifFromFile(const char* path) {
    if (!path) {
        return nullptr;
    }
    ByteSlice data = file::ReadFile(path);
    Bitmap* bmp = BitmapWithExifFromData(data);
    data.Free();
    return bmp;
}

TempStr EngineImage::GetPropertyTemp(const char* name) {
    Bitmap* bmp = BitmapWithExifFromFile(FilePath());
    TempStr res = nullptr;
    if (bmp) {
        if (str::Eq(name, kPropTitle)) {
            res = GetImagePropertyTemp(bmp, PropertyTagImageDescription, PropertyTagXPTitle);
        } else if (str::Eq(name, kPropSubject)) {
            res = GetImagePropertyTemp(bmp, PropertyTagXPSubject);
        } else if (str::Eq(name, kPropAuthor)) {
            res = GetImagePropertyTemp(bmp, PropertyTagArtist, PropertyTagXPAuthor);
        } else if (str::Eq(name, kPropCopyright)) {
            res = GetImagePropertyTemp(bmp, PropertyTagCopyright);
        } else if (str::Eq(name, kPropCreationDate)) {
            res = GetImagePropertyTemp(bmp, PropertyTagDateTime, PropertyTagExifDTDigitized);
        } else if (str::Eq(name, kPropCreatorApp)) {
            res = GetImagePropertyTemp(bmp, PropertyTagSoftwareUsed);
        }
        delete bmp;
    }
    return res;
}

static void GetBitmapExifProperties(Bitmap* bmp, StrVec& keyValOut) {
    TempStr val;

    // image dimensions
    uint w = bmp->GetWidth();
    uint h = bmp->GetHeight();
    if (w > 0 && h > 0) {
        val = str::FormatTemp("%u x %u", w, h);
        AddProp(keyValOut, kPropImageSize, val);
    }

    // DPI
    float dpiX = bmp->GetHorizontalResolution();
    float dpiY = bmp->GetVerticalResolution();
    if (dpiX > 0 && dpiY > 0) {
        if (dpiX == dpiY) {
            val = str::FormatTemp("%.0f", dpiX);
        } else {
            val = str::FormatTemp("%.0f x %.0f", dpiX, dpiY);
        }
        AddProp(keyValOut, kPropDpi, val);
    }

    // keywords
    val = GetImagePropertyTemp(bmp, PropertyTagXPKeywords);
    if (val) {
        AddProp(keyValOut, kPropKeywords, val);
    }

    // comment
    val = GetImagePropertyTemp(bmp, PropertyTagXPComment);
    if (val) {
        AddProp(keyValOut, kPropComment, val);
    }

    // camera make and model
    val = GetImagePropertyTemp(bmp, PropertyTagEquipMake);
    if (val) {
        AddProp(keyValOut, kPropCameraMake, val);
    }
    val = GetImagePropertyTemp(bmp, PropertyTagEquipModel);
    if (val) {
        AddProp(keyValOut, kPropCameraModel, val);
    }

    // date original
    val = GetImagePropertyTemp(bmp, PropertyTagExifDTOrig);
    if (val) {
        AddProp(keyValOut, kPropDateOriginal, val);
    }

    // exposure time
    ULONG num, den;
    if (GetImagePropertyRational(bmp, PropertyTagExifExposureTime, num, den)) {
        if (den > 0 && num > 0) {
            if (num == 1) {
                val = str::FormatTemp("1/%u s", den);
            } else {
                val = str::FormatTemp("%u/%u s", num, den);
            }
            AddProp(keyValOut, kPropExposureTime, val);
        }
    }

    // f-number
    if (GetImagePropertyRational(bmp, PropertyTagExifFNumber, num, den)) {
        if (den > 0) {
            float fNum = (float)num / (float)den;
            val = str::FormatTemp("f/%.1f", fNum);
            AddProp(keyValOut, kPropFNumber, val);
        }
    }

    // ISO speed
    ULONG isoVal;
    if (GetImagePropertyLong(bmp, PropertyTagExifISOSpeed, isoVal)) {
        val = str::FormatTemp("ISO %u", isoVal);
        AddProp(keyValOut, kPropIsoSpeed, val);
    }

    // focal length
    if (GetImagePropertyRational(bmp, PropertyTagExifFocalLength, num, den)) {
        if (den > 0) {
            float fl = (float)num / (float)den;
            val = str::FormatTemp("%.1f mm", fl);
            AddProp(keyValOut, kPropFocalLength, val);
        }
    }

    // focal length in 35mm equivalent
    ULONG fl35;
    if (GetImagePropertyLong(bmp, PropertyTagExifFocalLengthIn35mmFilm, fl35)) {
        val = str::FormatTemp("%u mm", fl35);
        AddProp(keyValOut, kPropFocalLength35mm, val);
    }

    // flash
    ULONG flashVal;
    if (GetImagePropertyLong(bmp, PropertyTagExifFlash, flashVal)) {
        const char* flashStr = (flashVal & 1) ? "Yes" : "No";
        AddProp(keyValOut, kPropFlash, flashStr);
    }

    // orientation
    ULONG orient;
    if (GetImagePropertyLong(bmp, PropertyTagOrientation, orient)) {
        val = str::FormatTemp("%u", orient);
        AddProp(keyValOut, kPropOrientation, val);
    }

    // exposure program
    ULONG expProg;
    if (GetImagePropertyLong(bmp, PropertyTagExifExposureProg, expProg)) {
        // clang-format off
        static const char* exposurePrograms[] = {
            "Not defined", "Manual", "Normal program", "Aperture priority",
            "Shutter priority", "Creative program", "Action program",
            "Portrait mode", "Landscape mode",
        };
        // clang-format on
        if (expProg < dimof(exposurePrograms)) {
            AddProp(keyValOut, kPropExposureProgram, exposurePrograms[expProg]);
        }
    }

    // metering mode
    ULONG metering;
    if (GetImagePropertyLong(bmp, PropertyTagExifMeteringMode, metering)) {
        // clang-format off
        static const char* meteringModes[] = {
            "Unknown", "Average", "Center Weighted Average", "Spot",
            "Multi Spot", "Pattern", "Partial",
        };
        // clang-format on
        if (metering < dimof(meteringModes)) {
            AddProp(keyValOut, kPropMeteringMode, meteringModes[metering]);
        }
    }

    // white balance
    ULONG wb;
    if (GetImagePropertyLong(bmp, PropertyTagExifWhiteBalance, wb)) {
        AddProp(keyValOut, kPropWhiteBalance, wb == 0 ? "Auto" : "Manual");
    }

    // exposure bias
    if (GetImagePropertyRational(bmp, PropertyTagExifExposureBias, num, den)) {
        if (den > 0) {
            float bias = (float)(LONG)num / (float)(LONG)den;
            val = str::FormatTemp("%+.1f EV", bias);
            AddProp(keyValOut, kPropExposureBias, val);
        }
    }

    // bits per sample
    ULONG bps;
    if (GetImagePropertyLong(bmp, PropertyTagBitsPerSample, bps)) {
        val = str::FormatTemp("%u", bps);
        AddProp(keyValOut, kPropBitsPerSample, val);
    }

    // resolution unit
    ULONG resUnit;
    if (GetImagePropertyLong(bmp, PropertyTagResolutionUnit, resUnit)) {
        // clang-format off
        const char* unitStr = nullptr;
        if (resUnit == 2) { unitStr = "inches"; }
        else if (resUnit == 3) { unitStr = "centimeters"; }
        else { unitStr = "unknown"; }
        // clang-format on
        AddProp(keyValOut, kPropResolutionUnit, unitStr);
    }

    // software
    val = GetImagePropertyTemp(bmp, PropertyTagSoftwareUsed);
    if (val) {
        AddProp(keyValOut, kPropSoftware, val);
    }

    // date/time
    val = GetImagePropertyTemp(bmp, PropertyTagDateTime);
    if (val) {
        AddProp(keyValOut, kPropDateTime, val);
    }

    // YCbCr positioning
    ULONG ycbcrPos;
    if (GetImagePropertyLong(bmp, PropertyTagYCbCrPositioning, ycbcrPos)) {
        const char* posStr = (ycbcrPos == 1) ? "centered" : "co-sited";
        AddProp(keyValOut, kPropYCbCrPositioning, posStr);
    }

    // exif version
    {
        PropertyItem* item = nullptr;
        if (GetImagePropertyItem(bmp, PropertyTagExifVer, &item)) {
            if (item->length >= 4) {
                val = str::FormatTemp("%.*s", (int)item->length, (char*)item->value);
                AddProp(keyValOut, kPropExifVersion, val);
            }
            free(item);
        }
    }

    // date/time digitized
    val = GetImagePropertyTemp(bmp, PropertyTagExifDTDigitized);
    if (val) {
        AddProp(keyValOut, kPropDateTimeDigitized, val);
    }

    // components configuration
    {
        PropertyItem* item = nullptr;
        if (GetImagePropertyItem(bmp, PropertyTagExifCompConfig, &item)) {
            // each byte: 0=does not exist, 1=Y, 2=Cb, 3=Cr, 4=R, 5=G, 6=B
            static const char* compNames[] = {"", "Y", "Cb", "Cr", "R", "G", "B"};
            StrBuilder s;
            u8* data = (u8*)item->value;
            for (ULONG i = 0; i < item->length; i++) {
                u8 c = data[i];
                if (c == 0) {
                    break;
                }
                if (s.size() > 0) {
                    s.Append(", ");
                }
                if (c < dimof(compNames)) {
                    s.Append(compNames[c]);
                }
            }
            if (s.size() > 0) {
                AddProp(keyValOut, kPropComponentsConfig, s.CStr());
            }
            free(item);
        }
    }

    // compressed bits per pixel
    if (GetImagePropertyRational(bmp, PropertyTagExifCompBPP, num, den)) {
        if (den > 0) {
            float cbpp = (float)num / (float)den;
            if (den == 1) {
                val = str::FormatTemp("%u", num);
            } else {
                val = str::FormatTemp("%.2f", cbpp);
            }
            AddProp(keyValOut, kPropCompressedBpp, val);
        }
    }

    // max aperture value
    if (GetImagePropertyRational(bmp, PropertyTagExifAperture, num, den)) {
        if (den > 0) {
            float aperture = (float)num / (float)den;
            val = str::FormatTemp("%.2f", aperture);
            AddProp(keyValOut, kPropMaxAperture, val);
        }
    }

    // light source
    ULONG lightSrc;
    if (GetImagePropertyLong(bmp, PropertyTagExifLightSource, lightSrc)) {
        // clang-format off
        const char* lightStr = nullptr;
        switch (lightSrc) {
            case 0:  lightStr = "Unknown"; break;
            case 1:  lightStr = "Daylight"; break;
            case 2:  lightStr = "Fluorescent"; break;
            case 3:  lightStr = "Tungsten"; break;
            case 4:  lightStr = "Flash"; break;
            case 9:  lightStr = "Fine weather"; break;
            case 10: lightStr = "Cloudy weather"; break;
            case 11: lightStr = "Shade"; break;
            case 17: lightStr = "Standard light A"; break;
            case 18: lightStr = "Standard light B"; break;
            case 19: lightStr = "Standard light C"; break;
            case 255: lightStr = "Other"; break;
            default: lightStr = "Unknown"; break;
        }
        // clang-format on
        AddProp(keyValOut, kPropLightSource, lightStr);
    }

    // user comment
    {
        PropertyItem* item = nullptr;
        if (GetImagePropertyItem(bmp, PropertyTagExifUserComment, &item)) {
            // first 8 bytes are character code identifier
            if (item->length > 8) {
                char* commentData = (char*)item->value + 8;
                ULONG commentLen = item->length - 8;
                // check if it's ASCII
                if (memcmp(item->value, "ASCII\0\0\0", 8) == 0) {
                    val = str::DupTemp(commentData, commentLen);
                    if (val && !str::IsEmpty(val)) {
                        AddProp(keyValOut, kPropUserComment, val);
                    }
                }
                // Unicode
                else if (memcmp(item->value, "UNICODE\0", 8) == 0) {
                    val = ToUtf8Temp((WCHAR*)commentData, commentLen / 2);
                    if (val && !str::IsEmpty(val)) {
                        AddProp(keyValOut, kPropUserComment, val);
                    }
                }
            }
            free(item);
        }
    }

    // flashpix version
    {
        PropertyItem* item = nullptr;
        if (GetImagePropertyItem(bmp, PropertyTagExifFPXVer, &item)) {
            if (item->length >= 4) {
                val = str::FormatTemp("%.*s", (int)item->length, (char*)item->value);
                AddProp(keyValOut, kPropFlashpixVersion, val);
            }
            free(item);
        }
    }

    // color space
    ULONG cs;
    if (GetImagePropertyLong(bmp, PropertyTagExifColorSpace, cs)) {
        const char* csStr = (cs == 1) ? "sRGB" : (cs == 0xFFFF) ? "Uncalibrated" : "Unknown";
        AddProp(keyValOut, kPropColorSpace, csStr);
    }

    // pixel X dimension
    ULONG pixX;
    if (GetImagePropertyLong(bmp, PropertyTagExifPixXDim, pixX)) {
        val = str::FormatTemp("%u", pixX);
        AddProp(keyValOut, kPropPixelXDimension, val);
    }

    // pixel Y dimension
    ULONG pixY;
    if (GetImagePropertyLong(bmp, PropertyTagExifPixYDim, pixY)) {
        val = str::FormatTemp("%u", pixY);
        AddProp(keyValOut, kPropPixelYDimension, val);
    }

    // file source
    {
        PropertyItem* item = nullptr;
        if (GetImagePropertyItem(bmp, PropertyTagExifFileSource, &item)) {
            if (item->length >= 1) {
                u8 src = *(u8*)item->value;
                if (src == 3) {
                    AddProp(keyValOut, kPropFileSource, "DSC");
                }
            }
            free(item);
        }
    }

    // scene type
    {
        PropertyItem* item = nullptr;
        if (GetImagePropertyItem(bmp, PropertyTagExifSceneType, &item)) {
            if (item->length >= 1) {
                u8 scene = *(u8*)item->value;
                if (scene == 1) {
                    AddProp(keyValOut, kPropSceneType, "A directly photographed image");
                }
            }
            free(item);
        }
    }
}

// decode image data with GDI+ (preserving EXIF), extract properties, add file size
static void GetExifPropertiesFromData(const ByteSlice& data, StrVec& keyValOut) {
    if (data.empty()) {
        return;
    }
    TempStr sizeStr = str::FormatTemp("%d", (int)data.size());
    AddProp(keyValOut, kPropImageFileSize, sizeStr);
    Bitmap* bmp = BitmapWithExifFromData(data);
    if (bmp) {
        GetBitmapExifProperties(bmp, keyValOut);
        delete bmp;
    }
}

void EngineImage::GetProperties(StrVec& keyValOut) {
    EngineBase::GetProperties(keyValOut);
}

void EngineImages::GetImageProperties(int pageNo, StrVec& keyValOut) {
    TempStr imgPath = GetImagePathTemp(pageNo);
    if (imgPath) {
        AddProp(keyValOut, kPropImagePath, imgPath);
    }
    ByteSlice data = GetImageData(pageNo);
    GetExifPropertiesFromData(data, keyValOut);
}

void EngineImage::GetImageProperties(int pageNo, StrVec& keyValOut) {
    ByteSlice data = file::ReadFile(FilePath());
    GetExifPropertiesFromData(data, keyValOut);
    data.Free();
}

Bitmap* EngineImage::LoadBitmapForPage(int pageNo, bool& deleteAfterUse) {
    if (1 == pageNo) {
        deleteAfterUse = false;
        return image;
    }

    // extract other frames from multi-page TIFFs and animated GIFs
    ReportIfNotMultiImage(this);
    const GUID* dim = imageFormat == kindFileTiff ? &FrameDimensionPage : &FrameDimensionTime;
    uint frameCount = image->GetFrameCount(dim);
    ReportIf((unsigned int)pageNo > frameCount);
    Bitmap* frame = image->Clone(0, 0, image->GetWidth(), image->GetHeight(), PixelFormat32bppARGB);
    if (!frame) {
        logf("EngineImage::LoadBitmapForPage: failed to clone bitmap for page %d, path: '%s'\n", pageNo, FilePath());
        return nullptr;
    }
    Status ok = frame->SelectActiveFrame(dim, pageNo - 1);
    if (ok != Ok) {
        delete frame;
        logf("EngineImage::LoadBitmapForPage: failed to select frame %d, status %d, path: '%s'\n", pageNo, ok,
             FilePath());
        return nullptr;
    }
    deleteAfterUse = true;
    return frame;
}

ByteSlice EngineImage::GetImageData(int) {
    ScopedCritSec scope(&cacheLock);
    auto pi = pageInfos[0];
    if (pi->rawData.empty()) {
        pi->rawData = file::ReadFile(FilePath());
    }
    return pi->rawData;
}

fz_image* EngineImage::LoadFzImageForPage(fz_context* ctx, int pageNo) {
    // mupdf can decode the file's first frame, but multi-frame TIFFs and
    // animated GIFs are handled by GDI+'s SelectActiveFrame in
    // LoadBitmapForPage. Fall back to the GDI+ path for non-first frames.
    if (pageNo != 1) {
        return nullptr;
    }
    return EngineImages::LoadFzImageForPage(ctx, pageNo);
}

RectF EngineImage::LoadMediabox(int pageNo) {
    if (1 == pageNo) {
        return RectF(0, 0, (float)image->GetWidth(), (float)image->GetHeight());
    }

    // fill the cache to prevent the first few frames from being unpacked twice
    ImagePage* page = GetPage(pageNo, MAX_IMAGE_PAGE_CACHE == pageCache.size());
    if (page) {
        if (page->bmp) {
            RectF mbox(0, 0, (float)page->bmp->GetWidth(), (float)page->bmp->GetHeight());
            DropPage(page, false);
            return mbox;
        }
        DropPage(page, false);
    }
    ReportIfNotMultiImage(this);
    RectF mbox = RectF(0, 0, (float)image->GetWidth(), (float)image->GetHeight());
    Bitmap* frame = image->Clone(0, 0, image->GetWidth(), image->GetHeight(), PixelFormat32bppARGB);
    if (!frame) {
        return mbox;
    }
    const GUID* dim = imageFormat == kindFileTiff ? &FrameDimensionPage : &FrameDimensionTime;
    Status ok = frame->SelectActiveFrame(dim, pageNo - 1);
    if (Ok == ok) {
        mbox = RectF(0, 0, (float)frame->GetWidth(), (float)frame->GetHeight());
    }
    delete frame;
    return mbox;
}

EngineBase* EngineImage::CreateFromFile(const char* path) {
    logf("EngineImage::CreateFromFile(%s)\n", path);
    EngineImage* engine = new EngineImage();
    if (!engine->LoadSingleFile(path)) {
        SafeEngineRelease(&engine);
        return nullptr;
    }
    return engine;
}

EngineBase* EngineImage::CreateFromStream(IStream* stream) {
    EngineImage* engine = new EngineImage();
    if (!engine->LoadFromStream(stream)) {
        SafeEngineRelease(&engine);
        return nullptr;
    }
    return engine;
}

// clang-format off
static Kind imageEngineKinds[] = {
    kindFilePng,  kindFileJpeg, kindFileGif,
    kindFileTiff, kindFileBmp,  kindFileTga,
    kindFileJxr,  kindFileHdp,  kindFileWdp,
    kindFileWebp, kindFileJp2,  kindFileHeic,
    kindFileAvif
};
// clang-format on

bool IsEngineImageSupportedFileType(Kind kind) {
    // logf("IsEngineImageSupportedFileType(%s)\n", kind);
    int n = (int)dimof(imageEngineKinds);
    return KindIndexOf(imageEngineKinds, n, kind) >= 0;
}

EngineBase* CreateEngineImageFromFile(const char* path) {
    logf("CreateEngineImageFromFile(%s)\n", path);
    return EngineImage::CreateFromFile(path);
}

EngineBase* CreateEngineImageFromStream(IStream* stream) {
    log("CreateEngineImageFromStream\n");
    return EngineImage::CreateFromStream(stream);
}

///// ImageDirEngine handles a directory full of image files /////

class EngineImageDir : public EngineImages {
  public:
    EngineImageDir() {
        fileDPI = 96.0f;
        kind = kindEngineImageDir;
        str::ReplaceWithCopy(&defaultExt, "");
        // TODO: is there a better place to expose pageFileNames
        // than through page labels?
        hasPageLabels = true;
    }

    ~EngineImageDir() override { delete tocTree; }

    EngineBase* Clone() override {
        const char* path = FilePath();
        if (path) {
            return CreateFromFile(path);
        }
        return nullptr;
    }

    ByteSlice GetFileData() override { return {}; }
    bool SaveFileAs(const char* copyFileName) override;

    TempStr GetPropertyTemp(const char*) override { return nullptr; }

    TempStr GetPageLabeTemp(int pageNo) const override;
    int GetPageByLabel(const char* label) const override;

    TocTree* GetToc() override;

    static EngineBase* CreateFromFile(const char* fileName);

    // protected:

    Bitmap* LoadBitmapForPage(int pageNo, bool& deleteAfterUse) override;
    RectF LoadMediabox(int pageNo) override;
    ByteSlice GetImageData(int pageNo) override;
    TempStr GetImagePathTemp(int pageNo) override { return str::DupTemp(pageFileNames.At(pageNo - 1)); }

    StrVec pageFileNames;
    TocTree* tocTree = nullptr;
};

static bool LoadImageDir(EngineImageDir* e, const char* dir) {
    e->SetFilePath(dir);

    DirIter di{dir};
    for (DirIterEntry* de : di) {
        auto path = de->filePath;
        Kind kind = GuessFileTypeFromName(path);
        if (IsEngineImageSupportedFileType(kind)) {
            e->pageFileNames.Append(path);
        }
    }

    int nFiles = e->pageFileNames.Size();
    if (nFiles == 0) {
        return false;
    }

    SortNatural(&e->pageFileNames);

    for (int i = 0; i < nFiles; i++) {
        ImagePageInfo* pi = new ImagePageInfo();
        e->pageInfos.Append(pi);
    }

    e->pageCount = nFiles;

    // TODO: better handle the case where images have different resolutions
    ImagePage* page = e->GetPage(1);
    if (page) {
        if (page->bmp) {
            e->fileDPI = page->bmp->GetHorizontalResolution();
        }
        e->DropPage(page, false);
    }
    return true;
}

TempStr EngineImageDir::GetPageLabeTemp(int pageNo) const {
    if (pageNo < 1 || PageCount() < pageNo) {
        return EngineBase::GetPageLabeTemp(pageNo);
    }

    const char* path = pageFileNames.At(pageNo - 1);
    TempStr fileName = path::GetBaseNameTemp(path);
    TempStr ext = path::GetExtTemp(fileName);
    if (!ext) {
        return str::DupTemp(fileName);
    }
    auto pos = str::Find(fileName, ext);
    size_t n = pos - fileName;
    return str::DupTemp(fileName, n);
}

int EngineImageDir::GetPageByLabel(const char* label) const {
    size_t nLabel = str::Len(label);
    for (int i = 0; i < pageFileNames.Size(); i++) {
        char* pagePath = pageFileNames[i];
        TempStr fileName = path::GetBaseNameTemp(pagePath);
        char* ext = path::GetExtTemp(fileName);
        if (!str::StartsWith(fileName, label)) {
            continue;
        }
        const char* maybeExt = fileName + nLabel;
        if (str::Eq(maybeExt, ext) || fileName[nLabel] == 0) {
            return i + 1;
        }
    }

    return EngineBase::GetPageByLabel(label);
}

static TocItem* newImageDirTocItem(TocItem* parent, char* title, int pageNo) {
    return new TocItem(parent, title, pageNo);
};

TocTree* EngineImageDir::GetToc() {
    if (tocTree) {
        return tocTree;
    }
    TempStr label = GetPageLabeTemp(1);
    TocItem* root = newImageDirTocItem(nullptr, label, 1);
    root->id = 1;
    for (int i = 2; i <= PageCount(); i++) {
        label = GetPageLabeTemp(i);
        TocItem* item = newImageDirTocItem(root, label, i);
        item->id = i;
        root->AddSiblingAtEnd(item);
    }
    auto realRoot = new TocItem();
    realRoot->child = root;
    tocTree = new TocTree(realRoot);
    return tocTree;
}

bool EngineImageDir::SaveFileAs(const char* dstPath) {
    // only copy the files if the target directory doesn't exist yet
    bool ok = dir::CreateAll(dstPath);
    if (!ok) {
        return false;
    }
    for (char* pathOld : pageFileNames) {
        TempStr fileName = path::GetBaseNameTemp(pathOld);
        TempStr pathNew = path::JoinTemp(dstPath, fileName);
        ok = ok && file::Copy(pathNew, pathOld, true);
    }
    return ok;
}

Bitmap* EngineImageDir::LoadBitmapForPage(int pageNo, bool& deleteAfterUse) {
    char* path = pageFileNames.At(pageNo - 1);
    ByteSlice bmpData = file::ReadFile(path);
    if (!bmpData) {
        return nullptr;
    }
    deleteAfterUse = true;
    Bitmap* res = BitmapFromData(bmpData);
    bmpData.Free();
    return res;
}

ByteSlice EngineImageDir::GetImageData(int pageNo) {
    ScopedCritSec scope(&cacheLock);
    auto pi = pageInfos[pageNo - 1];
    if (pi->rawData.empty()) {
        char* path = pageFileNames.At(pageNo - 1);
        pi->rawData = file::ReadFile(path);
    }
    return pi->rawData;
}

RectF EngineImageDir::LoadMediabox(int pageNo) {
    char* path = pageFileNames.At(pageNo - 1);
    ByteSlice bmpData = file::ReadFile(path);
    if (bmpData) {
        Size size = ImageSizeFromData(bmpData);
        bmpData.Free();
        return RectF(0, 0, (float)size.dx, (float)size.dy);
    }
    return RectF();
}

EngineBase* EngineImageDir::CreateFromFile(const char* fileName) {
    ReportIf(!dir::Exists(fileName));
    EngineImageDir* engine = new EngineImageDir();
    if (!LoadImageDir(engine, fileName)) {
        SafeEngineRelease(&engine);
        return nullptr;
    }
    return engine;
}

bool IsEngineImageDirSupportedFile(const char* fileName, bool) {
    // whether it actually contains images will be checked in LoadImageDir
    return dir::Exists(fileName);
}

EngineBase* CreateEngineImageDirFromFile(const char* fileName) {
    return EngineImageDir::CreateFromFile(fileName);
}

///// CbxEngine handles comic book files (either .cbz, .cbr, .cb7 or .cbt) /////

struct ComicInfoParser : json::ValueVisitor {
    // extracted metadata
    AutoFreeStr propTitle;
    StrVec propAuthors;
    AutoFreeStr propDate;
    AutoFreeStr propModDate;
    AutoFreeStr propCreator;
    AutoFreeStr propSummary;
    // temporary state needed for extracting metadata
    AutoFreeStr propAuthorTmp;

    // json::ValueVisitor
    bool Visit(const char* path, const char* value, json::Type type) override;

    void Parse(const ByteSlice& xmlData);
};

static void ComicInfoVisitNode(ComicInfoParser* cip, const GumboNode* node) {
    if (!node) {
        return;
    }
    if (node->type == GUMBO_NODE_ELEMENT) {
        if (GumboTagNameIs(node, "Title")) {
            TempStr v = GumboTextContentTemp(node);
            if (v) {
                cip->Visit("/ComicBookInfo/1.0/title", v, json::Type::String);
            }
        } else if (GumboTagNameIs(node, "Year")) {
            TempStr v = GumboTextContentTemp(node);
            if (v) {
                cip->Visit("/ComicBookInfo/1.0/publicationYear", v, json::Type::Number);
            }
        } else if (GumboTagNameIs(node, "Month")) {
            TempStr v = GumboTextContentTemp(node);
            if (v) {
                cip->Visit("/ComicBookInfo/1.0/publicationMonth", v, json::Type::Number);
            }
        } else if (GumboTagNameIs(node, "Summary")) {
            TempStr v = GumboTextContentTemp(node);
            if (v) {
                cip->Visit("/X-summary", v, json::Type::String);
            }
        } else if (GumboTagNameIs(node, "Writer")) {
            TempStr v = GumboTextContentTemp(node);
            if (v) {
                cip->Visit("/ComicBookInfo/1.0/credits[0]/person", v, json::Type::String);
                cip->Visit("/ComicBookInfo/1.0/credits[0]/primary", "true", json::Type::Bool);
            }
        } else if (GumboTagNameIs(node, "Penciller")) {
            TempStr v = GumboTextContentTemp(node);
            if (v) {
                cip->Visit("/ComicBookInfo/1.0/credits[1]/person", v, json::Type::String);
                cip->Visit("/ComicBookInfo/1.0/credits[1]/primary", "true", json::Type::Bool);
            }
        }
        const GumboVector* children = &node->v.element.children;
        for (unsigned int i = 0; i < children->length; i++) {
            ComicInfoVisitNode(cip, (const GumboNode*)children->data[i]);
        }
    } else if (node->type == GUMBO_NODE_DOCUMENT) {
        const GumboVector* children = &node->v.document.children;
        for (unsigned int i = 0; i < children->length; i++) {
            ComicInfoVisitNode(cip, (const GumboNode*)children->data[i]);
        }
    }
}

// extract ComicInfo.xml metadata
// cf. http://comicrack.cyolito.com/downloads/comicrack/ComicRack/Support-Files/ComicInfoSchema.zip/
void ComicInfoParser::Parse(const ByteSlice& xmlData) {
    if (xmlData.empty()) {
        return;
    }
    // Detect the encoding from a leading BOM and produce UTF-8 (gumbo expects
    // UTF-8 input). Handles UTF-8, UTF-16 LE, and UTF-16 BE BOMs; if there's
    // no BOM the data is treated as UTF-8 (ComicInfo.xml's spec encoding).
    TempStr utf8 = strconv::UnknownToUtf8Temp((const char*)xmlData.data(), xmlData.size());
    if (!utf8) {
        return;
    }
    size_t utf8Len = str::Len(utf8);

    GumboOptions opts = GumboMakeOptions();
    GumboOutput* output = gumbo_parse_with_options(&opts, utf8, utf8Len);
    if (!output) {
        return;
    }
    ComicInfoVisitNode(this, output->document);
    gumbo_destroy_output(&opts, output);
}

// extract ComicBookInfo metadata
// http://code.google.com/p/comicbookinfo/
bool ComicInfoParser::Visit(const char* path, const char* value, json::Type type) {
    if (json::Type::String == type && str::Eq(path, "/ComicBookInfo/1.0/title")) {
        propTitle.Set(str::Dup(value));
    } else if (json::Type::Number == type && str::Eq(path, "/ComicBookInfo/1.0/publicationYear")) {
        propDate.Set(str::Format("%s/%d", propDate ? propDate.Get() : "", atoi(value)));
    } else if (json::Type::Number == type && str::Eq(path, "/ComicBookInfo/1.0/publicationMonth")) {
        propDate.Set(str::Format("%d%s", atoi(value), propDate ? propDate.Get() : ""));
    } else if (json::Type::String == type && str::Eq(path, "/appID")) {
        propCreator.Set(str::Dup(value));
    } else if (json::Type::String == type && str::Eq(path, "/lastModified")) {
        propModDate.Set(str::Dup(value));
    } else if (json::Type::String == type && str::Eq(path, "/X-summary")) {
        propSummary.Set(str::Dup(value));
    } else if (str::StartsWith(path, "/ComicBookInfo/1.0/credits[")) {
        int idx = -1;
        const char* prop = str::Parse(path, "/ComicBookInfo/1.0/credits[%d]/", &idx);
        if (prop) {
            if (json::Type::String == type && str::Eq(prop, "person")) {
                propAuthorTmp.Set(str::Dup(value));
            } else if (json::Type::Bool == type && str::Eq(prop, "primary") && propAuthorTmp &&
                       !propAuthors.Contains(propAuthorTmp)) {
                propAuthors.Append(propAuthorTmp.Get());
            }
        }
        return true;
    }
    // stop parsing once we have all desired information
    return !propTitle || propAuthors.Size() == 0 || !propCreator || !propDate ||
           str::FindChar(propDate, '/') <= propDate;
}

class EngineCbx : public EngineImages {
  public:
    explicit EngineCbx(MultiFormatArchive* arch);
    ~EngineCbx() override;

    EngineBase* Clone() override;

    TempStr GetPropertyTemp(const char* name) override;
    void GetProperties(StrVec& keyValOut) override;

    TocTree* GetToc() override;

    // realPath: when non-null we actually open the archive from this
    // (local) path but still report `path` via FilePath() so callers
    // (file history, bookmarks, etc.) see the user's original file.
    static EngineBase* CreateFromFile(const char* path, const char* password = nullptr,
                                      MultiFormatArchive::Format* formatOut = nullptr, bool* isEncryptedOut = nullptr,
                                      Kind hintKind = nullptr, const char* realPath = nullptr);
    static EngineBase* CreateFromStream(IStream* stream);

  protected:
    Bitmap* LoadBitmapForPage(int pageNo, bool& deleteAfterUse) override;
    RectF LoadMediabox(int pageNo) override;
    ByteSlice GetImageData(int pageNo) override;
    TempStr GetImagePathTemp(int pageNo) override { return str::DupTemp(files[pageNo - 1]->name); }

    bool LoadFromFile(const char* fileName);
    bool LoadFromStream(IStream* stream);
    bool FinishLoading();

    // access to cbxFile must be protected after initialization (with cacheLock)
    MultiFormatArchive* cbxArchive = nullptr;
    Vec<MultiFormatArchive::FileInfo*> files;
    TocTree* tocTree = nullptr;

    // When set, the archive was actually opened from this local path (e.g.
    // a cached copy of a network-drive file). FilePath() still returns the
    // original path so file history / bookmarks / state track the user's
    // real file, not our cache.
    char* physicalPath = nullptr;

    ComicInfoParser cip;
};

// TODO: refactor so that doesn't have to keep <arch>
EngineCbx::EngineCbx(MultiFormatArchive* archive) {
    cbxArchive = archive;
    kind = kindEngineComicBooks;
}

EngineCbx::~EngineCbx() {
    delete tocTree;
    delete cbxArchive;
    str::Free(physicalPath);
}

EngineBase* EngineCbx::Clone() {
    if (fileStream) {
        ScopedComPtr<IStream> stm;
        HRESULT res = fileStream->Clone(&stm);
        if (SUCCEEDED(res)) {
            auto clone = CreateFromStream(stm);
            if (!clone) {
                logf("EngineCbx::Clone() failed: CreateFromStream() failed\n");
            }
            return clone;
        }
    }
    const char* path = FilePath();
    if (path) {
        // keep the cached-local-copy in play on the clone too
        auto clone = CreateFromFile(path, nullptr, nullptr, nullptr, nullptr, physicalPath);
        if (!clone) {
            logf("EngineCbx::Clone() failed: CreateFromFile('%s') failed\n", path);
        }
        return clone;
    }
    logf("EngineCbx::Clone() failed: no stream or file path\n");
    return nullptr;
}

bool EngineCbx::LoadFromFile(const char* file) {
    if (!file) {
        return false;
    }
    SetFilePath(file);
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
    const char* s1 = f1->name;
    const char* s2 = f2->name;
    int res = str::CmpNatural(s1, s2);
    return res < 0;
}

static const char* GetExtFromArchiveType(MultiFormatArchive* cbxFile) {
    switch (cbxFile->format) {
        case MultiFormatArchive::Format::Zip:
            return ".cbz";
        case MultiFormatArchive::Format::Rar:
            return ".cbr";
        case MultiFormatArchive::Format::SevenZip:
            return ".cb7";
        case MultiFormatArchive::Format::Tar:
            return ".cbt";
        case MultiFormatArchive::Format::Unknown:
            break;
    }
    ReportIf(true);
    return nullptr;
}

bool EngineCbx::FinishLoading() {
    ReportIf(!cbxArchive);
    if (!cbxArchive) {
        return false;
    }

    auto timeStart = TimeGet();
    defer {
        auto dur = TimeSinceInMs(timeStart);
        logf("EngineCbx::FinisHLoading() in %.2f ms\n", dur);
    };

    // not using the resolution of the contained images seems to be
    // expected, cf.
    // https://web.archive.org/web/20140201010902/http://forums.fofou.org:80/sumatrapdf/topic?id=3183827&comments=5
    // TODO: return DpiGetForHwnd(HWND_DESKTOP) instead?
    fileDPI = 96.f;

    const char* ext = GetExtFromArchiveType(cbxArchive);
    str::ReplaceWithCopy(&defaultExt, ext);

    Vec<MultiFormatArchive::FileInfo*> pageFiles;

    auto& fileInfos = cbxArchive->GetFileInfos();
    size_t n = fileInfos.size();
    for (size_t i = 0; i < n; i++) {
        auto* fileInfo = fileInfos[i];
        const char* fileName = fileInfo->name;
        if (str::Len(fileName) == 0) {
            continue;
        }
        if (MultiFormatArchive::Format::Zip == cbxArchive->format && str::StartsWithI(fileName, "_rels/.rels")) {
            // bail, if we accidentally try to load an XPS file
            return false;
        }

        Kind kind = GuessFileTypeFromName(fileName);
        if (IsEngineImageSupportedFileType(kind) &&
            // OS X occasionally leaves metadata with image extensions
            !str::StartsWith(path::GetBaseNameTemp(fileName), ".")) {
            pageFiles.Append(fileInfo);
        }
    }

    auto* metadataFi = cbxArchive->GetFileDataByName("ComicInfo.xml");
    if (metadataFi && metadataFi->data) {
        ByteSlice metadata{(u8*)metadataFi->data, metadataFi->fileSizeUncompressed};
        cip.Parse(metadata);
    }
    const char* comment = cbxArchive->GetComment();
    if (comment) {
        json::Parse(comment, &cip);
    }

    int nFiles = pageFiles.Size();
    if (nFiles == 0) {
        delete cbxArchive;
        cbxArchive = nullptr;
        return false;
    }

    // encrypted archives list entries but can't extract data without password
    if (cbxArchive->isEncrypted && !cbxArchive->password) {
        delete cbxArchive;
        cbxArchive = nullptr;
        return false;
    }

    // verify password by trying to extract the smallest file
    if (cbxArchive->isEncrypted && cbxArchive->password) {
        MultiFormatArchive::FileInfo* smallest = pageFiles[0];
        for (int i = 1; i < nFiles; i++) {
            if (pageFiles[i]->fileSizeUncompressed < smallest->fileSizeUncompressed) {
                smallest = pageFiles[i];
            }
        }
        auto* fi = cbxArchive->GetFileDataById(smallest->fileId);
        if (!fi || !fi->data) {
            logf("EngineCbx::FinishLoading(): wrong password, failed to extract file\n");
            delete cbxArchive;
            cbxArchive = nullptr;
            return false;
        }
    }

    std::sort(pageFiles.begin(), pageFiles.end(), cmpArchFileInfoByName);

    for (int i = 0; i < nFiles; i++) {
        auto pi = new ImagePageInfo();
        pageInfos.Append(pi);
    }
    files = std::move(pageFiles);
    pageCount = nFiles;

    TocItem* root = nullptr;
    TocItem* curr = nullptr;
    for (int i = 0; i < pageCount; i++) {
        const char* fname = files[i]->name;
        TempStr baseName = path::GetBaseNameTemp(fname);
        TocItem* ti = new TocItem(nullptr, baseName, i + 1);
        if (root == nullptr) {
            root = ti;
        } else {
            if (curr) { // just to silence cppcheck
                curr->next = ti;
            }
        }
        curr = ti;
    }
    if (root) {
        auto realRoot = new TocItem();
        realRoot->child = root;
        tocTree = new TocTree(realRoot);
    }

    return true;
}

TocTree* EngineCbx::GetToc() {
    return tocTree;
}

ByteSlice EngineCbx::GetImageData(int pageNo) {
    ReportIf((pageNo < 1) || (pageNo > PageCount()));
    size_t fileId = files[pageNo - 1]->fileId;
    auto* fi = cbxArchive->GetFileDataById(fileId);
    if (!fi || !fi->data) {
        return {};
    }
    return {(u8*)fi->data, fi->fileSizeUncompressed};
}

TempStr EngineCbx::GetPropertyTemp(const char* name) {
    if (str::Eq(name, kPropTitle)) {
        return cip.propTitle;
    }

    if (str::Eq(name, kPropAuthor)) {
        if (cip.propAuthors.Size() == 0) {
            return nullptr;
        }
        return JoinTemp(&cip.propAuthors, ", ");
    }

    if (str::Eq(name, kPropCreationDate)) {
        return cip.propDate;
    }
    if (str::Eq(name, kPropModificationDate)) {
        return cip.propModDate;
    }
    if (str::Eq(name, kPropCreatorApp)) {
        return cip.propCreator;
    }
    if (str::Eq(name, kPropSubject)) {
        // TODO: replace with Prop_Summary
        return cip.propSummary;
    }

    return nullptr;
}

void EngineCbx::GetProperties(StrVec& keyValOut) {
    EngineBase::GetProperties(keyValOut);

    StrBuilder filesStr;
    auto& fileInfos = cbxArchive->GetFileInfos();
    size_t n = fileInfos.size();
    for (size_t i = 0; i < n; i++) {
        auto* fi = fileInfos[i];
        if (str::IsEmpty(fi->name)) {
            continue;
        }
        if (fi->isDir) {
            continue;
        }
        filesStr.AppendChar('\n');
        filesStr.Append(fi->name);
    }
    // show paths in Windows style (#5543)
    str::TransCharsInPlace(filesStr.CStr(), "/", "\\");
    AddProp(keyValOut, kPropFiles, filesStr.CStr());
}

Bitmap* EngineCbx::LoadBitmapForPage(int pageNo, bool& deleteAfterUse) {
    auto timeStart = TimeGet();
    defer{};
    ByteSlice img = GetImageData(pageNo);
    if (img.empty()) {
        logf("EngineCbx::LoadBitmapForPage(page: %d) failed\n", pageNo);
        return nullptr;
    }
    deleteAfterUse = true;
    auto res = BitmapFromData(img);
    auto dur = TimeSinceInMs(timeStart);
    logf("EngineCbx::LoadBitmapForPage(page: %d) took %.2f ms\n", pageNo, dur);
    return res;
}

RectF EngineCbx::LoadMediabox(int pageNo) {
    size_t fileId = files[pageNo - 1]->fileId;

    // try to get image size from just the file header (first 1024 bytes)
    ByteSlice header = cbxArchive->GetFileDataPartById(fileId, 1024);
    if (!header.empty()) {
        Size size = ImageSizeFromHeader(header);
        header.Free();
        if (!size.IsEmpty()) {
            return RectF(0, 0, (float)size.dx, (float)size.dy);
        }
    }

    // fall back to getting the full image data
    ByteSlice img = GetImageData(pageNo);
    if (!img.empty()) {
        Size size = ImageSizeFromData(img);
        if (!size.IsEmpty()) {
            return RectF(0, 0, (float)size.dx, (float)size.dy);
        }
        // partial/corrupt header (e.g. dx>0 but dy==0) -- don't return that;
        // fall through to GetPage so we can use the actual decoded dimensions
        // and never hand back a zero-area mediabox (would div-by-zero in
        // CalcZoomReal).
        logf("EngineCbx::LoadMediabox: empty media box from header for page: %d\n", pageNo);
    }

    ImagePage* page = GetPage(pageNo, MAX_IMAGE_PAGE_CACHE == pageCache.size());
    if (page) {
        int w = 0, h = 0;
        if (page->img) {
            // mupdf decoded the image; use its dimensions
            w = page->img->w;
            h = page->img->h;
        } else if (page->bmp) {
            w = (int)page->bmp->GetWidth();
            h = (int)page->bmp->GetHeight();
        }
        DropPage(page, false);
        if (w > 0 && h > 0) {
            return RectF(0, 0, (float)w, (float)h);
        }
    }

    // use A4-like dimensions (at 96 DPI) as fallback for failed pages.
    // Important: this MUST be non-empty -- DisplayModel::CalcZoomReal divides
    // by the mediabox, and a zero-area box trips a debug-break assertion.
    return RectF(0, 0, 595, 842);
}

EngineBase* EngineCbx::CreateFromFile(const char* path, const char* password, MultiFormatArchive::Format* formatOut,
                                      bool* isEncryptedOut, Kind hintKind, const char* realPath) {
    auto timeStart = TimeGet();
    // we sniff the type from content first because the
    // files can be mis-named e.g. .cbr archive with .cbz ext
    // we only need the archive format (zip/rar/7z), not the sub-type
    // (epub/xps/fb2z), so use the ByteSlice overload to avoid
    // opening a full archive just for type detection
    MultiFormatArchive* archive = new MultiFormatArchive();
    archive->password = str::Dup(password);

    // realPath is a local copy of a file that lives on a slow drive (see
    // caller in EngineCreate.cpp). We open the archive from there but
    // still surface `path` as the logical file path.
    const char* openPath = realPath ? realPath : path;

    // eagerly decompress small archives up front so we don't have to
    // re-open the file for each page's image data.
    constexpr i64 kMaxEagerLoadSize = 32 * 1024 * 1024;
    i64 fileSize = file::GetSize(openPath);
    bool eagerLoad = fileSize > 0 && fileSize < kMaxEagerLoadSize;

    if (!archive->Open(openPath, eagerLoad, hintKind, gArchiveProgressCb)) {
        delete archive;
        return nullptr;
    }
    if (formatOut) {
        *formatOut = archive->format;
    }
    if (isEncryptedOut) {
        *isEncryptedOut = archive->isEncrypted;
    }
    logf("EngineCbx::CreateFromFile(): opening archive took %.2f\n", TimeSinceInMs(timeStart));

    auto* engine = new EngineCbx(archive);
    if (realPath) {
        engine->physicalPath = str::Dup(realPath);
    }
    if (engine->LoadFromFile(path)) {
        return engine;
    }
    SafeEngineRelease(&engine);
    return nullptr;
}

EngineBase* EngineCbx::CreateFromStream(IStream* stream) {
    // libarchive inside OpenArchiveFromStream tries every container it
    // knows (zip/rar/7z/tar/...) in one pass, so a single call replaces
    // the old try-each-format cascade.
    MultiFormatArchive* archive = OpenArchiveFromStream(stream);
    if (!archive) {
        return nullptr;
    }
    EngineCbx* engine = new EngineCbx(archive);
    if (engine->LoadFromStream(stream)) {
        return engine;
    }
    SafeEngineRelease(&engine);
    return nullptr;
}

static Kind cbxKinds[] = {
    kindFileCbz, kindFileCbr, kindFileCb7, kindFileCbt, kindFileZip, kindFileRar, kindFile7Z, kindFileTar,
};

bool IsEngineCbxSupportedFileType(Kind kind) {
    int n = dimof(cbxKinds);
    return KindIndexOf(cbxKinds, n, kind) >= 0;
}

EngineBase* CreateEngineCbxFromFile(const char* path, PasswordUI* pwdUI, Kind hintKind, const char* realPath) {
    MultiFormatArchive::Format fmt = MultiFormatArchive::Format::Unknown;
    bool isEncrypted = false;
    EngineBase* engine = EngineCbx::CreateFromFile(path, nullptr, &fmt, &isEncrypted, hintKind, realPath);
    if (engine || !pwdUI) {
        return engine;
    }
    if (!isEncrypted) {
        return nullptr;
    }
    // libarchive can't decrypt 7z archives, so don't prompt for password
    if (fmt == MultiFormatArchive::Format::SevenZip || fmt == MultiFormatArchive::Format::Unknown) {
        logf("CreateEngineCbxFromFile: encrypted 7z/unknown not supported\n");
        return nullptr;
    }
    // if opening failed, try with password
    // archive might be password-protected
    bool saveKey = false;
    for (;;) {
        char* pwd = pwdUI->GetPassword(path, nullptr, nullptr, &saveKey);
        if (!pwd) {
            return nullptr; // user cancelled
        }
        engine = EngineCbx::CreateFromFile(path, pwd, nullptr, nullptr, hintKind, realPath);
        str::Free(pwd);
        if (engine) {
            return engine;
        }
    }
}

EngineBase* CreateEngineCbxFromStream(IStream* stream) {
    return EngineCbx::CreateFromStream(stream);
}

bool IsEngineImages(EngineBase* engine) {
    if (!engine) {
        return false;
    }
    return IsOfKind(engine, kindEngineImage) || IsOfKind(engine, kindEngineImageDir) ||
           IsOfKind(engine, kindEngineComicBooks);
}

void EngineImagesGetImageProperties(EngineBase* engine, int pageNo, StrVec& keyValOut) {
    if (!IsEngineImages(engine)) {
        return;
    }
    ((EngineImages*)engine)->GetImageProperties(pageNo, keyValOut);
}
