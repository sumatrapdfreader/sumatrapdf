/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// EngineDjvuDec: a DjVu engine built on the small plain-C decoder in
// ext/djvudec (djvu.h / djvu.c).

extern "C" {
#include "djvu.h"
}

#include "base/Base.h"
#include "base/ScopedWin.h"
#include "base/File.h"
#include "base/GuessFileType.h"
#include "base/Win.h"

#include "wingui/UIModels.h"

#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"

#include "base/Log.h"

Kind kindEngineDjVu = "engineDjVu";

// parses "123", "#123", "# 123"; returns -1 for invalid page
static int ParseDjvuDecLink(Str link) {
    str::SkipChar(link, '#');
    str::SkipChar(link, ' ');
    if (!link) {
        return -1;
    }
    return ParseInt(link);
}

static bool DjvuDecCouldBeURL(Str link) {
    if (!link) {
        return false;
    }
    if (str::StartsWithI(link, "http:") || str::StartsWithI(link, "https:") || str::StartsWithI(link, "mailto:")) {
        return true;
    }
    return str::Contains(link, ".");
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

    Str GetValue2() override {
        if (value) {
            return value;
        }
        if (!DjvuDecCouldBeURL(link)) {
            return {};
        }
        value = str::Dup(link);
        url::DecodeInPlace(value);
        return value;
    }
};

static IPageDestination* NewDjvuDecDestination(Str link, Str comment) {
    if (!link || str::Eq(link, "#")) {
        return nullptr;
    }
    auto res = new PageDestinationDjvuDec(link, comment);
    res->rect = RectF(DEST_USE_DEFAULT, DEST_USE_DEFAULT, DEST_USE_DEFAULT, DEST_USE_DEFAULT);
    res->pageNo = ParseDjvuDecLink(link);
    return res;
}

static IPageElement* NewDjvuDecLink(int pageNo, Rect rect, Str link, Str comment) {
    auto dest = NewDjvuDecDestination(link, comment);
    if (!dest) {
        return nullptr;
    }
    auto res = new PageElementDestination(dest);
    res->rect = ToRectF(rect);
    res->pageNo = pageNo;
    return res;
}

static TocItem* NewDjvuDecTocItem(TocItem* parent, Str title, Str link) {
    auto res = new TocItem(parent, title, 0);
    res->dest = NewDjvuDecDestination(link, {});
    if (res->dest) {
        res->pageNo = PageDestGetPageNo(res->dest);
    }
    return res;
}

struct DjvuDecPageInfo {
    RectF mediabox;
    int dpi = 300;
    // upright pixel size at subsample=1 (after intrinsic page rotation)
    int uprightW = 0;
    int uprightH = 0;
    int intrinsicRotation = 0;
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

    Pixmap* RenderPage(RenderPageArgs&) override;

    RectF Transform(const RectF& rect, int pageNo, float zoom, int rotation, bool inverse = false) override;

    Str GetFileData() override;
    bool SaveFileAs(Str copyFileName) override;
    PageText ExtractPageText(int pageNo) override;
    bool HasClipOptimizations(int pageNo) override;

    TempStr GetPropertyTemp(Str name) override;
    bool BenchLoadPage(int pageNo) override;

    Vec<IPageElement*> GetElements(int pageNo) override;
    IPageElement* GetElementAtPos(int pageNo, PointF pt) override;
    bool HandleLink(IPageDestination*, ILinkHandler*) override;

    IPageDestination* GetNamedDest(Str name) override;
    TocTree* GetToc() override;

    bool Load(Str fileName);
    bool Load(IStream* stream);

  protected:
    IStream* stream = nullptr;
    Str fileData; // must outlive all docs

    // After djvu_init(), a djvu_doc is read-only and djvu_page_render /
    // djvu_page_text_get_zones / djvu_page_get_links are re-entrant on the same
    // doc. djvuCacheLock is passed to djvudec for per-page layer caching;
    // cacheLock guards lazy one-time caches (page links, TOC).
    djvu_ctx* ctx = nullptr;
    djvu_doc* doc = nullptr;
    CRITICAL_SECTION djvuCacheLock;
    CRITICAL_SECTION cacheLock;

    Vec<DjvuDecPageInfo*> pages;
    TocTree* tocTree = nullptr;

    PointF TransformPoint(PointF pt, int pageNo, float zoom, int rotation, bool inverse);
    bool FinishLoading();
    TocItem* BuildTocTree(TocItem* parent, djvu_outline_item* items, int n, int& idCounter);

    static void CacheLockCb(void* user, void* ctx);
    static void CacheUnlockCb(void* user, void* ctx);
};

EngineDjvuDec::EngineDjvuDec() {
    kind = kindEngineDjVu;
    SetDefaultExt(defaultExt, ".djvu");
    fileDPI = 300.0f;
    InitializeCriticalSection(&djvuCacheLock);
    InitializeCriticalSection(&cacheLock);
}

EngineDjvuDec::~EngineDjvuDec() {
    delete tocTree;
    DeleteVecMembers(pages);
    if (doc) {
        djvu_doc_close(doc);
    }
    if (ctx) {
        djvu_ctx_free(ctx);
    }
    str::Free(fileData);
    if (stream) {
        stream->Release();
    }
    DeleteCriticalSection(&djvuCacheLock);
    DeleteCriticalSection(&cacheLock);
}

EngineBase* EngineDjvuDec::Clone() {
    if (stream != nullptr) {
        return CreateEngineDjvuDecFromStream(stream);
    }
    Str path = FilePath();
    if (path) {
        return CreateEngineDjvuDecFromFile(path);
    }
    return nullptr;
}

bool EngineDjvuDec::Load(Str fileName) {
    SetFilePath(fileName);
    fileData = file::ReadFile(fileName);
    return FinishLoading();
}

bool EngineDjvuDec::Load(IStream* stm) {
    fileData = GetDataFromStream(stm, nullptr);
    if (!str::IsEmpty(fileData)) {
        stream = stm;
        stream->AddRef();
    }
    return FinishLoading();
}

static void DjvuDecErrorCb(void*, djvu_severity sev, const char* msg) {
    if (sev >= DJVU_SEVERITY_ERROR) {
        logf("djvudec: %s\n", Str(msg));
    }
}

void EngineDjvuDec::CacheLockCb(void* user, void*) {
    EnterCriticalSection(&((EngineDjvuDec*)user)->djvuCacheLock);
}

void EngineDjvuDec::CacheUnlockCb(void* user, void*) {
    LeaveCriticalSection(&((EngineDjvuDec*)user)->djvuCacheLock);
}

// djvu_init() must run once before concurrent decode (bilinear scaler table).
// Engines can be created on multiple threads (async document loads).
static SRWLOCK gDjvuDecInitLock = SRWLOCK_INIT;
static bool gDjvuDecInitialized = false;

static void DjvuDecInitOnce() {
    AcquireSRWLockExclusive(&gDjvuDecInitLock);
    if (!gDjvuDecInitialized) {
        djvu_init();
        gDjvuDecInitialized = true;
    }
    ReleaseSRWLockExclusive(&gDjvuDecInitLock);
}

bool EngineDjvuDec::FinishLoading() {
    if (str::IsEmpty(fileData)) {
        return false;
    }
    DjvuDecInitOnce();
    ctx = djvu_ctx_new(nullptr, nullptr, CacheLockCb, CacheUnlockCb, DjvuDecErrorCb, this);
    if (!ctx) {
        return false;
    }
    djvu_ctx_set_cache_precache_shared(ctx, 1);
    djvu_ctx_set_cache_per_page(ctx, 1);
    // ask the decoder to emit color output in B,G,R order so it lands in a
    // Windows DIB without a separate RGB->BGR pass (the swap is folded into the
    // decoder's final output copy at no cost).
    djvu_ctx_set_bgr(ctx, 1);
    doc = djvu_doc_open(ctx, (u8*)fileData.s, (size_t)fileData.len);
    if (!doc) {
        return false;
    }
    pageCount = djvu_doc_page_count(doc);
    if (pageCount <= 0) {
        return false;
    }

    for (int i = 0; i < pageCount; i++) {
        auto pi = new DjvuDecPageInfo();
        djvu_page_info info{};
        RectF mbox(0, 0, 8.5f * GetFileDPI(), 11.f * GetFileDPI()); // fallback: letter size
        if (djvu_doc_page_info(doc, i, &info) == 0) {
            int dpi = info.dpi;
            if (dpi < 25 || dpi > 6000) {
                dpi = 300;
            }
            pi->dpi = dpi;
            pi->intrinsicRotation = NormalizeRotation(info.rotation);
            // djvu_page_render at subsample=1 applies intrinsic rotation, so
            // upright dimensions swap width/height for 90/270
            int upW = info.width;
            int upH = info.height;
            if (pi->intrinsicRotation == 90 || pi->intrinsicRotation == 270) {
                std::swap(upW, upH);
            }
            pi->uprightW = upW;
            pi->uprightH = upH;
            float dx = upW * GetFileDPI() / dpi;
            float dy = upH * GetFileDPI() / dpi;
            bool isValid = dx > 0 && dx < 1e6f && dy > 0 && dy < 1e6f;
            if (isValid) {
                mbox = RectF(0, 0, dx, dy);
            }
        }
        pi->mediabox = mbox;
        pi->pageType = djvu_page_get_type(doc, i);
        pages.Append(pi);

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

bool EngineDjvuDec::HasClipOptimizations(int) {
    return false;
}

TempStr EngineDjvuDec::GetPropertyTemp(Str) {
    return {};
}

bool EngineDjvuDec::BenchLoadPage(int) {
    return true;
}

PointF EngineDjvuDec::TransformPoint(PointF pt, int pageNo, float zoom, int rotation, bool inverse) {
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
    PointF res = pt; // rotation == 0
    if (90 == rotation) {
        res = PointF(page.dy - pt.y, pt.x);
    } else if (180 == rotation) {
        res = PointF(page.dx - pt.x, page.dy - pt.y);
    } else if (270 == rotation) {
        res = PointF(pt.y, page.dx - pt.x);
    }
    res.x *= zoom;
    res.y *= zoom;
    return res;
}

RectF EngineDjvuDec::Transform(const RectF& rect, int pageNo, float zoom, int rotation, bool inverse) {
    PointF TL = TransformPoint(rect.TL(), pageNo, zoom, rotation, inverse);
    PointF BR = TransformPoint(rect.BR(), pageNo, zoom, rotation, inverse);
    return RectF::FromXY(TL, BR);
}

// rotate a top-down gray8 buffer clockwise by rotation (0/90/180/270)
static u8* RotateGray8(const u8* src, int dx, int dy, int rotation, int& dxOut, int& dyOut) {
    rotation = NormalizeRotation(rotation);
    if (rotation == 0) {
        dxOut = dx;
        dyOut = dy;
        u8* out = AllocArray<u8>((size_t)dx * dy);
        if (out) {
            memcpy(out, src, (size_t)dx * dy);
        }
        return out;
    }
    int ndx = (rotation == 180) ? dx : dy;
    int ndy = (rotation == 180) ? dy : dx;
    u8* out = AllocArray<u8>((size_t)ndx * ndy);
    if (!out) {
        return nullptr;
    }
    for (int y = 0; y < dy; y++) {
        for (int x = 0; x < dx; x++) {
            u8 v = src[(size_t)y * dx + x];
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
            out[(size_t)ny * ndx + nx] = v;
        }
    }
    dxOut = ndx;
    dyOut = ndy;
    return out;
}

// rotate a top-down 24bpp BGR buffer clockwise by rotation (0/90/180/270)
static u8* RotateBgr(const u8* src, int dx, int dy, int rotation, int& dxOut, int& dyOut) {
    rotation = NormalizeRotation(rotation);
    if (rotation == 0) {
        dxOut = dx;
        dyOut = dy;
        u8* out = AllocArray<u8>((size_t)dx * dy * 3);
        if (out) {
            memcpy(out, src, (size_t)dx * dy * 3);
        }
        return out;
    }
    int ndx = (rotation == 180) ? dx : dy;
    int ndy = (rotation == 180) ? dy : dx;
    u8* out = AllocArray<u8>((size_t)ndx * ndy * 3);
    if (!out) {
        return nullptr;
    }
    for (int y = 0; y < dy; y++) {
        for (int x = 0; x < dx; x++) {
            const u8* s = src + ((size_t)y * dx + x) * 3;
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
            u8* d = out + ((size_t)ny * ndx + nx) * 3;
            d[0] = s[0];
            d[1] = s[1];
            d[2] = s[2];
        }
    }
    dxOut = ndx;
    dyOut = ndy;
    return out;
}

// Pick the largest subsample whose decoded bitmap still covers the target pixel
// size, so StretchBlt only shrinks (never upscales) and HALFTONE gives
// anti-aliased edges like libdjvu. The coverage test is ceil(dim/(s+1)) >=
// target; plain floor division (dim/target) is off by one when target doesn't
// divide dim -- e.g. 3597/1799 == 1 even though ceil(3597/2) == 1799 still
// covers 1799, which made high-dpi pages decode at full resolution (4x the
// pixels) at 100% zoom. Compound pages stay at subsample=1 so the color
// composite runs.
static int DjvuDecPickSubsample(djvu_page_type pageType, int uprightW, int uprightH, int targetDx, int targetDy) {
    if (pageType == DJVU_PAGE_COMPOUND) {
        return 1;
    }
    if (uprightW <= 0 || uprightH <= 0 || targetDx <= 0 || targetDy <= 0) {
        return 1;
    }
    int subsample = 1;
    while ((uprightW + subsample) / (subsample + 1) >= targetDx &&
           (uprightH + subsample) / (subsample + 1) >= targetDy) {
        subsample++; // ceil(uprightW/(s+1)) >= targetDx && ceil(uprightH/(s+1)) >= targetDy
    }
    if (subsample > uprightW) {
        subsample = uprightW;
    }
    if (subsample > uprightH) {
        subsample = uprightH;
    }
    return subsample;
}

// create a top-down 24bpp DIB section and copy bgr into it (row-aligned)
static HBITMAP CreateBgrDib(const u8* bgr, int dx, int dy, HANDLE* hMapOut) {
    int stride = ((dx * 3 + 3) / 4) * 4;
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = dx;
    bmi.bmiHeader.biHeight = -dy; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biCompression = BI_RGB;
    bmi.bmiHeader.biSizeImage = stride * dy;

    void* data = nullptr;
    HANDLE hMap =
        CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, bmi.bmiHeader.biSizeImage, nullptr);
    HBITMAP hbmp = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &data, hMap, 0);
    if (hbmp && data && bgr) {
        for (int y = 0; y < dy; y++) {
            memcpy((u8*)data + (size_t)y * stride, bgr + (size_t)y * dx * 3, (size_t)dx * 3);
        }
    }
    if (hMapOut) {
        *hMapOut = hMap;
    }
    return hbmp;
}

// create a top-down 8bpp grayscale DIB (like EngineDjVu for bitonal pages)
static HBITMAP CreateGray8Dib(const u8* gray, int dx, int dy, HANDLE* hMapOut) {
    int stride = ((dx + 3) / 4) * 4;
    BITMAPINFO* bmi = (BITMAPINFO*)calloc(1, sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD));
    if (!bmi) {
        return nullptr;
    }
    for (int i = 0; i < 256; i++) {
        bmi->bmiColors[i].rgbRed = bmi->bmiColors[i].rgbGreen = bmi->bmiColors[i].rgbBlue = (BYTE)i;
    }
    bmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi->bmiHeader.biWidth = dx;
    bmi->bmiHeader.biHeight = -dy;
    bmi->bmiHeader.biPlanes = 1;
    bmi->bmiHeader.biBitCount = 8;
    bmi->bmiHeader.biCompression = BI_RGB;
    bmi->bmiHeader.biClrUsed = 256;
    bmi->bmiHeader.biSizeImage = stride * dy;

    void* data = nullptr;
    HANDLE hMap =
        CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, bmi->bmiHeader.biSizeImage, nullptr);
    HBITMAP hbmp = CreateDIBSection(nullptr, bmi, DIB_RGB_COLORS, &data, hMap, 0);
    if (hbmp && data && gray) {
        for (int y = 0; y < dy; y++) {
            memcpy((u8*)data + (size_t)y * stride, gray + (size_t)y * dx, (size_t)dx);
        }
    }
    free(bmi);
    if (hMapOut) {
        *hMapOut = hMap;
    }
    return hbmp;
}

static RenderedBitmap* StretchDibToScreen(HBITMAP srcBmp, HANDLE srcMap, int rdx, int rdy, const Rect& screen,
                                          const Rect& full, bool isBitonal) {
    HANDLE dstMap = nullptr;
    int ddx = screen.dx;
    int ddy = screen.dy;
    HBITMAP dstBmp = isBitonal ? CreateGray8Dib(nullptr, ddx, ddy, &dstMap) : CreateBgrDib(nullptr, ddx, ddy, &dstMap);
    RenderedBitmap* res = nullptr;
    if (dstBmp) {
        HDC hdc = GetDC(nullptr);
        HDC srcDC = CreateCompatibleDC(hdc);
        HDC dstDC = CreateCompatibleDC(hdc);
        HGDIOBJ oldSrc = SelectObject(srcDC, srcBmp);
        HGDIOBJ oldDst = SelectObject(dstDC, dstBmp);
        double sx = (double)rdx / full.dx;
        double sy = (double)rdy / full.dy;
        int srcX = (int)(screen.x * sx);
        int srcY = (int)(screen.y * sy);
        int srcW = (int)(screen.dx * sx + 0.5);
        int srcH = (int)(screen.dy * sy + 0.5);
        if (srcW < 1) {
            srcW = 1;
        }
        if (srcH < 1) {
            srcH = 1;
        }
        bool sameSize = (srcW == ddx && srcH == ddy);
        if (sameSize && screen.x == 0 && screen.y == 0) {
            BitBlt(dstDC, 0, 0, ddx, ddy, srcDC, srcX, srcY, SRCCOPY);
        } else {
            SetStretchBltMode(dstDC, HALFTONE);
            SetBrushOrgEx(dstDC, 0, 0, nullptr);
            StretchBlt(dstDC, 0, 0, ddx, ddy, srcDC, srcX, srcY, srcW, srcH, SRCCOPY);
        }
        SelectObject(srcDC, oldSrc);
        SelectObject(dstDC, oldDst);
        DeleteDC(srcDC);
        DeleteDC(dstDC);
        ReleaseDC(nullptr, hdc);
        res = new RenderedBitmap(dstBmp, screen.Size(), dstMap);
    } else if (dstMap) {
        CloseHandle(dstMap);
    }
    DeleteObject(srcBmp);
    if (srcMap) {
        CloseHandle(srcMap);
    }
    return res;
}

Pixmap* EngineDjvuDec::RenderPage(RenderPageArgs& args) {
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

    auto pi = pages[pageNo - 1];
    int subsample = DjvuDecPickSubsample(pi->pageType, pi->uprightW, pi->uprightH, full.dx, full.dy);
    // The decoder applies the page's intrinsic rotation at every subsample (via
    // a fast tiled transpose) and djvu_page_render_info already reports the
    // upright dims, so we only rotate here for an explicit user rotation (rare).
    int rotateAfter = userRotation;

    // Query the output geometry, then render straight into our own buffer (BGR
    // for color, since djvu_ctx_set_bgr is on) -- no intermediate djvu_image and
    // no separate RGB->BGR/copy pass. (A further optimization would render_into
    // the DIB bits directly to also drop the CreateBgrDib copy; kept as a plain
    // buffer here to preserve the user-rotate path below.)
    djvu_render_info ri{};
    if (djvu_page_render_info(doc, pageNo - 1, subsample, &ri) != 0) {
        return nullptr;
    }
    bool isBitonal = pi->pageType == DJVU_PAGE_BITONAL || ri.format == DJVU_FORMAT_GRAY8;
    int comp = (ri.format == DJVU_FORMAT_GRAY8) ? 1 : 3;
    int sdx = ri.width, sdy = ri.height;
    u8* pixels = AllocArray<u8>((size_t)sdx * sdy * comp);
    if (!pixels) {
        return nullptr;
    }
    if (djvu_page_render_into(doc, pageNo - 1, subsample, pixels, sdx * comp) != 0) {
        free(pixels);
        return nullptr;
    }

    u8* rotated = pixels;
    int rdx = sdx, rdy = sdy;
    if (rotateAfter != 0) {
        if (isBitonal) {
            rotated = RotateGray8(pixels, sdx, sdy, rotateAfter, rdx, rdy);
        } else {
            rotated = RotateBgr(pixels, sdx, sdy, rotateAfter, rdx, rdy);
        }
        free(pixels);
        if (!rotated) {
            return {};
        }
    }

    HANDLE srcMap = nullptr;
    HBITMAP srcBmp = isBitonal ? CreateGray8Dib(rotated, rdx, rdy, &srcMap) : CreateBgrDib(rotated, rdx, rdy, &srcMap);
    free(rotated);
    if (!srcBmp) {
        if (srcMap) {
            CloseHandle(srcMap);
        }
        return nullptr;
    }

    return PixmapFromRenderedBitmap(StretchDibToScreen(srcBmp, srcMap, rdx, rdy, screen, full, isBitonal));
}

Str EngineDjvuDec::GetFileData() {
    return GetStreamOrFileData(stream, FilePath());
}

bool EngineDjvuDec::SaveFileAs(Str dstPath) {
    if (stream) {
        Str d = GetDataFromStream(stream, nullptr);
        bool ok = !str::IsEmpty(d) && file::WriteFile(dstPath, d);
        str::Free(d);
        if (ok) {
            return true;
        }
    }
    Str srcPath = FilePath();
    if (!srcPath) {
        return false;
    }
    return file::Copy(dstPath, srcPath, false);
}

// recursively collect word-level text + coords from the zone tree.
// zone coords are top-down full-resolution page pixels; dpiF scales them to
// mediabox (fileDPI) units.
static void CollectZonesUtf8(djvu_text_zone* z, float dpiF, str::Builder& sb, Vec<Rect>& coords) {
    if (!z) {
        return;
    }
    if (z->nchildren > 0) {
        for (int i = 0; i < z->nchildren; i++) {
            djvu_text_zone* c = &z->children[i];
            CollectZonesUtf8(c, dpiF, sb, coords);
            if (c->type == DJVU_ZONE_WORD) {
                coords.Append(Rect((int)((c->x + c->w) * dpiF), (int)(c->y * dpiF), 2, (int)(c->h * dpiF)));
                sb.AppendChar(' ');
            } else if (c->type == DJVU_ZONE_LINE) {
                coords.Append(Rect());
                sb.AppendChar('\n');
            }
        }
        return;
    }
    if (str::IsEmpty(z->text)) {
        return;
    }
    Rect r((int)(z->x * dpiF), (int)(z->y * dpiF), (int)(z->w * dpiF), (int)(z->h * dpiF));
    // zones are usually word-granularity, so approximate per-glyph rects by
    // evenly splitting the box horizontally (computed from endpoints so slices
    // tile exactly); this makes partial-word search hits and selections
    // highlight roughly just the matched characters
    int n = Utf8CodepointCount(z->text);
    for (int i = 0; i < n; i++) {
        int xStart = r.x + (i * r.dx) / n;
        int xEnd = r.x + ((i + 1) * r.dx) / n;
        coords.Append(Rect(xStart, r.y, xEnd - xStart, r.dy));
    }
    sb.Append(z->text);
}

PageText EngineDjvuDec::ExtractPageText(int pageNo) {
    djvu_page_text_zones* z = djvu_page_text_get_zones(doc, pageNo - 1);
    if (!z || !z->root) {
        if (z) {
            djvu_text_zones_destroy(ctx, z);
        }
        return {};
    }
    float dpiF = GetFileDPI() / (float)pages[pageNo - 1]->dpi;
    str::Builder sb;
    Vec<Rect> coords;
    CollectZonesUtf8(z->root, dpiF, sb, coords);
    djvu_text_zones_destroy(ctx, z);

    if (len(sb) == 0) {
        return {};
    }
    int nCodepoints = Utf8CodepointCount(ToStr(sb));
    ReportIf(nCodepoints != len(coords));
    PageText res;
    res.len = len(sb);
    res.nCodepoints = nCodepoints;
    res.text = sb.TakeStr();
    res.coords = coords.Take();
    return res;
}

// returns a numeric DjVu link to a named page (if the name resolves)
static TempStr ResolveNamedDestDjvuDecTemp(djvu_doc* doc, Str name) {
    if (!name) {
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
    auto pi = pages[pageNo - 1];
    if (pi->gotElements) {
        return pi->allElements;
    }
    ScopedCritSec scope(&cacheLock);
    if (pi->gotElements) {
        return pi->allElements;
    }
    pi->gotElements = true;
    auto& els = pi->allElements;

    djvu_page_links* links = djvu_page_get_links(doc, pageNo - 1);
    if (!links) {
        return els;
    }
    float dpiF = GetFileDPI() / (float)pi->dpi;
    for (int i = 0; i < links->nlinks; i++) {
        djvu_link& l = links->links[i];
        Str url = Str(l.url);
        if (!url) {
            continue;
        }
        Rect rect((int)(l.x * dpiF), (int)(l.y * dpiF), (int)(l.w * dpiF), (int)(l.h * dpiF));
        TempStr link = ResolveNamedDestDjvuDecTemp(doc, url);
        if (!link) {
            link = url;
        }
        auto el = NewDjvuDecLink(pageNo, rect, link, Str(l.comment));
        if (el) {
            els.Append(el);
        }
    }
    djvu_page_links_destroy(ctx, links);
    return els;
}

IPageElement* EngineDjvuDec::GetElementAtPos(int pageNo, PointF pt) {
    Vec<IPageElement*> els = GetElements(pageNo);
    int n = len(els);
    for (int i = n - 1; i >= 0; i--) {
        auto el = els.at(i);
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
    auto ddest = (PageDestinationDjvuDec*)dest;
    Str link = ddest->link;
    auto ctrl = linkHandler->GetDocController();
    if (str::Eq(link, "#+1")) {
        ctrl->GoToNextPage();
        return true;
    }
    if (str::Eq(link, "#-1")) {
        ctrl->GoToPrevPage();
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
    ctrl->GoToPage(pageNo, true);
    return true;
}

IPageDestination* EngineDjvuDec::GetNamedDest(Str name) {
    Str n = name;
    if (str::StartsWith(n, "#")) {
        n = Str(n.s + 1, n.len - 1);
    }
    TempStr link = ResolveNamedDestDjvuDecTemp(doc, n);
    if (link) {
        return NewDjvuDecDestination(link, {});
    }
    return nullptr;
}

TocItem* EngineDjvuDec::BuildTocTree(TocItem* parent, djvu_outline_item* items, int n, int& idCounter) {
    TocItem* node = nullptr;
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
        TocItem* tocItem = NewDjvuDecTocItem(parent, title, link);
        tocItem->id = ++idCounter;
        tocItem->child = BuildTocTree(tocItem, it.children, it.nchildren, idCounter);
        if (!node) {
            node = tocItem;
        } else {
            node->AddSiblingAtEnd(tocItem);
        }
    }
    return node;
}

TocTree* EngineDjvuDec::GetToc() {
    if (tocTree) {
        return tocTree;
    }
    ScopedCritSec scope(&cacheLock);
    if (tocTree) {
        return tocTree;
    }
    djvu_outline_item* root = djvu_doc_outline(doc);
    if (!root) {
        return nullptr;
    }
    int idCounter = 0;
    TocItem* rootItem = BuildTocTree(nullptr, root->children, root->nchildren, idCounter);
    djvu_outline_destroy(ctx, root);
    if (!rootItem) {
        return nullptr;
    }
    auto realRoot = new TocItem();
    realRoot->child = rootItem;
    tocTree = new TocTree(realRoot);
    return tocTree;
}

bool IsEngineDjVuSupportedFileType(Kind kind) {
    return kind == kindFileDjVu;
}

EngineBase* CreateEngineDjvuDecFromStream(IStream* stream) {
    EngineDjvuDec* engine = new EngineDjvuDec();
    if (engine->Load(stream)) {
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
