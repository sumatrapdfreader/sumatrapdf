/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// hack to prevent libdjvu from being built as an export/import library
#define DDJVUAPI    /**/
#define MINILISPAPI /**/

#include <ddjvuapi.h>
#include <miniexp.h>

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/ByteReader.h"
#include "utils/FileUtil.h"
#include "utils/GuessFileType.h"
#include "utils/WinUtil.h"

#include "wingui/UIModels.h"

#include "SumatraConfig.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"

#include "utils/Log.h"

Kind kindEngineDjVu = "engineDjVu";

// TODO: libdjvu leaks memory - among others
//       DjVuPort::corpse_lock, DjVuPort::corpse_head, pcaster,
//       DataPool::OpenFiles::global_ptr, FCPools::global_ptr
//       cf. http://sourceforge.net/projects/djvu/forums/forum/103286/topic/3553602

// parses "123", "#123", "# 123"
// returns -1 for invalid page
static int ParseDjVuLink(const char* link) {
    if (!link) {
        return -1;
    }
    if (link[0] == '#') {
        ++link;
    }
    if (link[0] == ' ') {
        ++link;
    }
    int n = atoi(link);
    return n;
}

static bool CouldBeURL(const char* link) {
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
    const char* link = nullptr;
    char* value = nullptr;

    PageDestinationDjVu(const char* l, const char* comment) {
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

    char* GetValue() override {
        if (value) {
            return value;
        }
        if (!CouldBeURL(link)) {
            return nullptr;
        }
        value = str::Dup(link);
        return value;
    }
};

// the link format can be any of
//   #[ ]<pageNo>      e.g. #1 for FirstPage and # 13 for page 13
//   #[+-]<pageCount>  e.g. #+1 for NextPage and #-1 for PrevPage
//   #filename.djvu    use ResolveNamedDest to get a link in #<pageNo> format
//   http://example.net/#hyperlink
static IPageDestination* NewDjVuDestination(const char* link, const char* comment) {
    if (str::IsEmpty(link) || str::Eq(link, "#")) {
        return nullptr;
    }
    auto res = new PageDestinationDjVu(link, comment);
    res->rect = RectF(DEST_USE_DEFAULT, DEST_USE_DEFAULT, DEST_USE_DEFAULT, DEST_USE_DEFAULT);
    res->pageNo = ParseDjVuLink(link);
    return res;
}

static IPageElement* NewDjVuLink(int pageNo, Rect rect, const char* link, const char* comment) {
    auto dest = NewDjVuDestination(link, comment);
    if (!dest) {
        return nullptr;
    }
    auto res = new PageElementDestination(dest);
    res->rect = ToRectF(rect);
    res->pageNo = pageNo;
    return res;
}

static TocItem* NewDjVuTocItem(TocItem* parent, const char* title, const char* link) {
    auto res = new TocItem(parent, title, 0);
    res->dest = NewDjVuDestination(link, nullptr);
    if (res->dest) {
        res->pageNo = res->dest->GetPageNo();
    }
    return res;
}

struct DjVuContext {
    ddjvu_context_t* ctx = nullptr;
    int refCount = 1;
    CRITICAL_SECTION lock;

    DjVuContext() {
        InitializeCriticalSection(&lock);
        ctx = ddjvu_context_create("DjVuEngine");
        // reset the locale to "C" as most other code expects
        setlocale(LC_ALL, "C");
        CrashIf(!ctx);
    }

    int AddRef() {
        EnterCriticalSection(&lock);
        ++refCount;
        int res = refCount;
        LeaveCriticalSection(&lock);
        return res;
    }

    int Release() {
        EnterCriticalSection(&lock);
        CrashIf(refCount <= 0);
        --refCount;
        LeaveCriticalSection(&lock);
        return refCount;
    }

    ~DjVuContext() {
        EnterCriticalSection(&lock);
        if (ctx) {
            ddjvu_context_release(ctx);
        }
        LeaveCriticalSection(&lock);
        DeleteCriticalSection(&lock);
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

    ddjvu_document_t* OpenFile(const char* fileName) {
        ScopedCritSec scope(&lock);
        // TODO: libdjvu sooner or later crashes inside its caching code; cf.
        //       http://code.google.com/p/sumatrapdf/issues/detail?id=1434
        return ddjvu_document_create_by_filename_utf8(ctx, fileName, /* cache */ FALSE);
    }

    ddjvu_document_t* OpenStream(IStream* stream) {
        ScopedCritSec scope(&lock);
        ByteSlice d = GetDataFromStream(stream, nullptr);
        AutoFree dFree(d.Get());
        if (d.empty() || d.size() > ULONG_MAX) {
            return nullptr;
        }
        auto res = ddjvu_document_create_by_data(ctx, d, (ULONG)d.size());
        return res;
    }
};

// TODO: make it non-static because it accesses other static state
// in djvu which got deleted first
static DjVuContext* gDjVuContext;

static DjVuContext* GetDjVuContext() {
    if (!gDjVuContext) {
        gDjVuContext = new DjVuContext();
    } else {
        gDjVuContext->AddRef();
    }
    return gDjVuContext;
}

static void ReleaseDjVuContext() {
    CrashIf(!gDjVuContext);
    int refCount = gDjVuContext->Release();
    if (refCount != 0) {
        return;
    }
}

void CleanupEngineDjVu() {
    if (gDjVuContext) {
        CrashIf(gDjVuContext->refCount != 0);
        delete gDjVuContext;
        gDjVuContext = nullptr;
    }
    minilisp_finish();
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

    RenderedBitmap* RenderPage(RenderPageArgs&) override;

    PointF TransformPoint(PointF pt, int pageNo, float zoom, int rotation, bool inverse = false);
    RectF Transform(const RectF& rect, int pageNo, float zoom, int rotation, bool inverse = false) override;

    ByteSlice GetFileData() override;
    bool SaveFileAs(const char* copyFileName) override;
    PageText ExtractPageText(int pageNo) override;
    bool HasClipOptimizations(int pageNo) override;

    char* GetProperty(DocumentProperty prop) override;

    // we currently don't load pages lazily, so there's nothing to do here
    bool BenchLoadPage(int pageNo) override;

    Vec<IPageElement*> GetElements(int pageNo) override;
    IPageElement* GetElementAtPos(int pageNo, PointF pt) override;
    bool HandleLink(IPageDestination*, ILinkHandler*) override;

    IPageDestination* GetNamedDest(const char* name) override;
    TocTree* GetToc() override;

    TempStr GetPageLabeTemp(int pageNo) const override;
    int GetPageByLabel(const char* label) const override;

    static EngineBase* CreateFromFile(const char* path);
    static EngineBase* CreateFromStream(IStream* stream);

  protected:
    IStream* stream = nullptr;

    Vec<DjVuPageInfo*> pages;

    ddjvu_document_t* doc = nullptr;
    miniexp_t outline = miniexp_nil;
    TocTree* tocTree = nullptr;

    Vec<ddjvu_fileinfo_t> fileInfos;

    RenderedBitmap* CreateRenderedBitmap(const char* bmpData, Size size, bool grayscale) const;
    bool ExtractPageText(miniexp_t item, str::WStr& extracted, Vec<Rect>& coords);
    TempStr ResolveNamedDestTemp(const char* name);
    TocItem* BuildTocTree(TocItem* parent, miniexp_t entry, int& idCounter);
    bool Load(const char* fileName);
    bool Load(IStream* stream);
    bool FinishLoading();
    bool LoadMediaboxes();
};

EngineDjVu::EngineDjVu() {
    kind = kindEngineDjVu;
    str::ReplaceWithCopy(&defaultExt, ".djvu");
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
    }
    if (doc) {
        ddjvu_document_release(doc);
    }
    if (stream) {
        stream->Release();
    }
    ReleaseDjVuContext();
}

EngineBase* EngineDjVu::Clone() {
    if (stream != nullptr) {
        return CreateFromStream(stream);
    }
    const char* path = FilePath();
    if (path) {
        return CreateFromFile(path);
    }
    return nullptr;
}

RectF EngineDjVu::PageMediabox(int pageNo) {
    CrashIf(pageNo < 1 || pageNo > pageCount);
    DjVuPageInfo* pi = pages[pageNo - 1];
    return pi->mediabox;
}

bool EngineDjVu::HasClipOptimizations(int) {
    return false;
}

char* EngineDjVu::GetProperty(DocumentProperty) {
    return nullptr;
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

#include <poppack.h>

static_assert(sizeof(DjVuInfoChunk) == 10, "wrong size of DjVuInfoChunk structure");

bool EngineDjVu::LoadMediaboxes() {
    const char* path = FilePath();
    if (!path) {
        return false;
    }
    AutoCloseHandle h(file::OpenReadOnly(path));
    if (!h.IsValid()) {
        return false;
    }
    char buffer[16];
    ByteReader r(buffer, sizeof(buffer));
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
            CrashIf(!ok);
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

bool EngineDjVu::Load(const char* fileName) {
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
        gDjVuContext->SpinMessageLoop();
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
                gDjVuContext->SpinMessageLoop();
            }
            if (DDJVU_JOB_OK == status) {
                DjVuPageInfo* pi = pages[i];
                float dx = (float)info.width * GetFileDPI() / (float)info.dpi;
                float dy = (float)info.height * GetFileDPI() / (float)info.dpi;
                pi->mediabox.dx = dx;
                pi->mediabox.dy = dy;
            }
        }
    }

    while ((outline = ddjvu_document_get_outline(doc)) == miniexp_dummy) {
        gDjVuContext->SpinMessageLoop();
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
            gDjVuContext->SpinMessageLoop();
        }
        if (DDJVU_JOB_OK == status && info.type == 'P' && info.pageno >= 0) {
            fileInfos.Append(info);
            hasPageLabels = hasPageLabels || !str::Eq(info.title, info.id);
        }
    }

    return true;
}

RenderedBitmap* EngineDjVu::CreateRenderedBitmap(const char* bmpData, Size size, bool grayscale) const {
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

RenderedBitmap* EngineDjVu::RenderPage(RenderPageArgs& args) {
    ScopedCritSec scope(&gDjVuContext->lock);
    auto pageRect = args.pageRect;
    auto zoom = args.zoom;
    auto pageNo = args.pageNo;
    auto rotation = NormalizeRotation(args.rotation);
    RectF pageRc = pageRect ? *pageRect : PageMediabox(pageNo);
    Rect screen = Transform(pageRc, pageNo, zoom, rotation).Round();
    Rect full = Transform(PageMediabox(pageNo), pageNo, zoom, rotation).Round();
    screen = full.Intersect(screen);

    ddjvu_page_t* page = ddjvu_page_create_by_pageno(doc, pageNo - 1);
    if (!page) {
        return nullptr;
    }
    while (!ddjvu_page_decoding_done(page)) {
        gDjVuContext->SpinMessageLoop();
    }
    if (ddjvu_page_decoding_error(page)) {
        return nullptr;
    }

    ddjvu_page_rotation_t rot = DDJVU_ROTATE_0;
    switch (rotation) {
        case 0:
            rot = DDJVU_ROTATE_0;
            break;
        // for whatever reason, 90 and 270 are reverased compared to what I expect
        // maybe I'm doing other parts of the code wrong
        case 90:
            rot = DDJVU_ROTATE_270;
            break;
        case 180:
            rot = DDJVU_ROTATE_180;
            break;
        case 270:
            rot = DDJVU_ROTATE_90;
            break;
        default:
            CrashIf("invalid rotation");
            break;
    }
    ddjvu_page_set_rotation(page, rot);

    bool isBitonal = DDJVU_PAGETYPE_BITONAL == ddjvu_page_get_type(page);
    ddjvu_format_style_t style = isBitonal ? DDJVU_FORMAT_GREY8 : DDJVU_FORMAT_BGR24;
    ddjvu_format_t* fmt = ddjvu_format_create(style, 0, nullptr);

    defer {
        ddjvu_format_release(fmt);
        ddjvu_page_release(page);
    };

    int topToBottom = TRUE;
    ddjvu_format_set_row_order(fmt, topToBottom);
    ddjvu_rect_t prect = {full.x, full.y, (uint)full.dx, (uint)full.dy};
    ddjvu_rect_t rrect = {screen.x, 2 * full.y - screen.y + full.dy - screen.dy, (uint)screen.dx, (uint)screen.dy};

    RenderedBitmap* bmp = nullptr;
    size_t bytesPerPixel = isBitonal ? 1 : 3;
    size_t dx = (size_t)screen.dx;
    size_t dy = (size_t)screen.dy;
    size_t stride = ((dx * bytesPerPixel + 3) / 4) * 4;
    size_t nBytes = stride * (dy + 5);
    char* bmpData = AllocArrayTemp<char>(nBytes);
    if (!bmpData) {
        return nullptr;
    }

    ddjvu_render_mode_t mode = isBitonal ? DDJVU_RENDER_MASKONLY : DDJVU_RENDER_COLOR;
    int ok = ddjvu_page_render(page, mode, &prect, &rrect, fmt, (unsigned long)stride, bmpData);
    if (!ok) {
        // nothing was rendered, leave the page blank (same as WinDjView)
        memset(bmpData, 0xFF, stride * dy);
        isBitonal = true;
    }
    bmp = CreateRenderedBitmap(bmpData, screen.Size(), isBitonal);

    return bmp;
}

RectF EngineDjVu::PageContentBox(int pageNo, RenderTarget) {
    ScopedCritSec scope(&gDjVuContext->lock);

    RectF pageRc = PageMediabox(pageNo);
    ddjvu_page_t* page = ddjvu_page_create_by_pageno(doc, pageNo - 1);
    if (!page) {
        return pageRc;
    }

    while (!ddjvu_page_decoding_done(page)) {
        gDjVuContext->SpinMessageLoop();
    }
    if (ddjvu_page_decoding_error(page)) {
        return pageRc;
    }
    ddjvu_page_set_rotation(page, DDJVU_ROTATE_0);

    // render the page in 8-bit grayscale up to 250x250 px in size
    ddjvu_format_t* fmt = ddjvu_format_create(DDJVU_FORMAT_GREY8, 0, nullptr);

    defer {
        ddjvu_format_release(fmt);
        ddjvu_page_release(page);
    };

    ddjvu_format_set_row_order(fmt, /* top_to_bottom */ TRUE);
    float zoom = std::min(std::min(250.0f / pageRc.dx, 250.0f / pageRc.dy), 1.0f);
    Rect full = RectF(0, 0, pageRc.dx * zoom, pageRc.dy * zoom).Round();
    ddjvu_rect_t prect = {full.x, full.y, (uint)full.dx, (uint)full.dy};
    ddjvu_rect_t rrect = prect;

    char* bmpData = AllocArrayTemp<char>(full.dx * full.dy + 1);
    if (!bmpData) {
        return pageRc;
    }

    int ok = ddjvu_page_render(page, DDJVU_RENDER_MASKONLY, &prect, &rrect, fmt, full.dx, bmpData);
    if (!ok) {
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

    return pageRc;
}

PointF EngineDjVu::TransformPoint(PointF pt, int pageNo, float zoom, int rotation, bool inverse) {
    CrashIf(zoom <= 0);
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

ByteSlice EngineDjVu::GetFileData() {
    return GetStreamOrFileData(stream, FilePath());
}

bool EngineDjVu::SaveFileAs(const char* dstPath) {
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

static void AppendNewline(str::WStr& extracted, Vec<Rect>& coords, const WCHAR* lineSep) {
    if (extracted.size() > 0 && ' ' == extracted.Last()) {
        extracted.RemoveLast();
        coords.RemoveLast();
    }
    extracted.Append(lineSep);
    coords.AppendBlanks(str::Len(lineSep));
}

bool EngineDjVu::ExtractPageText(miniexp_t item, str::WStr& extracted, Vec<Rect>& coords) {
    const WCHAR* lineSep = L"\n";
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
            coords.size() > 0 && rect.y < coords.Last().y - coords.Last().dy * 0.8) {
            AppendNewline(extracted, coords, lineSep);
        }
        const char* content = miniexp_to_str(str);
        WCHAR* value = ToWstr(content);
        if (value) {
            size_t len = str::Len(value);
            // TODO: split the rectangle into individual parts per glyph
            for (size_t i = 0; i < len; i++) {
                coords.Append(Rect(rect.x, rect.y, rect.dx, rect.dy));
            }
            extracted.AppendAndFree(value);
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
    const WCHAR* lineSep = L"\n";
    ScopedCritSec scope(&gDjVuContext->lock);

    miniexp_t pagetext;
    while ((pagetext = ddjvu_document_get_pagetext(doc, pageNo - 1, nullptr)) == miniexp_dummy) {
        gDjVuContext->SpinMessageLoop();
    }
    if (miniexp_nil == pagetext) {
        return {};
    }

    str::WStr extracted;
    Vec<Rect> coords;
    bool success = ExtractPageText(pagetext, extracted, coords);
    ddjvu_miniexp_release(doc, pagetext);
    if (!success) {
        return {};
    }
    if (extracted.size() > 0 && !str::EndsWith(extracted.Get(), lineSep)) {
        AppendNewline(extracted, coords, lineSep);
    }

    PageText res;

    CrashIf(str::Len(extracted.Get()) != coords.size());
    ddjvu_status_t status;
    ddjvu_pageinfo_t info;
    while ((status = ddjvu_document_get_pageinfo(doc, pageNo - 1, &info)) < DDJVU_JOB_OK) {
        gDjVuContext->SpinMessageLoop();
    }
    float dpiFactor = 1.0;
    if (DDJVU_JOB_OK == status) {
        dpiFactor = GetFileDPI() / info.dpi;
    }

    // TODO: the coordinates aren't completely correct yet
    Rect page = PageMediabox(pageNo).Round();
    for (size_t i = 0; i < coords.size(); i++) {
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
    CrashIf(coords.size() != extracted.size());
    res.len = (int)extracted.size();
    res.text = extracted.StealData();
    res.coords = coords.StealData();
    return res;
}

Vec<IPageElement*> EngineDjVu::GetElements(int pageNo) {
    CrashIf(pageNo < 1 || pageNo > PageCount());
    auto pi = pages[pageNo - 1];
    if (pi->gotAllElements) {
        return pi->allElements;
    }
    pi->gotAllElements = true;
    auto& els = pi->allElements;

    if (pi->annos == miniexp_dummy) {
        ScopedCritSec scope(&gDjVuContext->lock);
        while (pi->annos == miniexp_dummy) {
            pi->annos = ddjvu_document_get_pageanno(doc, pageNo - 1);
            if (pi->annos == miniexp_dummy) {
                gDjVuContext->SpinMessageLoop();
            }
        }
    }

    if (!pi->annos) {
        return els;
    }

    ScopedCritSec scope(&gDjVuContext->lock);

    Rect page = PageMediabox(pageNo).Round();

    ddjvu_status_t status;
    ddjvu_pageinfo_t info;
    while ((status = ddjvu_document_get_pageinfo(doc, pageNo - 1, &info)) < DDJVU_JOB_OK) {
        gDjVuContext->SpinMessageLoop();
    }
    float dpiFactor = 1.0;
    if (DDJVU_JOB_OK == status) {
        dpiFactor = GetFileDPI() / info.dpi;
    }

    miniexp_t* links = ddjvu_anno_get_hyperlinks(pi->annos);
    for (int i = 0; links[i]; i++) {
        miniexp_t anno = miniexp_cdr(links[i]);

        miniexp_t url = miniexp_car(anno);
        const char* urlA = nullptr;
        if (miniexp_stringp(url)) {
            urlA = miniexp_to_str(url);
        } else if (miniexp_consp(url) && miniexp_car(url) == miniexp_symbol("url") &&
                   miniexp_stringp(miniexp_cadr(url)) && miniexp_stringp(miniexp_caddr(url))) {
            urlA = miniexp_to_str(miniexp_cadr(url));
        }
        if (!urlA || !*urlA) {
            continue;
        }

        anno = miniexp_cdr(anno);
        miniexp_t comment = miniexp_car(anno);
        const char* commentUtf8 = nullptr;
        if (miniexp_stringp(comment)) {
            commentUtf8 = miniexp_to_str(comment);
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
            link = (TempStr)urlA;
        }
        auto el = NewDjVuLink(pageNo, rect, link, commentUtf8);
        if (!el || el->GetKind() == kindDestinationNone) {
            logf("invalid link '%s', pages in document: %d\n", link ? link : "", PageCount());
            ReportIf(true);
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

    int n = els.isize();
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

    if (CouldBeURL(link)) {
        linkHandler->LaunchURL(link);
        return true;
    }

    int pageNo = ParseDjVuLink(link);
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
TempStr EngineDjVu::ResolveNamedDestTemp(const char* name) {
    if (!str::StartsWith(name, "#")) {
        return nullptr;
    }
    for (size_t i = 0; i < fileInfos.size(); i++) {
        ddjvu_fileinfo_t& fi = fileInfos[i];
        if (str::EqI(name + 1, fi.id)) {
            return str::FormatTemp("#%d", fi.pageno + 1);
        }
    }
    return nullptr;
}

IPageDestination* EngineDjVu::GetNamedDest(const char* name) {
    if (!str::StartsWith(name, "#")) {
        name = str::JoinTemp("#", name);
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

        const char* name = miniexp_to_str(miniexp_car(item));
        const char* link = miniexp_to_str(miniexp_cadr(item));
        if (!name || !link) {
            continue;
        }

        TocItem* tocItem = nullptr;
        TempStr linkNo = ResolveNamedDestTemp(link);
        if (!linkNo) {
            tocItem = NewDjVuTocItem(parent, name, link);
        } else if (!str::IsEmpty(name) && !str::Eq(name, link + 1)) {
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
    for (size_t i = 0; i < fileInfos.size(); i++) {
        ddjvu_fileinfo_t& info = fileInfos.at(i);
        if (pageNo - 1 == info.pageno && !str::Eq(info.title, info.id)) {
            return str::DupTemp(info.title);
        }
    }
    return EngineBase::GetPageLabeTemp(pageNo);
}

int EngineDjVu::GetPageByLabel(const char* label) const {
    for (size_t i = 0; i < fileInfos.size(); i++) {
        ddjvu_fileinfo_t& info = fileInfos.at(i);
        if (str::EqI(info.title, label) && !str::Eq(info.title, info.id)) {
            return info.pageno + 1;
        }
    }
    return EngineBase::GetPageByLabel(label);
}

EngineBase* EngineDjVu::CreateFromFile(const char* path) {
    EngineDjVu* engine = new EngineDjVu();
    if (!engine->Load(path)) {
        delete engine;
        return nullptr;
    }
    return engine;
}

EngineBase* EngineDjVu::CreateFromStream(IStream* stream) {
    EngineDjVu* engine = new EngineDjVu();
    if (!engine->Load(stream)) {
        delete engine;
        return nullptr;
    }
    return engine;
}

bool IsEngineDjVuSupportedFileType(Kind kind) {
    return kind == kindFileDjVu;
}

EngineBase* CreateEngineDjVuFromFile(const char* path) {
    return EngineDjVu::CreateFromFile(path);
}

EngineBase* CreateEngineDjVuFromStream(IStream* stream) {
    return EngineDjVu::CreateFromStream(stream);
}
