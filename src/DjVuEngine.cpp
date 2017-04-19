/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// hack to prevent libdjvu from being built as an export/import library
#define DDJVUAPI /**/
#define MINILISPAPI /**/

// utils
#include "BaseUtil.h"
#include <ddjvuapi.h>
#include <miniexp.h>
#include "ByteReader.h"
#include "FileUtil.h"
#include "WinUtil.h"
// rendering engines
#include "BaseEngine.h"
#include "DjVuEngine.h"

// TODO: libdjvu leaks memory - among others
//       DjVuPort::corpse_lock, DjVuPort::corpse_head, pcaster,
//       DataPool::OpenFiles::global_ptr, FCPools::global_ptr
//       cf. http://sourceforge.net/projects/djvu/forums/forum/103286/topic/3553602

class DjVuDestination : public PageDestination {
    // the link format can be any of
    //   #[ ]<pageNo>      e.g. #1 for FirstPage and # 13 for page 13
    //   #[+-]<pageCount>  e.g. #+1 for NextPage and #-1 for PrevPage
    //   #filename.djvu    use ResolveNamedDest to get a link in #<pageNo> format
    //   http://example.net/#hyperlink
    ScopedMem<char> link;

    bool IsPageLink(const char *link) const {
        return link && link[0] == '#' && (str::IsDigit(link[1]) || link[1] == ' ' && str::IsDigit(link[2]));
    }

public:
    explicit DjVuDestination(const char *link) : link(str::Dup(link)) { }

    PageDestType GetDestType() const override {
        if (IsPageLink(link))
            return Dest_ScrollTo;
        if (str::Eq(link, "#+1"))
            return Dest_NextPage;
        if (str::Eq(link, "#-1"))
            return Dest_PrevPage;
        if (str::StartsWithI(link, "http:") || str::StartsWithI(link, "https:") || str::StartsWithI(link, "mailto:"))
            return Dest_LaunchURL;
        return Dest_None;
    }
    int GetDestPageNo() const override {
        if (IsPageLink(link))
            return atoi(link + 1);
        return 0;
    }
    RectD GetDestRect() const override {
        return RectD(DEST_USE_DEFAULT, DEST_USE_DEFAULT, DEST_USE_DEFAULT, DEST_USE_DEFAULT);
    }
    WCHAR *GetDestValue() const override {
        if (Dest_LaunchURL == GetDestType())
            return str::conv::FromUtf8(link);
        return nullptr;
    }
};

class DjVuLink : public PageElement {
    DjVuDestination *dest;
    int pageNo;
    RectD rect;
    WCHAR *value;

public:
    DjVuLink(int pageNo, RectI rect, const char *link, const char *comment) :
        pageNo(pageNo), rect(rect.Convert<double>()), value(nullptr) {
        dest = new DjVuDestination(link);
        if (!str::IsEmpty(comment))
            value = str::conv::FromUtf8(comment);
    }
    ~DjVuLink()  override {
        delete dest;
        free(value);
    }

    PageElementType GetType() const  override { return Element_Link; }
    int GetPageNo() const  override { return pageNo; }
    RectD GetRect() const override { return rect; }
    WCHAR *GetValue() const override {
        if (value)
            return str::Dup(value);
        if (Dest_LaunchURL == dest->GetDestType())
            return dest->GetDestValue();
        return nullptr;
    }

    virtual PageDestination *AsLink() { return dest; }
};

class DjVuTocItem : public DocTocItem {
    DjVuDestination *dest;

public:
    DjVuTocItem(const char *title, const char *link) :
        DocTocItem(str::conv::FromUtf8(title)) {
        dest = new DjVuDestination(link);
        pageNo = dest->GetDestPageNo();
    }
    virtual ~DjVuTocItem() { delete dest; }

    virtual PageDestination *GetLink() { return dest; }
};

class DjVuContext {
    bool initialized;
    ddjvu_context_t *ctx;

public:
    CRITICAL_SECTION lock;

    DjVuContext() : ctx(nullptr), initialized(false) { }
    ~DjVuContext() {
        if (initialized) {
            EnterCriticalSection(&lock);
            if (ctx)
                ddjvu_context_release(ctx);
            LeaveCriticalSection(&lock);
            DeleteCriticalSection(&lock);
        }
        minilisp_finish();
    }

    bool Initialize() {
        if (!initialized) {
            initialized = true;
            InitializeCriticalSection(&lock);
            ctx = ddjvu_context_create("DjVuEngine");
            // reset the locale to "C" as most other code expects
            setlocale(LC_ALL, "C");
        }

        return ctx != nullptr;
    }

    void SpinMessageLoop(bool wait=true) {
        UNUSED(wait);
        const ddjvu_message_t *msg;
#if THREADMODEL!=NOTHREADS
        if (wait)
            ddjvu_message_wait(ctx);
#endif
        while ((msg = ddjvu_message_peek(ctx)) != nullptr) {
            if (DDJVU_NEWSTREAM == msg->m_any.tag && msg->m_newstream.streamid != 0)
                ddjvu_stream_close(msg->m_any.document, msg->m_newstream.streamid, /* stop */ FALSE);
            ddjvu_message_pop(ctx);
        }
    }

    ddjvu_document_t *OpenFile(const WCHAR *fileName) {
        ScopedCritSec scope(&lock);
        ScopedMem<char> fileNameUtf8(str::conv::ToUtf8(fileName));
        // TODO: libdjvu sooner or later crashes inside its caching code; cf.
        //       http://code.google.com/p/sumatrapdf/issues/detail?id=1434
        return ddjvu_document_create_by_filename_utf8(ctx, fileNameUtf8, /* cache */ FALSE);
    }

    ddjvu_document_t *OpenStream(IStream *stream) {
        ScopedCritSec scope(&lock);
        size_t datalen;
        ScopedMem<char> data((char *)GetDataFromStream(stream, &datalen));
        if (!data || datalen > ULONG_MAX)
            return nullptr;
        return ddjvu_document_create_by_data(ctx, data, (ULONG)datalen);
    }
};

static DjVuContext gDjVuContext;

class DjVuEngineImpl : public BaseEngine {
public:
    DjVuEngineImpl() { };
    virtual ~DjVuEngineImpl();
    BaseEngine *Clone() override {
        if (stream != nullptr) {
            return CreateFromStream(stream);
        }
        if (FileName() != nullptr) {
            return CreateFromFile(FileName());
        }
        return nullptr;
    }

    int PageCount() const override { return pageCount; }

    RectD PageMediabox(int pageNo) override {
        assert(1 <= pageNo && pageNo <= PageCount());
        return mediaboxes[pageNo-1];
    }
    RectD PageContentBox(int pageNo, RenderTarget target=Target_View) override;

    RenderedBitmap *RenderBitmap(int pageNo, float zoom, int rotation,
                         RectD *pageRect=nullptr, /* if nullptr: defaults to the page's mediabox */
                         RenderTarget target=Target_View, AbortCookie **cookie_out=nullptr) override;

    PointD Transform(PointD pt, int pageNo, float zoom, int rotation, bool inverse=false) override;
    RectD Transform(RectD rect, int pageNo, float zoom, int rotation, bool inverse=false) override;

    unsigned char *GetFileData(size_t *cbCount) override;
    bool SaveFileAs(const WCHAR *copyFileName, bool includeUserAnnots=false) override;
    WCHAR * ExtractPageText(int pageNo, const WCHAR *lineSep, RectI **coordsOut=nullptr,
                                    RenderTarget target=Target_View) override;
    bool HasClipOptimizations(int pageNo) override { UNUSED(pageNo);  return false; }
    PageLayoutType PreferredLayout() override { return Layout_Single; }

    WCHAR *GetProperty(DocumentProperty prop) override { UNUSED(prop);  return nullptr; }

    bool SupportsAnnotation(bool forSaving=false) const override { return !forSaving; }
    void UpdateUserAnnotations(Vec<PageAnnotation> *list) override;

    // DPI isn't constant for all pages and thus premultiplied
    float GetFileDPI() const override { return 300.0f; }
    const WCHAR *GetDefaultFileExt() const override { return L".djvu"; }

    // we currently don't load pages lazily, so there's nothing to do here
    bool BenchLoadPage(int pageNo) override { UNUSED(pageNo); return true; }

    Vec<PageElement *> *GetElements(int pageNo) override;
    PageElement *GetElementAtPos(int pageNo, PointD pt) override;

    PageDestination *GetNamedDest(const WCHAR *name) override;
    bool HasTocTree() const override { return outline != miniexp_nil; }
    DocTocItem *GetTocTree() override;

    bool HasPageLabels() const override { return hasPageLabels; }
    WCHAR *GetPageLabel(int pageNo) const override;
    int GetPageByLabel(const WCHAR *label) const override;

    static BaseEngine *CreateFromFile(const WCHAR *fileName);
    static BaseEngine *CreateFromStream(IStream *stream);

protected:
    IStream *stream = nullptr;

    int pageCount = 0;
    RectD *mediaboxes = nullptr;

    ddjvu_document_t *doc = nullptr;
    miniexp_t outline = miniexp_nil;
    miniexp_t *annos = nullptr;
    Vec<PageAnnotation> userAnnots;
    bool hasPageLabels = false;

    Vec<ddjvu_fileinfo_t> fileInfo;

    RenderedBitmap *CreateRenderedBitmap(const char *bmpData, SizeI size, bool grayscale) const;
    void AddUserAnnots(RenderedBitmap *bmp, int pageNo, float zoom, int rotation, RectI screen);
    bool ExtractPageText(miniexp_t item, const WCHAR *lineSep,
                         str::Str<WCHAR>& extracted, Vec<RectI>& coords);
    char *ResolveNamedDest(const char *name);
    DjVuTocItem *BuildTocTree(miniexp_t entry, int& idCounter);
    bool Load(const WCHAR *fileName);
    bool Load(IStream *stream);
    bool FinishLoading();
    bool LoadMediaboxes();
};

DjVuEngineImpl::~DjVuEngineImpl()
{
    ScopedCritSec scope(&gDjVuContext.lock);

    free(mediaboxes);

    if (annos) {
        for (int i = 0; i < pageCount; i++) {
            if (annos[i])
                ddjvu_miniexp_release(doc, annos[i]);
        }
        free(annos);
    }
    if (outline != miniexp_nil)
        ddjvu_miniexp_release(doc, outline);
    if (doc)
        ddjvu_document_release(doc);
    if (stream)
        stream->Release();
}

// Most functions of the ddjvu API such as ddjvu_document_get_pageinfo
// are quite inefficient when used for all pages of a document in a row,
// so try to either only use them when actually needed or replace them
// with a function that extracts all the data at once:

static bool ReadBytes(HANDLE h, DWORD offset, void *buffer, DWORD count)
{
    DWORD res = SetFilePointer(h, offset, nullptr, FILE_BEGIN);
    if (res != offset)
        return false;
    bool ok = ReadFile(h, buffer, count, &res, nullptr);
    return ok && res == count;
}

#define DJVU_MARK_MAGIC 0x41542654L /* AT&T */
#define DJVU_MARK_FORM  0x464F524DL /* FORM */
#define DJVU_MARK_DJVM  0x444A564DL /* DJVM */
#define DJVU_MARK_DJVU  0x444A5655L /* DJVU */
#define DJVU_MARK_INFO  0x494E464FL /* INFO */

#include <pshpack1.h>

struct DjVuInfoChunk {
    WORD width, height;
    BYTE minor, major;
    BYTE dpiLo, dpiHi;
    BYTE gamma, flags;
};

#include <poppack.h>

static_assert(sizeof(DjVuInfoChunk) == 10, "wrong size of DjVuInfoChunk structure");

bool DjVuEngineImpl::LoadMediaboxes()
{
    if (!fileName)
        return false;
    ScopedHandle h(file::OpenReadOnly(fileName));
    if (h == INVALID_HANDLE_VALUE)
        return false;
    char buffer[16];
    ByteReader r(buffer, sizeof(buffer));
    if (!ReadBytes(h, 0, buffer, 16) || r.DWordBE(0) != DJVU_MARK_MAGIC || r.DWordBE(4) != DJVU_MARK_FORM)
        return false;

    DWORD offset = r.DWordBE(12) == DJVU_MARK_DJVM ? 16 : 4;
    for (int pages = 0; pages < pageCount; ) {
        if (!ReadBytes(h, offset, buffer, 16))
            return false;
        int partLen = r.DWordBE(4);
        if (partLen < 0)
            return false;
        if (r.DWordBE(0) == DJVU_MARK_FORM && r.DWordBE(8) == DJVU_MARK_DJVU &&
            r.DWordBE(12) == DJVU_MARK_INFO) {
            if (!ReadBytes(h, offset + 16, buffer, 14))
                return false;
            DjVuInfoChunk info;
            bool ok = r.UnpackBE(&info, sizeof(info), "2w6b", 4);
            CrashIf(!ok);
            int dpi = MAKEWORD(info.dpiLo, info.dpiHi); // dpi is little-endian
            // DjVuLibre ignores DPI values outside 25 to 6000 in DjVuInfo::decode
            if (dpi < 25 || 6000 < dpi)
                dpi = 300;
            mediaboxes[pages].dx = GetFileDPI() * info.width / dpi;
            mediaboxes[pages].dy = GetFileDPI() * info.height / dpi;
            if ((info.flags & 4))
                std::swap(mediaboxes[pages].dx, mediaboxes[pages].dy);
            pages++;
        }
        offset += 8 + partLen + (partLen & 1);
    }

    return true;
}

bool DjVuEngineImpl::Load(const WCHAR *fileName)
{
    if (!gDjVuContext.Initialize())
        return false;

    SetFileName(fileName);
    doc = gDjVuContext.OpenFile(fileName);

    return FinishLoading();
}

bool DjVuEngineImpl::Load(IStream *stream)
{
    if (!gDjVuContext.Initialize())
        return false;

    doc = gDjVuContext.OpenStream(stream);

    return FinishLoading();
}

bool DjVuEngineImpl::FinishLoading()
{
    if (!doc)
        return false;

    ScopedCritSec scope(&gDjVuContext.lock);

    while (!ddjvu_document_decoding_done(doc))
        gDjVuContext.SpinMessageLoop();
    if (ddjvu_document_decoding_error(doc))
        return false;

    pageCount = ddjvu_document_get_pagenum(doc);
    if (0 == pageCount)
        return false;

    mediaboxes = AllocArray<RectD>(pageCount);
    bool ok = LoadMediaboxes();
    if (!ok) {
        // fall back to the slower but safer way to extract page mediaboxes
        for (int i = 0; i < pageCount; i++) {
            ddjvu_status_t status;
            ddjvu_pageinfo_t info;
            while ((status = ddjvu_document_get_pageinfo(doc, i, &info)) < DDJVU_JOB_OK)
                gDjVuContext.SpinMessageLoop();
            if (DDJVU_JOB_OK == status)
                mediaboxes[i] = RectD(0, 0, info.width * GetFileDPI() / info.dpi,
                                            info.height * GetFileDPI() / info.dpi);
        }
    }

    annos = AllocArray<miniexp_t>(pageCount);
    for (int i = 0; i < pageCount; i++)
        annos[i] = miniexp_dummy;

    while ((outline = ddjvu_document_get_outline(doc)) == miniexp_dummy)
        gDjVuContext.SpinMessageLoop();
    if (!miniexp_consp(outline) || miniexp_car(outline) != miniexp_symbol("bookmarks")) {
        ddjvu_miniexp_release(doc, outline);
        outline = miniexp_nil;
    }

    int fileCount = ddjvu_document_get_filenum(doc);
    for (int i = 0; i < fileCount; i++) {
        ddjvu_status_t status;
        ddjvu_fileinfo_s info;
        while ((status = ddjvu_document_get_fileinfo(doc, i, &info)) < DDJVU_JOB_OK)
            gDjVuContext.SpinMessageLoop();
        if (DDJVU_JOB_OK == status && info.type == 'P' && info.pageno >= 0) {
            fileInfo.Append(info);
            hasPageLabels = hasPageLabels || !str::Eq(info.title, info.id);
        }
    }

    return true;
}

// TODO: use AdjustLightness instead to compensate for the alpha?
static Gdiplus::Color Unblend(PageAnnotation::Color c, BYTE alpha)
{
    alpha = (BYTE)(alpha * c.a / 255.f);
    BYTE R = (BYTE)floorf(std::max(c.r - (255 - alpha), 0) * 255.0f / alpha + 0.5f);
    BYTE G = (BYTE)floorf(std::max(c.g - (255 - alpha), 0) * 255.0f / alpha + 0.5f);
    BYTE B = (BYTE)floorf(std::max(c.b - (255 - alpha), 0) * 255.0f / alpha + 0.5f);
    return Gdiplus::Color(alpha, R, G, B);
}

static inline Gdiplus::Color FromColor(PageAnnotation::Color c)
{
    return Gdiplus::Color(c.a, c.r, c.g, c.b);
}

void DjVuEngineImpl::AddUserAnnots(RenderedBitmap *bmp, int pageNo, float zoom, int rotation, RectI screen)
{
    using namespace Gdiplus;

    if (!bmp || userAnnots.Count() == 0)
        return;

    HDC hdc = CreateCompatibleDC(nullptr);
    {
        ScopedHdcSelect bmpScope(hdc, bmp->GetBitmap());
        Graphics g(hdc);
        g.SetCompositingQuality(CompositingQualityHighQuality);
        g.SetPageUnit(UnitPixel);

        for (size_t i = 0; i < userAnnots.Count(); i++) {
            PageAnnotation& annot = userAnnots.At(i);
            if (annot.pageNo != pageNo)
                continue;
            RectD arect;
            switch (annot.type) {
            case Annot_Highlight:
                arect = Transform(annot.rect, pageNo, zoom, rotation);
                arect.Offset(-screen.x, -screen.y);
                {
                SolidBrush tmpBrush(Unblend(annot.color, 119));
                g.FillRectangle(&tmpBrush, arect.ToGdipRectF());
                }
                break;
            case Annot_Underline:
            case Annot_StrikeOut:
                arect = RectD(annot.rect.x, annot.rect.BR().y, annot.rect.dx, 0);
                if (Annot_StrikeOut == annot.type)
                    arect.y -= annot.rect.dy / 2;
                arect = Transform(arect, pageNo, zoom, rotation);
                arect.Offset(-screen.x, -screen.y);
                {
                Pen tmpPen(FromColor(annot.color), zoom);
                g.DrawLine(&tmpPen, (float)arect.x,
                           (float)arect.y, (float)arect.BR().x, (float)arect.BR().y);
                }
                break;
            case Annot_Squiggly:
                {
                    Pen p(FromColor(annot.color), 0.5f * zoom);
                    REAL dash[2] = { 2, 2 };
                    p.SetDashPattern(dash, dimof(dash));
                    p.SetDashOffset(1);
                    arect = Transform(RectD(annot.rect.x, annot.rect.BR().y - 0.25f, annot.rect.dx, 0), pageNo, zoom, rotation);
                    arect.Offset(-screen.x, -screen.y);
                    g.DrawLine(&p, (float)arect.x, (float)arect.y, (float)arect.BR().x, (float)arect.BR().y);
                    p.SetDashOffset(3);
                    arect = Transform(RectD(annot.rect.x, annot.rect.BR().y + 0.25f, annot.rect.dx, 0), pageNo, zoom, rotation);
                    arect.Offset(-screen.x, -screen.y);
                    g.DrawLine(&p, (float)arect.x, (float)arect.y, (float)arect.BR().x, (float)arect.BR().y);
                }
                break;
            }
        }
    }
    DeleteDC(hdc);
}

RenderedBitmap *DjVuEngineImpl::CreateRenderedBitmap(const char *bmpData, SizeI size, bool grayscale) const
{
    int stride = ((size.dx * (grayscale ? 1 : 3) + 3) / 4) * 4;

    BITMAPINFO *bmi = (BITMAPINFO *)calloc(1, sizeof(BITMAPINFOHEADER) + (grayscale ? 256 * sizeof(RGBQUAD) : 0));
    if (!bmi)
        return nullptr;

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

    void *data = nullptr;
    HANDLE hMap = CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, bmi->bmiHeader.biSizeImage, nullptr);
    HBITMAP hbmp = CreateDIBSection(nullptr, bmi, DIB_RGB_COLORS, &data, hMap, 0);
    if (hbmp)
        memcpy(data, bmpData, bmi->bmiHeader.biSizeImage);

    free(bmi);

    return new RenderedBitmap(hbmp, size, hMap);
}

RenderedBitmap *DjVuEngineImpl::RenderBitmap(int pageNo, float zoom, int rotation, RectD *pageRect, RenderTarget target, AbortCookie **cookieOut)
{
    UNUSED(cookieOut); UNUSED(target);

    ScopedCritSec scope(&gDjVuContext.lock);

    RectD pageRc = pageRect ? *pageRect : PageMediabox(pageNo);
    RectI screen = Transform(pageRc, pageNo, zoom, rotation).Round();
    RectI full = Transform(PageMediabox(pageNo), pageNo, zoom, rotation).Round();
    screen = full.Intersect(screen);

    ddjvu_page_t *page = ddjvu_page_create_by_pageno(doc, pageNo-1);
    if (!page)
        return nullptr;
    int rotation4 = (((-rotation / 90) % 4) + 4) % 4;
    ddjvu_page_set_rotation(page, (ddjvu_page_rotation_t)rotation4);

    while (!ddjvu_page_decoding_done(page))
        gDjVuContext.SpinMessageLoop();
    if (ddjvu_page_decoding_error(page))
        return nullptr;

    bool isBitonal = DDJVU_PAGETYPE_BITONAL == ddjvu_page_get_type(page);
    ddjvu_format_t *fmt = ddjvu_format_create(isBitonal ? DDJVU_FORMAT_GREY8 : DDJVU_FORMAT_BGR24, 0, nullptr);
    ddjvu_format_set_row_order(fmt, /* top_to_bottom */ TRUE);
    ddjvu_rect_t prect = { full.x, full.y, full.dx, full.dy };
    ddjvu_rect_t rrect = { screen.x, 2 * full.y - screen.y + full.dy - screen.dy, screen.dx, screen.dy };

    RenderedBitmap *bmp = nullptr;
    int stride = ((screen.dx * (isBitonal ? 1 : 3) + 3) / 4) * 4;
    ScopedMem<char> bmpData(AllocArray<char>(stride * (screen.dy + 5)));
    if (bmpData) {
#ifndef DEBUG
        ddjvu_render_mode_t mode = isBitonal ? DDJVU_RENDER_MASKONLY : DDJVU_RENDER_COLOR;
#else
        // TODO: there seems to be a heap corruption in IW44Image.cpp
        //       in debug builds when passing in DDJVU_RENDER_COLOR
        ddjvu_render_mode_t mode = DDJVU_RENDER_MASKONLY;
#endif
        if (!ddjvu_page_render(page, mode, &prect, &rrect, fmt, stride, bmpData.Get())) {
            // nothing was rendered, leave the page blank (same as WinDjView)
            memset(bmpData, 0xFF, stride * screen.dy);
            isBitonal = true;
        }
        bmp = CreateRenderedBitmap(bmpData, screen.Size(), isBitonal);
        AddUserAnnots(bmp, pageNo, zoom, rotation, screen);
    }

    ddjvu_format_release(fmt);
    ddjvu_page_release(page);

    return bmp;
}

RectD DjVuEngineImpl::PageContentBox(int pageNo, RenderTarget target)
{
    UNUSED(target);
    ScopedCritSec scope(&gDjVuContext.lock);

    RectD pageRc = PageMediabox(pageNo);
    ddjvu_page_t *page = ddjvu_page_create_by_pageno(doc, pageNo-1);
    if (!page)
        return pageRc;
    ddjvu_page_set_rotation(page, DDJVU_ROTATE_0);

    while (!ddjvu_page_decoding_done(page))
        gDjVuContext.SpinMessageLoop();
    if (ddjvu_page_decoding_error(page))
        return pageRc;

    // render the page in 8-bit grayscale up to 250x250 px in size
    ddjvu_format_t *fmt = ddjvu_format_create(DDJVU_FORMAT_GREY8, 0, nullptr);
    ddjvu_format_set_row_order(fmt, /* top_to_bottom */ TRUE);
    double zoom = std::min(std::min(250.0 / pageRc.dx, 250.0 / pageRc.dy), 1.0);
    RectI full = RectD(0, 0, pageRc.dx * zoom, pageRc.dy * zoom).Round();
    ddjvu_rect_t prect = { full.x, full.y, full.dx, full.dy }, rrect = prect;

    ScopedMem<char> bmpData(AllocArray<char>(full.dx * full.dy + 1));
    if (bmpData && ddjvu_page_render(page, DDJVU_RENDER_MASKONLY, &prect, &rrect, fmt, full.dx, bmpData.Get())) {
        // determine the content box by counting white pixels from the edges
        RectD content(full.dx, -1, 0, 0);
        for (int y = 0; y < full.dy; y++) {
            int x;
            for (x = 0; x < full.dx && bmpData[y * full.dx + x] == '\xFF'; x++);
            if (x < full.dx) {
                // narrow the left margin down (if necessary)
                if (x < content.x)
                    content.x = x;
                // narrow the right margin down (if necessary)
                for (x = full.dx - 1; x > content.x + content.dx && bmpData[y * full.dx + x] == '\xFF'; x--);
                if (x > content.x + content.dx)
                    content.dx = x - content.x + 1;
                // narrow either the top or the bottom margin down
                if (content.y == -1)
                    content.y = y;
                else
                    content.dy = y - content.y + 1;
            }
        }
        if (!content.IsEmpty()) {
            // undo the zoom and round generously
            content.x /= zoom; content.dx /= zoom;
            content.y /= zoom; content.dy /= zoom;
            pageRc = content.Round().Convert<double>();
        }
    }

    ddjvu_format_release(fmt);
    ddjvu_page_release(page);

    return pageRc;
}

PointD DjVuEngineImpl::Transform(PointD pt, int pageNo, float zoom, int rotation, bool inverse)
{
    assert(zoom > 0);
    if (zoom <= 0)
        return pt;

    SizeD page = PageMediabox(pageNo).Size();

    if (inverse) {
        // transform the page size to get a correct frame of reference
        page.dx *= zoom; page.dy *= zoom;
        if (rotation % 180 != 0)
            std::swap(page.dx, page.dy);
        // invert rotation and zoom
        rotation = -rotation;
        zoom = 1.0f / zoom;
    }

    PointD res;
    rotation = rotation % 360;
    if (rotation < 0) rotation += 360;
    if (90 == rotation)
        res = PointD(page.dy - pt.y, pt.x);
    else if (180 == rotation)
        res = PointD(page.dx - pt.x, page.dy - pt.y);
    else if (270 == rotation)
        res = PointD(pt.y, page.dx - pt.x);
    else // if (0 == rotation)
        res = pt;

    res.x *= zoom; res.y *= zoom;
    return res;
}

RectD DjVuEngineImpl::Transform(RectD rect, int pageNo, float zoom, int rotation, bool inverse)
{
    PointD TL = Transform(rect.TL(), pageNo, zoom, rotation, inverse);
    PointD BR = Transform(rect.BR(), pageNo, zoom, rotation, inverse);
    return RectD::FromXY(TL, BR);
}

unsigned char *DjVuEngineImpl::GetFileData(size_t *cbCount)
{
    if (stream != nullptr) {
        ScopedMem<void> data(GetDataFromStream(stream, cbCount));
        if (data != nullptr) {
            return (unsigned char *)data.StealData();
        }
    }
    if (FileName() == nullptr) {
        return nullptr;
    }
    return (unsigned char *)file::ReadAll(FileName(), cbCount);
}

bool DjVuEngineImpl::SaveFileAs(const WCHAR *copyFileName, bool includeUserAnnots)
{
    UNUSED(includeUserAnnots);
    if (stream) {
        size_t len;
        ScopedMem<void> data(GetDataFromStream(stream, &len));
        if (data && file::WriteAll(copyFileName, data, len))
            return true;
    }
    if (!fileName)
        return false;
    return CopyFile(fileName, copyFileName, FALSE);
}

static void AppendNewline(str::Str<WCHAR>& extracted, Vec<RectI>& coords, const WCHAR *lineSep)
{
    if (extracted.Count() > 0 && ' ' == extracted.Last()) {
        extracted.Pop();
        coords.Pop();
    }
    extracted.Append(lineSep);
    coords.AppendBlanks(str::Len(lineSep));
}

bool DjVuEngineImpl::ExtractPageText(miniexp_t item, const WCHAR *lineSep, str::Str<WCHAR>& extracted, Vec<RectI>& coords)
{
    miniexp_t type = miniexp_car(item);
    if (!miniexp_symbolp(type))
        return false;
    item = miniexp_cdr(item);

    if (!miniexp_numberp(miniexp_car(item))) return false;
    int x0 = miniexp_to_int(miniexp_car(item)); item = miniexp_cdr(item);
    if (!miniexp_numberp(miniexp_car(item))) return false;
    int y0 = miniexp_to_int(miniexp_car(item)); item = miniexp_cdr(item);
    if (!miniexp_numberp(miniexp_car(item))) return false;
    int x1 = miniexp_to_int(miniexp_car(item)); item = miniexp_cdr(item);
    if (!miniexp_numberp(miniexp_car(item))) return false;
    int y1 = miniexp_to_int(miniexp_car(item)); item = miniexp_cdr(item);
    RectI rect = RectI::FromXY(x0, y0, x1, y1);

    miniexp_t str = miniexp_car(item);
    if (miniexp_stringp(str) && !miniexp_cdr(item)) {
        if (type != miniexp_symbol("char") && type != miniexp_symbol("word") ||
            coords.Count() > 0 && rect.y < coords.Last().y - coords.Last().dy * 0.8) {
            AppendNewline(extracted, coords, lineSep);
        }
        const char *content = miniexp_to_str(str);
        WCHAR *value = str::conv::FromUtf8(content);
        if (value) {
            size_t len = str::Len(value);
            // TODO: split the rectangle into individual parts per glyph
            for (size_t i = 0; i < len; i++)
                coords.Append(RectI(rect.x, rect.y, rect.dx, rect.dy));
            extracted.AppendAndFree(value);
        }
        if (miniexp_symbol("word") == type) {
            extracted.Append(' ');
            coords.Append(RectI(rect.x + rect.dx, rect.y, 2, rect.dy));
        }
        item = miniexp_cdr(item);
    }
    while (miniexp_consp(str)) {
        ExtractPageText(str, lineSep, extracted, coords);
        item = miniexp_cdr(item);
        str = miniexp_car(item);
    }
    return !item;
}

WCHAR *DjVuEngineImpl::ExtractPageText(int pageNo, const WCHAR *lineSep, RectI **coordsOut, RenderTarget target)
{
    UNUSED(target);
    ScopedCritSec scope(&gDjVuContext.lock);

    miniexp_t pagetext;
    while ((pagetext = ddjvu_document_get_pagetext(doc, pageNo-1, nullptr)) == miniexp_dummy)
        gDjVuContext.SpinMessageLoop();
    if (miniexp_nil == pagetext)
        return nullptr;

    str::Str<WCHAR> extracted;
    Vec<RectI> coords;
    bool success = ExtractPageText(pagetext, lineSep, extracted, coords);
    ddjvu_miniexp_release(doc, pagetext);
    if (!success)
        return nullptr;
    if (extracted.Count() > 0 && !str::EndsWith(extracted.Get(), lineSep))
        AppendNewline(extracted, coords, lineSep);

    assert(str::Len(extracted.Get()) == coords.Count());
    if (coordsOut) {
        ddjvu_status_t status;
        ddjvu_pageinfo_t info;
        while ((status = ddjvu_document_get_pageinfo(doc, pageNo-1, &info)) < DDJVU_JOB_OK)
            gDjVuContext.SpinMessageLoop();
        float dpiFactor = 1.0;
        if (DDJVU_JOB_OK == status)
            dpiFactor = GetFileDPI() / info.dpi;

        // TODO: the coordinates aren't completely correct yet
        RectI page = PageMediabox(pageNo).Round();
        for (size_t i = 0; i < coords.Count(); i++) {
            if (coords.At(i) != RectI()) {
                if (dpiFactor != 1.0) {
                    geomutil::RectT<float> pageF = coords.At(i).Convert<float>();
                    pageF.x *= dpiFactor; pageF.dx *= dpiFactor;
                    pageF.y *= dpiFactor; pageF.dy *= dpiFactor;
                    coords.At(i) = pageF.Round();
                }
                coords.At(i).y = page.dy - coords.At(i).y - coords.At(i).dy;
            }
        }
        CrashIf(coords.Count() != extracted.Count());
        *coordsOut = coords.StealData();
    }

    return extracted.StealData();
}

void DjVuEngineImpl::UpdateUserAnnotations(Vec<PageAnnotation> *list)
{
    ScopedCritSec scope(&gDjVuContext.lock);
    if (list) {
        userAnnots = *list;
    } else {
        userAnnots.Reset();
    }
}

Vec<PageElement *> *DjVuEngineImpl::GetElements(int pageNo)
{
    AssertCrash(1 <= pageNo && pageNo <= PageCount());
    if (annos && miniexp_dummy == annos[pageNo-1]) {
        ScopedCritSec scope(&gDjVuContext.lock);
        while ((annos[pageNo - 1] = ddjvu_document_get_pageanno(doc, pageNo - 1)) == miniexp_dummy) {
            gDjVuContext.SpinMessageLoop();
        }
    }
    if (!annos || !annos[pageNo - 1]) {
        return nullptr;
    }

    ScopedCritSec scope(&gDjVuContext.lock);

    Vec<PageElement *> *els = new Vec<PageElement *>();
    RectI page = PageMediabox(pageNo).Round();

    ddjvu_status_t status;
    ddjvu_pageinfo_t info;
    while ((status = ddjvu_document_get_pageinfo(doc, pageNo - 1, &info)) < DDJVU_JOB_OK) {
        gDjVuContext.SpinMessageLoop();
    }
    float dpiFactor = 1.0;
    if (DDJVU_JOB_OK == status) {
        dpiFactor = GetFileDPI() / info.dpi;
    }

    miniexp_t *links = ddjvu_anno_get_hyperlinks(annos[pageNo-1]);
    for (int i = 0; links[i]; i++) {
        miniexp_t anno = miniexp_cdr(links[i]);

        miniexp_t url = miniexp_car(anno);
        const char *urlUtf8 = nullptr;
        if (miniexp_stringp(url)) {
            urlUtf8 = miniexp_to_str(url);
        }
        else if (miniexp_consp(url) && miniexp_car(url) == miniexp_symbol("url") &&
                 miniexp_stringp(miniexp_cadr(url)) && miniexp_stringp(miniexp_caddr(url))) {
            urlUtf8 = miniexp_to_str(miniexp_cadr(url));
        }
        if (!urlUtf8) {
            continue;
        }

        anno = miniexp_cdr(anno);
        miniexp_t comment = miniexp_car(anno);
        const char *commentUtf8 = nullptr;
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
        if (!miniexp_numberp(miniexp_car(area))) continue;
        int x = miniexp_to_int(miniexp_car(area)); area = miniexp_cdr(area);
        if (!miniexp_numberp(miniexp_car(area))) continue;
        int y = miniexp_to_int(miniexp_car(area)); area = miniexp_cdr(area);
        if (!miniexp_numberp(miniexp_car(area))) continue;
        int w = miniexp_to_int(miniexp_car(area)); area = miniexp_cdr(area);
        if (!miniexp_numberp(miniexp_car(area))) continue;
        int h = miniexp_to_int(miniexp_car(area)); area = miniexp_cdr(area);
        if (dpiFactor != 1.0) {
            x = (int)(x * dpiFactor); w = (int)(w * dpiFactor);
            y = (int)(y * dpiFactor); h = (int)(h * dpiFactor);
        }
        RectI rect(x, page.dy - y - h, w, h);

        ScopedMem<char> link(ResolveNamedDest(urlUtf8));
        els->Append(new DjVuLink(pageNo, rect, link ? link : urlUtf8, commentUtf8));
    }
    ddjvu_free(links);

    return els;
}

PageElement *DjVuEngineImpl::GetElementAtPos(int pageNo, PointD pt)
{
    Vec<PageElement *> *els = GetElements(pageNo);
    if (!els) {
        return nullptr;
    }

    // elements are extracted bottom-to-top but are accessed
    // in top-to-bottom order, so reverse the list first
    els->Reverse();

    PageElement *el = nullptr;
    for (size_t i = 0; i < els->Count() && !el; i++) {
        if (els->At(i)->GetRect().Contains(pt)) {
            el = els->At(i);
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
char *DjVuEngineImpl::ResolveNamedDest(const char *name)
{
    if (!str::StartsWith(name, "#"))
        return nullptr;
    for (size_t i = 0; i < fileInfo.Count(); i++) {
        if (str::EqI(name + 1, fileInfo.At(i).id))
            return str::Format("#%d", fileInfo.At(i).pageno + 1);
    }
    return nullptr;
}

PageDestination *DjVuEngineImpl::GetNamedDest(const WCHAR *name)
{
    ScopedMem<char> nameUtf8(str::conv::ToUtf8(name));
    if (!str::StartsWith(nameUtf8.Get(), "#"))
        nameUtf8.Set(str::Join("#", nameUtf8));

    ScopedMem<char> link(ResolveNamedDest(nameUtf8));
    if (link)
        return new DjVuDestination(link);
    return nullptr;
}

DjVuTocItem *DjVuEngineImpl::BuildTocTree(miniexp_t entry, int& idCounter)
{
    DjVuTocItem *node = nullptr;

    for (miniexp_t rest = entry; miniexp_consp(rest); rest = miniexp_cdr(rest)) {
        miniexp_t item = miniexp_car(rest);
        if (!miniexp_consp(item) || !miniexp_consp(miniexp_cdr(item)) ||
            !miniexp_stringp(miniexp_car(item)) || !miniexp_stringp(miniexp_cadr(item)))
            continue;

        const char *name = miniexp_to_str(miniexp_car(item));
        const char *link = miniexp_to_str(miniexp_cadr(item));

        DjVuTocItem *tocItem = nullptr;
        ScopedMem<char> linkNo(ResolveNamedDest(link));
        if (!linkNo)
            tocItem = new DjVuTocItem(name, link);
        else if (!str::IsEmpty(name) && !str::Eq(name, link + 1))
            tocItem = new DjVuTocItem(name, linkNo);
        else {
            // ignore generic (name-less) entries
            delete BuildTocTree(miniexp_cddr(item), idCounter);
            continue;
        }

        tocItem->id = ++idCounter;
        tocItem->child = BuildTocTree(miniexp_cddr(item), idCounter);

        if (!node)
            node = tocItem;
        else
            node->AddSibling(tocItem);
    }

    return node;
}

DocTocItem *DjVuEngineImpl::GetTocTree()
{
    if (!HasTocTree())
        return nullptr;

    ScopedCritSec scope(&gDjVuContext.lock);
    int idCounter = 0;
    DjVuTocItem *root = BuildTocTree(outline, idCounter);
    if (root)
        root->OpenSingleNode();
    return root;
}

WCHAR *DjVuEngineImpl::GetPageLabel(int pageNo) const
{
    for (size_t i = 0; i < fileInfo.Count(); i++) {
        ddjvu_fileinfo_t& info = fileInfo.At(i);
        if (pageNo - 1 == info.pageno && !str::Eq(info.title, info.id))
            return str::conv::FromUtf8(info.title);
    }
    return BaseEngine::GetPageLabel(pageNo);
}

int DjVuEngineImpl::GetPageByLabel(const WCHAR *label) const
{
    ScopedMem<char> labelUtf8(str::conv::ToUtf8(label));
    for (size_t i = 0; i < fileInfo.Count(); i++) {
        ddjvu_fileinfo_t& info = fileInfo.At(i);
        if (str::EqI(info.title, labelUtf8) && !str::Eq(info.title, info.id))
            return info.pageno + 1;
    }
    return BaseEngine::GetPageByLabel(label);
}

BaseEngine *DjVuEngineImpl::CreateFromFile(const WCHAR *fileName)
{
    DjVuEngineImpl *engine = new DjVuEngineImpl();
    if (!engine->Load(fileName)) {
        delete engine;
        return nullptr;
    }
    return engine;
}

BaseEngine *DjVuEngineImpl::CreateFromStream(IStream *stream)
{
    DjVuEngineImpl *engine = new DjVuEngineImpl();
    if (!engine->Load(stream)) {
        delete engine;
        return nullptr;
    }
    return engine;
}

namespace DjVuEngine {

bool IsSupportedFile(const WCHAR *fileName, bool sniff)
{
    if (sniff)
        return file::StartsWith(fileName, "AT&T");

    return str::EndsWithI(fileName, L".djvu");
}

BaseEngine *CreateFromFile(const WCHAR *fileName)
{
    return DjVuEngineImpl::CreateFromFile(fileName);
}

BaseEngine *CreateFromStream(IStream *stream)
{
    return DjVuEngineImpl::CreateFromStream(stream);
}

}
