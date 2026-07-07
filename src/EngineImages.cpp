/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Archive.h"
#include "base/Exif.h"
#include "base/File.h"
#include "base/GuessFileType.h"
#include "base/Pixmap.h"
#include "GumboHelpers.h"
#include "base/JsonParser.h"
#include "base/Timer.h"
#include "base/DirIter.h"

#if OS_WIN
#include "base/Win.h"
#endif

#include <algorithm>

extern "C" {
#include <mupdf/fitz.h>
}

#include "FzImgReader.h"
#include "DocProperties.h"
#include "DocController.h"
#include "TreeModel.h"
#include "EngineBase.h"

Kind kindEngineImage = "engineImage";
Kind kindEngineImageDir = "engineImageDir";
Kind kindEngineComicBooks = "engineComicBooks";

// number of decoded images to cache for quicker rendering.
// Sized for multi-threaded prefetch: enough to hold a few visible pages,
// the worker pool's in-flight pages, and a few prefetch slots without
// thrashing. Each cached entry holds the decoded Pixmap, so the
// memory cost scales with image dimensions -- bump cautiously.
#define MAX_IMAGE_PAGE_CACHE 32

///// EngineImages methods apply to all types of engines handling full-page images /////

struct ImagePage {
    int pageNo = 0;
    // Decoded forms; at most one is non-null. img (mupdf, lazy) is preferred
    // for RenderPage when available: each render decodes the JPEG at near-
    // target scale (DCT-domain 1/2, 1/4, 1/8 downsampling), so big images
    // displayed at small zooms cost only a fraction of a full-res decode.
    // pixmap is the fallback for decoded frames and transformed renders.
    Pixmap* pixmap = nullptr;
    fz_image* img = nullptr;
    bool ownPixmap = true;
    bool failedToLoad = false;
    // true while LoadPixmapForPage / LoadFzImageForPage is running on a worker;
    // concurrent GetPage callers wait on loaded instead of serializing on cacheLock
    bool loading = false;

    // refcount: cache holds 1, every successful GetPage adds 1.
    // mutated atomically so DropPage's common case (refs > 0 after decrement)
    // doesn't need to acquire cacheLock. ++ stays under cacheLock in GetPage
    // because we need exclusion against eviction-in-progress.
    AtomicInt refs = 1;

    Mutex loadLock;
    ConditionVariable loaded;

    // Serializes lazy fallback pixmap decode for pages initially loaded as
    // fz_image. Different pages have different locks so they render in parallel.
    Mutex drawLock;

    ImagePage(int pageNo, Pixmap* pixmap) {
        this->pageNo = pageNo;
        this->pixmap = pixmap;
    }
    ~ImagePage() {
        // img is dropped by DropPage before delete (it needs the engine's
        // per-thread fz_context to call into mupdf safely).
        ReportIf(img);
    }
};

struct ImagePageInfo {
    Vec<IPageElement*> allElements;
    PageElementImage imageElement;
    bool hasImageElement = false;
    RectF mediabox{};
    PageInfoState state = PageInfoState::Unknown;
    // raw image bytes; populated lazily by GetImageData for file-backed
    // engines (EngineImage, EngineImageDir). Unused by EngineCbx (which
    // returns a view into the archive's cache).
    Str rawData;
    ImagePageInfo() = default;
    ~ImagePageInfo() { str::Free(rawData); }
};

class EngineImages : public EngineBase {
  public:
    EngineImages();
    ~EngineImages() override;

    RectF PageMediabox(int pageNo) override;

    Pixmap* RenderPage(RenderPageArgs& args) override;

    RectF Transform(const RectF& rect, int pageNo, float zoom, int rotation, bool inverse = false) override;

    Str GetFileData() override;
    bool SaveFileAs(Str copyFileName) override;
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

    IStream* fileStream = nullptr;

    RecursiveMutex cacheLock;
    Vec<ImagePage*> pageCache;
    Vec<ImagePageInfo*> pageInfos;

    // root mupdf context. Each thread that calls into mupdf gets its own
    // cloned context via Ctx() -- mupdf's per-context setjmp/error stack
    // is NOT thread-safe (concurrent fz_try would clobber each other's
    // jmp_buf and fz_throw would longjmp into a stale stack frame). Cloned
    // contexts share the underlying allocator/store/etc via refcounts.
    fz_context* fz_ctx = nullptr;
    struct ThreadCtx {
        ThreadId threadID;
        fz_context* ctx;
    };
    Vec<ThreadCtx> threadCtxs;
    Mutex threadCtxsLock;

    fz_context* Ctx();

    PointF TransformPoint(PointF pt, int pageNo, float zoom, int rotation, bool inverse);

    virtual Pixmap* LoadPixmapForPage(int pageNo, bool& deleteAfterUse) = 0;
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
    virtual Str GetImageData(int pageNo) = 0;
    virtual TempStr GetImagePathTemp(int pageNo) { return {}; }

    ImagePage* GetPage(int pageNo, bool tryOnly = false);
    void DropPage(ImagePage* page, bool forceRemove);

    RectF PageContentBox(int pageNo, RenderTarget) override;
    void GetImageProperties(int pageNo, Props& propsOut);
};

EngineImages::EngineImages() {
    kind = kindEngineImage;

    preferredLayout = PageLayout();
    preferredLayout.nonContinuous = true;
    isImageCollection = true;

    fz_ctx = fz_new_context_windows();
}

fz_context* EngineImages::Ctx() {
    ThreadId tid = GetCurrentThreadId();
    {
        ScopedMutex scope(&threadCtxsLock);
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
        ScopedMutex scope(&threadCtxsLock);
        threadCtxs.Append({tid, newCtx});
    }
    return newCtx;
}

EngineImages::~EngineImages() {
    // logged so a leaked engine can be identified: its creation is logged by
    // CreateEngineImageFromFile et al. but this line will be missing
    logf("~EngineImages: '%s'\n", FilePath());
    cacheLock.Lock();
    while (len(pageCache) > 0) {
        ImagePage* lastPage = pageCache.Last();
        ReportIf(lastPage->refs != 1);
        DropPage(lastPage, true);
    }
    DeleteVecMembers(pageInfos);
    cacheLock.Unlock();

    // Drop pages before per-thread contexts: DropPage() may need Ctx() to
    // release a page's fz_image, and dropping clones first can make it create
    // a replacement context during destruction.
    for (auto& tc : threadCtxs) {
        logf("EngineImages::~EngineImages: tc.ctx = %p\n", tc.ctx);
        fz_drop_context(tc.ctx);
    }
    threadCtxs.Reset();

    if (fz_ctx) {
        fz_drop_context_windows(fz_ctx);
    }
    if (fileStream) {
        fileStream->Release();
    }
}

// Wrap the page's raw image bytes in an fz_image for lazy mupdf decoding.
// The actual JPEG/PNG decode happens later in RenderPage at near-target
// scale, much cheaper than decoding at full resolution up front.
fz_image* EngineImages::LoadFzImageForPage(fz_context* ctx, int pageNo) {
    // these are binary bytes and formats like JP2 legitimately start with a 0 byte
    Str data = GetImageData(pageNo);
    if (len(data) == 0) {
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
        buf = fz_new_buffer_from_copied_data(ctx, (u8*)data.s, (size_t)data.len);
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

static Size ImageSizeFromDataPortable(Str data) {
    if (len(data) == 0) {
        return {};
    }

    Size res;
    fz_context* ctx = fz_new_context_windows();
    if (!ctx) {
        return {};
    }
    fz_buffer* buf = nullptr;
    fz_image* img = nullptr;
    fz_var(buf);
    fz_var(img);
    fz_try(ctx) {
        buf = fz_new_buffer_from_shared_data(ctx, (const u8*)data.s, (size_t)data.len);
        img = fz_new_image_from_buffer(ctx, buf);
        res = Size(img->w, img->h);
        uint8_t orientation = img->orientation;
        if (orientation != 0 && (orientation & 1) == 0) {
            std::swap(res.dx, res.dy);
        }
    }
    fz_always(ctx) {
        fz_drop_image(ctx, img);
        fz_drop_buffer(ctx, buf);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        res = {};
    }
    fz_drop_context_windows(ctx);
    if (!res.IsEmpty()) {
        return res;
    }

    Pixmap* pixmap = PixmapFromData(data);
    if (pixmap) {
        res = Size(pixmap->width, pixmap->height);
        FreePixmap(pixmap);
    }
    return res;
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

static Pixmap* FzPixmapToPixmap(fz_context* ctx, fz_pixmap* pixmap) {
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
    Pixmap* res = AllocPixmap(w, h, n == 3 ? PixmapFormat::BGR8 : PixmapFormat::BGRA8, n == 4);
    if (res) {
        res->xres = (float)bgr->xres;
        res->yres = (float)bgr->yres;
        int rowBytes = w * n;
        for (int y = 0; y < h; y++) {
            memcpy(res->data + (size_t)y * res->stride, bgr->samples + (size_t)y * bgr->stride, rowBytes);
        }
    }
    fz_drop_pixmap(ctx, bgr);
    return res;
}

static Pixmap* FzImageToPixmap(fz_context* ctx, fz_image* img) {
    fz_pixmap* pixmap = nullptr;
    fz_var(pixmap);
    fz_try(ctx) {
        pixmap = fz_get_pixmap_from_image(ctx, img, nullptr, nullptr, nullptr, nullptr);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        return nullptr;
    }
    if (!pixmap) {
        return nullptr;
    }
    Pixmap* res = FzPixmapToPixmap(ctx, pixmap);
    fz_drop_pixmap(ctx, pixmap);
    return res;
}

static inline int ClampInt(int v, int minVal, int maxVal) {
    return std::min(std::max(v, minVal), maxVal);
}

static void GetPixmapPixelBgra(const Pixmap* pixmap, int x, int y, u8* bgra) {
    int bpp = PixmapBytesPerPixel(pixmap->format);
    const u8* src = pixmap->data + (size_t)y * pixmap->stride + (size_t)x * bpp;
    u8 r, g, b, a;
    if (pixmap->format == PixmapFormat::RGBA8) {
        r = src[0];
        g = src[1];
        b = src[2];
        a = src[3];
    } else {
        b = src[0];
        g = src[1];
        r = src[2];
        a = pixmap->format == PixmapFormat::BGR8 ? 255 : src[3];
    }

    if (a < 255) {
        if (pixmap->premultiplied) {
            b = (u8)std::min(255, b + (255 - a));
            g = (u8)std::min(255, g + (255 - a));
            r = (u8)std::min(255, r + (255 - a));
        } else {
            b = (u8)((b * a + 255 * (255 - a)) / 255);
            g = (u8)((g * a + 255 * (255 - a)) / 255);
            r = (u8)((r * a + 255 * (255 - a)) / 255);
        }
    }
    bgra[0] = b;
    bgra[1] = g;
    bgra[2] = r;
    bgra[3] = 255;
}

static uint32_t GetPixmapPixelRgbKey(const Pixmap* pixmap, int x, int y) {
    u8 bgra[4];
    GetPixmapPixelBgra(pixmap, x, y, bgra);
    uint32_t rgb = (bgra[2] << 16) | (bgra[1] << 8) | bgra[0];
    return rgb & (~0x070707U);
}

static void FillPixmapWhite(Pixmap* pixmap) {
    for (int y = 0; y < pixmap->height; y++) {
        u8* row = pixmap->data + (size_t)y * pixmap->stride;
        for (int x = 0; x < pixmap->width; x++) {
            row[x * 4 + 0] = 255;
            row[x * 4 + 1] = 255;
            row[x * 4 + 2] = 255;
            row[x * 4 + 3] = 255;
        }
    }
}

Pixmap* EngineImages::RenderPage(RenderPageArgs& args) {
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
    if (screen.IsEmpty()) {
        DropPage(page, false);
        return nullptr;
    }

    // Mupdf fast path: rotation 0 + full-page render + cached fz_image available.
    // Tiles (pageRect != mediabox) and rotations fall through to the GDI+ path.
    // Images with EXIF orientation also use the GDI+ path, which applies it.
    uint8_t fzOrientation = page->img ? page->img->orientation : 0;
    bool needsExifOrientation = fzOrientation != 0 && fzOrientation != 1;
    if (page->img && rotation == 0 && !page->failedToLoad && !needsExifOrientation) {
        Rect mediaScreen = Transform(mediabox, pageNo, zoom, rotation).Round();
        bool isFullPage = (mediaScreen.dx == screen.dx && mediaScreen.dy == screen.dy);
        if (isFullPage) {
            // Per-thread cloned context lets multiple workers decode/scale concurrently.
            fz_context* ctx = Ctx();
            Pixmap* result = nullptr;
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
                result = FzPixmapToPixmap(ctx, final);
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
            // fall through to full decode on failure
        }
    }

    // Pixmap path: needs page->pixmap. If we only have img (subclass loaded via
    // mupdf), lazy-load/decode the Pixmap on demand for this rare path
    // (rotation, sub-rect tile, or mupdf decode/scale failure).
    if (!page->pixmap && !page->failedToLoad) {
        ScopedMutex scope(&page->drawLock);
        if (!page->pixmap) {
            bool ownPixmap = true;
            page->pixmap = LoadPixmapForPage(pageNo, ownPixmap);
            page->ownPixmap = ownPixmap;
            if (!page->pixmap && page->img) {
                page->pixmap = FzImageToPixmap(Ctx(), page->img);
                page->ownPixmap = true;
            }
        }
    }

    Pixmap* result = AllocPixmap(screen.dx, screen.dy, PixmapFormat::BGRA8, true);
    if (!result) {
        DropPage(page, false);
        return nullptr;
    }
    FillPixmapWhite(result);

    if (page->failedToLoad) {
        DropPage(page, false);
        return result;
    }

    Pixmap* src = page->pixmap;
    if (!src || !src->data) {
        DropPage(page, false);
        FreePixmap(result);
        return nullptr;
    }

    RectF mediaBox = PageMediabox(pageNo);
    for (int y = 0; y < result->height; y++) {
        u8* dst = result->data + (size_t)y * result->stride;
        for (int x = 0; x < result->width; x++) {
            PointF devPt((float)(screen.x + x) + 0.5f, (float)(screen.y + y) + 0.5f);
            PointF srcPt = TransformPoint(devPt, pageNo, zoom, rotation, true);
            if (!mediaBox.Contains(srcPt)) {
                dst += 4;
                continue;
            }
            int sx = ClampInt((int)srcPt.x, 0, src->width - 1);
            int sy = ClampInt((int)srcPt.y, 0, src->height - 1);
            GetPixmapPixelBgra(src, sx, sy, dst);
            dst += 4;
        }
    }
    DropPage(page, false);
    return result;
}

PointF EngineImages::TransformPoint(PointF pt, int pageNo, float zoom, int rotation, bool inverse) {
    ReportIf(zoom <= 0);
    if (zoom <= 0) {
        return pt;
    }
    SizeF page = PageMediabox(pageNo).Size();
    if (inverse) {
        page.dx *= zoom;
        page.dy *= zoom;
        if (rotation % 180 != 0) {
            std::swap(page.dx, page.dy);
        }
        rotation = -rotation;
        zoom = 1.0f / zoom;
    }
    rotation = NormalizeRotation(rotation);
    PointF res = pt;
    if (rotation == 90) {
        res = PointF(page.dy - pt.y, pt.x);
    } else if (rotation == 180) {
        res = PointF(page.dx - pt.x, page.dy - pt.y);
    } else if (rotation == 270) {
        res = PointF(pt.y, page.dx - pt.x);
    }
    res.x *= zoom;
    res.y *= zoom;
    return res;
}

RectF EngineImages::Transform(const RectF& rect, int pageNo, float zoom, int rotation, bool inverse) {
    PointF tl = TransformPoint(rect.TL(), pageNo, zoom, rotation, inverse);
    PointF br = TransformPoint(rect.BR(), pageNo, zoom, rotation, inverse);
    RectF res = RectF::FromXY(tl, br);
    // try to undo rounding errors caused by a rotation
    // (necessary correction determined by experimentation)
    if (rotation != 0) {
        res.Inflate(-0.01f, -0.01f);
    }
    return res;
}

// don't delete the result
Vec<IPageElement*> EngineImages::GetElements(int pageNo) {
    ReportIf(pageNo < 1 || pageNo > pageCount);
    auto* pi = pageInfos[pageNo - 1];
    if (pi->hasImageElement) {
        return pi->allElements;
    }
    auto mbox = PageMediabox(pageNo);

    pi->imageElement.pageNo = pageNo;
    pi->imageElement.rect = RectF(0, 0, mbox.dx, mbox.dy);
    pi->imageElement.imageID = pageNo;
    pi->allElements.Append(&pi->imageElement);
    pi->hasImageElement = true;
    return pi->allElements;
}

// don't delete the result
IPageElement* EngineImages::GetElementAtPos(int pageNo, PointF pt) {
    if (!PageMediabox(pageNo).Contains(pt)) {
        return nullptr;
    }
    auto els = GetElements(pageNo);
    if (len(els) == 0) {
        return nullptr;
    }
    IPageElement* el = els[0];
    return el;
}

RenderedBitmap* EngineImages::GetImageForPageElement(IPageElement* pel) {
#if !OS_WIN
    (void)pel;
    return nullptr;
#else
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

    if (!page->pixmap && !page->failedToLoad) {
        ScopedMutex scope(&page->drawLock);
        if (!page->pixmap) {
            bool ownPixmap = true;
            page->pixmap = LoadPixmapForPage(pageNo, ownPixmap);
            page->ownPixmap = ownPixmap;
            if (!page->pixmap && page->img) {
                page->pixmap = FzImageToPixmap(Ctx(), page->img);
                page->ownPixmap = true;
            }
        }
    }
    if (!page->pixmap) {
        DropPage(page, false);
        return nullptr;
    }

    Pixmap* pixmap = ClonePixmap(page->pixmap);
    DropPage(page, false);
    return RenderedBitmapFromPixmap(pixmap);
#endif
}

Str EngineImages::GetFileData() {
#if OS_WIN
    return GetStreamOrFileData(fileStream, FilePath());
#else
    if (fileStream) {
        return {};
    }
    return file::ReadFile(FilePath());
#endif
}

bool EngineImages::SaveFileAs(Str dstPath) {
    Str srcPath = FilePath();
    if (srcPath) {
        bool ok = file::Copy(dstPath, srcPath, false);
        if (ok) {
            return true;
        }
    }
    Str d = GetFileData();
    if (len(d) == 0) {
        return false;
    }
    return file::WriteFile(dstPath, d);
}

ImagePage* EngineImages::GetPage(int pageNo, bool tryOnly) {
    ImagePage* result = nullptr;
    bool isLoader = false;
    bool waitForLoad = false;

    {
        ScopedRecursiveMutex scope(&cacheLock);

        for (int i = 0; i < len(pageCache); i++) {
            if (pageCache[i]->pageNo == pageNo) {
                result = pageCache[i];
                break;
            }
        }
        if (!result && tryOnly) {
            return {};
        }

        if (!result) {
            // TODO: drop most memory intensive pages first
            if (len(pageCache) >= MAX_IMAGE_PAGE_CACHE) {
                ReportIf(len(pageCache) != MAX_IMAGE_PAGE_CACHE);
                DropPage(pageCache.Last(), true);
            }
            // insert a loading placeholder; do the actual decode without
            // holding cacheLock so other threads can keep using the cache
            result = new ImagePage(pageNo, nullptr);
            result->loading = true;
            pageCache.InsertAt(0, result);
            isLoader = true;
        } else if (result != pageCache[0]) {
            // keep the list Most Recently Used first
            pageCache.Remove(result);
            pageCache.InsertAt(0, result);
        }

        waitForLoad = !isLoader;
        // ++ under lock: prevents racing with eviction that would otherwise
        // delete the page between our lookup and our ref bump.
        AtomicIntInc(&result->refs);
    }

    if (isLoader) {
        // Slow path: load without cacheLock held. The page is pinned
        // (refs >= 2) so it can't be deleted under us, even if some other
        // thread evicts it from the cache while we're working.
        // Try the mupdf image path first (lazy decode at render time at near-
        // target scale); fall back to Pixmap if the subclass opts out
        // or mupdf can't handle the format.
        fz_image* img = LoadFzImageForPage(Ctx(), pageNo);
        Pixmap* pixmap = nullptr;
        bool ownPixmap = true;
        if (!img) {
            pixmap = LoadPixmapForPage(pageNo, ownPixmap);
        }
        {
            ScopedRecursiveMutex scope(&cacheLock);
            result->img = img;
            result->pixmap = pixmap;
            result->ownPixmap = ownPixmap;
            if (!img && !pixmap) {
                result->failedToLoad = true;
            }
        }
        {
            ScopedMutex scope(&result->loadLock);
            result->loading = false;
            result->loaded.WakeAll();
        }
    } else if (waitForLoad) {
        // Another thread is decoding this same page; wait for it to finish.
        ScopedMutex scope(&result->loadLock);
        while (result->loading) {
            result->loaded.Wait(&result->loadLock);
        }
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
        ScopedRecursiveMutex scope(&cacheLock);
        // pageCache.Remove is a no-op if the page was already evicted earlier
        pageCache.Remove(page);
    }

    if (newRefs == 0) {
        if (page->ownPixmap) {
            FreePixmap(page->pixmap);
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

    if (!page->pixmap && !page->failedToLoad) {
        ScopedMutex scope(&page->drawLock);
        if (!page->pixmap) {
            bool ownPixmap = true;
            page->pixmap = LoadPixmapForPage(pageNo, ownPixmap);
            page->ownPixmap = ownPixmap;
            if (!page->pixmap && page->img) {
                page->pixmap = FzImageToPixmap(Ctx(), page->img);
                page->ownPixmap = true;
            }
        }
    }

    auto pixmap = page->pixmap;
    if (!pixmap || !pixmap->data) return RectF{};

    const int w = pixmap->width, h = pixmap->height;

    // Handle degenerate cases where the image is too small for margin detection
    // Minimum sensible dimension for margin cropping is about 10 pixels
    if (w < 10 || h < 10) {
        return RectF(0, 0, (float)w, (float)h);
    }

    // don't need pixel-perfect margin, so scan 200 points at most
    const int deltaX = std::max(1, w / 200), deltaY = std::max(1, h / 200);

    Rect r(0, 0, w, h);

    auto getPixel = [pixmap](int x, int y) -> uint32_t {
        ReportIf(x < 0 || x >= pixmap->width || y < 0 || y >= pixmap->height);
        return GetPixmapPixelRgbKey(pixmap, x, y);
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
    return ToRectF(r);
}

///// ImageEngine handles a single image file /////

class EngineImage : public EngineImages {
  public:
    EngineImage();
    ~EngineImage() override;

    EngineBase* Clone() override;

    TempStr GetPropertyTemp(DocProp prop) override;
    void GetProperties(Props& propsOut) override;
    void GetImageProperties(int pageNo, Props& propsOut);

    static EngineBase* CreateFromFile(Str fileName);
    static EngineBase* CreateFromStream(IStream* stream);

    // decoded frames: 1 for normal images, N for multi-page TIFF / animated GIF.
    // owned by the engine; per-page cache entries may borrow these.
    Vec<Pixmap*> frames;
    Kind imageFormat = nullptr;

    bool LoadSingleFile(Str fileName);
    bool LoadFromStream(IStream* stream);
    bool FinishLoading(Size fallbackSize = {});

    Pixmap* LoadPixmapForPage(int pageNo, bool& deleteAfterUse) override;
    fz_image* LoadFzImageForPage(fz_context* ctx, int pageNo) override;
    RectF LoadMediabox(int pageNo) override;
    Str GetImageData(int pageNo) override;
};

EngineImage::EngineImage() {
    kind = kindEngineImage;
}

EngineImage::~EngineImage() {
    for (Pixmap* px : frames) {
        FreePixmap(px);
    }
}

EngineBase* EngineImage::Clone() {
    if ((frames.empty() || !frames[0]) && len(pageInfos) == 0) {
        logf("EngineImage::Clone() failed: no image data for '%s'\n", FilePath() ? FilePath() : StrL("(null)"));
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
    for (Pixmap* px : frames) {
        clone->frames.Append(ClonePixmap(px));
    }
    Size fallbackSize;
    if (len(pageInfos) > 0) {
        fallbackSize = PageMediabox(1).Round().Size();
    }
    clone->FinishLoading(fallbackSize);

    return clone;
}

bool EngineImage::LoadSingleFile(Str path) {
    if (!path) {
        return false;
    }
    SetFilePath(path);

    Str data = file::ReadFile(path);
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
    TempStr fileExt = GfxFileExtFromDataTemp(data);
    if (!fileExt) {
        // imageFormat already holds the Kind we resolved above; skip the
        // redundant GuessFileTypeFromName call.
        fileExt = GfxFileExtFromKindTemp(imageFormat);
    }
    if (!fileExt) {
        fileExt = path::GetExtTemp(path);
    }
    if (!fileExt) {
        fileExt = StrL("");
    }
    SetDefaultExt(defaultExt, fileExt);
    frames = PixmapsFromData(data);
    Size fallbackSize = ImageSizeFromDataPortable(data);
    bool ok = FinishLoading(fallbackSize);
    if (ok) {
        pageInfos[0]->rawData = data;
    } else {
        str::Free(data);
    }
    return ok;
}

bool EngineImage::LoadFromStream(IStream* stream) {
#if !OS_WIN
    (void)stream;
    return false;
#else
    if (!stream) {
        return false;
    }
    fileStream = stream;
    fileStream->AddRef();

    Str fileExt;
    u8 header[18];
    if (ReadDataFromStream(stream, header, sizeof(header))) {
        Str d = Str((char*)header, (int)sizeof(header));
        fileExt = GfxFileExtFromDataTemp(d);
    }
    if (!fileExt) {
        return false;
    }
    SetDefaultExt(defaultExt, path::GetExtTemp(fileExt));

    Str data = GetDataFromStream(stream, nullptr);
    frames = PixmapsFromData(data);
    Size fallbackSize = ImageSizeFromDataPortable(data);
    bool ok = FinishLoading(fallbackSize);
    if (ok) {
        pageInfos[0]->rawData = data;
    } else {
        str::Free(data);
    }
    return ok;
#endif
}

bool EngineImage::FinishLoading(Size fallbackSize) {
    if ((frames.empty() || !frames[0]) && fallbackSize.IsEmpty()) {
        return false;
    }
    Pixmap* p0 = frames.empty() ? nullptr : frames[0];
    if (p0) {
        fileDPI = p0->xres;
    }

    auto pi = new ImagePageInfo();
    int w = p0 ? p0->width : fallbackSize.dx;
    int h = p0 ? p0->height : fallbackSize.dy;
    pi->mediabox = RectF(0, 0, (float)w, (float)h);
    pageInfos.Append(pi);
    pi->state = PageInfoState::Known;

    // one page per decoded frame (multi-page TIFFs and animated GIFs have >1)
    for (int i = 1; i < len(frames); i++) {
        pageInfos.Append(new ImagePageInfo());
    }
    pageCount = len(pageInfos);

    return pageCount > 0;
}

static bool GetExifInt(const ExifParser& parser, ExifProp prop, i64& val) {
    return parser.GetIntProp(prop, &val);
}

static bool GetExifFloat(const ExifParser& parser, ExifProp prop, double& val) {
    return parser.GetFloatProp(prop, &val);
}

static void AddExifStringProp(Props& propsOut, DocProp docProp, const ExifParser& parser, ExifProp prop,
                              ExifProp altProp = ExifProp::None) {
    TempStr val = parser.GetStringProp(prop, altProp);
    if (val) {
        AddProp(propsOut, docProp, val);
    }
}

TempStr EngineImage::GetPropertyTemp(DocProp prop) {
    Str data = file::ReadFile(FilePath());
    if (len(data) == 0) {
        return nullptr;
    }

    ExifParser parser;
    TempStr res = nullptr;
    if (parser.Parse(data)) {
        if (prop == DocProp::Title) {
            res = parser.GetStringProp(ExifProp::ImageDescription, ExifProp::XPTitle);
        } else if (prop == DocProp::Subject) {
            res = parser.GetStringProp(ExifProp::XPSubject);
        } else if (prop == DocProp::Author) {
            res = parser.GetStringProp(ExifProp::Artist, ExifProp::XPAuthor);
        } else if (prop == DocProp::Copyright) {
            res = parser.GetStringProp(ExifProp::Copyright);
        } else if (prop == DocProp::CreationDate) {
            res = parser.GetStringProp(ExifProp::DateTime, ExifProp::DateTimeDigitized);
        } else if (prop == DocProp::CreatorApp) {
            res = parser.GetStringProp(ExifProp::Software);
        }
    }
    str::Free(data);
    return res;
}

static void AddParsedExifProperties(Str data, const ExifParser& parser, Props& propsOut) {
    TempStr val;
    i64 intVal;
    double fVal;

    Size imgSize = ImageSizeFromDataPortable(data);
    if (!imgSize.IsEmpty()) {
        AddProp(propsOut, DocProp::ImageSize, fmt("%d x %d", imgSize.dx, imgSize.dy));
    } else {
        i64 w = 0, h = 0;
        if ((!GetExifInt(parser, ExifProp::ExifImageWidth, w) && !GetExifInt(parser, ExifProp::ImageWidth, w)) ||
            (!GetExifInt(parser, ExifProp::ExifImageLength, h) && !GetExifInt(parser, ExifProp::ImageLength, h))) {
            w = h = 0;
        }
        if (w > 0 && h > 0) {
            AddProp(propsOut, DocProp::ImageSize, fmt("%d x %d", (int)w, (int)h));
        }
    }

    double dpiX = 0;
    double dpiY = 0;
    if (GetExifFloat(parser, ExifProp::XResolution, dpiX) && GetExifFloat(parser, ExifProp::YResolution, dpiY) &&
        dpiX > 0 && dpiY > 0) {
        if (dpiX == dpiY) {
            AddProp(propsOut, DocProp::Dpi, fmt("%.0f", dpiX));
        } else {
            AddProp(propsOut, DocProp::Dpi, fmt("%.0f x %.0f", dpiX, dpiY));
        }
    }

    AddExifStringProp(propsOut, DocProp::Keywords, parser, ExifProp::XPKeywords);
    AddExifStringProp(propsOut, DocProp::Comment, parser, ExifProp::XPComment);
    AddExifStringProp(propsOut, DocProp::CameraMake, parser, ExifProp::Make);
    AddExifStringProp(propsOut, DocProp::CameraModel, parser, ExifProp::Model);
    AddExifStringProp(propsOut, DocProp::DateOriginal, parser, ExifProp::DateTimeOriginal);

    ExifRational rat;
    if (parser.GetRationalProp(ExifProp::ExposureTime, &rat) && rat.den > 0 && rat.num > 0) {
        if (rat.num == 1) {
            AddProp(propsOut, DocProp::ExposureTime, fmt("1/%u s", (u32)rat.den));
        } else {
            AddProp(propsOut, DocProp::ExposureTime, fmt("%u/%u s", (u32)rat.num, (u32)rat.den));
        }
    }

    if (GetExifFloat(parser, ExifProp::FNumber, fVal) && fVal > 0) {
        AddProp(propsOut, DocProp::FNumber, fmt("f/%.1f", fVal));
    }

    if (GetExifInt(parser, ExifProp::ISOSpeed, intVal)) {
        AddProp(propsOut, DocProp::IsoSpeed, fmt("ISO %u", (u32)intVal));
    }

    if (GetExifFloat(parser, ExifProp::FocalLength, fVal) && fVal > 0) {
        AddProp(propsOut, DocProp::FocalLength, fmt("%.1f mm", fVal));
    }

    if (GetExifInt(parser, ExifProp::FocalLengthIn35mmFilm, intVal)) {
        AddProp(propsOut, DocProp::FocalLength35mm, fmt("%u mm", (u32)intVal));
    }

    if (GetExifInt(parser, ExifProp::Flash, intVal)) {
        AddProp(propsOut, DocProp::Flash, (intVal & 1) ? StrL("Yes") : StrL("No"));
    }

    if (GetExifInt(parser, ExifProp::Orientation, intVal)) {
        AddProp(propsOut, DocProp::Orientation, fmt("%u", (u32)intVal));
    }

    if (GetExifInt(parser, ExifProp::ExposureProgram, intVal)) {
        static const Str exposurePrograms[] = {
            StrL("Not defined"),       StrL("Manual"),           StrL("Normal program"),
            StrL("Aperture priority"), StrL("Shutter priority"), StrL("Creative program"),
            StrL("Action program"),    StrL("Portrait mode"),    StrL("Landscape mode"),
        };
        if (intVal >= 0 && intVal < dimof(exposurePrograms)) {
            AddProp(propsOut, DocProp::ExposureProgram, exposurePrograms[intVal]);
        }
    }

    if (GetExifInt(parser, ExifProp::MeteringMode, intVal)) {
        static const Str meteringModes[] = {StrL("Unknown"), StrL("Average"),    StrL("Center Weighted Average"),
                                            StrL("Spot"),    StrL("Multi Spot"), StrL("Pattern"),
                                            StrL("Partial")};
        if (intVal >= 0 && intVal < dimof(meteringModes)) {
            AddProp(propsOut, DocProp::MeteringMode, meteringModes[intVal]);
        }
    }

    if (GetExifInt(parser, ExifProp::WhiteBalance, intVal)) {
        AddProp(propsOut, DocProp::WhiteBalance, intVal == 0 ? StrL("Auto") : StrL("Manual"));
    }

    if (GetExifFloat(parser, ExifProp::ExposureBiasValue, fVal)) {
        AddProp(propsOut, DocProp::ExposureBias, fmt("%+.1f EV", fVal));
    }

    if (GetExifInt(parser, ExifProp::BitsPerSample, intVal)) {
        AddProp(propsOut, DocProp::BitsPerSample, fmt("%u", (u32)intVal));
    }

    if (GetExifInt(parser, ExifProp::ResolutionUnit, intVal)) {
        Str unitStr = StrL("unknown");
        if (intVal == 2) {
            unitStr = StrL("inches");
        } else if (intVal == 3) {
            unitStr = StrL("centimeters");
        }
        AddProp(propsOut, DocProp::ResolutionUnit, unitStr);
    }

    AddExifStringProp(propsOut, DocProp::Software, parser, ExifProp::Software);
    AddExifStringProp(propsOut, DocProp::DateTime, parser, ExifProp::DateTime);

    if (GetExifInt(parser, ExifProp::YCbCrPositioning, intVal)) {
        AddProp(propsOut, DocProp::YCbCrPositioning, intVal == 1 ? StrL("centered") : StrL("co-sited"));
    }

    AddExifStringProp(propsOut, DocProp::ExifVersion, parser, ExifProp::ExifVersion);
    AddExifStringProp(propsOut, DocProp::DateTimeDigitized, parser, ExifProp::DateTimeDigitized);

    val = parser.GetFormattedPropTemp(ExifProp::ComponentsConfiguration);
    if (val) {
        AddProp(propsOut, DocProp::ComponentsConfig, val);
    }

    if (parser.GetRationalProp(ExifProp::CompressedBitsPerPixel, &rat) && rat.den > 0) {
        double cbpp = (double)rat.num / (double)rat.den;
        if (rat.den == 1) {
            AddProp(propsOut, DocProp::CompressedBpp, fmt("%u", (u32)rat.num));
        } else {
            AddProp(propsOut, DocProp::CompressedBpp, fmt("%.2f", cbpp));
        }
    }

    if (GetExifFloat(parser, ExifProp::MaxApertureValue, fVal)) {
        AddProp(propsOut, DocProp::MaxAperture, fmt("%.2f", fVal));
    }

    if (GetExifInt(parser, ExifProp::LightSource, intVal)) {
        Str lightStr;
        switch (intVal) {
            case 0:
                lightStr = StrL("Unknown");
                break;
            case 1:
                lightStr = StrL("Daylight");
                break;
            case 2:
                lightStr = StrL("Fluorescent");
                break;
            case 3:
                lightStr = StrL("Tungsten");
                break;
            case 4:
                lightStr = StrL("Flash");
                break;
            case 9:
                lightStr = StrL("Fine weather");
                break;
            case 10:
                lightStr = StrL("Cloudy weather");
                break;
            case 11:
                lightStr = StrL("Shade");
                break;
            case 17:
                lightStr = StrL("Standard light A");
                break;
            case 18:
                lightStr = StrL("Standard light B");
                break;
            case 19:
                lightStr = StrL("Standard light C");
                break;
            case 255:
                lightStr = StrL("Other");
                break;
            default:
                lightStr = StrL("Unknown");
                break;
        }
        AddProp(propsOut, DocProp::LightSource, lightStr);
    }

    AddExifStringProp(propsOut, DocProp::UserComment, parser, ExifProp::UserComment);
    AddExifStringProp(propsOut, DocProp::FlashpixVersion, parser, ExifProp::FlashpixVersion);

    if (GetExifInt(parser, ExifProp::ColorSpace, intVal)) {
        Str csStr = (intVal == 1) ? StrL("sRGB") : (intVal == 0xFFFF) ? StrL("Uncalibrated") : StrL("Unknown");
        AddProp(propsOut, DocProp::ColorSpace, csStr);
    }

    if (GetExifInt(parser, ExifProp::ExifImageWidth, intVal)) {
        AddProp(propsOut, DocProp::PixelXDimension, fmt("%u", (u32)intVal));
    }
    if (GetExifInt(parser, ExifProp::ExifImageLength, intVal)) {
        AddProp(propsOut, DocProp::PixelYDimension, fmt("%u", (u32)intVal));
    }

    if (GetExifInt(parser, ExifProp::FileSource, intVal) && intVal == 3) {
        AddProp(propsOut, DocProp::FileSource, StrL("DSC"));
    }

    if (GetExifInt(parser, ExifProp::SceneType, intVal) && intVal == 1) {
        AddProp(propsOut, DocProp::SceneType, StrL("A directly photographed image"));
    }
}

static void GetExifPropertiesFromData(Str data, Props& propsOut) {
    if (len(data) == 0) {
        return;
    }
    AddProp(propsOut, DocProp::ImageFileSize, fmt("%d", (int)data.len));

    ExifParser parser;
    if (parser.Parse(data)) {
        AddParsedExifProperties(data, parser, propsOut);
    }
}

void EngineImage::GetProperties(Props& propsOut) {
    EngineBase::GetProperties(propsOut);
}

void EngineImages::GetImageProperties(int pageNo, Props& propsOut) {
    TempStr imgPath = GetImagePathTemp(pageNo);
    if (imgPath) {
        AddProp(propsOut, DocProp::ImagePath, imgPath);
    }
    Str data = GetImageData(pageNo);
    GetExifPropertiesFromData(data, propsOut);
}

void EngineImage::GetImageProperties(int pageNo, Props& propsOut) {
    Str data = file::ReadFile(FilePath());
    GetExifPropertiesFromData(data, propsOut);
    str::Free(data);
}

Pixmap* EngineImage::LoadPixmapForPage(int pageNo, bool& deleteAfterUse) {
    int idx = pageNo - 1;
    if (idx < 0 || idx >= len(frames)) {
        Str data = GetImageData(pageNo);
        deleteAfterUse = true;
        return PixmapFromData(data);
    }
    deleteAfterUse = false;
    return frames[idx];
}

Str EngineImage::GetImageData(int) {
    ScopedRecursiveMutex scope(&cacheLock);
    auto pi = pageInfos[0];
    if (len(pi->rawData) == 0) {
        pi->rawData = file::ReadFile(FilePath());
    }
    return pi->rawData;
}

fz_image* EngineImage::LoadFzImageForPage(fz_context* ctx, int pageNo) {
    // mupdf decodes the file's first frame lazily at render scale. Additional
    // frames of multi-page TIFFs / animated GIFs come from the pre-decoded
    // `frames` list via LoadPixmapForPage, so opt out of the mupdf path for them.
    if (pageNo != 1) {
        return nullptr;
    }
    return EngineImages::LoadFzImageForPage(ctx, pageNo);
}

RectF EngineImage::LoadMediabox(int pageNo) {
    int idx = pageNo - 1;
    if (idx >= 0 && idx < len(frames) && frames[idx]) {
        return RectF(0, 0, (float)frames[idx]->width, (float)frames[idx]->height);
    }
    return RectF();
}

EngineBase* EngineImage::CreateFromFile(Str path) {
    logf("EngineImage::CreateFromFile(%s)\n", path);
    EngineImage* engine = new EngineImage();
    bool ok = engine->LoadSingleFile(path);
    // decoding might run a 3rd-party WIC codec (e.g. CopyTrans HEIC) that
    // unmasks fp exceptions on this thread, which would crash later float math
#if OS_WIN
    MaskFpExceptions();
#endif
    if (!ok) {
        SafeEngineRelease(&engine);
        return nullptr;
    }
    return engine;
}

EngineBase* EngineImage::CreateFromStream(IStream* stream) {
#if !OS_WIN
    (void)stream;
    return nullptr;
#else
    EngineImage* engine = new EngineImage();
    bool ok = engine->LoadFromStream(stream);
    MaskFpExceptions();
    if (!ok) {
        SafeEngineRelease(&engine);
        return nullptr;
    }
    return engine;
#endif
}

// clang-format off
static Kind imageEngineKinds[] = {
    kindFilePng,  kindFileJpeg, kindFileGif,
    kindFileTiff, kindFileBmp,  kindFileTga,
    kindFileJxr,  kindFileHdp,  kindFileWdp,
    kindFileWebp, kindFileJp2,  kindFileHeic,
    kindFileAvif, kindFileJxl
};
// clang-format on

bool IsEngineImageSupportedFileType(Kind kind) {
    // logf("IsEngineImageSupportedFileType(%s)\n", kind);
    int n = (int)dimof(imageEngineKinds);
    return KindIndexOf(imageEngineKinds, n, kind) >= 0;
}

EngineBase* CreateEngineImageFromFile(Str path) {
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
        SetDefaultExt(defaultExt, "");
        // TODO: is there a better place to expose pageFileNames
        // than through page labels?
        hasPageLabels = true;
    }

    ~EngineImageDir() override { delete tocTree; }

    EngineBase* Clone() override {
        Str path = FilePath();
        if (path) {
            return CreateFromFile(path);
        }
        return nullptr;
    }

    Str GetFileData() override { return {}; }
    bool SaveFileAs(Str copyFileName) override;

    TempStr GetPropertyTemp(DocProp) override { return nullptr; }

    TempStr GetPageLabeTemp(int pageNo) const override;
    int GetPageByLabel(Str label) const override;

    TocTree* GetToc() override;

    static EngineBase* CreateFromFile(Str fileName);

    // protected:

    Pixmap* LoadPixmapForPage(int pageNo, bool& deleteAfterUse) override;
    RectF LoadMediabox(int pageNo) override;
    Str GetImageData(int pageNo) override;
    TempStr GetImagePathTemp(int pageNo) override { return str::DupTemp(pageFileNames[pageNo - 1]); }

    StrVec pageFileNames;
    TocTree* tocTree = nullptr;
};

static bool LoadImageDir(EngineImageDir* e, Str dir) {
    e->SetFilePath(dir);

    DirIter di{dir};
    for (DirIterEntry* de : di) {
        auto path = de->filePath;
        Kind kind = GuessFileTypeFromName(path);
        if (IsEngineImageSupportedFileType(kind)) {
            e->pageFileNames.Append(path);
        }
    }

    int nFiles = len(e->pageFileNames);
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
        if (page->pixmap) {
            e->fileDPI = page->pixmap->xres;
        }
        e->DropPage(page, false);
    }
    return true;
}

TempStr EngineImageDir::GetPageLabeTemp(int pageNo) const {
    if (pageNo < 1 || PageCount() < pageNo) {
        return EngineBase::GetPageLabeTemp(pageNo);
    }

    Str path = pageFileNames[pageNo - 1];
    TempStr fileName = path::GetBaseNameTemp(path);
    TempStr ext = path::GetExtTemp(fileName);
    if (!ext) {
        return str::DupTemp(fileName);
    }
    int n = str::IndexOf(fileName, ext);
    if (n < 0) {
        n = fileName.len;
    }
    return str::DupTemp(Str(fileName.s, n));
}

int EngineImageDir::GetPageByLabel(Str label) const {
    int nLabel = len(label);
    for (int i = 0; i < len(pageFileNames); i++) {
        Str pagePath = pageFileNames[i];
        TempStr fileName = path::GetBaseNameTemp(pagePath);
        TempStr ext = path::GetExtTemp(fileName);
        if (!str::StartsWith(fileName, label)) {
            continue;
        }
        Str maybeExt(fileName.s + nLabel, fileName.len - nLabel);
        if (str::Eq(maybeExt, ext) || nLabel == fileName.len) {
            return i + 1;
        }
    }

    return EngineBase::GetPageByLabel(label);
}

static TocItem* newImageDirTocItem(TocItem* parent, Str title, int pageNo) {
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

bool EngineImageDir::SaveFileAs(Str dstPath) {
    // only copy the files if the target directory doesn't exist yet
    bool ok = dir::CreateAll(dstPath);
    if (!ok) {
        return false;
    }
    for (Str pathOld : pageFileNames) {
        TempStr fileName = path::GetBaseNameTemp(pathOld);
        TempStr pathNew = path::JoinTemp(dstPath, fileName);
        ok = ok && file::Copy(pathNew, pathOld, true);
    }
    return ok;
}

Pixmap* EngineImageDir::LoadPixmapForPage(int pageNo, bool& deleteAfterUse) {
    Str path = pageFileNames[pageNo - 1];
    Str bmpData = file::ReadFile(path);
    if (!bmpData) {
        return nullptr;
    }
    deleteAfterUse = true;
    Pixmap* res = PixmapFromData(bmpData);
    str::Free(bmpData);
    return res;
}

Str EngineImageDir::GetImageData(int pageNo) {
    ScopedRecursiveMutex scope(&cacheLock);
    auto pi = pageInfos[pageNo - 1];
    if (len(pi->rawData) == 0) {
        Str path = pageFileNames[pageNo - 1];
        pi->rawData = file::ReadFile(path);
    }
    return pi->rawData;
}

RectF EngineImageDir::LoadMediabox(int pageNo) {
    Str path = pageFileNames[pageNo - 1];
    Str bmpData = file::ReadFile(path);
    if (bmpData) {
        Size size = ImageSizeFromDataPortable(bmpData);
        str::Free(bmpData);
        return RectF(0, 0, (float)size.dx, (float)size.dy);
    }
    return RectF();
}

EngineBase* EngineImageDir::CreateFromFile(Str fileName) {
    ReportIf(!dir::Exists(fileName));
    EngineImageDir* engine = new EngineImageDir();
    if (!LoadImageDir(engine, fileName)) {
        SafeEngineRelease(&engine);
        return nullptr;
    }
    return engine;
}

bool IsEngineImageDirSupportedFile(Str fileName, bool) {
    // whether it actually contains images will be checked in LoadImageDir
    return dir::Exists(fileName);
}

EngineBase* CreateEngineImageDirFromFile(Str fileName) {
    return EngineImageDir::CreateFromFile(fileName);
}

///// CbxEngine handles comic book files (either .cbz, .cbr, .cb7 or .cbt) /////

struct ComicInfoParser : json::ValueVisitor {
    // extracted metadata
    Str propTitle;
    StrVec propAuthors;
    Str propDate;
    Str propModDate;
    Str propCreator;
    Str propSummary;
    // temporary state needed for extracting metadata
    Str propAuthorTmp;

    // ComicInfo.xml <Page Image="N" Bookmark="..."/> entries (Image is 0-based)
    Vec<int> bookmarkImageIdx;
    StrVec bookmarkTitles;

    ~ComicInfoParser() override {
        str::Free(propTitle);
        str::Free(propDate);
        str::Free(propModDate);
        str::Free(propCreator);
        str::Free(propSummary);
        str::Free(propAuthorTmp);
    }

    // json::ValueVisitor
    bool Visit(Str path, Str value, json::Type type) override;

    void Parse(Str xmlData);
    void AddBookmark(int imageIdx, Str title);
};

void ComicInfoParser::AddBookmark(int imageIdx, Str title) {
    if (!title || imageIdx < 0) {
        return;
    }
    bookmarkImageIdx.Append(imageIdx);
    bookmarkTitles.Append(str::Dup(title));
}

static void ComicInfoVisitNode(ComicInfoParser* cip, const GumboNode* root) {
    // iterative pre-order DFS so a deeply nested document can't overflow the stack
    Vec<const GumboNode*> toVisit;
    toVisit.Append(root);
    while (len(toVisit) > 0) {
        const GumboNode* node = toVisit.Pop();
        if (!node) {
            continue;
        }
        const GumboVector* children = nullptr;
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
            } else if (GumboTagNameIs(node, "Page")) {
                const GumboAttribute* imageAttr = gumbo_get_attribute(&node->v.element.attributes, "Image");
                const GumboAttribute* bookmarkAttr = gumbo_get_attribute(&node->v.element.attributes, "Bookmark");
                if (imageAttr && bookmarkAttr) {
                    cip->AddBookmark(ParseInt(Str(imageAttr->value)), Str(bookmarkAttr->value));
                }
            }
            children = &node->v.element.children;
        } else if (node->type == GUMBO_NODE_DOCUMENT) {
            children = &node->v.document.children;
        }
        if (children) {
            // push in reverse so children are visited in document order
            for (unsigned int i = children->length; i > 0; i--) {
                toVisit.Append((const GumboNode*)children->data[i - 1]);
            }
        }
    }
}

// extract ComicInfo.xml metadata
// cf. http://comicrack.cyolito.com/downloads/comicrack/ComicRack/Support-Files/ComicInfoSchema.zip/
void ComicInfoParser::Parse(Str xmlData) {
    if (len(xmlData) == 0) {
        return;
    }
    // Detect the encoding from a leading BOM and produce UTF-8 (gumbo expects
    // UTF-8 input). Handles UTF-8, UTF-16 LE, and UTF-16 BE BOMs; if there's
    // no BOM the data is treated as UTF-8 (ComicInfo.xml's spec encoding).
    TempStr utf8 = strconv::UnknownToUtf8Temp(xmlData);
    if (!utf8) {
        return;
    }
    int utf8Len = len(utf8);

    GumboOptions opts = GumboMakeOptions();
    GumboOutput* output = gumbo_parse_with_options(&opts, utf8.s, utf8Len);
    if (!output) {
        return;
    }
    ComicInfoVisitNode(this, output->document);
    gumbo_destroy_output_iter(&opts, output);
}

// extract ComicBookInfo metadata
// https://code.google.com/archive/p/comicbookinfo/
bool ComicInfoParser::Visit(Str path, Str value, json::Type type) {
    if (json::Type::String == type && str::Eq(path, "/ComicBookInfo/1.0/title")) {
        str::Free(propTitle);
        propTitle = str::Dup(value);
    } else if (json::Type::Number == type && str::Eq(path, "/ComicBookInfo/1.0/publicationYear")) {
        Str newDate = str::Dup(fmt("%s/%d", len(propDate) == 0 ? "" : propDate, ParseInt(value)));
        str::Free(propDate);
        propDate = newDate;
    } else if (json::Type::Number == type && str::Eq(path, "/ComicBookInfo/1.0/publicationMonth")) {
        Str newDate = str::Dup(fmt("%d%s", ParseInt(value), len(propDate) == 0 ? "" : propDate));
        str::Free(propDate);
        propDate = newDate;
    } else if (json::Type::String == type && str::Eq(path, "/appID")) {
        str::Free(propCreator);
        propCreator = str::Dup(value);
    } else if (json::Type::String == type && str::Eq(path, "/lastModified")) {
        str::Free(propModDate);
        propModDate = str::Dup(value);
    } else if (json::Type::String == type && str::Eq(path, "/X-summary")) {
        str::Free(propSummary);
        propSummary = str::Dup(value);
    } else if (str::StartsWith(path, "/ComicBookInfo/1.0/credits[")) {
        int idx = -1;
        Str prop = str::Parse(path, "/ComicBookInfo/1.0/credits[%d]/", &idx);
        if (prop) {
            if (json::Type::String == type && str::Eq(prop, "person")) {
                str::Free(propAuthorTmp);
                propAuthorTmp = str::Dup(value);
            } else if (json::Type::Bool == type && str::Eq(prop, "primary") && len(propAuthorTmp) > 0 &&
                       !propAuthors.Contains(propAuthorTmp)) {
                propAuthors.Append(propAuthorTmp);
            }
        }
        return true;
    }
    // stop parsing once we have all desired information
    Str dateStr = propDate;
    int slash = str::IndexOfChar(dateStr, '/');
    return len(propTitle) == 0 || len(propAuthors) == 0 || len(propCreator) == 0 || len(propDate) == 0 || slash <= 0;
}

class EngineCbx : public EngineImages {
  public:
    explicit EngineCbx(MultiFormatArchive* arch);
    ~EngineCbx() override;

    EngineBase* Clone() override;

    TempStr GetPropertyTemp(DocProp prop) override;
    void GetProperties(Props& propsOut) override;

    TocTree* GetToc() override;

    // realPath: when non-null we actually open the archive from this
    // (local) path but still report `path` via FilePath() so callers
    // (file history, bookmarks, etc.) see the user's original file.
    static EngineBase* CreateFromFile(Str path, Str password = {}, MultiFormatArchive::Format* formatOut = nullptr,
                                      bool* isEncryptedOut = nullptr, Kind hintKind = nullptr, Str realPath = {});
    static EngineBase* CreateFromStream(IStream* stream);

  protected:
    Pixmap* LoadPixmapForPage(int pageNo, bool& deleteAfterUse) override;
    RectF LoadMediabox(int pageNo) override;
    Str GetImageData(int pageNo) override;
    TempStr GetImagePathTemp(int pageNo) override { return str::DupTemp(files[pageNo - 1]->name); }

    bool LoadFromFile(Str fileName);
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
    Str physicalPath;

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
        IStream* stm = nullptr;
        HRESULT res = fileStream->Clone(&stm);
        if (SUCCEEDED(res) && stm) {
            auto clone = CreateFromStream(stm);
            stm->Release();
            if (!clone) {
                logf("EngineCbx::Clone() failed: CreateFromStream() failed\n");
            }
            return clone;
        }
    }
    Str path = FilePath();
    if (path) {
        // keep the cached-local-copy in play on the clone too
        auto clone = CreateFromFile(path, {}, nullptr, nullptr, nullptr, physicalPath);
        if (!clone) {
            logf("EngineCbx::Clone() failed: CreateFromFile('%s') failed\n", path);
        }
        return clone;
    }
    logf("EngineCbx::Clone() failed: no stream or file path\n");
    return nullptr;
}

bool EngineCbx::LoadFromFile(Str file) {
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
    return str::CmpNatural(f1->name, f2->name) < 0;
}

static Str GetExtFromArchiveType(MultiFormatArchive* cbxFile) {
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
    return {};
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

    Str ext = GetExtFromArchiveType(cbxArchive);
    SetDefaultExt(defaultExt, ext);

    Vec<MultiFormatArchive::FileInfo*> pageFiles;

    auto& fileInfos = cbxArchive->GetFileInfos();
    int n = len(fileInfos);
    for (int i = 0; i < n; i++) {
        auto* fileInfo = fileInfos[i];
        Str fileName = fileInfo->name;
        if (!fileName) {
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
        Str metadata = Str((char*)(metadataFi->data), metadataFi->fileSizeUncompressed);
        cip.Parse(metadata);
    }
    Str comment = cbxArchive->GetComment();
    if (comment) {
        json::Parse(comment, &cip);
    }

    int nFiles = len(pageFiles);
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

    TocItem* tocBuildRoot = nullptr;
    TocItem* tocBuildCurr = nullptr;
    auto addTocItem = [&](Str title, int pageNo) {
        TocItem* ti = new TocItem(nullptr, title, pageNo);
        if (!tocBuildRoot) {
            tocBuildRoot = ti;
        } else if (tocBuildCurr) {
            tocBuildCurr->next = ti;
        }
        tocBuildCurr = ti;
    };

    int nBookmarks = len(cip.bookmarkImageIdx);
    if (nBookmarks > 0) {
        Vec<int> order;
        for (int i = 0; i < nBookmarks; i++) {
            order.Append(i);
        }
        std::sort(order.begin(), order.end(),
                  [&](int a, int b) { return cip.bookmarkImageIdx[a] < cip.bookmarkImageIdx[b]; });
        for (int oi = 0; oi < nBookmarks; oi++) {
            int bi = order[oi];
            int pageNo = cip.bookmarkImageIdx[bi] + 1;
            if (pageNo < 1 || pageNo > pageCount) {
                continue;
            }
            addTocItem(cip.bookmarkTitles[bi], pageNo);
        }
    } else {
        for (int i = 0; i < pageCount; i++) {
            Str fname = files[i]->name;
            TempStr baseName = path::GetBaseNameTemp(fname);
            addTocItem(baseName, i + 1);
        }
    }
    if (tocBuildRoot) {
        auto realRoot = new TocItem();
        realRoot->child = tocBuildRoot;
        tocTree = new TocTree(realRoot);
    }

    return true;
}

TocTree* EngineCbx::GetToc() {
    return tocTree;
}

Str EngineCbx::GetImageData(int pageNo) {
    ReportIf((pageNo < 1) || (pageNo > PageCount()));
    int fileId = files[pageNo - 1]->fileId;
    auto* fi = cbxArchive->GetFileDataById(fileId);
    if (!fi || !fi->data) {
        return {};
    }
    return Str((char*)(fi->data), fi->fileSizeUncompressed);
}

TempStr EngineCbx::GetPropertyTemp(DocProp prop) {
    if (prop == DocProp::Title) {
        return cip.propTitle;
    }

    if (prop == DocProp::Author) {
        if (len(cip.propAuthors) == 0) {
            return {};
        }
        return JoinTemp(&cip.propAuthors, ", ");
    }

    if (prop == DocProp::CreationDate) {
        return cip.propDate;
    }
    if (prop == DocProp::ModificationDate) {
        return cip.propModDate;
    }
    if (prop == DocProp::CreatorApp) {
        return cip.propCreator;
    }
    if (prop == DocProp::Subject) {
        // TODO: replace with Prop_Summary
        return cip.propSummary;
    }

    return {};
}

void EngineCbx::GetProperties(Props& propsOut) {
    EngineBase::GetProperties(propsOut);

    str::Builder filesStr;
    auto& fileInfos = cbxArchive->GetFileInfos();
    int n = len(fileInfos);
    for (int i = 0; i < n; i++) {
        auto* fi = fileInfos[i];
        if (len(fi->name) == 0) {
            continue;
        }
        if (fi->isDir) {
            continue;
        }
        filesStr.AppendChar('\n');
        filesStr.Append(fi->name);
    }
    // show paths in Windows style (#5543)
    str::TransCharsInPlace(ToStr(filesStr), StrL("/"), StrL("\\"));
    AddProp(propsOut, DocProp::Files, ToStr(filesStr));
}

Pixmap* EngineCbx::LoadPixmapForPage(int pageNo, bool& deleteAfterUse) {
    auto timeStart = TimeGet();
    defer{};
    Str img = GetImageData(pageNo);
    if (len(img) == 0) {
        logf("EngineCbx::LoadPixmapForPage(page: %d) failed\n", pageNo);
        return nullptr;
    }
    deleteAfterUse = true;
    auto res = PixmapFromData(img);
    auto dur = TimeSinceInMs(timeStart);
    logf("EngineCbx::LoadPixmapForPage(page: %d) took %.2f ms\n", pageNo, dur);
    return res;
}

RectF EngineCbx::LoadMediabox(int pageNo) {
    int fileId = files[pageNo - 1]->fileId;

    // try to get image size from just the file header (first 1024 bytes)
    Str header = cbxArchive->GetFileDataPartById(fileId, 1024);
    if (len(header) > 0) {
        Size size = ImageSizeFromDataPortable(header);
        str::Free(header);
        if (!size.IsEmpty()) {
            return RectF(0, 0, (float)size.dx, (float)size.dy);
        }
    }

    // fall back to getting the full image data
    Str img = GetImageData(pageNo);
    if (len(img) > 0) {
        Size size = ImageSizeFromDataPortable(img);
        if (!size.IsEmpty()) {
            return RectF(0, 0, (float)size.dx, (float)size.dy);
        }
        // partial/corrupt header (e.g. dx>0 but dy==0) -- don't return that;
        // fall through to GetPage so we can use the actual decoded dimensions
        // and never hand back a zero-area mediabox (would div-by-zero in
        // CalcZoomReal).
        logf("EngineCbx::LoadMediabox: empty media box from header for page: %d\n", pageNo);
    }

    ImagePage* page = GetPage(pageNo, MAX_IMAGE_PAGE_CACHE == len(pageCache));
    if (page) {
        int w = 0, h = 0;
        if (page->img) {
            // mupdf decoded the image; use its dimensions
            w = page->img->w;
            h = page->img->h;
            uint8_t orientation = page->img->orientation;
            if (orientation != 0 && (orientation & 1) == 0) {
                std::swap(w, h);
            }
        } else if (page->pixmap) {
            w = page->pixmap->width;
            h = page->pixmap->height;
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

EngineBase* EngineCbx::CreateFromFile(Str path, Str password, MultiFormatArchive::Format* formatOut,
                                      bool* isEncryptedOut, Kind hintKind, Str realPath) {
    auto timeStart = TimeGet();
    // we sniff the type from content first because the
    // files can be mis-named e.g. .cbr archive with .cbz ext
    // we only need the archive format (zip/rar/7z), not the sub-type
    // (epub/xps/fb2z), so use the Str overload to avoid
    // opening a full archive just for type detection
    MultiFormatArchive* archive = new MultiFormatArchive();
    archive->password = str::Dup(password);

    // realPath is a local copy of a file that lives on a slow drive (see
    // caller in EngineCreate.cpp). We open the archive from there but
    // still surface `path` as the logical file path.
    Str openPath = realPath ? realPath : path;

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

EngineBase* CreateEngineCbxFromFile(Str path, PasswordUI* pwdUI, Kind hintKind, Str realPath) {
    MultiFormatArchive::Format fmt = MultiFormatArchive::Format::Unknown;
    bool isEncrypted = false;
    EngineBase* engine = EngineCbx::CreateFromFile(path, {}, &fmt, &isEncrypted, hintKind, realPath);
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
        Str pwd = pwdUI->GetPassword(path, nullptr, nullptr, &saveKey);
        if (!pwd) {
            return {}; // user cancelled
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

void EngineImagesGetImageProperties(EngineBase* engine, int pageNo, Props& propsOut) {
    if (!IsEngineImages(engine)) {
        return;
    }
    ((EngineImages*)engine)->GetImageProperties(pageNo, propsOut);
}
