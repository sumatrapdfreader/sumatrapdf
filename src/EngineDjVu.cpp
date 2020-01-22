/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// hack to prevent libdjvu from being built as an export/import library
#define DDJVUAPI    /**/
#define MINILISPAPI /**/

#include "utils/BaseUtil.h"
#include <ddjvuapi.h>
#include <miniexp.h>
#include "utils/ByteReader.h"
#include "utils/FileUtil.h"
#include "utils/WinUtil.h"
#include "utils/ScopedWin.h"
#include "utils/Log.h"

#include "SumatraConfig.h"
#include "wingui/TreeModel.h"
#include "EngineBase.h"
#include "EngineDjVu.h"

Kind kindEngineDjVu = "engineDjVu";

// TODO: libdjvu leaks memory - among others
//       DjVuPort::corpse_lock, DjVuPort::corpse_head, pcaster,
//       DataPool::OpenFiles::global_ptr, FCPools::global_ptr
//       cf. http://sourceforge.net/projects/djvu/forums/forum/103286/topic/3553602

static bool IsPageLink(const char* link) {
    return link && link[0] == '#' && (str::IsDigit(link[1]) || link[1] == ' ' && str::IsDigit(link[2]));
}

// the link format can be any of
//   #[ ]<pageNo>      e.g. #1 for FirstPage and # 13 for page 13
//   #[+-]<pageCount>  e.g. #+1 for NextPage and #-1 for PrevPage
//   #filename.djvu    use ResolveNamedDest to get a link in #<pageNo> format
//   http://example.net/#hyperlink
static PageDestination* newDjVuDestination(const char* link) {
    auto res = new PageDestination();
    res->rect = RectD(DEST_USE_DEFAULT, DEST_USE_DEFAULT, DEST_USE_DEFAULT, DEST_USE_DEFAULT);

    if (str::IsEmpty(link)) {
        res->kind = kindDestinationNone;
        return res;
    }

    // invalid but seen in a crash report
    if (str::Eq(link, "#")) {
        res->kind = kindDestinationNone;
        return res;
    }

    if (str::Eq(link, "#+1")) {
        res->kind = kindDestinationNextPage;
        return res;
    }

    if (str::Eq(link, "#-1")) {
        res->kind = kindDestinationPrevPage;
        return res;
    }

    if (IsPageLink(link)) {
        res->kind = kindDestinationScrollTo;
        res->pageNo = atoi(link + 1);
        return res;
    }

    // there are links like: "#Here"
    if (str::StartsWith(link, "#")) {
        // TODO: don't know how to handle those
        // Probably need to use ResolveNamedDest()
        res->kind = kindDestinationNone;
        return res;
    }

    if (str::StartsWithI(link, "http:") || str::StartsWithI(link, "https:") || str::StartsWithI(link, "mailto:")) {
        res->kind = kindDestinationLaunchURL;
        res->value = strconv::Utf8ToWstr(link);
        return res;
    }

    // very lenient heuristic
    bool couldBeURL = str::Contains(link, ".");
    if (couldBeURL) {
        res->kind = kindDestinationLaunchURL;
        res->value = strconv::Utf8ToWstr(link);
        return res;
    }

    if (!res->kind) {
        logf("unsupported djvu link: '%s'\n", link);
        CrashIf(!res->kind);
    }

    res->kind = kindDestinationNone;
    return res;
}

static PageElement* newDjVuLink(int pageNo, RectI rect, const char* link, const char* comment) {
    auto res = new PageElement();
    res->rect = rect.Convert<double>();
    res->pageNo = pageNo;
    res->dest = newDjVuDestination(link);
    if (!str::IsEmpty(comment)) {
        res->value = strconv::Utf8ToWstr(comment);
    }
    res->kind = kindPageElementDest;
    if (!str::IsEmpty(comment)) {
        res->value = strconv::Utf8ToWstr(comment);
    } else {
        if (kindDestinationLaunchURL == res->dest->Kind()) {
            res->value = str::Dup(res->dest->GetValue());
        }
    }
    return res;
}

static TocItem* newDjVuTocItem(TocItem* parent, const char* title, const char* link) {
    AutoFreeWstr s = strconv::Utf8ToWstr(title);
    auto res = new TocItem(parent, s, 0);
    res->dest = newDjVuDestination(link);
    res->pageNo = res->dest->GetPageNo();
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

    void SpinMessageLoop(bool wait = true) {
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

    ddjvu_document_t* OpenFile(const WCHAR* fileName) {
        ScopedCritSec scope(&lock);
        AutoFree fileNameUtf8(strconv::WstrToUtf8(fileName));
        // TODO: libdjvu sooner or later crashes inside its caching code; cf.
        //       http://code.google.com/p/sumatrapdf/issues/detail?id=1434
        return ddjvu_document_create_by_filename_utf8(ctx, fileNameUtf8.Get(), /* cache */ FALSE);
    }

    ddjvu_document_t* OpenStream(IStream* stream) {
        ScopedCritSec scope(&lock);
        AutoFree d = GetDataFromStream(stream, nullptr);
        if (d.empty() || d.size() > ULONG_MAX) {
            return nullptr;
        }
        auto res = ddjvu_document_create_by_data(ctx, d.data, (ULONG)d.size());
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

void CleanupDjVuEngine() {
    if (gDjVuContext) {
        CrashIf(gDjVuContext->refCount != 0);
        delete gDjVuContext;
        gDjVuContext = nullptr;
    }
    minilisp_finish();
}

class EngineDjVu : public EngineBase {
  public:
    EngineDjVu();
    virtual ~EngineDjVu();
    EngineBase* Clone() override;

    RectD PageMediabox(int pageNo) override;
    RectD PageContentBox(int pageNo, RenderTarget target = RenderTarget::View) override;

    RenderedBitmap* RenderPage(RenderPageArgs&) override;

    PointD TransformPoint(PointD pt, int pageNo, float zoom, int rotation, bool inverse = false);
    RectD Transform(RectD rect, int pageNo, float zoom, int rotation, bool inverse = false) override;

    std::string_view GetFileData() override;
    bool SaveFileAs(const char* copyFileName, bool includeUserAnnots = false) override;
    WCHAR* ExtractPageText(int pageNo, RectI** coordsOut = nullptr) override;
    bool HasClipOptimizations(int pageNo) override;

    WCHAR* GetProperty(DocumentProperty prop) override;

    void UpdateUserAnnotations(Vec<PageAnnotation>* list) override;

    // we currently don't load pages lazily, so there's nothing to do here
    bool BenchLoadPage(int pageNo) override;

    Vec<PageElement*>* GetElements(int pageNo) override;
    PageElement* GetElementAtPos(int pageNo, PointD pt) override;

    PageDestination* GetNamedDest(const WCHAR* name) override;
    TocTree* GetToc() override;

    WCHAR* GetPageLabel(int pageNo) const override;
    int GetPageByLabel(const WCHAR* label) const override;

    static EngineBase* CreateFromFile(const WCHAR* fileName);
    static EngineBase* CreateFromStream(IStream* stream);

  protected:
    IStream* stream = nullptr;

    RectD* mediaboxes = nullptr;

    ddjvu_document_t* doc = nullptr;
    miniexp_t outline = miniexp_nil;
    miniexp_t* annos = nullptr;
    TocTree* tocTree = nullptr;
    Vec<PageAnnotation> userAnnots;

    Vec<ddjvu_fileinfo_t> fileInfos;

    RenderedBitmap* CreateRenderedBitmap(const char* bmpData, SizeI size, bool grayscale) const;
    void AddUserAnnots(RenderedBitmap* bmp, int pageNo, float zoom, int rotation, RectI screen);
    bool ExtractPageText(miniexp_t item, str::WStr& extracted, Vec<RectI>& coords);
    char* ResolveNamedDest(const char* name);
    TocItem* BuildTocTree(TocItem* parent, miniexp_t entry, int& idCounter);
    bool Load(const WCHAR* fileName);
    bool Load(IStream* stream);
    bool FinishLoading();
    bool LoadMediaboxes();
};

EngineDjVu::EngineDjVu() {
    kind = kindEngineDjVu;
    defaultFileExt = L".djvu";
    // DPI isn't constant for all pages and thus premultiplied
    fileDPI = 300.0f;
    supportsAnnotations = true;
    supportsAnnotationsForSaving = false;
    GetDjVuContext();
}

EngineDjVu::~EngineDjVu() {
    ScopedCritSec scope(&gDjVuContext->lock);

    delete tocTree;
    free(mediaboxes);

    if (annos) {
        for (int i = 0; i < pageCount; i++) {
            if (annos[i]) {
                ddjvu_miniexp_release(doc, annos[i]);
            }
        }
        free(annos);
    }
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
    if (FileName() != nullptr) {
        return CreateFromFile(FileName());
    }
    return nullptr;
}

RectD EngineDjVu::PageMediabox(int pageNo) {
    CrashIf(pageNo < 1 || pageNo > pageCount);
    return mediaboxes[pageNo - 1];
}

bool EngineDjVu::HasClipOptimizations(int pageNo) {
    UNUSED(pageNo);
    return false;
}

WCHAR* EngineDjVu::GetProperty(DocumentProperty prop) {
    UNUSED(prop);
    return nullptr;
}

// we currently don't load pages lazily, so there's nothing to do here
bool EngineDjVu::BenchLoadPage(int pageNo) {
    UNUSED(pageNo);
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
    const WCHAR* fileName = FileName();
    if (!fileName) {
        return false;
    }
    AutoCloseHandle h(file::OpenReadOnly(fileName));
    if (!h.IsValid()) {
        return false;
    }
    char buffer[16];
    ByteReader r(buffer, sizeof(buffer));
    if (!ReadBytes(h, 0, buffer, 16) || r.DWordBE(0) != DJVU_MARK_MAGIC || r.DWordBE(4) != DJVU_MARK_FORM) {
        return false;
    }

    DWORD offset = r.DWordBE(12) == DJVU_MARK_DJVM ? 16 : 4;
    for (int pages = 0; pages < pageCount;) {
        if (!ReadBytes(h, offset, buffer, 16)) {
            return false;
        }
        int partLen = r.DWordBE(4);
        if (partLen < 0) {
            return false;
        }
        if (r.DWordBE(0) == DJVU_MARK_FORM && r.DWordBE(8) == DJVU_MARK_DJVU && r.DWordBE(12) == DJVU_MARK_INFO) {
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
            mediaboxes[pages].dx = GetFileDPI() * info.width / dpi;
            mediaboxes[pages].dy = GetFileDPI() * info.height / dpi;
            if ((info.flags & 4)) {
                std::swap(mediaboxes[pages].dx, mediaboxes[pages].dy);
            }
            pages++;
        }
        offset += 8 + partLen + (partLen & 1);
    }

    return true;
}

bool EngineDjVu::Load(const WCHAR* fileName) {
    SetFileName(fileName);
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

    mediaboxes = AllocArray<RectD>(pageCount);
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
                double dx = info.width * GetFileDPI() / info.dpi;
                double dy = info.height * GetFileDPI() / info.dpi;
                mediaboxes[i] = RectD(0, 0, dx, dy);
            }
        }
    }

    annos = AllocArray<miniexp_t>(pageCount);
    for (int i = 0; i < pageCount; i++) {
        annos[i] = miniexp_dummy;
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

void EngineDjVu::AddUserAnnots(RenderedBitmap* bmp, int pageNo, float zoom, int rotation, RectI screen) {
    using namespace Gdiplus;

    if (!bmp || userAnnots.size() == 0) {
        return;
    }

    HDC hdc = CreateCompatibleDC(nullptr);
    {
        ScopedSelectObject bmpScope(hdc, bmp->GetBitmap());
        Graphics g(hdc);
        g.SetCompositingQuality(CompositingQualityHighQuality);
        g.SetPageUnit(UnitPixel);

        for (size_t i = 0; i < userAnnots.size(); i++) {
            PageAnnotation& annot = userAnnots.at(i);
            if (annot.pageNo != pageNo) {
                continue;
            }
            RectD arect;
            switch (annot.type) {
                case PageAnnotType::Highlight:
                    arect = Transform(annot.rect, pageNo, zoom, rotation);
                    arect.Offset(-screen.x, -screen.y);
                    {
                        SolidBrush tmpBrush(Unblend(annot.color, 119));
                        g.FillRectangle(&tmpBrush, arect.ToGdipRectF());
                    }
                    break;
                case PageAnnotType::Underline:
                case PageAnnotType::StrikeOut:
                    arect = RectD(annot.rect.x, annot.rect.BR().y, annot.rect.dx, 0);
                    if (PageAnnotType::StrikeOut == annot.type)
                        arect.y -= annot.rect.dy / 2;
                    arect = Transform(arect, pageNo, zoom, rotation);
                    arect.Offset(-screen.x, -screen.y);
                    {
                        Pen tmpPen(FromColor(annot.color), zoom);
                        g.DrawLine(&tmpPen, (float)arect.x, (float)arect.y, (float)arect.BR().x, (float)arect.BR().y);
                    }
                    break;
                case PageAnnotType::Squiggly: {
                    Pen p(FromColor(annot.color), 0.5f * zoom);
                    REAL dash[2] = {2, 2};
                    p.SetDashPattern(dash, dimof(dash));
                    p.SetDashOffset(1);
                    arect = Transform(RectD(annot.rect.x, annot.rect.BR().y - 0.25f, annot.rect.dx, 0), pageNo, zoom,
                                      rotation);
                    arect.Offset(-screen.x, -screen.y);
                    g.DrawLine(&p, (float)arect.x, (float)arect.y, (float)arect.BR().x, (float)arect.BR().y);
                    p.SetDashOffset(3);
                    arect = Transform(RectD(annot.rect.x, annot.rect.BR().y + 0.25f, annot.rect.dx, 0), pageNo, zoom,
                                      rotation);
                    arect.Offset(-screen.x, -screen.y);
                    g.DrawLine(&p, (float)arect.x, (float)arect.y, (float)arect.BR().x, (float)arect.BR().y);
                } break;
            }
        }
    }
    DeleteDC(hdc);
}

RenderedBitmap* EngineDjVu::CreateRenderedBitmap(const char* bmpData, SizeI size, bool grayscale) const {
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
    auto rotation = args.rotation;
    RectD pageRc = pageRect ? *pageRect : PageMediabox(pageNo);
    RectI screen = Transform(pageRc, pageNo, zoom, rotation).Round();
    RectI full = Transform(PageMediabox(pageNo), pageNo, zoom, rotation).Round();
    screen = full.Intersect(screen);

    ddjvu_page_t* page = ddjvu_page_create_by_pageno(doc, pageNo - 1);
    if (!page) {
        return nullptr;
    }
    int rotation4 = (((-rotation / 90) % 4) + 4) % 4;
    ddjvu_page_set_rotation(page, (ddjvu_page_rotation_t)rotation4);

    while (!ddjvu_page_decoding_done(page)) {
        gDjVuContext->SpinMessageLoop();
    }
    if (ddjvu_page_decoding_error(page)) {
        return nullptr;
    }

    bool isBitonal = DDJVU_PAGETYPE_BITONAL == ddjvu_page_get_type(page);
    ddjvu_format_style_t style = isBitonal ? DDJVU_FORMAT_GREY8 : DDJVU_FORMAT_BGR24;
    ddjvu_format_t* fmt = ddjvu_format_create(style, 0, nullptr);

    defer {
        ddjvu_format_release(fmt);
        ddjvu_page_release(page);
    };

    int topToBottom = TRUE;
    ddjvu_format_set_row_order(fmt, topToBottom);
    ddjvu_rect_t prect = {full.x, full.y, full.dx, full.dy};
    ddjvu_rect_t rrect = {screen.x, 2 * full.y - screen.y + full.dy - screen.dy, screen.dx, screen.dy};

    RenderedBitmap* bmp = nullptr;
    size_t bytesPerPixel = isBitonal ? 1 : 3;
    size_t dx = (size_t)screen.dx;
    size_t dy = (size_t)screen.dy;
    size_t stride = ((dx * bytesPerPixel + 3) / 4) * 4;
    size_t nBytes = stride * (dy + 5);
    AutoFree bmpData = AllocArray<char>(nBytes);
    if (!bmpData) {
        return nullptr;
    }

    ddjvu_render_mode_t mode = isBitonal ? DDJVU_RENDER_MASKONLY : DDJVU_RENDER_COLOR;
    int ok = ddjvu_page_render(page, mode, &prect, &rrect, fmt, (unsigned long)stride, bmpData.Get());
    if (!ok) {
        // nothing was rendered, leave the page blank (same as WinDjView)
        memset(bmpData, 0xFF, stride * dy);
        isBitonal = true;
    }
    bmp = CreateRenderedBitmap(bmpData, screen.Size(), isBitonal);
    AddUserAnnots(bmp, pageNo, zoom, rotation, screen);

    return bmp;
}

RectD EngineDjVu::PageContentBox(int pageNo, RenderTarget target) {
    UNUSED(target);
    ScopedCritSec scope(&gDjVuContext->lock);

    RectD pageRc = PageMediabox(pageNo);
    ddjvu_page_t* page = ddjvu_page_create_by_pageno(doc, pageNo - 1);
    if (!page) {
        return pageRc;
    }
    ddjvu_page_set_rotation(page, DDJVU_ROTATE_0);

    while (!ddjvu_page_decoding_done(page)) {
        gDjVuContext->SpinMessageLoop();
    }
    if (ddjvu_page_decoding_error(page)) {
        return pageRc;
    }

    // render the page in 8-bit grayscale up to 250x250 px in size
    ddjvu_format_t* fmt = ddjvu_format_create(DDJVU_FORMAT_GREY8, 0, nullptr);

    defer {
        ddjvu_format_release(fmt);
        ddjvu_page_release(page);
    };

    ddjvu_format_set_row_order(fmt, /* top_to_bottom */ TRUE);
    double zoom = std::min(std::min(250.0 / pageRc.dx, 250.0 / pageRc.dy), 1.0);
    RectI full = RectD(0, 0, pageRc.dx * zoom, pageRc.dy * zoom).Round();
    ddjvu_rect_t prect = {full.x, full.y, full.dx, full.dy}, rrect = prect;

    AutoFree bmpData = AllocArray<char>(full.dx * full.dy + 1);
    if (!bmpData) {
        return pageRc;
    }

    int ok = ddjvu_page_render(page, DDJVU_RENDER_MASKONLY, &prect, &rrect, fmt, full.dx, bmpData.Get());
    if (!ok) {
        return pageRc;
    }

    // determine the content box by counting white pixels from the edges
    RectD content(full.dx, -1, 0, 0);
    for (int y = 0; y < full.dy; y++) {
        int x;
        for (x = 0; x < full.dx && bmpData[y * full.dx + x] == '\xFF'; x++) {
            // no-op
        }
        if (x < full.dx) {
            // narrow the left margin down (if necessary)
            if (x < content.x) {
                content.x = x;
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
                content.y = y;
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
        pageRc = content.Round().Convert<double>();
    }

    return pageRc;
}

PointD EngineDjVu::TransformPoint(PointD pt, int pageNo, float zoom, int rotation, bool inverse) {
    CrashIf(zoom <= 0);
    if (zoom <= 0) {
        return pt;
    }

    SizeD page = PageMediabox(pageNo).Size();

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

    rotation = rotation % 360;
    while (rotation < 0) {
        rotation += 360;
    }
    PointD res = pt; // for rotation == 0
    if (90 == rotation) {
        res = PointD(page.dy - pt.y, pt.x);
    } else if (180 == rotation) {
        res = PointD(page.dx - pt.x, page.dy - pt.y);
    } else if (270 == rotation) {
        res = PointD(pt.y, page.dx - pt.x);
    }
    res.x *= zoom;
    res.y *= zoom;
    return res;
}

RectD EngineDjVu::Transform(RectD rect, int pageNo, float zoom, int rotation, bool inverse) {
    PointD TL = TransformPoint(rect.TL(), pageNo, zoom, rotation, inverse);
    PointD BR = TransformPoint(rect.BR(), pageNo, zoom, rotation, inverse);
    return RectD::FromXY(TL, BR);
}

std::string_view EngineDjVu::GetFileData() {
    return GetStreamOrFileData(stream, FileName());
}

bool EngineDjVu::SaveFileAs(const char* copyFileName, bool includeUserAnnots) {
    UNUSED(includeUserAnnots);
    AutoFreeWstr path = strconv::Utf8ToWstr(copyFileName);
    if (stream) {
        AutoFree d = GetDataFromStream(stream, nullptr);
        bool ok = !d.empty() && file::WriteFile(path, d.as_view());
        if (ok) {
            return true;
        }
    }
    const WCHAR* fileName = FileName();
    if (!fileName) {
        return false;
    }
    return CopyFile(fileName, path, FALSE);
}

static void AppendNewline(str::WStr& extracted, Vec<RectI>& coords, const WCHAR* lineSep) {
    if (extracted.size() > 0 && ' ' == extracted.Last()) {
        extracted.Pop();
        coords.Pop();
    }
    extracted.Append(lineSep);
    coords.AppendBlanks(str::Len(lineSep));
}

bool EngineDjVu::ExtractPageText(miniexp_t item, str::WStr& extracted, Vec<RectI>& coords) {
    WCHAR* lineSep = L"\n";
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
    RectI rect = RectI::FromXY(x0, y0, x1, y1);

    miniexp_t str = miniexp_car(item);
    if (miniexp_stringp(str) && !miniexp_cdr(item)) {
        if (type != miniexp_symbol("char") && type != miniexp_symbol("word") ||
            coords.size() > 0 && rect.y < coords.Last().y - coords.Last().dy * 0.8) {
            AppendNewline(extracted, coords, lineSep);
        }
        const char* content = miniexp_to_str(str);
        WCHAR* value = strconv::Utf8ToWstr(content);
        if (value) {
            size_t len = str::Len(value);
            // TODO: split the rectangle into individual parts per glyph
            for (size_t i = 0; i < len; i++) {
                coords.Append(RectI(rect.x, rect.y, rect.dx, rect.dy));
            }
            extracted.AppendAndFree(value);
        }
        if (miniexp_symbol("word") == type) {
            extracted.Append(' ');
            coords.Append(RectI(rect.x + rect.dx, rect.y, 2, rect.dy));
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

WCHAR* EngineDjVu::ExtractPageText(int pageNo, RectI** coordsOut) {
    const WCHAR* lineSep = L"\n";
    ScopedCritSec scope(&gDjVuContext->lock);

    miniexp_t pagetext;
    while ((pagetext = ddjvu_document_get_pagetext(doc, pageNo - 1, nullptr)) == miniexp_dummy) {
        gDjVuContext->SpinMessageLoop();
    }
    if (miniexp_nil == pagetext) {
        return nullptr;
    }

    str::WStr extracted;
    Vec<RectI> coords;
    bool success = ExtractPageText(pagetext, extracted, coords);
    ddjvu_miniexp_release(doc, pagetext);
    if (!success) {
        return nullptr;
    }
    if (extracted.size() > 0 && !str::EndsWith(extracted.Get(), lineSep)) {
        AppendNewline(extracted, coords, lineSep);
    }

    CrashIf(str::Len(extracted.Get()) != coords.size());
    if (coordsOut) {
        ddjvu_status_t status;
        ddjvu_pageinfo_t info;
        while ((status = ddjvu_document_get_pageinfo(doc, pageNo - 1, &info)) < DDJVU_JOB_OK) {
            gDjVuContext->SpinMessageLoop();
        }
        float dpiFactor = 1.0;
        if (DDJVU_JOB_OK == status)
            dpiFactor = GetFileDPI() / info.dpi;

        // TODO: the coordinates aren't completely correct yet
        RectI page = PageMediabox(pageNo).Round();
        for (size_t i = 0; i < coords.size(); i++) {
            if (coords.at(i) != RectI()) {
                if (dpiFactor != 1.0) {
                    geomutil::RectT<float> pageF = coords.at(i).Convert<float>();
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
        *coordsOut = coords.StealData();
    }

    return extracted.StealData();
}

void EngineDjVu::UpdateUserAnnotations(Vec<PageAnnotation>* list) {
    ScopedCritSec scope(&gDjVuContext->lock);
    if (list) {
        userAnnots = *list;
    } else {
        userAnnots.Reset();
    }
}

Vec<PageElement*>* EngineDjVu::GetElements(int pageNo) {
    CrashIf(pageNo < 1 || pageNo > PageCount());
    if (annos && miniexp_dummy == annos[pageNo - 1]) {
        ScopedCritSec scope(&gDjVuContext->lock);
        while ((annos[pageNo - 1] = ddjvu_document_get_pageanno(doc, pageNo - 1)) == miniexp_dummy) {
            gDjVuContext->SpinMessageLoop();
        }
    }
    if (!annos || !annos[pageNo - 1]) {
        return nullptr;
    }

    ScopedCritSec scope(&gDjVuContext->lock);

    Vec<PageElement*>* els = new Vec<PageElement*>();
    RectI page = PageMediabox(pageNo).Round();

    ddjvu_status_t status;
    ddjvu_pageinfo_t info;
    while ((status = ddjvu_document_get_pageinfo(doc, pageNo - 1, &info)) < DDJVU_JOB_OK) {
        gDjVuContext->SpinMessageLoop();
    }
    float dpiFactor = 1.0;
    if (DDJVU_JOB_OK == status) {
        dpiFactor = GetFileDPI() / info.dpi;
    }

    miniexp_t* links = ddjvu_anno_get_hyperlinks(annos[pageNo - 1]);
    for (int i = 0; links[i]; i++) {
        miniexp_t anno = miniexp_cdr(links[i]);

        miniexp_t url = miniexp_car(anno);
        const char* urlUtf8 = nullptr;
        if (miniexp_stringp(url)) {
            urlUtf8 = miniexp_to_str(url);
        } else if (miniexp_consp(url) && miniexp_car(url) == miniexp_symbol("url") &&
                   miniexp_stringp(miniexp_cadr(url)) && miniexp_stringp(miniexp_caddr(url))) {
            urlUtf8 = miniexp_to_str(miniexp_cadr(url));
        }
        if (!urlUtf8) {
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
        if (!miniexp_numberp(miniexp_car(area)))
            continue;
        int x = miniexp_to_int(miniexp_car(area));
        area = miniexp_cdr(area);
        if (!miniexp_numberp(miniexp_car(area)))
            continue;
        int y = miniexp_to_int(miniexp_car(area));
        area = miniexp_cdr(area);
        if (!miniexp_numberp(miniexp_car(area)))
            continue;
        int w = miniexp_to_int(miniexp_car(area));
        area = miniexp_cdr(area);
        if (!miniexp_numberp(miniexp_car(area)))
            continue;
        int h = miniexp_to_int(miniexp_car(area));
        area = miniexp_cdr(area);
        if (dpiFactor != 1.0) {
            x = (int)(x * dpiFactor);
            w = (int)(w * dpiFactor);
            y = (int)(y * dpiFactor);
            h = (int)(h * dpiFactor);
        }
        RectI rect(x, page.dy - y - h, w, h);

        AutoFree link = ResolveNamedDest(urlUtf8);
        const char* tmp = link.get();
        if (!tmp) {
            tmp = urlUtf8;
        }
        auto el = newDjVuLink(pageNo, rect, tmp, commentUtf8);
        els->Append(el);
    }
    ddjvu_free(links);

    return els;
}

PageElement* EngineDjVu::GetElementAtPos(int pageNo, PointD pt) {
    Vec<PageElement*>* els = GetElements(pageNo);
    if (!els) {
        return nullptr;
    }

    // elements are extracted bottom-to-top but are accessed
    // in top-to-bottom order, so reverse the list first
    els->Reverse();

    PageElement* el = nullptr;
    for (size_t i = 0; i < els->size() && !el; i++) {
        if (els->at(i)->GetRect().Contains(pt)) {
            el = els->at(i);
        }
    }

    if (el) {
        els->Remove(el);
    }
    DeleteVecMembers(*els);
    delete els;

    return el;
}

// returns a numeric DjVu link to a named page (if the name resolves)
// caller needs to free() the result
char* EngineDjVu::ResolveNamedDest(const char* name) {
    if (!str::StartsWith(name, "#")) {
        return nullptr;
    }
    for (size_t i = 0; i < fileInfos.size(); i++) {
        ddjvu_fileinfo_t& fi = fileInfos[i];
        if (str::EqI(name + 1, fi.id)) {
            return str::Format("#%d", fi.pageno + 1);
        }
    }
    return nullptr;
}

PageDestination* EngineDjVu::GetNamedDest(const WCHAR* name) {
    AutoFree nameUtf8 = strconv::WstrToUtf8(name);
    if (!str::StartsWith(nameUtf8.Get(), "#")) {
        nameUtf8.TakeOwnershipOf(str::Join("#", nameUtf8.Get()));
    }

    AutoFree link = ResolveNamedDest(nameUtf8.Get());
    if (link) {
        return newDjVuDestination(link);
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
        AutoFree linkNo = ResolveNamedDest(link);
        if (!linkNo) {
            tocItem = newDjVuTocItem(parent, name, link);
        } else if (!str::IsEmpty(name) && !str::Eq(name, link + 1)) {
            tocItem = newDjVuTocItem(parent, name, linkNo);
        } else {
            // ignore generic (name-less) entries
            auto* tocTree = BuildTocTree(nullptr, miniexp_cddr(item), idCounter);
            delete tocTree;
            continue;
        }

        tocItem->id = ++idCounter;
        tocItem->child = BuildTocTree(tocItem, miniexp_cddr(item), idCounter);

        if (!node) {
            node = tocItem;
        } else {
            node->AddSibling(tocItem);
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
    tocTree = new TocTree(root);
    return tocTree;
}

WCHAR* EngineDjVu::GetPageLabel(int pageNo) const {
    for (size_t i = 0; i < fileInfos.size(); i++) {
        ddjvu_fileinfo_t& info = fileInfos.at(i);
        if (pageNo - 1 == info.pageno && !str::Eq(info.title, info.id)) {
            return strconv::Utf8ToWstr(info.title);
        }
    }
    return EngineBase::GetPageLabel(pageNo);
}

int EngineDjVu::GetPageByLabel(const WCHAR* label) const {
    AutoFree labelUtf8(strconv::WstrToUtf8(label));
    for (size_t i = 0; i < fileInfos.size(); i++) {
        ddjvu_fileinfo_t& info = fileInfos.at(i);
        if (str::EqI(info.title, labelUtf8.Get()) && !str::Eq(info.title, info.id)) {
            return info.pageno + 1;
        }
    }
    return EngineBase::GetPageByLabel(label);
}

EngineBase* EngineDjVu::CreateFromFile(const WCHAR* fileName) {
    EngineDjVu* engine = new EngineDjVu();
    if (!engine->Load(fileName)) {
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

bool IsDjVuEngineSupportedFile(const WCHAR* fileName, bool sniff) {
    if (sniff) {
        return file::StartsWith(fileName, "AT&T");
    }

    return str::EndsWithI(fileName, L".djvu");
}

EngineBase* CreateDjVuEngineFromFile(const WCHAR* fileName) {
    return EngineDjVu::CreateFromFile(fileName);
}

EngineBase* CreateDjVuEngineFromStream(IStream* stream) {
    return EngineDjVu::CreateFromStream(stream);
}
