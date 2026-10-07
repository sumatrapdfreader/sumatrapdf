/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// EngineDjvuDec: a DjVu engine built on the small plain-C decoder in
// ext/djvudec (djvu.h / djvu.c).

#include "base/Base.h"
#include "base/File.h"
#include "base/Pixmap.h"
#include "base/GuessFileType.h"

extern "C" {
#include "djvu.h"
}

#include "gui/UIModels.h"
#include "EngineBase.h"
#include "EngineAll.h"

Kind kindEngineDjVu = "engineDjVu";

// if true, files >= kMemoryMapMinFileSize are memory-mapped
// instead of being read into memory, so opening a multi-GB file doesn't
// commit private memory for the whole file: pages come from the OS file
// cache on demand and can be discarded under memory pressure. Files on
// network drives are always read into memory: if the connection drops while
// mapped, touching a page raises EXCEPTION_IN_PAGE_ERROR (a crash) instead
// of failing with an error code.
bool gMemoryMapLargeFiles = true;

constexpr i64 kMemoryMapMinFileSize = 128LL * 1024 * 1024;

// parses "123", "#123", "# 123"; returns -1 for invalid page
static int ParseDjvuDecLink(Str link) {
    str::TrimChar(link, '#');
    str::TrimChar(link, ' ');
    if (len(link) == 0) {
        return -1;
    }
    return ParseInt(link);
}

static bool DjvuDecCouldBeURL(Str link) {
    if (len(link) == 0) {
        return false;
    }
    if (str::StartsWithI(link, StrL("http:")) || str::StartsWithI(link, StrL("https:")) ||
        str::StartsWithI(link, StrL("mailto:"))) {
        return true;
    }
    return str::Contains(link, StrL("."));
}

struct PageDestinationDjvuDec : IPageDestination {
    Str link;
    Str value;

    PageDestinationDjvuDec(Str l, Str comment) {
        kind = kindDestinationDjVu;
        link = str::Dup(l);
        if (comment) {
            value = str::Dup(comment);
        }
    }
    ~PageDestinationDjvuDec() override {
        str::Free(link);
        str::Free(value);
    }

    Str GetValue() override {
        if (value) {
            return value;
        }
        if (!DjvuDecCouldBeURL(link)) {
            return {};
        }
        value = str::Dup(url::DecodeTemp(link));
        return value;
    }
};

static IPageDestination* NewDjvuDecDestination(Arena* arena, Str link, Str comment) {
    if (len(link) == 0 || str::Eq(link, StrL("#"))) {
        return nullptr;
    }
    auto* res = arena ? New<PageDestinationDjvuDec>(arena, link, comment) : new PageDestinationDjvuDec(link, comment);
    res->rect = RectF(kDestUseDefault, kDestUseDefault, kDestUseDefault, kDestUseDefault);
    res->pageNo = ParseDjvuDecLink(link);
    return res;
}

static IPageElement* NewDjvuDecLink(int pageNo, Rect rect, Str link, Str comment) {
    auto* dest = NewDjvuDecDestination(nullptr, link, comment);
    if (!dest) {
        return nullptr;
    }
    auto* res = new PageElementDestination(dest);
    res->rect = ToRectF(rect);
    res->pageNo = pageNo;
    return res;
}

static TocItem* NewDjvuDecTocItem(Arena* arena, TocItem* parent, Str title, Str link) {
    auto* res = AllocTocItem(arena, title, 0);
    res->parent = parent;
    res->dest = NewDjvuDecDestination(arena, link, {});
    if (res->dest) {
        res->pageNo = PageDestGetPageNo(res->dest);
    }
    return res;
}

constexpr int kMaxConcurrentDjvuRenders = 3;

// Per-render djvu_abort token: cancels only this render. Never use the
// ctx-wide djvu_request_abort here -- the ctx is shared by all pages of the
// document, so it also kills concurrent in-flight renders of other pages,
// which RenderCache then mis-reports as permanently failed (blank pages).
class DjvuDecAbortCookie : public AbortCookie {
  public:
    djvu_abort ab;

    DjvuDecAbortCookie() { djvu_abort_init(&ab); }
    void Abort() override { djvu_abort_request(&ab); }
    void* GetData() override { return nullptr; }
};

struct DjvuDecPageInfo {
    RectF mediabox;
    int dpi = 300;
    // upright pixel size at subsample=1 (after intrinsic page rotation)
    int uprightW = 0;
    int uprightH = 0;
    djvu_page_type pageType = DJVU_PAGE_UNKNOWN;
    Vec<IPageElement*> allElements;
    bool gotElements = false;
};

class EngineDjvuDec : public EngineBase {
  public:
    EngineDjvuDec();
    ~EngineDjvuDec() override;
    EngineBase* Clone() override;

    RectF PageMediabox(int pageNo) override;

    Pixmap* RenderPage(RenderPageArgs& args) override;

    RectF Transform(const RectF& rect, int pageNo, float zoom, int rotation, bool inverse = false) override;

    Str GetFileData() override;
    bool SaveFileAs(Str dstPath) override;
    PageText ExtractPageText(int pageNo) override;
    bool HasClipOptimizations(int pageNo) override;

    TempStr GetPropertyTemp(DocProp prop) override;
    bool BenchLoadPage(int pageNo) override;

    Vec<IPageElement*> GetElements(int pageNo) override;
    IPageElement* GetElementAtPos(int pageNo, PointF pt) override;
    bool HandleLink(IPageDestination* dest, ILinkHandler* linkHandler) override;

    IPageDestination* GetNamedDest(Str name) override;
    TocTree* GetToc() override;

    bool Load(Str fileName);
    bool LoadFromData(Str data);

    struct ScopedRenderSlot {
        EngineDjvuDec* eng;
        bool acquired = false;

        ScopedRenderSlot(EngineDjvuDec* e, const djvu_abort* ab) : eng(e) {
            for (;;) {
                eng->renderSlotsLock.Lock();
                if (eng->activeRenders < kMaxConcurrentDjvuRenders) {
                    eng->activeRenders++;
                    acquired = true;
                    eng->renderSlotsLock.Unlock();
                    return;
                }
                eng->renderSlotsLock.Unlock();
                if (ab && ab->requested) {
                    return;
                }
                SleepInMs(10);
            }
        }
        ~ScopedRenderSlot() {
            if (!acquired) {
                return;
            }
            eng->renderSlotsLock.Lock();
            eng->activeRenders--;
            eng->renderSlotsLock.Unlock();
        }
    };

  protected:
    Str fileData;          // must outlive all docs
    file::Mapping fileMap; // used instead of fileData for large files; must outlive all docs

    // After djvu_init(), a djvu_doc is read-only and djvu_page_render /
    // djvu_page_text_get_zones / djvu_page_get_links are re-entrant on the same
    // doc. djvuCacheLock is passed to djvudec for per-page layer caching
    // (Sjbz / IW44 / composited bg). Do not hold it while calling
    // djvu_doc_drop_page_cache / djvu_doc_page_cache_size — those re-enter the
    // same lock via CacheLockCb. cacheLock guards TOC/links and the LRU list.
    djvu_ctx* ctx = nullptr;
    djvu_doc* doc = nullptr;
    Mutex djvuCacheLock;
    Mutex cacheLock;
    Mutex renderSlotsLock;
    int activeRenders = 0;
    // 0-based page indices with live djvudec page-local cache, MRU first.
    Vec<int> pageCacheLru;

    Vec<DjvuDecPageInfo*> pages;
    TocTree* tocTree = nullptr;

    PointF TransformPoint(PointF pt, int pageNo, float zoom, int rotation, bool inverse);
    bool FinishLoading();
    TocItem* BuildTocTree(TocItem* parent, djvu_outline_item* items, int n, int& idCounter, int depth);
    // After a successful render of page0: mark MRU and drop cold pages if the
    // decoder's page-local cache exceeds the byte/page budget.
    void NotePageCacheAfterRender(int page0);

    static void CacheLockCb(void* user, void* ctx);
    static void CacheUnlockCb(void* user, void* ctx);
};

// Cap decoder page-local cache (Sjbz/IW44/bg) so multi-GB books don't keep
// every visited page. Tuned for interactive re-paint of a few recent pages.
constexpr size_t kDjvuPageCacheMaxBytes = 64ull * 1024 * 1024;
constexpr int kDjvuPageCacheMaxPages = 32;

EngineDjvuDec::EngineDjvuDec() {
    kind = kindEngineDjVu;
    str::ReplaceWithCopy(&defaultExt, StrL(".djvu"));
    fileDPI = 300.0f;
}

EngineDjvuDec::~EngineDjvuDec() {
    DestroyTocTree(tocTree);
    DeleteVecMembers(pages);
    djvu_doc_close(doc);
    djvu_ctx_free(ctx);
    file::MemoryUnmap(&fileMap);
    str::Free(fileData);
}

EngineBase* EngineDjvuDec::Clone() {
    Str path = FilePath();
    if (path) {
        return CreateEngineDjvuDecFromFile(path);
    }
    if (fileData) {
        return CreateEngineDjvuDecFromData(fileData);
    }
    return nullptr;
}

bool EngineDjvuDec::Load(Str fileName) {
    SetFilePath(fileName);
    bool tryMap =
        gMemoryMapLargeFiles && !path::IsOnNetworkDrive(fileName) && file::GetSize(fileName) >= kMemoryMapMinFileSize;
    if (tryMap && file::MemoryMap(fileName, &fileMap)) {
        return FinishLoading();
    }
    fileData = file::ReadFile(fileName);
    return FinishLoading();
}

bool EngineDjvuDec::LoadFromData(Str data) {
    if (len(data) == 0) {
        return false;
    }
    fileData = str::Dup(data);
    return FinishLoading();
}

static void DjvuDecErrorCb(void* /*user*/, djvu_severity sev, const char* msg) {
    if (sev >= DJVU_SEVERITY_ERROR) {
        logf("djvudec: %s\n", Str(msg));
    }
}

void EngineDjvuDec::CacheLockCb(void* user, void* /*ctx*/) {
    ((EngineDjvuDec*)user)->djvuCacheLock.Lock();
}

void EngineDjvuDec::CacheUnlockCb(void* user, void* /*ctx*/) {
    ((EngineDjvuDec*)user)->djvuCacheLock.Unlock();
}

bool EngineDjvuDec::FinishLoading() {
    const u8* data = (const u8*)fileData.s;
    size_t dataLen = (size_t)fileData.len;
    if (fileMap.data) {
        data = fileMap.data;
        dataLen = (size_t)fileMap.size;
    }
    if (!data || dataLen == 0) {
        return false;
    }
    // Initialize scaler tables once before engines decode concurrently.
    [[maybe_unused]] static const bool initialized = [] {
        djvu_init();
        return true;
    }();

    ctx = djvu_ctx_new(nullptr, nullptr, CacheLockCb, CacheUnlockCb, DjvuDecErrorCb, this);
    if (!ctx) {
        return false;
    }
    // Lazy per-page layer cache (first render fills Sjbz/IW44/bg; later paints
    // reuse). Requires lock/unlock callbacks (set above). Budget-enforced in
    // NotePageCacheAfterRender via djvu_doc_drop_page_cache so large files
    // (e.g. multi-GB books) do not retain every page forever.
    // https://github.com/sumatrapdfreader/sumatrapdf/issues/5778
    djvu_ctx_set_cache_per_page(ctx, 1);
    // ask the decoder to emit color output in B,G,R order so it lands in a
    // Windows DIB without a separate RGB->BGR pass (the swap is folded into the
    // decoder's final output copy at no cost).
    djvu_ctx_set_bgr(ctx, 1);
    doc = djvu_doc_open(ctx, data, dataLen);
    if (!doc) {
        return false;
    }
    pageCount = djvu_doc_page_count(doc);
    if (pageCount <= 0) {
        return false;
    }

    for (int i = 0; i < pageCount; i++) {
        auto* pi = new DjvuDecPageInfo();
        djvu_page_info info{};
        RectF mbox(0, 0, 8.5f * fileDPI, 11.f * fileDPI); // fallback: letter size
        if (djvu_doc_page_info(doc, i, &info) == 0) {
            int dpi = info.dpi;
            if (dpi < 25 || dpi > 6000) {
                dpi = 300;
            }
            pi->dpi = dpi;
            int rotation = NormalizeRotation(info.rotation);
            // djvu_page_render at subsample=1 applies intrinsic rotation, so
            // upright dimensions swap width/height for 90/270
            int upW = info.width;
            int upH = info.height;
            if (rotation == 90 || rotation == 270) {
                std::swap(upW, upH);
            }
            pi->uprightW = upW;
            pi->uprightH = upH;
            float dx = (float)upW * fileDPI / (float)dpi;
            float dy = (float)upH * fileDPI / (float)dpi;
            bool isValid = dx > 0 && dx < 1e6f && dy > 0 && dy < 1e6f;
            if (isValid) {
                mbox = RectF(0, 0, dx, dy);
            }
        }
        pi->mediabox = mbox;
        pi->pageType = djvu_page_get_type(doc, i);
        VecAppend(pages, pi);

        Str title = Str(djvu_doc_page_title(doc, i));
        Str id = Str(djvu_doc_page_id(doc, i));
        if (title && id && !str::Eq(title, id)) {
            hasPageLabels = true;
        }
    }
    return true;
}

RectF EngineDjvuDec::PageMediabox(int pageNo) {
    ReportIf(pageNo < 1 || pageNo > pageCount);
    return pages[pageNo - 1]->mediabox;
}

bool EngineDjvuDec::HasClipOptimizations(int /*pageNo*/) {
    return false;
}

TempStr EngineDjvuDec::GetPropertyTemp(DocProp /*prop*/) {
    return {};
}

bool EngineDjvuDec::BenchLoadPage(int /*pageNo*/) {
    return true;
}

PointF EngineDjvuDec::TransformPoint(PointF pt, int pageNo, float zoom, int rotation, bool inverse) {
    TransformDir dir = inverse ? TransformDir::ToPage : TransformDir::ToScreen;
    if (zoom <= 0) {
        return TransformPagePoint(pt, {}, zoom, rotation, dir);
    }
    SizeF page = PageMediabox(pageNo).Size();
    return TransformPagePoint(pt, page, zoom, rotation, dir);
}

RectF EngineDjvuDec::Transform(const RectF& rect, int pageNo, float zoom, int rotation, bool inverse) {
    PointF TL = TransformPoint(rect.TL(), pageNo, zoom, rotation, inverse);
    PointF BR = TransformPoint(rect.BR(), pageNo, zoom, rotation, inverse);
    return RectF::FromXY(TL, BR);
}

constexpr int kGrayChannels = 1;
constexpr int kBgrChannels = 3;

// Rotate a top-down buffer clockwise, keeping each pixel's channels together.
template <int channels>
static u8* RotatePixels(const u8* src, int dx, int dy, int rotation, int& dxOut, int& dyOut) {
    rotation = NormalizeRotation(rotation);
    if (rotation == 0) {
        dxOut = dx;
        dyOut = dy;
        u8* out = AllocArray<u8>(dx * dy * channels);
        if (out) {
            memcpy(out, src, (size_t)dx * dy * channels);
        }
        return out;
    }
    int ndx = (rotation == 180) ? dx : dy;
    int ndy = (rotation == 180) ? dy : dx;
    u8* out = AllocArray<u8>(ndx * ndy * channels);
    if (!out) {
        return nullptr;
    }
    for (int y = 0; y < dy; y++) {
        for (int x = 0; x < dx; x++) {
            int nx = 0, ny = 0;
            if (rotation == 90) {
                nx = dy - 1 - y;
                ny = x;
            } else if (rotation == 180) {
                nx = dx - 1 - x;
                ny = dy - 1 - y;
            } else { // 270
                nx = y;
                ny = dx - 1 - x;
            }
            const u8* pixel = src + (((size_t)y * dx + x) * channels);
            memcpy(out + (((size_t)ny * ndx + nx) * channels), pixel, channels);
        }
    }
    dxOut = ndx;
    dyOut = ndy;
    return out;
}

// ceil(size / sample) >= target bounds sample by (size - 1) / (target - 1).
// A one-pixel target only needs the native dimension cap.
static int DjvuDecPickSubsample(int uprightW, int uprightH, int targetDx, int targetDy) {
    if (uprightW <= 0 || uprightH <= 0 || targetDx <= 0 || targetDy <= 0) {
        return 1;
    }
    auto limit = [](int size, int target) { return target > 1 ? (size - 1) / (target - 1) : size; };
    return std::max(1, std::min(limit(uprightW, targetDx), limit(uprightH, targetDy)));
}

static inline u8 BilinearByte(float v00, float v10, float v01, float v11, float tx, float ty) {
    float v0 = v00 + ((v10 - v00) * tx);
    float v1 = v01 + ((v11 - v01) * tx);
    float v = v0 + ((v1 - v0) * ty);
    return (u8)ClampI((int)lroundf(v), 0, 255);
}

// Map decoded page pixels into the target pixmap with bilinear filtering. The
// previous Windows path used GDI StretchBlt(HALFTONE); nearest-neighbor here
// visibly degraded text/graphics when decoded and screen sizes differ slightly
// (common even at 100% zoom due to rounding).
static Pixmap* ScaleDjvuPixelsToPixmap(const u8* src, int srcDx, int srcDy, int comp, const Rect& screen,
                                       const Rect& full) {
    Pixmap* res = AllocPixmap(screen.dx, screen.dy, PixmapFormat::BGR8);
    if (!res) {
        return nullptr;
    }

    // 1:1 blit of the visible sub-rect (no resampling)
    if (srcDx == screen.dx && srcDy == screen.dy && screen.x == full.x && screen.y == full.y && srcDx == full.dx &&
        srcDy == full.dy) {
        for (int y = 0; y < screen.dy; y++) {
            const u8* sp = src + ((size_t)y * srcDx * comp);
            u8* dst = res->data + ((size_t)y * res->stride);
            if (comp == 1) {
                for (int x = 0; x < screen.dx; x++) {
                    u8 g = sp[x];
                    dst[0] = g;
                    dst[1] = g;
                    dst[2] = g;
                    dst += 3;
                }
            } else {
                memcpy(dst, sp, (size_t)screen.dx * 3);
            }
        }
        return res;
    }

    double sx = (double)srcDx / (double)full.dx;
    double sy = (double)srcDy / (double)full.dy;
    for (int y = 0; y < screen.dy; y++) {
        float srcYf = (float)((((screen.y - full.y) + y + 0.5) * sy) - 0.5);
        int y0 = ClampI((int)floorf(srcYf), 0, srcDy - 1);
        int y1 = ClampI(y0 + 1, 0, srcDy - 1);
        float ty = srcYf - (float)y0;
        u8* dst = res->data + ((size_t)y * res->stride);
        for (int x = 0; x < screen.dx; x++) {
            float srcXf = (float)((((screen.x - full.x) + x + 0.5) * sx) - 0.5);
            int x0 = ClampI((int)floorf(srcXf), 0, srcDx - 1);
            int x1 = ClampI(x0 + 1, 0, srcDx - 1);
            float tx = srcXf - (float)x0;
            size_t off00 = (((size_t)y0 * srcDx) + x0) * comp;
            size_t off10 = (((size_t)y0 * srcDx) + x1) * comp;
            size_t off01 = (((size_t)y1 * srcDx) + x0) * comp;
            size_t off11 = (((size_t)y1 * srcDx) + x1) * comp;
            for (int c = 0; c < comp; c++) {
                dst[c] = BilinearByte(src[off00 + c], src[off10 + c], src[off01 + c], src[off11 + c], tx, ty);
            }
            if (comp == 1) {
                dst[1] = dst[2] = dst[0];
            }
            dst += 3;
        }
    }
    return res;
}

Pixmap* EngineDjvuDec::RenderPage(RenderPageArgs& args) {
    DjvuDecAbortCookie* cookie = nullptr;
    const djvu_abort* ab = nullptr;
    if (args.cookie_out) {
        cookie = new DjvuDecAbortCookie();
        *args.cookie_out = cookie;
        ab = &cookie->ab;
    }
    ScopedRenderSlot renderSlot(this, ab);
    if (!renderSlot.acquired || (ab && ab->requested)) {
        return nullptr;
    }

    int pageNo = args.pageNo;
    int userRotation = NormalizeRotation(args.rotation);
    float zoom = args.zoom;

    RectF pageRc = args.pageRect ? *args.pageRect : PageMediabox(pageNo);
    Rect screen = Transform(pageRc, pageNo, zoom, userRotation).Round();
    Rect full = Transform(PageMediabox(pageNo), pageNo, zoom, userRotation).Round();
    screen = full.Intersect(screen);
    if (screen.IsEmpty() || full.IsEmpty()) {
        return nullptr;
    }

    auto* pi = pages[pageNo - 1];
    int subsample = DjvuDecPickSubsample(pi->uprightW, pi->uprightH, full.dx, full.dy);
    // Query the output geometry, then render straight into our own buffer (BGR
    // for color, since djvu_ctx_set_bgr is on) -- no intermediate djvu_image and
    // no separate RGB->BGR/copy pass.
    djvu_render_info ri{};
    if (djvu_page_render_info(doc, pageNo - 1, subsample, &ri) != 0) {
        return nullptr;
    }
    bool isBitonal = pi->pageType == DJVU_PAGE_BITONAL || ri.format == DJVU_FORMAT_GRAY8;
    int comp = (ri.format == DJVU_FORMAT_GRAY8) ? 1 : 3;
    int sdx = ri.width, sdy = ri.height;
    i64 stride = (i64)sdx * comp;
    i64 pixelCount = stride * sdy;
    if (sdx <= 0 || sdy <= 0 || stride > INT_MAX || pixelCount > INT_MAX) {
        return nullptr;
    }
    AutoFree<u8> pixels(AllocArray<u8>((int)pixelCount));
    if (!pixels) {
        return nullptr;
    }
    if (djvu_page_render_into_abortable(doc, pageNo - 1, subsample, pixels, (int)stride, ab) != 0) {
        return nullptr;
    }
    NotePageCacheAfterRender(pageNo - 1);

    // The decoder applies intrinsic rotation; only user rotation remains.
    int rdx = sdx, rdy = sdy;
    if (userRotation != 0) {
        if (isBitonal) {
            pixels.Set(RotatePixels<kGrayChannels>(pixels, sdx, sdy, userRotation, rdx, rdy));
        } else {
            pixels.Set(RotatePixels<kBgrChannels>(pixels, sdx, sdy, userRotation, rdx, rdy));
        }
        if (!pixels) {
            return {};
        }
    }

    return ScaleDjvuPixelsToPixmap(pixels, rdx, rdy, comp, screen, full);
}

Str EngineDjvuDec::GetFileData() {
    Str path = FilePath();
    if (path) {
        return file::ReadFile(path);
    }
    return str::Dup(fileData);
}

bool EngineDjvuDec::SaveFileAs(Str dstPath) {
    return SaveFileOrData(FilePath(), fileData, dstPath);
}

// recursively collect word-level text from the zone tree, with the rect of
// each byte's glyph in byteCoords. Zone coords are top-down full-resolution
// page pixels; dpiF scales them to mediabox (fileDPI) units.
static void CollectZonesUtf8(djvu_text_zone* z, float dpiF, str::Builder& sb, Vec<Rect>& byteCoords) {
    if (!z) {
        return;
    }
    if (z->nchildren > 0) {
        for (int i = 0; i < z->nchildren; i++) {
            djvu_text_zone* c = &z->children[i];
            CollectZonesUtf8(c, dpiF, sb, byteCoords);
            if (c->type == DJVU_ZONE_WORD) {
                VecAppend(byteCoords, Rect((int)((float)(c->x + c->w) * dpiF), (int)((float)c->y * dpiF), 2,
                                           (int)((float)c->h * dpiF)));
                sb.AppendChar(' ');
            } else if (c->type == DJVU_ZONE_LINE) {
                VecAppend(byteCoords, Rect());
                sb.AppendChar('\n');
            }
        }
        return;
    }
    Str text(z->text);
    if (len(text) == 0) {
        return;
    }
    Rect r((int)((float)z->x * dpiF), (int)((float)z->y * dpiF), (int)((float)z->w * dpiF), (int)((float)z->h * dpiF));
    // zones are usually word-granularity, so approximate per-glyph rects by
    // evenly splitting the box horizontally (computed from endpoints so slices
    // tile exactly); this makes partial-word search hits and selections
    // highlight roughly just the matched characters
    int n = Utf8CodepointCount(text);
    int i = 0;
    for (int byteIdx = 0; byteIdx < len(text); i++) {
        int xStart = r.x + ((i * r.dx) / n);
        int xEnd = r.x + (((i + 1) * r.dx) / n);
        int byteStart = byteIdx;
        Utf8CodepointNext(text, byteIdx);
        for (int b = byteStart; b < byteIdx; b++) {
            VecAppend(byteCoords, Rect(xStart, r.y, xEnd - xStart, r.dy));
        }
    }
    sb.Append(text);
}

PageText EngineDjvuDec::ExtractPageText(int pageNo) {
    djvu_page_text_zones* z = djvu_page_text_get_zones(doc, pageNo - 1);
    AutoCall freeZones(djvu_text_zones_destroy, ctx, z);
    if (!z || !z->root) {
        return {};
    }
    float dpiF = fileDPI / (float)pages[pageNo - 1]->dpi;
    return DjvuZonesToPageText(z->root, dpiF);
}

// zone text is cut out of the page text by byte offsets, so a multi-byte
// character can be split across zones and count as two codepoints there but
// one in the page text; taking the rects per codepoint of the page text keeps
// coords and text in step
PageText DjvuZonesToPageText(djvu_text_zone* root, float dpiF) {
    str::Builder sb;
    Vec<Rect> byteCoords;
    CollectZonesUtf8(root, dpiF, sb, byteCoords);

    if (len(sb) == 0) {
        return {};
    }
    Str text = ToStr(sb);
    ReportIf(len(byteCoords) != len(text));
    Vec<Rect> coords;
    for (int byteIdx = 0; byteIdx < len(text);) {
        VecAppend(coords, byteCoords[byteIdx]);
        Utf8CodepointNext(text, byteIdx);
    }
    PageText res;
    res.nCodepoints = len(coords);
    res.text = sb.TakeStr();
    res.coords = VecTake(coords);
    return res;
}

// returns a numeric DjVu link to a named page (if the name resolves)
static TempStr ResolveNamedDestDjvuDecTemp(djvu_doc* doc, Str name) {
    if (len(name) == 0) {
        return {};
    }
    int pageNo = djvu_doc_page_by_name(doc, CStrTemp(name));
    if (pageNo < 0) {
        return {};
    }
    return fmt("#%d", pageNo + 1);
}

Vec<IPageElement*> EngineDjvuDec::GetElements(int pageNo) {
    ReportIf(pageNo < 1 || pageNo > PageCount());
    auto* pi = pages[pageNo - 1];
    if (pi->gotElements) {
        return pi->allElements;
    }
    ScopedMutex scope(&cacheLock);
    if (pi->gotElements) {
        return pi->allElements;
    }
    pi->gotElements = true;
    auto& els = pi->allElements;

    djvu_page_links* links = djvu_page_get_links(doc, pageNo - 1);
    if (!links) {
        return els;
    }
    float dpiF = fileDPI / (float)pi->dpi;
    for (int i = 0; i < links->nlinks; i++) {
        djvu_link& l = links->links[i];
        Str url = Str(l.url);
        if (len(url) == 0) {
            continue;
        }
        Rect rect((int)((float)l.x * dpiF), (int)((float)l.y * dpiF), (int)((float)l.w * dpiF),
                  (int)((float)l.h * dpiF));
        TempStr link = ResolveNamedDestDjvuDecTemp(doc, url);
        if (len(link) == 0) {
            link = url;
        }
        auto* el = NewDjvuDecLink(pageNo, rect, link, Str(l.comment));
        if (el) {
            VecAppend(els, el);
        }
    }
    djvu_page_links_destroy(ctx, links);
    return els;
}

IPageElement* EngineDjvuDec::GetElementAtPos(int pageNo, PointF pt) {
    Vec<IPageElement*> els = GetElements(pageNo);
    int n = len(els);
    for (int i = n - 1; i >= 0; i--) {
        auto* el = els[i];
        if (el->GetRect().Contains(pt)) {
            return el;
        }
    }
    return nullptr;
}

bool EngineDjvuDec::HandleLink(IPageDestination* dest, ILinkHandler* linkHandler) {
    if (dest->GetKind() != kindDestinationDjVu) {
        return false;
    }
    auto* ddest = (PageDestinationDjvuDec*)dest;
    Str link = ddest->link;
    if (str::Eq(link, StrL("#+1"))) {
        linkHandler->GoToNextPage();
        return true;
    }
    if (str::Eq(link, StrL("#-1"))) {
        linkHandler->GoToPrevPage();
        return true;
    }
    if (DjvuDecCouldBeURL(link)) {
        linkHandler->LaunchURL(link);
        return true;
    }
    int pageNo = ParseDjvuDecLink(link);
    if (pageNo < 1 || pageNo > pageCount) {
        TempStr resolved = ResolveNamedDestDjvuDecTemp(doc, link);
        if (resolved) {
            pageNo = ParseDjvuDecLink(resolved);
        }
    }
    if (pageNo < 1 || pageNo > pageCount) {
        logf("EngineDjvuDec::HandleLink: invalid link '%s'\n", link);
        return false;
    }
    linkHandler->GoToPage(pageNo, true);
    return true;
}

// engine-owned; do not delete
IPageDestination* EngineDjvuDec::GetNamedDest(Str name) {
    Str n = name;
    str::TrimPrefix(n, StrL("#"));
    TempStr link = ResolveNamedDestDjvuDecTemp(doc, n);
    if (link) {
        return NewDjvuDecDestination(arena, link, {});
    }
    return nullptr;
}

TocItem* EngineDjvuDec::BuildTocTree(TocItem* parent, djvu_outline_item* items, int n, int& idCounter, int depth) {
    if (depth >= 64) {
        return nullptr;
    }
    TocItem* root = nullptr;
    TocItem** next = &root;
    for (int i = 0; i < n; i++) {
        djvu_outline_item& it = items[i];
        Str title = Str(it.title);
        Str url = Str(it.url);
        TempStr link = url;
        TempStr resolved = ResolveNamedDestDjvuDecTemp(doc, url);
        if (resolved) {
            link = resolved;
        } else if (it.page_no >= 0) {
            link = fmt("#%d", it.page_no + 1);
        }
        TocItem* tocItem = NewDjvuDecTocItem(arena, parent, title, link);
        tocItem->id = ++idCounter;
        tocItem->child = BuildTocTree(tocItem, it.children, it.nchildren, idCounter, depth + 1);
        *next = tocItem;
        next = &tocItem->next;
    }
    return root;
}

TocTree* EngineDjvuDec::GetToc() {
    if (tocTree) {
        return tocTree;
    }
    ScopedMutex scope(&cacheLock);
    if (tocTree) {
        return tocTree;
    }
    djvu_outline_item* root = djvu_doc_outline(doc);
    if (!root) {
        return nullptr;
    }
    int idCounter = 0;
    TocItem* rootItem = BuildTocTree(nullptr, root->children, root->nchildren, idCounter, 0);
    djvu_outline_destroy(ctx, root);
    if (!rootItem) {
        return nullptr;
    }
    auto* realRoot = AllocTocItem(arena, {}, 0);
    realRoot->child = rootItem;
    tocTree = AllocTocTree(arena, realRoot);
    return tocTree;
}

void EngineDjvuDec::NotePageCacheAfterRender(int page0) {
    if (!doc || page0 < 0 || page0 >= pageCount) {
        return;
    }
    // Reorder LRU under cacheLock. Size queries and drops re-enter djvuCacheLock
    // via the decoder callbacks — never hold djvuCacheLock here.
    {
        ScopedMutex scope(&cacheLock);
        VecRemove(pageCacheLru, page0);
        VecInsertAt(pageCacheLru, 0, page0);
    }

    for (;;) {
        int dropPage = -1;
        {
            ScopedMutex scope(&cacheLock);
            int n = len(pageCacheLru);
            if (n <= 1) {
                return;
            }
            size_t total = 0;
            for (int i = 0; i < n; i++) {
                total += djvu_doc_page_cache_size(doc, pageCacheLru[i]);
            }
            if (total <= kDjvuPageCacheMaxBytes && n <= kDjvuPageCacheMaxPages) {
                return;
            }
            // Evict least-recently used that is not the page we just rendered.
            dropPage = VecLast(pageCacheLru);
            if (dropPage == page0) {
                return;
            }
            VecRemoveLast(pageCacheLru);
        }
        djvu_doc_drop_page_cache(doc, dropPage);
    }
}

/* EngineDjvuDec.cpp: DjVu engine built on ext/djvudec */
bool IsEngineDjVuSupportedFileType(FileType kind) {
    return kind == FileType::DjVu;
}

EngineBase* CreateEngineDjvuDecFromData(Str data) {
    EngineDjvuDec* engine = new EngineDjvuDec();
    if (engine->LoadFromData(data)) {
        return engine;
    }
    SafeEngineRelease(&engine);
    return nullptr;
}

EngineBase* CreateEngineDjvuDecFromFile(Str path) {
    EngineDjvuDec* engine = new EngineDjvuDec();
    if (engine->Load(path)) {
        return engine;
    }
    SafeEngineRelease(&engine);
    return nullptr;
}

#if IS_DEBUG
bool EngineDjvuDec_UnitTestRender() {
    const int subsamples[][5] = {
        {INT_MAX, INT_MAX, INT_MAX / 2, INT_MAX / 2, 2},
        {5, 7, 2, 2, 4},
        {300, 200, 100, 100, 2},
        {300, 200, 1, 1, 200},
        {1, 5, 1, 1, 1},
        {300, 200, 1, 2, 199},
        {300, 200, 301, 201, 1},
        {0, 200, 1, 1, 1},
    };
    for (const auto& c : subsamples) {
        if (DjvuDecPickSubsample(c[0], c[1], c[2], c[3]) != c[4]) {
            return false;
        }
    }

    const u8 expected[][6] = {{1, 2, 3, 4, 5, 6}, {4, 1, 5, 2, 6, 3}, {6, 5, 4, 3, 2, 1}, {3, 6, 2, 5, 1, 4}};
    const int channels[] = {kGrayChannels, kBgrChannels};
    for (int comp : channels) {
        u8 src[18];
        for (int i = 0; i < 6; i++) {
            for (int c = 0; c < comp; c++) {
                src[i * comp + c] = (u8)(i + 1 + c * 10);
            }
        }
        for (int r = 0; r < 4; r++) {
            int dx = 0, dy = 0;
            AutoFree<u8> out(comp == 1 ? RotatePixels<kGrayChannels>(src, 3, 2, r * 90, dx, dy)
                                       : RotatePixels<kBgrChannels>(src, 3, 2, r * 90, dx, dy));
            if (!out || dx != (r % 2 == 0 ? 3 : 2) || dy != (r % 2 == 0 ? 2 : 3)) {
                return false;
            }
            for (int i = 0; i < 6; i++) {
                for (int c = 0; c < comp; c++) {
                    if (out.Get()[i * comp + c] != expected[r][i] + c * 10) {
                        return false;
                    }
                }
            }
        }
    }
    return true;
}
#endif
