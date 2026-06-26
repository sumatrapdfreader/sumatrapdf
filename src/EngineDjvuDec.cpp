/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// EngineDjvuDec: a DjVu engine built on the small plain-C decoder in
// ext/djvudec (djvu.h / djvu.c) instead of libdjvu. Selected via the
// DjvuEngine advanced setting; see EngineCreate.cpp.

extern "C" {
#include "djvu.h"
}

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/FileUtil.h"
#include "utils/WinUtil.h"

#include "wingui/UIModels.h"

#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"

#include "utils/Log.h"

// reuse the same Kind so DjVu-specific code paths treat both engines alike
extern Kind kindEngineDjVu;

// parses "123", "#123", "# 123"; returns -1 for invalid page
static int ParseDjvuDecLink(const char* link) {
    if (!link) {
        return -1;
    }
    if (link[0] == '#') {
        ++link;
    }
    if (link[0] == ' ') {
        ++link;
    }
    return atoi(link);
}

static bool DjvuDecCouldBeURL(const char* link) {
    if (!link) {
        return false;
    }
    if (str::StartsWithI(link, "http:") || str::StartsWithI(link, "https:") || str::StartsWithI(link, "mailto:")) {
        return true;
    }
    return str::Contains(link, ".");
}

struct PageDestinationDjvuDec : IPageDestination {
    char* link = nullptr;
    char* value = nullptr;

    PageDestinationDjvuDec(const char* l, const char* comment) {
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

    char* GetValue2() override {
        if (value) {
            return value;
        }
        if (!DjvuDecCouldBeURL(link)) {
            return nullptr;
        }
        value = str::Dup(link);
        url::DecodeInPlace(value);
        return value;
    }
};

static IPageDestination* NewDjvuDecDestination(const char* link, const char* comment) {
    if (str::IsEmpty(link) || str::Eq(link, "#")) {
        return nullptr;
    }
    auto res = new PageDestinationDjvuDec(link, comment);
    res->rect = RectF(DEST_USE_DEFAULT, DEST_USE_DEFAULT, DEST_USE_DEFAULT, DEST_USE_DEFAULT);
    res->pageNo = ParseDjvuDecLink(link);
    return res;
}

static IPageElement* NewDjvuDecLink(int pageNo, Rect rect, const char* link, const char* comment) {
    auto dest = NewDjvuDecDestination(link, comment);
    if (!dest) {
        return nullptr;
    }
    auto res = new PageElementDestination(dest);
    res->rect = ToRectF(rect);
    res->pageNo = pageNo;
    return res;
}

static TocItem* NewDjvuDecTocItem(TocItem* parent, const char* title, const char* link) {
    auto res = new TocItem(parent, title, 0);
    res->dest = NewDjvuDecDestination(link, nullptr);
    if (res->dest) {
        res->pageNo = PageDestGetPageNo(res->dest);
    }
    return res;
}

struct DjvuDecPageInfo {
    RectF mediabox;
    int dpi = 300;
    Vec<IPageElement*> allElements;
    bool gotElements = false;
};

class EngineDjvuDec : public EngineBase {
  public:
    EngineDjvuDec();
    ~EngineDjvuDec() override;
    EngineBase* Clone() override;

    RectF PageMediabox(int pageNo) override;

    RenderedBitmap* RenderPage(RenderPageArgs&) override;

    RectF Transform(const RectF& rect, int pageNo, float zoom, int rotation, bool inverse = false) override;

    ByteSlice GetFileData() override;
    bool SaveFileAs(const char* copyFileName) override;
    PageText ExtractPageText(int pageNo) override;
    PageTextUtf8 ExtractPageTextUtf8(int pageNo) override;
    bool HasClipOptimizations(int pageNo) override;

    TempStr GetPropertyTemp(const char* name) override;
    bool BenchLoadPage(int pageNo) override;

    Vec<IPageElement*> GetElements(int pageNo) override;
    IPageElement* GetElementAtPos(int pageNo, PointF pt) override;
    bool HandleLink(IPageDestination*, ILinkHandler*) override;

    IPageDestination* GetNamedDest(const char* name) override;
    TocTree* GetToc() override;

    bool Load(const char* fileName);
    bool Load(IStream* stream);

  protected:
    IStream* stream = nullptr;
    ByteSlice fileData; // must outlive all docs

    // a djvu_doc isn't thread-safe, so we keep one doc for metadata queries
    // (TOC, links, text) used by the UI thread under `lock`, and a separate
    // pool of docs for rendering used by the background render threads. This
    // way a slow render never blocks UI-thread metadata calls, and multiple
    // render threads can decode different pages concurrently.
    djvu_ctx* ctx = nullptr;
    djvu_doc* doc = nullptr;
    CRITICAL_SECTION lock; // protects `doc` (metadata)

    // render-doc pool, each entry an independent (ctx, doc) over fileData
    struct RenderDoc {
        djvu_ctx* ctx = nullptr;
        djvu_doc* doc = nullptr;
        bool inUse = false;
    };
    Vec<RenderDoc*> renderDocs;
    CRITICAL_SECTION poolLock; // protects renderDocs bookkeeping

    Vec<DjvuDecPageInfo*> pages;
    TocTree* tocTree = nullptr;

    RenderDoc* AcquireRenderDoc();
    void ReleaseRenderDoc(RenderDoc*);

    PointF TransformPoint(PointF pt, int pageNo, float zoom, int rotation, bool inverse);
    bool FinishLoading();
    TocItem* BuildTocTree(TocItem* parent, djvu_outline_item* items, int n, int& idCounter);
};

EngineDjvuDec::EngineDjvuDec() {
    kind = kindEngineDjVu;
    str::ReplaceWithCopy(&defaultExt, ".djvu");
    fileDPI = 300.0f;
    InitializeCriticalSection(&lock);
    InitializeCriticalSection(&poolLock);
}

EngineDjvuDec::~EngineDjvuDec() {
    delete tocTree;
    DeleteVecMembers(pages);
    for (RenderDoc* rd : renderDocs) {
        if (rd->doc) {
            djvu_doc_close(rd->doc);
        }
        if (rd->ctx) {
            djvu_ctx_free(rd->ctx);
        }
        delete rd;
    }
    if (doc) {
        djvu_doc_close(doc);
    }
    if (ctx) {
        djvu_ctx_free(ctx);
    }
    fileData.Free();
    if (stream) {
        stream->Release();
    }
    DeleteCriticalSection(&lock);
    DeleteCriticalSection(&poolLock);
}

// get a free render doc from the pool (creating one if needed). Each render
// doc is an independent djvudec document over the same in-memory file, so
// renders run without contending with metadata queries or each other.
EngineDjvuDec::RenderDoc* EngineDjvuDec::AcquireRenderDoc() {
    {
        ScopedCritSec scope(&poolLock);
        for (RenderDoc* rd : renderDocs) {
            if (!rd->inUse) {
                rd->inUse = true;
                return rd;
            }
        }
    }
    // none free: open a new independent doc (djvu_doc_open over a read-only
    // shared buffer is safe to call concurrently)
    auto rd = new RenderDoc();
    rd->ctx = djvu_ctx_new(nullptr, nullptr, nullptr, nullptr);
    if (rd->ctx) {
        rd->doc = djvu_doc_open(rd->ctx, fileData.data(), fileData.size());
    }
    if (!rd->doc) {
        if (rd->ctx) {
            djvu_ctx_free(rd->ctx);
        }
        delete rd;
        return nullptr;
    }
    rd->inUse = true;
    ScopedCritSec scope(&poolLock);
    renderDocs.Append(rd);
    return rd;
}

void EngineDjvuDec::ReleaseRenderDoc(RenderDoc* rd) {
    if (!rd) {
        return;
    }
    ScopedCritSec scope(&poolLock);
    rd->inUse = false;
}

EngineBase* EngineDjvuDec::Clone() {
    if (stream != nullptr) {
        return CreateEngineDjvuDecFromStream(stream);
    }
    const char* path = FilePath();
    if (path) {
        return CreateEngineDjvuDecFromFile(path);
    }
    return nullptr;
}

bool EngineDjvuDec::Load(const char* fileName) {
    SetFilePath(fileName);
    fileData = file::ReadFile(fileName);
    return FinishLoading();
}

bool EngineDjvuDec::Load(IStream* stm) {
    fileData = GetDataFromStream(stm, nullptr);
    if (!fileData.empty()) {
        stream = stm;
        stream->AddRef();
    }
    return FinishLoading();
}

static void DjvuDecErrorCb(void*, djvu_severity sev, const char* msg) {
    if (sev >= DJVU_SEVERITY_ERROR) {
        logf("djvudec: %s\n", msg);
    }
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
    if (fileData.empty()) {
        return false;
    }
    DjvuDecInitOnce();
    ctx = djvu_ctx_new(nullptr, nullptr, DjvuDecErrorCb, nullptr);
    if (!ctx) {
        return false;
    }
    doc = djvu_doc_open(ctx, fileData.data(), fileData.size());
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
            // djvu_page_render at subsample=1 applies the page's intrinsic
            // rotation, so the upright dimensions swap width/height for 90/270
            int upW = info.width;
            int upH = info.height;
            if (info.rotation == 90 || info.rotation == 270) {
                std::swap(upW, upH);
            }
            float dx = upW * GetFileDPI() / dpi;
            float dy = upH * GetFileDPI() / dpi;
            bool isValid = dx > 0 && dx < 1e6f && dy > 0 && dy < 1e6f;
            if (isValid) {
                mbox = RectF(0, 0, dx, dy);
            }
        }
        pi->mediabox = mbox;
        pages.Append(pi);

        const char* title = djvu_doc_page_title(doc, i);
        const char* id = djvu_doc_page_id(doc, i);
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

TempStr EngineDjvuDec::GetPropertyTemp(const char*) {
    return nullptr;
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

// build a top-down 24bpp BGR buffer from the djvu image (gray8 or rgb24)
static u8* DjvuImageToBgr(djvu_image* img, int& dxOut, int& dyOut) {
    int dx = img->width;
    int dy = img->height;
    u8* out = AllocArray<u8>((size_t)dx * dy * 3);
    if (!out) {
        return nullptr;
    }
    for (int y = 0; y < dy; y++) {
        const u8* src = img->data + (size_t)y * img->stride;
        u8* dst = out + (size_t)y * dx * 3;
        if (img->format == DJVU_FORMAT_GRAY8) {
            for (int x = 0; x < dx; x++) {
                u8 v = src[x];
                dst[x * 3 + 0] = v;
                dst[x * 3 + 1] = v;
                dst[x * 3 + 2] = v;
            }
        } else {
            for (int x = 0; x < dx; x++) {
                dst[x * 3 + 0] = src[x * 3 + 2]; // B
                dst[x * 3 + 1] = src[x * 3 + 1]; // G
                dst[x * 3 + 2] = src[x * 3 + 0]; // R
            }
        }
    }
    dxOut = dx;
    dyOut = dy;
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

RenderedBitmap* EngineDjvuDec::RenderPage(RenderPageArgs& args) {
    int pageNo = args.pageNo;
    int rotation = NormalizeRotation(args.rotation);
    float zoom = args.zoom;

    RectF pageRc = args.pageRect ? *args.pageRect : PageMediabox(pageNo);
    Rect screen = Transform(pageRc, pageNo, zoom, rotation).Round();
    Rect full = Transform(PageMediabox(pageNo), pageNo, zoom, rotation).Round();
    screen = full.Intersect(screen);
    if (screen.IsEmpty() || full.IsEmpty()) {
        return nullptr;
    }

    // render on a dedicated pool doc so we don't block (or get blocked by)
    // UI-thread metadata queries on the main doc, and so multiple render
    // threads can decode concurrently
    RenderDoc* rd = AcquireRenderDoc();
    if (!rd) {
        return nullptr;
    }
    // render the whole page upright at full resolution (subsample=1 applies the
    // page's intrinsic rotation), then scale/rotate into the requested rect
    djvu_image* img = djvu_page_render(rd->doc, pageNo - 1, 1);
    int sdx = 0, sdy = 0;
    u8* bgr = img ? DjvuImageToBgr(img, sdx, sdy) : nullptr;
    if (img) {
        djvu_image_destroy(rd->ctx, img);
    }
    ReleaseRenderDoc(rd);
    if (!bgr) {
        return nullptr;
    }

    // apply the user rotation to the upright source buffer
    int rdx = 0, rdy = 0;
    u8* rbgr = RotateBgr(bgr, sdx, sdy, rotation, rdx, rdy);
    free(bgr);
    if (!rbgr) {
        return nullptr;
    }

    HANDLE srcMap = nullptr;
    HBITMAP srcBmp = CreateBgrDib(rbgr, rdx, rdy, &srcMap);
    free(rbgr);
    if (!srcBmp) {
        if (srcMap) {
            CloseHandle(srcMap);
        }
        return nullptr;
    }

    // dest bitmap is the requested screen sub-rectangle
    HANDLE dstMap = nullptr;
    int ddx = screen.dx;
    int ddy = screen.dy;
    HBITMAP dstBmp = CreateBgrDib(nullptr, ddx, ddy, &dstMap);
    RenderedBitmap* res = nullptr;
    if (dstBmp) {
        HDC hdc = GetDC(nullptr);
        HDC srcDC = CreateCompatibleDC(hdc);
        HDC dstDC = CreateCompatibleDC(hdc);
        HGDIOBJ oldSrc = SelectObject(srcDC, srcBmp);
        HGDIOBJ oldDst = SelectObject(dstDC, dstBmp);
        SetStretchBltMode(dstDC, HALFTONE);
        SetBrushOrgEx(dstDC, 0, 0, nullptr);
        // map the screen sub-rect (within `full`) back to source pixels
        // full origin is (0,0) since it's the transform of the mediabox
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
        StretchBlt(dstDC, 0, 0, ddx, ddy, srcDC, srcX, srcY, srcW, srcH, SRCCOPY);
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

ByteSlice EngineDjvuDec::GetFileData() {
    return GetStreamOrFileData(stream, FilePath());
}

bool EngineDjvuDec::SaveFileAs(const char* dstPath) {
    if (stream) {
        ByteSlice d = GetDataFromStream(stream, nullptr);
        bool ok = !d.empty() && file::WriteFile(dstPath, d);
        d.Free();
        if (ok) {
            return true;
        }
    }
    const char* srcPath = FilePath();
    if (!srcPath) {
        return false;
    }
    return file::Copy(dstPath, srcPath, false);
}

// recursively collect word-level text + coords from the zone tree.
// zone coords are top-down full-resolution page pixels; dpiF scales them to
// mediabox (fileDPI) units.
static void CollectZonesUtf8(djvu_text_zone* z, float dpiF, StrBuilder& sb, Vec<Rect>& coords) {
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
    size_t n = str::Len(z->text);
    for (size_t i = 0; i < n; i++) {
        coords.Append(r);
    }
    sb.Append(z->text);
}

PageTextUtf8 EngineDjvuDec::ExtractPageTextUtf8(int pageNo) {
    ScopedCritSec scope(&lock);
    djvu_page_text_zones* z = djvu_page_text_get_zones(doc, pageNo - 1);
    if (!z || !z->root) {
        if (z) {
            djvu_text_zones_destroy(ctx, z);
        }
        return {};
    }
    float dpiF = GetFileDPI() / (float)pages[pageNo - 1]->dpi;
    StrBuilder sb;
    Vec<Rect> coords;
    CollectZonesUtf8(z->root, dpiF, sb, coords);
    djvu_text_zones_destroy(ctx, z);

    if (sb.size() == 0) {
        return {};
    }
    ReportIf((size_t)sb.size() != coords.size());
    PageTextUtf8 res;
    res.len = (int)sb.size();
    res.text = sb.StealData();
    res.coords = coords.StealData();
    return res;
}

PageText EngineDjvuDec::ExtractPageText(int pageNo) {
    PageTextUtf8 u = ExtractPageTextUtf8(pageNo);
    if (!u.text) {
        return {};
    }
    // convert utf8 -> wchar, expanding per-byte coords to per-wchar coords
    PageText res;
    StrBuilder ignore;
    WCHAR* ws = ToWStr(u.text);
    int wlen = (int)str::Len(ws);
    Rect* wcoords = AllocArray<Rect>(wlen);
    // walk utf8 and wchar in lockstep, assigning the coord of each utf8 lead
    // byte to its decoded wchar(s)
    const char* s = u.text;
    int wi = 0;
    int bi = 0;
    while (*s && wi < wlen) {
        int nb = 1;
        u8 c = (u8)*s;
        if (c >= 0xF0) {
            nb = 4;
        } else if (c >= 0xE0) {
            nb = 3;
        } else if (c >= 0xC0) {
            nb = 2;
        }
        Rect r = (bi < u.len) ? u.coords[bi] : Rect();
        wcoords[wi++] = r;
        // surrogate pair for codepoints > 0xFFFF (nb == 4)
        if (nb == 4 && wi < wlen) {
            wcoords[wi++] = r;
        }
        s += nb;
        bi += nb;
    }
    res.text = ws;
    res.coords = wcoords;
    res.len = wlen;
    str::Free(u.text);
    free(u.coords);
    return res;
}

// returns a numeric DjVu link to a named page (if the name resolves)
static TempStr ResolveNamedDestDjvuDecTemp(djvu_doc* doc, const char* name) {
    if (str::IsEmpty(name)) {
        return nullptr;
    }
    int pageNo = djvu_doc_page_by_name(doc, name);
    if (pageNo < 0) {
        return nullptr;
    }
    return str::FormatTemp("#%d", pageNo + 1);
}

Vec<IPageElement*> EngineDjvuDec::GetElements(int pageNo) {
    ReportIf(pageNo < 1 || pageNo > PageCount());
    auto pi = pages[pageNo - 1];
    ScopedCritSec scope(&lock);
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
        if (str::IsEmpty(l.url)) {
            continue;
        }
        Rect rect((int)(l.x * dpiF), (int)(l.y * dpiF), (int)(l.w * dpiF), (int)(l.h * dpiF));
        TempStr link = ResolveNamedDestDjvuDecTemp(doc, l.url);
        if (!link) {
            link = (TempStr)l.url;
        }
        auto el = NewDjvuDecLink(pageNo, rect, link, l.comment);
        if (el) {
            els.Append(el);
        }
    }
    djvu_page_links_destroy(ctx, links);
    return els;
}

IPageElement* EngineDjvuDec::GetElementAtPos(int pageNo, PointF pt) {
    Vec<IPageElement*> els = GetElements(pageNo);
    int n = els.Size();
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
    const char* link = ddest->link;
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

IPageDestination* EngineDjvuDec::GetNamedDest(const char* name) {
    const char* n = name;
    if (str::StartsWith(n, "#")) {
        n = n + 1;
    }
    TempStr link = ResolveNamedDestDjvuDecTemp(doc, n);
    if (link) {
        return NewDjvuDecDestination(link, nullptr);
    }
    return nullptr;
}

TocItem* EngineDjvuDec::BuildTocTree(TocItem* parent, djvu_outline_item* items, int n, int& idCounter) {
    TocItem* node = nullptr;
    for (int i = 0; i < n; i++) {
        djvu_outline_item& it = items[i];
        const char* title = it.title ? it.title : "";
        const char* url = it.url ? it.url : "";
        TempStr link = (TempStr)url;
        TempStr resolved = ResolveNamedDestDjvuDecTemp(doc, url);
        if (resolved) {
            link = resolved;
        } else if (it.page_no >= 0) {
            link = str::FormatTemp("#%d", it.page_no + 1);
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
    ScopedCritSec scope(&lock);
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

EngineBase* CreateEngineDjvuDecFromStream(IStream* stream) {
    EngineDjvuDec* engine = new EngineDjvuDec();
    if (engine->Load(stream)) {
        return engine;
    }
    SafeEngineRelease(&engine);
    return nullptr;
}

EngineBase* CreateEngineDjvuDecFromFile(const char* path) {
    EngineDjvuDec* engine = new EngineDjvuDec();
    if (engine->Load(path)) {
        return engine;
    }
    SafeEngineRelease(&engine);
    return nullptr;
}
