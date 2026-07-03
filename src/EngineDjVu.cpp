/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// hack to prevent libdjvu from being built as an export/import library
#define DDJVUAPI    /**/
#define MINILISPAPI /**/

#include <ddjvuapi.h>
#include <miniexp.h>

#include "base/Base.h"
#include "base/ScopedWin.h"
#include "base/ByteReader.h"
#include "base/File.h"
#include "base/GuessFileType.h"
#include "base/Win.h"

#include "wingui/UIModels.h"

#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"

#include "base/Log.h"

Kind kindEngineDjVu = "engineDjVu";

// TODO: libdjvu leaks memory - among others
//       pcaster, DataPool::OpenFiles::global_ptr, FCPools::global_ptr
//       cf. http://sourceforge.net/projects/djvu/forums/forum/103286/topic/3553602

// parses "123", "#123", "# 123"
// returns -1 for invalid page
static int ParseDjVuLink(Str link) {
    if (!link) {
        return -1;
    }
    str::SkipChar(link, '#');
    str::SkipChar(link, ' ');
    if (!link) {
        return -1;
    }
    return ParseInt(link);
}

static bool CouldBeURL(Str link) {
    if (!link) {
        return false;
    }

    if (str::StartsWithI(link, "http:") || str::StartsWithI(link, "https:") || str::StartsWithI(link, "mailto:")) {
        return true;
    }

    // very lenient heuristic
    return str::Contains(link, ".");
}

struct PageDestinationDjVu : IPageDestination {
    Str link;
    Str value;

    PageDestinationDjVu(Str l, Str comment) {
        kind = kindDestinationDjVu;
        link = str::Dup(l);
        if (comment) {
            value = str::Dup(comment);
        }
    }
    ~PageDestinationDjVu() {
        str::Free(link);
        str::Free(value);
    }

    Str GetValue2() override {
        if (value) {
            return value;
        }
        if (!CouldBeURL(link)) {
            return {};
        }
        value = str::Dup(link);
        url::DecodeInPlace(value.s);
        return value;
    }
};

// the link format can be any of
//   #[ ]<pageNo>      e.g. #1 for FirstPage and # 13 for page 13
//   #[+-]<pageCount>  e.g. #+1 for NextPage and #-1 for PrevPage
//   #filename.djvu    use ResolveNamedDest to get a link in #<pageNo> format
//   http://example.net/#hyperlink
static IPageDestination* NewDjVuDestination(Str link, Str comment) {
    if (str::IsEmpty(link) || str::Eq(link, "#")) {
        return nullptr;
    }
    auto res = new PageDestinationDjVu(link, comment);
    res->rect = RectF(DEST_USE_DEFAULT, DEST_USE_DEFAULT, DEST_USE_DEFAULT, DEST_USE_DEFAULT);
    res->pageNo = ParseDjVuLink(link);
    return res;
}

static IPageElement* NewDjVuLink(int pageNo, Rect rect, Str link, Str comment) {
    auto dest = NewDjVuDestination(link, comment);
    if (!dest) {
        return nullptr;
    }
    auto res = new PageElementDestination(dest);
    res->rect = ToRectF(rect);
    res->pageNo = pageNo;
    return res;
}

static TocItem* NewDjVuTocItem(TocItem* parent, Str title, Str link) {
    auto res = new TocItem(parent, title, 0);
    res->dest = NewDjVuDestination(link, nullptr);
    if (res->dest) {
        res->pageNo = PageDestGetPageNo(res->dest);
    }
    return res;
}

struct DjVuContext {
    ddjvu_context_t* ctx = nullptr;
    int refCount = 1;
    CRITICAL_SECTION lock;
    CRITICAL_SECTION spinLock;

    DjVuContext() {
        InitializeCriticalSection(&lock);
        InitializeCriticalSection(&spinLock);
        ctx = ddjvu_context_create("DjVuEngine");
        // reset the locale to "C" as most other code expects
        setlocale(LC_ALL, "C");
        ReportIf(!ctx);
    }

    // refCount is protected by gDjVuContextAccess (not by lock: ~EngineDjVu
    // holds lock while calling ReleaseDjVuContext() which takes
    // gDjVuContextAccess, so taking lock here would invert the lock order
    // vs. GetDjVuContext() and could deadlock)
    int AddRef() {
        ++refCount;
        return refCount;
    }

    int Release() {
        ReportIf(refCount <= 0);
        --refCount;
        return refCount;
    }

    ~DjVuContext() {
        EnterCriticalSection(&lock);
        if (ctx) {
            ddjvu_cache_clear(ctx);
            ddjvu_context_release(ctx);
        }
        LeaveCriticalSection(&lock);
        DeleteCriticalSection(&lock);
        DeleteCriticalSection(&spinLock);
    }

    void SpinMessageLoop(bool wait = true) const {
        const ddjvu_message_t* msg = nullptr;
        if (wait) {
            ddjvu_message_wait(ctx);
        }
        while ((msg = ddjvu_message_peek(ctx)) != nullptr) {
            auto tag = msg->m_any.tag;
            if (DDJVU_NEWSTREAM == tag) {
                auto streamId = msg->m_newstream.streamid;
                if (streamId != 0) {
                    BOOL stop = FALSE;
                    ddjvu_stream_close(msg->m_any.document, streamId, stop);
                }
            }
            ddjvu_message_pop(ctx);
        }
    }

    // temporarily release the lock and process any pending libdjvu messages.
    // spinLock serializes message processing so only one thread peeks/pops
    // at a time, preventing use-after-free on the returned message pointer.
    // uses non-blocking peek to avoid two threads both blocking inside
    // GMonitor::wait() on the same ddjvu_context, which corrupts monitor state
    void SpinMessageLoopWithUnlock() {
        LeaveCriticalSection(&lock);
        EnterCriticalSection(&spinLock);
        SpinMessageLoop(false);
        LeaveCriticalSection(&spinLock);
        Sleep(10);
        EnterCriticalSection(&lock);
    }

    ddjvu_document_t* OpenFile(Str fileName) {
        ScopedCritSec scope(&lock);
        // TODO: libdjvu sooner or later crashes inside its caching code; cf.
        //       https://code.google.com/archive/p/sumatrapdf/issues/1434
        return ddjvu_document_create_by_filename_utf8(ctx, CStrTemp(fileName), /* cache */ FALSE);
    }

    ddjvu_document_t* OpenStream(IStream* stream) {
        ScopedCritSec scope(&lock);
        Str d = GetDataFromStream(stream, nullptr);
        defer {
            str::Free(d);
        };
        if (str::IsEmpty(d) || (size_t)d.len > ULONG_MAX) {
            return {};
        }
        auto res = ddjvu_document_create_by_data(ctx, d.s, (ULONG)d.len);
        return res;
    }
};

// TODO: make it non-static because it accesses other static state
// in djvu which got deleted first
static DjVuContext* gDjVuContext;

// engines are created / destroyed on multiple threads (async document loads),
// so creation of gDjVuContext and refCount changes must be serialized.
// SRWLOCK because it can be statically initialized
static SRWLOCK gDjVuContextAccess = SRWLOCK_INIT;

static DjVuContext* GetDjVuContext() {
    AcquireSRWLockExclusive(&gDjVuContextAccess);
    if (!gDjVuContext) {
        gDjVuContext = new DjVuContext();
    } else {
        gDjVuContext->AddRef();
    }
    DjVuContext* res = gDjVuContext;
    ReleaseSRWLockExclusive(&gDjVuContextAccess);
    return res;
}

static void ReleaseDjVuContext() {
    AcquireSRWLockExclusive(&gDjVuContextAccess);
    ReportIf(!gDjVuContext);
    if (gDjVuContext) {
        gDjVuContext->Release();
    }
    ReleaseSRWLockExclusive(&gDjVuContextAccess);
}

void CleanupEngineDjVu() {
    if (gDjVuContext) {
        ReportIf(gDjVuContext->refCount != 0);
        if (gDjVuContext->ctx) {
            ddjvu_cache_clear(gDjVuContext->ctx);
        }
        delete gDjVuContext;
        gDjVuContext = nullptr;
    }
    minilisp_finish();
    ddjvu_free_port_corpses();
}

struct DjVuPageInfo {
    RectF mediabox;
    Vec<IPageElement*> allElements;
    miniexp_t annos{miniexp_dummy};
    bool gotAllElements = false;
};

class EngineDjVu : public EngineBase {
  public:
    EngineDjVu();
    ~EngineDjVu() override;
    EngineBase* Clone() override;

    RectF PageMediabox(int pageNo) override;
    RectF PageContentBox(int pageNo, RenderTarget target = RenderTarget::View) override;

    Pixmap* RenderPage(RenderPageArgs&) override;

    PointF TransformPoint(PointF pt, int pageNo, float zoom, int rotation, bool inverse = false);
    RectF Transform(const RectF& rect, int pageNo, float zoom, int rotation, bool inverse = false) override;

    Str GetFileData() override;
    bool SaveFileAs(Str copyFileName) override;
    PageText ExtractPageText(int pageNo) override;
    bool HasClipOptimizations(int pageNo) override;

    TempStr GetPropertyTemp(Str name) override;

    // we currently don't load pages lazily, so there's nothing to do here
    bool BenchLoadPage(int pageNo) override;

    Vec<IPageElement*> GetElements(int pageNo) override;
    IPageElement* GetElementAtPos(int pageNo, PointF pt) override;
    bool HandleLink(IPageDestination*, ILinkHandler*) override;

    IPageDestination* GetNamedDest(Str name) override;
    TocTree* GetToc() override;

    TempStr GetPageLabeTemp(int pageNo) const override;
    int GetPageByLabel(Str label) const override;

    bool Load(Str fileName);
    bool Load(IStream* stream);

  protected:
    IStream* stream = nullptr;

    Vec<DjVuPageInfo*> pages;

    ddjvu_document_t* doc = nullptr;
    miniexp_t outline = miniexp_nil;
    TocTree* tocTree = nullptr;

    Vec<ddjvu_fileinfo_t> fileInfos;

    RenderedBitmap* CreateRenderedBitmap(const u8* bmpData, Size size, bool grayscale) const;
    bool ExtractPageText(miniexp_t item, str::Builder& extracted, Vec<Rect>& coords);
    TempStr ResolveNamedDestTemp(Str name);
    TocItem* BuildTocTree(TocItem* parent, miniexp_t entry, int& idCounter);
    bool FinishLoading();
    bool LoadMediaboxes();
};

EngineDjVu::EngineDjVu() {
    kind = kindEngineDjVu;
    SetDefaultExt(defaultExt, ".djvu");
    // DPI isn't constant for all pages and thus premultiplied
    fileDPI = 300.0f;
    GetDjVuContext();
}

EngineDjVu::~EngineDjVu() {
    ScopedCritSec scope(&gDjVuContext->lock);

    delete tocTree;

    for (auto pi : pages) {
        if (pi->annos && pi->annos != miniexp_dummy) {
            ddjvu_miniexp_release(doc, pi->annos);
            pi->annos = nullptr;
        }
    }
    DeleteVecMembers(pages);

    if (outline != miniexp_nil) {
        ddjvu_miniexp_release(doc, outline);
        outline = miniexp_nil;
    }
    if (doc) {
        ddjvu_job_stop(ddjvu_document_job(doc));
        gDjVuContext->SpinMessageLoop(false);
        ddjvu_document_release(doc);
        doc = nullptr;
        gDjVuContext->SpinMessageLoop(false);
    }
    if (stream) {
        stream->Release();
        stream = nullptr;
    }
    ReleaseDjVuContext();
}

EngineBase* EngineDjVu::Clone() {
    if (stream != nullptr) {
        auto res = CreateEngineDjVuFromStream(stream);
        if (!res) {
            logf("EngineDjVu::Clone() failed: CreateEngineDjVuFromStream() failed\n");
        }
        return res;
    }
    Str path = FilePath();
    if (path) {
        auto res = CreateEngineDjVuFromFile(path);
        if (!res) {
            logf("EngineDjVu::Clone() failed: CreateEngineDjVuFromFile('%s') failed\n", path);
        }
        return res;
    }
    logf("EngineDjVu::Clone() failed: no stream or file path\n");
    return nullptr;
}

RectF EngineDjVu::PageMediabox(int pageNo) {
    ReportIf(pageNo < 1 || pageNo > pageCount);
    DjVuPageInfo* pi = pages[pageNo - 1];
    return pi->mediabox;
}

bool EngineDjVu::HasClipOptimizations(int) {
    return false;
}

TempStr EngineDjVu::GetPropertyTemp(Str) {
    return {};
}

// we currently don't load pages lazily, so there's nothing to do here
bool EngineDjVu::BenchLoadPage(int) {
    return true;
}

// Most functions of the ddjvu API such as ddjvu_document_get_pageinfo
// are quite inefficient when used for all pages of a document in a row,
// so try to either only use them when actually needed or replace them
// with a function that extracts all the data at once:

static bool ReadBytes(HANDLE h, DWORD offset, void* buffer, DWORD count) {
    DWORD res = SetFilePointer(h, offset, nullptr, FILE_BEGIN);
    if (res != offset) {
        return false;
    }
    bool ok = ReadFile(h, buffer, count, &res, nullptr);
    return ok && res == count;
}

#define DJVU_MARK_MAGIC 0x41542654L /* AT&T */
#define DJVU_MARK_FORM 0x464F524DL  /* FORM */
#define DJVU_MARK_DJVM 0x444A564DL  /* DJVM */
#define DJVU_MARK_DJVU 0x444A5655L  /* DJVU */
#define DJVU_MARK_INFO 0x494E464FL  /* INFO */

#include <pshpack1.h>

struct DjVuInfoChunk {
    WORD width, height;
    BYTE minor, major;
    BYTE dpiLo, dpiHi;
    BYTE gamma, flags;
};

static_assert(sizeof(DjVuInfoChunk) == 10, "wrong size of DjVuInfoChunk structure");

bool EngineDjVu::LoadMediaboxes() {
    Str path = FilePath();
    if (!path) {
        return false;
    }
    AutoCloseHandle h(file::OpenReadOnly(path));
    if (!h.IsValid()) {
        return false;
    }
    char buffer[16];
    ByteReader r(Str(buffer, (int)sizeof(buffer)));
    if (!ReadBytes(h, 0, buffer, 16) || r.DWordBE(0) != DJVU_MARK_MAGIC || r.DWordBE(4) != DJVU_MARK_FORM) {
        return false;
    }

    DWORD offset = r.DWordBE(12) == DJVU_MARK_DJVM ? 16 : 4;
    for (int pageNo = 0; pageNo < pageCount; /* no op, must inc inside isMark */) {
        if (!ReadBytes(h, offset, buffer, 16)) {
            return false;
        }
        int partLen = r.DWordBE(4);
        if (partLen < 0) {
            return false;
        }
        bool isMark =
            r.DWordBE(0) == DJVU_MARK_FORM && r.DWordBE(8) == DJVU_MARK_DJVU && r.DWordBE(12) == DJVU_MARK_INFO;
        if (isMark) {
            if (!ReadBytes(h, offset + 16, buffer, 14)) {
                return false;
            }
            DjVuInfoChunk info;
            bool ok = r.UnpackBE(&info, sizeof(info), "2w6b", 4);
            ReportIf(!ok);
            int dpi = MAKEWORD(info.dpiLo, info.dpiHi); // dpi is little-endian
            // DjVuLibre ignores DPI values outside 25 to 6000 in DjVuInfo::decode
            if (dpi < 25 || 6000 < dpi) {
                dpi = 300;
            }
            DjVuPageInfo* pi = pages[pageNo];
            // auto&& mediabox = pi->mediabox;
            float dx = GetFileDPI() * info.width / dpi;
            float dy = GetFileDPI() * info.height / dpi;
            if (info.flags & 4) {
                pi->mediabox.dx = dy;
                pi->mediabox.dy = dx;
            } else {
                pi->mediabox.dx = dx;
                pi->mediabox.dy = dy;
            }
            pageNo++;
        }
        offset += 8 + partLen + (partLen & 1);
    }

    return true;
}

bool EngineDjVu::Load(Str fileName) {
    SetFilePath(fileName);
    doc = gDjVuContext->OpenFile(fileName);
    return FinishLoading();
}

bool EngineDjVu::Load(IStream* stream) {
    doc = gDjVuContext->OpenStream(stream);
    return FinishLoading();
}

bool EngineDjVu::FinishLoading() {
    if (!doc) {
        return false;
    }

    ScopedCritSec scope(&gDjVuContext->lock);

    while (!ddjvu_document_decoding_done(doc)) {
        gDjVuContext->SpinMessageLoopWithUnlock();
    }

    if (ddjvu_document_decoding_error(doc)) {
        return false;
    }

    pageCount = ddjvu_document_get_pagenum(doc);
    if (0 == pageCount) {
        return false;
    }

    for (int i = 0; i < pageCount; i++) {
        pages.Append(new DjVuPageInfo());
    }
    bool ok = LoadMediaboxes();
    if (!ok) {
        // fall back to the slower but safer way to extract page mediaboxes
        for (int i = 0; i < pageCount; i++) {
            ddjvu_status_t status;
            ddjvu_pageinfo_t info;
            while ((status = ddjvu_document_get_pageinfo(doc, i, &info)) < DDJVU_JOB_OK) {
                gDjVuContext->SpinMessageLoopWithUnlock();
            }
            if (DDJVU_JOB_OK == status) {
                DjVuPageInfo* pi = pages[i];
                float dx = (float)info.width * GetFileDPI() / (float)info.dpi;
                float dy = (float)info.height * GetFileDPI() / (float)info.dpi;
                if (info.rotation & 1) {
                    // 90 or 270 degree rotation: swap width and height
                    pi->mediabox.dx = dy;
                    pi->mediabox.dy = dx;
                } else {
                    pi->mediabox.dx = dx;
                    pi->mediabox.dy = dy;
                }
            }
        }
    }

    // a page can end up with an invalid mediabox: a broken INFO chunk declaring
    // zero dimensions, ddjvu_document_get_pageinfo failing (mediabox stays 0 x 0)
    // or info.dpi being 0 (mediabox becomes inf). a zero-sized page lays out with
    // zoom 0, triggering "zoom <= 0" reports in TransformPoint. use a letter-sized
    // mediabox instead so the page renders as a blank rectangle
    for (int i = 0; i < pageCount; i++) {
        RectF& mbox = pages[i]->mediabox;
        // legit dimensions are at most 65535 * GetFileDPI() / 25, well below 1e6
        bool isValid = mbox.dx > 0 && mbox.dx < 1e6f && mbox.dy > 0 && mbox.dy < 1e6f;
        if (!isValid) {
            logf("EngineDjVu::FinishLoading: invalid mediabox (%.2f x %.2f) for page %d, using letter size\n", mbox.dx,
                 mbox.dy, i + 1);
            mbox = RectF(0, 0, 8.5f * GetFileDPI(), 11.f * GetFileDPI());
        }
    }

    while ((outline = ddjvu_document_get_outline(doc)) == miniexp_dummy) {
        gDjVuContext->SpinMessageLoopWithUnlock();
    }
    if (!miniexp_consp(outline) || miniexp_car(outline) != miniexp_symbol("bookmarks")) {
        ddjvu_miniexp_release(doc, outline);
        outline = miniexp_nil;
    }

    int fileCount = ddjvu_document_get_filenum(doc);
    for (int i = 0; i < fileCount; i++) {
        ddjvu_status_t status;
        ddjvu_fileinfo_s info;
        while ((status = ddjvu_document_get_fileinfo(doc, i, &info)) < DDJVU_JOB_OK) {
            gDjVuContext->SpinMessageLoopWithUnlock();
        }
        if (DDJVU_JOB_OK == status && info.type == 'P' && info.pageno >= 0) {
            fileInfos.Append(info);
            hasPageLabels = hasPageLabels || !str::Eq(info.title, info.id);
        }
    }

    return true;
}

RenderedBitmap* EngineDjVu::CreateRenderedBitmap(const u8* bmpData, Size size, bool grayscale) const {
    int stride = ((size.dx * (grayscale ? 1 : 3) + 3) / 4) * 4;

    BITMAPINFO* bmi = (BITMAPINFO*)calloc(1, sizeof(BITMAPINFOHEADER) + (grayscale ? 256 * sizeof(RGBQUAD) : 0));
    if (!bmi) {
        return nullptr;
    }

    if (grayscale) {
        for (int i = 0; i < 256; i++) {
            bmi->bmiColors[i].rgbRed = bmi->bmiColors[i].rgbGreen = bmi->bmiColors[i].rgbBlue = (BYTE)i;
        }
    }

    bmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi->bmiHeader.biWidth = size.dx;
    bmi->bmiHeader.biHeight = -size.dy;
    bmi->bmiHeader.biPlanes = 1;
    bmi->bmiHeader.biCompression = BI_RGB;
    bmi->bmiHeader.biBitCount = grayscale ? 8 : 24;
    bmi->bmiHeader.biSizeImage = size.dy * stride;
    bmi->bmiHeader.biClrUsed = grayscale ? 256 : 0;

    void* data = nullptr;
    HANDLE hMap =
        CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, bmi->bmiHeader.biSizeImage, nullptr);
    HBITMAP hbmp = CreateDIBSection(nullptr, bmi, DIB_RGB_COLORS, &data, hMap, 0);
    if (hbmp) {
        memcpy(data, bmpData, bmi->bmiHeader.biSizeImage);
    }

    free(bmi);

    return new RenderedBitmap(hbmp, size, hMap);
}

Pixmap* EngineDjVu::RenderPage(RenderPageArgs& args) {
    ddjvu_page_t* page = nullptr;
    ddjvu_format_t* fmt = nullptr;
    RenderedBitmap* bmp = nullptr;

    EnterCriticalSection(&gDjVuContext->lock);

    auto pageRect = args.pageRect;
    auto zoom = args.zoom;
    auto pageNo = args.pageNo;
    auto rotation = NormalizeRotation(args.rotation);
    RectF pageRc = pageRect ? *pageRect : PageMediabox(pageNo);
    Rect screen = Transform(pageRc, pageNo, zoom, rotation).Round();
    Rect full = Transform(PageMediabox(pageNo), pageNo, zoom, rotation).Round();
    screen = full.Intersect(screen);

    page = ddjvu_page_create_by_pageno(doc, pageNo - 1);
    if (!page) {
        LeaveCriticalSection(&gDjVuContext->lock);
        return nullptr;
    }
    while (!ddjvu_page_decoding_done(page)) {
        gDjVuContext->SpinMessageLoopWithUnlock();
    }
    if (ddjvu_page_decoding_error(page)) {
        LeaveCriticalSection(&gDjVuContext->lock);
        ddjvu_page_release(page);
        return nullptr;
    }

    ddjvu_page_rotation_t userRot = DDJVU_ROTATE_0;
    switch (rotation) {
        case 0:
            userRot = DDJVU_ROTATE_0;
            break;
        // for whatever reason, 90 and 270 are reversed compared to what I expect
        // maybe I'm doing other parts of the code wrong
        case 90:
            userRot = DDJVU_ROTATE_270;
            break;
        case 180:
            userRot = DDJVU_ROTATE_180;
            break;
        case 270:
            userRot = DDJVU_ROTATE_90;
            break;
        default:
            ReportIf("invalid rotation");
            break;
    }
    // combine user rotation with the page's inherent rotation
    ddjvu_page_rotation_t initialRot = ddjvu_page_get_initial_rotation(page);
    int combinedRot = ((int)initialRot + (int)userRot) % 4;
    ddjvu_page_set_rotation(page, (ddjvu_page_rotation_t)combinedRot);

    bool isBitonal = DDJVU_PAGETYPE_BITONAL == ddjvu_page_get_type(page);
    ddjvu_format_style_t style = isBitonal ? DDJVU_FORMAT_GREY8 : DDJVU_FORMAT_BGR24;
    fmt = ddjvu_format_create(style, 0, nullptr);

    int topToBottom = TRUE;
    ddjvu_format_set_row_order(fmt, topToBottom);
    ddjvu_rect_t prect = {full.x, full.y, (uint)full.dx, (uint)full.dy};
    ddjvu_rect_t rrect = {screen.x, 2 * full.y - screen.y + full.dy - screen.dy, (uint)screen.dx, (uint)screen.dy};

    size_t bytesPerPixel = isBitonal ? 1 : 3;
    size_t dx = (size_t)screen.dx;
    size_t dy = (size_t)screen.dy;
    size_t stride = ((dx * bytesPerPixel + 3) / 4) * 4;
    size_t nBytes = stride * (dy + 5);
    u8* bmpData = (u8*)calloc(nBytes, 1);
    if (!bmpData) {
        LeaveCriticalSection(&gDjVuContext->lock);
        ddjvu_format_release(fmt);
        ddjvu_page_release(page);
        return nullptr;
    }

    ddjvu_render_mode_t mode = isBitonal ? DDJVU_RENDER_MASKONLY : DDJVU_RENDER_COLOR;
    int ok = ddjvu_page_render(page, mode, &prect, &rrect, fmt, (unsigned long)stride, (char*)bmpData);
    if (!ok) {
        // nothing was rendered, leave the page blank (same as WinDjView)
        memset(bmpData, 0xFF, stride * dy);
        isBitonal = true;
    }
    bmp = CreateRenderedBitmap(bmpData, screen.Size(), isBitonal);
    free(bmpData);

    // release the lock before releasing djvu objects to avoid deadlock:
    // ddjvu_page_release can trigger DjVuFile::~DjVuFile -> GMonitor::~GMonitor
    // which acquires libdjvu internal locks
    LeaveCriticalSection(&gDjVuContext->lock);
    ddjvu_format_release(fmt);
    ddjvu_page_release(page);

    return PixmapFromRenderedBitmap(bmp);
}

RectF EngineDjVu::PageContentBox(int pageNo, RenderTarget) {
    EnterCriticalSection(&gDjVuContext->lock);

    RectF pageRc = PageMediabox(pageNo);
    ddjvu_page_t* page = ddjvu_page_create_by_pageno(doc, pageNo - 1);
    if (!page) {
        LeaveCriticalSection(&gDjVuContext->lock);
        return pageRc;
    }

    while (!ddjvu_page_decoding_done(page)) {
        gDjVuContext->SpinMessageLoopWithUnlock();
    }
    if (ddjvu_page_decoding_error(page)) {
        LeaveCriticalSection(&gDjVuContext->lock);
        ddjvu_page_release(page);
        return pageRc;
    }
    ddjvu_page_set_rotation(page, ddjvu_page_get_initial_rotation(page));

    // render the page in 8-bit grayscale up to 250x250 px in size
    ddjvu_format_t* fmt = ddjvu_format_create(DDJVU_FORMAT_GREY8, 0, nullptr);

    ddjvu_format_set_row_order(fmt, /* top_to_bottom */ TRUE);
    float zoom = std::min(std::min(250.0f / pageRc.dx, 250.0f / pageRc.dy), 1.0f);
    Rect full = RectF(0, 0, pageRc.dx * zoom, pageRc.dy * zoom).Round();
    ddjvu_rect_t prect = {full.x, full.y, (uint)full.dx, (uint)full.dy};
    ddjvu_rect_t rrect = prect;

    u8* bmpData = AllocArrayTemp<u8>(full.dx * full.dy + 1);
    if (!bmpData) {
        // release the lock before releasing djvu objects to avoid deadlock:
        // ddjvu_page_release can trigger DjVuFile::~DjVuFile -> GMonitor::~GMonitor
        // which acquires libdjvu internal locks
        LeaveCriticalSection(&gDjVuContext->lock);
        ddjvu_format_release(fmt);
        ddjvu_page_release(page);
        return pageRc;
    }

    int ok = ddjvu_page_render(page, DDJVU_RENDER_MASKONLY, &prect, &rrect, fmt, full.dx, (char*)bmpData);
    if (!ok) {
        LeaveCriticalSection(&gDjVuContext->lock);
        ddjvu_format_release(fmt);
        ddjvu_page_release(page);
        return pageRc;
    }

    // determine the content box by counting white pixels from the edges
    RectF content((float)full.dx, -1, 0, 0);
    for (int y = 0; y < full.dy; y++) {
        int x;
        for (x = 0; x < full.dx && bmpData[y * full.dx + x] == '\xFF'; x++) {
            // no-op
        }
        if (x < full.dx) {
            // narrow the left margin down (if necessary)
            if (x < content.x) {
                content.x = (float)x;
            }
            // narrow the right margin down (if necessary)
            for (x = full.dx - 1; x > content.x + content.dx && bmpData[y * full.dx + x] == '\xFF'; x--) {
                // no-op
            }
            if (x > content.x + content.dx) {
                content.dx = x - content.x + 1;
            }
            // narrow either the top or the bottom margin down
            if (content.y == -1) {
                content.y = (float)y;
            } else {
                content.dy = y - content.y + 1;
            }
        }
    }
    if (!content.IsEmpty()) {
        // undo the zoom and round generously
        content.x /= zoom;
        content.dx /= zoom;
        content.y /= zoom;
        content.dy /= zoom;
        pageRc = ToRectF(content.Round());
    }

    // release the lock before releasing djvu objects to avoid deadlock:
    // ddjvu_page_release can trigger DjVuFile::~DjVuFile -> GMonitor::~GMonitor
    // which acquires libdjvu internal locks
    LeaveCriticalSection(&gDjVuContext->lock);
    ddjvu_format_release(fmt);
    ddjvu_page_release(page);

    return pageRc;
}

PointF EngineDjVu::TransformPoint(PointF pt, int pageNo, float zoom, int rotation, bool inverse) {
    ReportIf(zoom <= 0);
    if (zoom <= 0) {
        return pt;
    }

    SizeF page = PageMediabox(pageNo).Size();

    if (inverse) {
        // transform the page size to get a correct frame of reference
        page.dx *= zoom;
        page.dy *= zoom;
        if (rotation % 180 != 0) {
            std::swap(page.dx, page.dy);
        }
        // invert rotation and zoom
        rotation = -rotation;
        zoom = 1.0f / zoom;
    }

    rotation = NormalizeRotation(rotation);

    PointF res = pt; // for rotation == 0
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

RectF EngineDjVu::Transform(const RectF& rect, int pageNo, float zoom, int rotation, bool inverse) {
    PointF TL = TransformPoint(rect.TL(), pageNo, zoom, rotation, inverse);
    PointF BR = TransformPoint(rect.BR(), pageNo, zoom, rotation, inverse);
    return RectF::FromXY(TL, BR);
}

Str EngineDjVu::GetFileData() {
    return GetStreamOrFileData(stream, FilePath());
}

bool EngineDjVu::SaveFileAs(Str dstPath) {
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

static void AppendNewlineUtf8(str::Builder& extracted, Vec<Rect>& coords, Str lineSep) {
    if (' ' == extracted.LastChar()) {
        extracted.RemoveLast();
        coords.RemoveLast();
    }
    extracted.Append(lineSep);
    coords.AppendBlanks(len(lineSep));
}

bool EngineDjVu::ExtractPageText(miniexp_t item, str::Builder& extracted, Vec<Rect>& coords) {
    const Str lineSep = StrL("\n");
    miniexp_t type = miniexp_car(item);
    if (!miniexp_symbolp(type)) {
        return false;
    }
    item = miniexp_cdr(item);

    if (!miniexp_numberp(miniexp_car(item))) {
        return false;
    }
    int x0 = miniexp_to_int(miniexp_car(item));
    item = miniexp_cdr(item);
    if (!miniexp_numberp(miniexp_car(item))) {
        return false;
    }
    int y0 = miniexp_to_int(miniexp_car(item));
    item = miniexp_cdr(item);
    if (!miniexp_numberp(miniexp_car(item))) {
        return false;
    }
    int x1 = miniexp_to_int(miniexp_car(item));
    item = miniexp_cdr(item);
    if (!miniexp_numberp(miniexp_car(item))) {
        return false;
    }
    int y1 = miniexp_to_int(miniexp_car(item));
    item = miniexp_cdr(item);
    Rect rect = Rect::FromXY(x0, y0, x1, y1);

    miniexp_t str = miniexp_car(item);
    if (miniexp_stringp(str) && !miniexp_cdr(item)) {
        if (type != miniexp_symbol("char") && type != miniexp_symbol("word") ||
            len(coords) > 0 && rect.y < coords.Last().y - coords.Last().dy * 0.8) {
            AppendNewlineUtf8(extracted, coords, lineSep);
        }
        Str content = Str(miniexp_to_str(str));
        if (content) {
            // the text layer usually only has word-granularity boxes, so
            // approximate per-glyph rects by evenly splitting the box
            // horizontally (computed from endpoints so slices tile exactly);
            // this makes partial-word search hits and selections highlight
            // roughly just the matched characters
            int nCodepoints = Utf8CodepointCount(content);
            for (int i = 0; i < nCodepoints; i++) {
                int xStart = rect.x + (i * rect.dx) / nCodepoints;
                int xEnd = rect.x + ((i + 1) * rect.dx) / nCodepoints;
                coords.Append(Rect(xStart, rect.y, xEnd - xStart, rect.dy));
            }
            extracted.Append(content);
        }
        if (miniexp_symbol("word") == type) {
            extracted.AppendChar(' ');
            coords.Append(Rect(rect.x + rect.dx, rect.y, 2, rect.dy));
        }
        item = miniexp_cdr(item);
    }
    while (miniexp_consp(str)) {
        ExtractPageText(str, extracted, coords);
        item = miniexp_cdr(item);
        str = miniexp_car(item);
    }
    return !item;
}

PageText EngineDjVu::ExtractPageText(int pageNo) {
    const Str lineSep = StrL("\n");
    ScopedCritSec scope(&gDjVuContext->lock);

    miniexp_t pagetext;
    while ((pagetext = ddjvu_document_get_pagetext(doc, pageNo - 1, nullptr)) == miniexp_dummy) {
        gDjVuContext->SpinMessageLoopWithUnlock();
    }
    if (miniexp_nil == pagetext) {
        return {};
    }

    str::Builder extracted;
    Vec<Rect> coords;
    bool success = ExtractPageText(pagetext, extracted, coords);
    ddjvu_miniexp_release(doc, pagetext);
    minilisp_gc();
    if (!success) {
        return {};
    }
    if (len(extracted) > 0 && !str::EndsWith(ToStr(extracted), lineSep)) {
        AppendNewlineUtf8(extracted, coords, lineSep);
    }

    PageText res;

    int nCodepoints = Utf8CodepointCount(ToStr(extracted));
    ReportIf(nCodepoints != len(coords));
    ddjvu_status_t status;
    ddjvu_pageinfo_t info;
    while ((status = ddjvu_document_get_pageinfo(doc, pageNo - 1, &info)) < DDJVU_JOB_OK) {
        gDjVuContext->SpinMessageLoopWithUnlock();
    }
    float dpiFactor = 1.0;
    if (DDJVU_JOB_OK == status) {
        dpiFactor = GetFileDPI() / info.dpi;
    }

    // TODO: the coordinates aren't completely correct yet
    Rect page = PageMediabox(pageNo).Round();
    for (int i = 0; i < len(coords); i++) {
        if (!coords.at(i).IsEmpty()) {
            if (dpiFactor != 1.0) {
                RectF pageF = ToRectF(coords.at(i));
                pageF.x *= dpiFactor;
                pageF.dx *= dpiFactor;
                pageF.y *= dpiFactor;
                pageF.dy *= dpiFactor;
                coords.at(i) = pageF.Round();
            }
            coords.at(i).y = page.dy - coords.at(i).y - coords.at(i).dy;
        }
    }
    ReportIf(len(coords) != nCodepoints);
    res.len = len(extracted);
    res.nCodepoints = nCodepoints;
    res.text = extracted.TakeStr();
    res.coords = coords.Take();
    return res;
}

Vec<IPageElement*> EngineDjVu::GetElements(int pageNo) {
    ReportIf(pageNo < 1 || pageNo > PageCount());
    auto pi = pages[pageNo - 1];

    ScopedCritSec scope(&gDjVuContext->lock);

    if (pi->gotAllElements) {
        return pi->allElements;
    }
    pi->gotAllElements = true;
    auto& els = pi->allElements;

    if (pi->annos == miniexp_dummy) {
        while (pi->annos == miniexp_dummy) {
            pi->annos = ddjvu_document_get_pageanno(doc, pageNo - 1);
            if (pi->annos == miniexp_dummy) {
                gDjVuContext->SpinMessageLoopWithUnlock();
            }
        }
    }

    if (!pi->annos) {
        return els;
    }

    Rect page = PageMediabox(pageNo).Round();

    ddjvu_status_t status;
    ddjvu_pageinfo_t info;
    while ((status = ddjvu_document_get_pageinfo(doc, pageNo - 1, &info)) < DDJVU_JOB_OK) {
        gDjVuContext->SpinMessageLoopWithUnlock();
    }
    float dpiFactor = 1.0;
    if (DDJVU_JOB_OK == status) {
        dpiFactor = GetFileDPI() / info.dpi;
    }

    miniexp_t* links = ddjvu_anno_get_hyperlinks(pi->annos);
    for (int i = 0; links[i]; i++) {
        miniexp_t anno = miniexp_cdr(links[i]);

        miniexp_t url = miniexp_car(anno);
        Str urlA;
        if (miniexp_stringp(url)) {
            urlA = Str(miniexp_to_str(url));
        } else if (miniexp_consp(url) && miniexp_car(url) == miniexp_symbol("url") &&
                   miniexp_stringp(miniexp_cadr(url)) && miniexp_stringp(miniexp_caddr(url))) {
            urlA = Str(miniexp_to_str(miniexp_cadr(url)));
        }
        if (!urlA) {
            continue;
        }

        anno = miniexp_cdr(anno);
        miniexp_t comment = miniexp_car(anno);
        Str commentUtf8;
        if (miniexp_stringp(comment)) {
            commentUtf8 = Str(miniexp_to_str(comment));
        }

        anno = miniexp_cdr(anno);
        miniexp_t area = miniexp_car(anno);
        miniexp_t type = miniexp_car(area);
        if (type != miniexp_symbol("rect") && type != miniexp_symbol("oval") && type != miniexp_symbol("text")) {
            continue; // unsupported shape;
        }

        area = miniexp_cdr(area);
        if (!miniexp_numberp(miniexp_car(area))) {
            continue;
        }
        int x = miniexp_to_int(miniexp_car(area));
        area = miniexp_cdr(area);
        if (!miniexp_numberp(miniexp_car(area))) {
            continue;
        }
        int y = miniexp_to_int(miniexp_car(area));
        area = miniexp_cdr(area);
        if (!miniexp_numberp(miniexp_car(area))) {
            continue;
        }
        int w = miniexp_to_int(miniexp_car(area));
        area = miniexp_cdr(area);
        if (!miniexp_numberp(miniexp_car(area))) {
            continue;
        }
        int h = miniexp_to_int(miniexp_car(area));
        // area = miniexp_cdr(area); // TODO: dead store, why was it here?
        if (dpiFactor != 1.0) {
            x = (int)(x * dpiFactor);
            w = (int)(w * dpiFactor);
            y = (int)(y * dpiFactor);
            h = (int)(h * dpiFactor);
        }
        Rect rect(x, page.dy - y - h, w, h);

        TempStr link = ResolveNamedDestTemp(urlA);
        if (!link) {
            link = urlA;
        }
        auto el = NewDjVuLink(pageNo, rect, link, commentUtf8);
        if (!el || el->GetKind() == kindDestinationNone) {
            logf("invalid link '%s', pages in document: %d\n", link ? link : StrL(""), PageCount());
            ReportIf(true);
            delete el; // not appended to els, so we own it
            continue;
        }
        els.Append(el);
    }
    ddjvu_free(links);

    return els;
}

// don't delete the result
IPageElement* EngineDjVu::GetElementAtPos(int pageNo, PointF pt) {
    Vec<IPageElement*> els = GetElements(pageNo);

    int n = len(els);
    // elements are extracted bottom-to-top but are accessed
    // in top-to-bottom order, so search bacwards
    for (int i = n - 1; i >= 0; i--) {
        auto el = els.at(i);
        if (el->GetRect().Contains(pt)) {
            return el;
        }
    }
    return nullptr;
}

bool EngineDjVu::HandleLink(IPageDestination* dest, ILinkHandler* linkHandler) {
    auto kind = dest->GetKind();
    if (kind != kindDestinationDjVu) {
        return false;
    }
    PageDestinationDjVu* ddest = (PageDestinationDjVu*)dest;

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

    if (CouldBeURL(link)) {
        linkHandler->LaunchURL(link);
        return true;
    }

    int pageNo = ParseDjVuLink(link);
    if ((pageNo < 1) || (pageNo > pageCount)) {
        // try resolving as a named destination (e.g. "#vii" for named pages)
        TempStr resolved = ResolveNamedDestTemp(link);
        if (resolved) {
            pageNo = ParseDjVuLink(resolved);
        }
    }
    if ((pageNo < 1) || (pageNo > pageCount)) {
        logf("EngineDjVu::HandleLink: invalid page in a link '%s', pageNo: %d, number of pages: %d\n", link, pageNo,
             pageCount);
        ReportIf(true);
        return false;
    }

    ctrl->GoToPage(pageNo, true);
    return true;

#if 0
    if (!res->kind) {
        logf("unsupported djvu link: '%s'\n", link);
    }

    res->kind = kindDestinationNone;
    return res;
#endif

    return true;
}

// returns a numeric DjVu link to a named page (if the name resolves)
// caller needs to free() the result
TempStr EngineDjVu::ResolveNamedDestTemp(Str name) {
    if (!str::StartsWith(name, "#")) {
        return {};
    }
    Str nameWithoutHash = name.len > 1 ? Str(name.s + 1, name.len - 1) : Str{};
    for (int i = 0; i < len(fileInfos); i++) {
        ddjvu_fileinfo_t& fi = fileInfos[i];
        if (str::EqI(nameWithoutHash, Str(fi.id))) {
            return fmt("#%d", fi.pageno + 1);
        }
    }
    return {};
}

IPageDestination* EngineDjVu::GetNamedDest(Str name) {
    if (!str::StartsWith(name, "#")) {
        name = str::JoinTemp(StrL("#"), name);
    }

    TempStr link = ResolveNamedDestTemp(name);
    if (link) {
        return NewDjVuDestination(link, nullptr);
    }
    return nullptr;
}

TocItem* EngineDjVu::BuildTocTree(TocItem* parent, miniexp_t entry, int& idCounter) {
    TocItem* node = nullptr;

    for (miniexp_t rest = entry; miniexp_consp(rest); rest = miniexp_cdr(rest)) {
        miniexp_t item = miniexp_car(rest);
        if (!miniexp_consp(item) || !miniexp_consp(miniexp_cdr(item))) {
            continue;
        }

        Str name = Str(miniexp_to_str(miniexp_car(item)));
        Str link = Str(miniexp_to_str(miniexp_cadr(item)));
        if (!name || !link) {
            continue;
        }

        TocItem* tocItem = nullptr;
        TempStr linkNo = ResolveNamedDestTemp(link);
        Str linkWithoutHash = link.len > 1 ? Str(link.s + 1, link.len - 1) : Str{};
        if (!linkNo) {
            tocItem = NewDjVuTocItem(parent, name, link);
        } else if (!str::IsEmpty(name) && !str::Eq(name, linkWithoutHash)) {
            tocItem = NewDjVuTocItem(parent, name, linkNo);
        } else {
            // ignore generic (name-less) entries
            TocItem* tmp = BuildTocTree(nullptr, miniexp_cddr(item), idCounter);
            delete tmp;
            continue;
        }

        tocItem->id = ++idCounter;
        tocItem->child = BuildTocTree(tocItem, miniexp_cddr(item), idCounter);

        if (!node) {
            node = tocItem;
        } else {
            node->AddSiblingAtEnd(tocItem);
        }
    }

    return node;
}

TocTree* EngineDjVu::GetToc() {
    if (outline == miniexp_nil) {
        return nullptr;
    }

    if (tocTree) {
        return tocTree;
    }
    ScopedCritSec scope(&gDjVuContext->lock);
    int idCounter = 0;
    TocItem* root = BuildTocTree(nullptr, outline, idCounter);
    if (!root) {
        return nullptr;
    }
    auto realRoot = new TocItem();
    realRoot->child = root;
    tocTree = new TocTree(realRoot);
    return tocTree;
}

TempStr EngineDjVu::GetPageLabeTemp(int pageNo) const {
    for (int i = 0; i < len(fileInfos); i++) {
        ddjvu_fileinfo_t& info = fileInfos.at(i);
        if (pageNo - 1 == info.pageno && !str::Eq(info.title, info.id)) {
            return str::DupTemp(info.title);
        }
    }
    return EngineBase::GetPageLabeTemp(pageNo);
}

int EngineDjVu::GetPageByLabel(Str label) const {
    for (int i = 0; i < len(fileInfos); i++) {
        ddjvu_fileinfo_t& info = fileInfos.at(i);
        if (str::EqI(info.title, label) && !str::Eq(info.title, info.id)) {
            return info.pageno + 1;
        }
    }
    return EngineBase::GetPageByLabel(label);
}

bool IsEngineDjVuSupportedFileType(Kind kind) {
    return kind == kindFileDjVu;
}

EngineBase* CreateEngineDjVuFromStream(IStream* stream) {
    EngineDjVu* engine = new EngineDjVu();
    if (engine->Load(stream)) {
        return engine;
    }
    SafeEngineRelease(&engine);
    return nullptr;
}

EngineBase* CreateEngineDjVuFromFile(Str path) {
    EngineDjVu* engine = new EngineDjVu();
    if (engine->Load(path)) {
        return engine;
    }
    SafeEngineRelease(&engine);
    return nullptr;
}
