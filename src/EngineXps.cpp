/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

extern "C" {
#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
#include <../mupdf/source/xps/xps-imp.h>
}

#include "utils/BaseUtil.h"
#include "utils/Archive.h"
#include "utils/ScopedWin.h"
#include "utils/FileUtil.h"
#include "utils/GuessFileType.h"
#include "utils/HtmlParserLookup.h"
#include "utils/HtmlPullParser.h"
#include "utils/TrivialHtmlParser.h"
#include "utils/WinUtil.h"
#include "utils/ZipUtil.h"
#include "utils/Log.h"

#include "AppColors.h"
#include "wingui/TreeModel.h"

#include "Annotation.h"
#include "EngineBase.h"
#include "EngineFzUtil.h"
#include "EngineXps.h"

Kind kindEngineXps = "engineXps";

// TODO: use http://schemas.openxps.org/oxps/v1.0 as well once NS actually matters
#define NS_XPS_MICROSOFT "http://schemas.microsoft.com/xps/2005/06"

#if 0
fz_rect xps_bound_page_quick(xps_document* doc, int number) {
    fz_rect bounds = fz_empty_rect;
    fz_page* page = doc->first_page;
    for (int n = 0; n < number && page; n++)
        page = page->next;

    xps_part* part = page ? xps_read_part(doc, page->name) : nullptr;
    if (!part)
        return fz_empty_rect;

    const char* data = (const char*)part->data;
    size_t data_size = part->size;

    AutoFree dataUtf8;
    if (str::StartsWith(data, UTF16BE_BOM)) {
        for (int i = 0; i + 1 < part->size; i += 2) {
            std::swap(part->data[i], part->data[i + 1]);
        }
    }
    if (str::StartsWith(data, UTF16_BOM)) {
        const WCHAR* s = (const WCHAR*)(part->data + 2);
        size_t n = (part->size - 2) / 2;
        auto tmp = strconv::ToUtf8(s, n);
        dataUtf8.Set(tmp.StealData());
        data = dataUtf8;
        data_size = str::Len(dataUtf8);
    } else if (str::StartsWith(data, UTF8_BOM)) {
        data += 3;
        data_size -= 3;
    }

    HtmlPullParser p(data, data_size);
    HtmlToken* tok = p.Next();
    if (tok && tok->IsStartTag() && tok->NameIsNS("FixedPage", NS_XPS_MICROSOFT)) {
        AttrInfo* attr = tok->GetAttrByNameNS("Width", NS_XPS_MICROSOFT);
        if (attr)
            bounds.x1 = fz_atof(attr->val) * 72.0f / 96.0f;
        attr = tok->GetAttrByNameNS("Height", NS_XPS_MICROSOFT);
        if (attr)
            bounds.y1 = fz_atof(attr->val) * 72.0f / 96.0f;
    }

    xps_drop_part(ctx, doc, part);
    return bounds;
}
#endif

class xps_doc_props {
  public:
    AutoFreeWstr title;
    AutoFreeWstr author;
    AutoFreeWstr subject;
    AutoFreeWstr creation_date;
    AutoFreeWstr modification_date;
};

static fz_xml_doc* xps_open_and_parse(fz_context* ctx, xps_document* doc, const char* path) {
    fz_xml_doc* root = nullptr;
    xps_part* part = xps_read_part(ctx, doc, (char*)path);

    int preserve_white = 0;
    fz_try(ctx) {
        root = fz_parse_xml(ctx, part->data, preserve_white);
    }
    fz_always(ctx) {
        xps_drop_part(ctx, doc, part);
    }
    fz_catch(ctx) {
        fz_rethrow(ctx);
    }

    return root;
}

static WCHAR* xps_get_core_prop(fz_context* ctx, fz_xml* item) {
    fz_xml* text = fz_xml_down(item);

    if (!text) {
        return nullptr;
    }
    if (!fz_xml_text(text) || fz_xml_next(text)) {
        fz_warn(ctx, "non-text content for property %s", fz_xml_tag(item));
        return nullptr;
    }

    char* start = fz_xml_text(text);
    while (str::IsWs(*start)) {
        ++start;
    }
    char* end = start + strlen(start);
    while (end > start && str::IsWs(*(end - 1))) {
        --end;
    }

    return strconv::FromHtmlUtf8(start, end - start);
}

#define REL_CORE_PROPERTIES "http://schemas.openxmlformats.org/package/2006/relationships/metadata/core-properties"

xps_doc_props* xps_extract_doc_props(fz_context* ctx, xps_document* xpsdoc) {
    fz_xml_doc* xmldoc = xps_open_and_parse(ctx, xpsdoc, "/_rels/.rels");

    fz_xml* root = fz_xml_root(xmldoc);

    if (!fz_xml_is_tag(root, "Relationships")) {
        fz_drop_xml(ctx, xmldoc);
        fz_throw(ctx, FZ_ERROR_GENERIC, "couldn't parse part '/_rels/.rels'");
    }

    bool has_correct_root = false;
    for (fz_xml* item = fz_xml_down(root); item; item = fz_xml_next(item)) {
        if (fz_xml_is_tag(item, "Relationship") && str::Eq(fz_xml_att(item, "Type"), REL_CORE_PROPERTIES) &&
            fz_xml_att(item, "Target")) {
            char path[1024];
            char empty[1] = {0};
            char* attr = fz_xml_att(item, "Target");
            xps_resolve_url(ctx, xpsdoc, path, empty, attr, nelem(path));
            fz_drop_xml(ctx, xmldoc);
            xmldoc = xps_open_and_parse(ctx, xpsdoc, path);
            root = fz_xml_root(xmldoc);
            has_correct_root = true;
            break;
        }
    }

    if (!has_correct_root) {
        fz_drop_xml(ctx, xmldoc);
        return nullptr;
    }

    xps_doc_props* props = new xps_doc_props();

    for (fz_xml* item = fz_xml_down(root); item; item = fz_xml_next(item)) {
        if (fz_xml_is_tag(item, /*"dc:"*/ "title") && !props->title) {
            props->title.Set(xps_get_core_prop(ctx, item));
        } else if (fz_xml_is_tag(item, /*"dc:"*/ "creator") && !props->author) {
            props->author.Set(xps_get_core_prop(ctx, item));
        } else if (fz_xml_is_tag(item, /*"dc:"*/ "subject") && !props->subject) {
            props->subject.Set(xps_get_core_prop(ctx, item));
        } else if (fz_xml_is_tag(item, /*"dcterms:"*/ "created") && !props->creation_date) {
            props->creation_date.Set(xps_get_core_prop(ctx, item));
        } else if (fz_xml_is_tag(item, /*"dcterms:"*/ "modified") && !props->modification_date) {
            props->modification_date.Set(xps_get_core_prop(ctx, item));
        }
    }
    fz_drop_xml(ctx, xmldoc);

    return props;
}

///// XpsEngine is also based on Fitz and shares quite some code with PdfEngine /////

class XpsTocItem;

static xps_document* xps_document_from_fz_document(fz_document* doc) {
    return (xps_document*)doc;
}

class EngineXps : public EngineBase {
  public:
    EngineXps();
    virtual ~EngineXps();
    EngineBase* Clone() override;

    RectF PageMediabox(int pageNo) override;
    RectF PageContentBox(int pageNo, RenderTarget target = RenderTarget::View) override;

    RenderedBitmap* RenderPage(RenderPageArgs& args) override;

    RectF Transform(const RectF& rect, int pageNo, float zoom, int rotation, bool inverse = false) override;

    std::span<u8> GetFileData() override;
    bool SaveFileAs(const char* copyFileName, bool includeUserAnnots = false) override;
    PageText ExtractPageText(int pageNo) override;
    bool HasClipOptimizations(int pageNo) override;
    WCHAR* GetProperty(DocumentProperty prop) override;

    RenderedBitmap* GetImageForPageElement(IPageElement*) override;

    bool BenchLoadPage(int pageNo) override {
        return GetFzPageInfo(pageNo, false) != nullptr;
    }

    Vec<IPageElement*>* GetElements(int pageNo) override;
    IPageElement* GetElementAtPos(int pageNo, PointF pt) override;

    PageDestination* GetNamedDest(const WCHAR* name) override;
    TocTree* GetToc() override;
    void ResolveLinks(Vec<IPageElement*>* els, fz_link* links);

    static EngineBase* CreateFromFile(const WCHAR* fileName);
    static EngineBase* CreateFromStream(IStream* stream);

  public:
    // make sure to never ask for pagesAccess in an ctxAccess
    // protected critical section in order to avoid deadlocks
    CRITICAL_SECTION* ctxAccess;
    CRITICAL_SECTION pagesAccess;
    CRITICAL_SECTION mutexes[FZ_LOCK_MAX];

    fz_context* ctx = nullptr;
    fz_locks_context fz_locks_ctx;
    fz_document* _doc = nullptr;
    fz_stream* _docStream = nullptr;
    Vec<FzPageInfo*> _pages;
    fz_outline* _outline = nullptr;
    xps_doc_props* _info = nullptr;
    fz_rect** imageRects = nullptr;

    TocTree* tocTree = nullptr;

    bool Load(const WCHAR* fileName);
    bool Load(IStream* stream);
    // TODO(port): fz_stream can't be re-opened anymore
    // bool Load(fz_stream* stm);
    bool LoadFromStream(fz_stream* stm);

    FzPageInfo* GetFzPageInfo(int pageNo, bool failIfBusy);
    int GetPageNo(fz_page* page);
    fz_matrix viewctm(int pageNo, float zoom, int rotation) {
        const fz_rect tmpRect = To_fz_rect(PageMediabox(pageNo));
        return fz_create_view_ctm(tmpRect, zoom, rotation);
    }
    fz_matrix viewctm(fz_page* page, float zoom, int rotation) {
        fz_rect r = fz_bound_page(ctx, page);
        return fz_create_view_ctm(r, zoom, rotation);
    }

    TocItem* BuildTocTree(TocItem* parent, fz_outline* outline, int& idCounter);
    RenderedBitmap* GetPageImage(int pageNo, RectF rect, int imageIdx);
    WCHAR* ExtractFontList();
};

static void fz_lock_context_cs(void* user, int lock) {
    EngineXps* e = (EngineXps*)user;
    EnterCriticalSection(&e->mutexes[lock]);
}

static void fz_unlock_context_cs(void* user, int lock) {
    EngineXps* e = (EngineXps*)user;
    LeaveCriticalSection(&e->mutexes[lock]);
}

static void fz_print_cb(void* user, const char* msg) {
    log(msg);
}

static void installFitzErrorCallbacks(fz_context* ctx) {
    fz_set_warning_callback(ctx, fz_print_cb, nullptr);
    fz_set_error_callback(ctx, fz_print_cb, nullptr);
}

EngineXps::EngineXps() {
    kind = kindEngineXps;
    defaultFileExt = L".xps";
    fileDPI = 72.0f;

    for (size_t i = 0; i < dimof(mutexes); i++) {
        InitializeCriticalSection(&mutexes[i]);
    }
    InitializeCriticalSection(&pagesAccess);
    ctxAccess = &mutexes[FZ_LOCK_ALLOC];

    fz_locks_ctx.user = this;
    fz_locks_ctx.lock = fz_lock_context_cs;
    fz_locks_ctx.unlock = fz_unlock_context_cs;
    ctx = fz_new_context(nullptr, &fz_locks_ctx, FZ_STORE_DEFAULT);
    installFitzErrorCallbacks(ctx);
}

EngineXps::~EngineXps() {
    EnterCriticalSection(&pagesAccess);
    EnterCriticalSection(ctxAccess);

    for (auto* pi : _pages) {
        if (pi->links) {
            fz_drop_link(ctx, pi->links);
        }
        if (pi->page) {
            fz_drop_page(ctx, pi->page);
        }
    }

    DeleteVecMembers(_pages);

    fz_drop_outline(ctx, _outline);
    delete _info;

    if (imageRects) {
        for (int i = 0; i < PageCount(); i++) {
            free(imageRects[i]);
        }
        free(imageRects);
    }

    fz_drop_document(ctx, _doc);
    fz_drop_context(ctx);

    for (size_t i = 0; i < dimof(mutexes); i++) {
        LeaveCriticalSection(&mutexes[i]);
        DeleteCriticalSection(&mutexes[i]);
    }
    LeaveCriticalSection(&pagesAccess);
    DeleteCriticalSection(&pagesAccess);
}

EngineBase* EngineXps::Clone() {
    ScopedCritSec scope(ctxAccess);

    // TODO: we used to support cloning streams
    // but mupdf removed ability to clone fz_stream
    const WCHAR* path = FileName();
    if (!path) {
        return nullptr;
    }

    EngineXps* clone = new EngineXps();
    bool ok = clone->Load(FileName());
    if (!ok) {
        delete clone;
        return nullptr;
    }

    return clone;
}

bool EngineXps::Load(const WCHAR* fileName) {
    CrashIf(!(!FileName() && !_doc && !_docStream && ctx));
    SetFileName(fileName);
    if (!ctx) {
        return false;
    }

    if (dir::Exists(fileName)) {
        // load uncompressed documents as a recompressed ZIP stream
        ScopedComPtr<IStream> zipStream(OpenDirAsZipStream(fileName, true));
        if (!zipStream) {
            return false;
        }
        return Load(zipStream);
    }

    fz_stream* stm = nullptr;
    fz_try(ctx) {
        stm = fz_open_file2(ctx, fileName);
    }
    fz_catch(ctx) {
        return false;
    }
    return LoadFromStream(stm);
}

bool EngineXps::Load(IStream* stream) {
    CrashIf(!(!_doc && !_docStream && ctx));
    if (!ctx) {
        return false;
    }

    fz_stream* stm = nullptr;
    fz_try(ctx) {
        stm = fz_open_istream(ctx, stream);
    }
    fz_catch(ctx) {
        return false;
    }
    return LoadFromStream(stm);
}

bool EngineXps::LoadFromStream(fz_stream* stm) {
    if (!stm) {
        return false;
    }

    _docStream = stm;
    fz_try(ctx) {
        _doc = xps_open_document_with_stream(ctx, stm);
        pageCount = fz_count_pages(ctx, _doc);
    }
    fz_always(ctx) {
        fz_drop_stream(ctx, stm);
    }
    fz_catch(ctx) {
        return false;
    }

    if (pageCount == 0) {
        fz_warn(ctx, "document has no pages");
        return false;
    }

    // TODO: this might be slow. Try port xps_bound_page_quick
    for (int i = 0; i < pageCount; i++) {
        FzPageInfo* pageInfo = new FzPageInfo();
        pageInfo->pageNo = i + 1;

        fz_rect mbox{};

        fz_try(ctx) {
            pageInfo->page = fz_load_page(ctx, _doc, i);
            mbox = fz_bound_page(ctx, pageInfo->page);
        }
        fz_catch(ctx) {
        }
        if (fz_is_empty_rect(mbox)) {
            fz_warn(ctx, "cannot find page size for page %d", i);
            mbox.x0 = 0;
            mbox.y0 = 0;
            mbox.x1 = 612;
            mbox.y1 = 792;
        }
        pageInfo->mediabox = ToRectFl(mbox);
        _pages.Append(pageInfo);
    }

    fz_try(ctx) {
        _outline = fz_load_outline(ctx, _doc);
    }
    fz_catch(ctx) {
        fz_warn(ctx, "Couldn't load outline");
    }
    fz_try(ctx) {
        _info = xps_extract_doc_props(ctx, (xps_document*)_doc);
    }
    fz_catch(ctx) {
        fz_warn(ctx, "Couldn't load document properties");
    }

    return true;
}

FzPageInfo* EngineXps::GetFzPageInfo(int pageNo, bool failIfBusy) {
    ScopedCritSec scope(&pagesAccess);

    CrashIf(pageNo < 1 || pageNo > pageCount);
    int pageIdx = pageNo - 1;
    FzPageInfo* pageInfo = _pages[pageIdx];
    // TODO: not sure what failIfBusy is supposed to do
    if (pageInfo->page || failIfBusy) {
        return pageInfo;
    }

    ScopedCritSec ctxScope(ctxAccess);

#if 0
    // was loaded in LoadFromStream
    fz_var(page);
    fz_try(ctx) {
        page = fz_load_page(ctx, _doc, pageIdx);
        pageInfo->page = page;
    }
    fz_catch(ctx) {
    }
#endif

    fz_page* page = pageInfo->page;
    fz_rect bounds;
    fz_display_list* list = NULL;
    fz_device* dev = NULL;
    fz_cookie cookie = {0};
    fz_var(list);
    fz_var(dev);

    /* TODO: handle try later?
        if (fz_caught(ctx) != FZ_ERROR_TRYLATER) {
            return nullptr;
        }
    */

    // TODO: use fz_new_display_list_from_page
    fz_try(ctx) {
        bounds = fz_bound_page(ctx, page);
        list = fz_new_display_list(ctx, bounds);
        dev = fz_new_list_device(ctx, list);
        // TODO(port): should this be just fz_run_page_contents?
        fz_run_page(ctx, page, dev, fz_identity, &cookie);
        fz_close_device(ctx, dev);
    }
    fz_always(ctx) {
        fz_drop_device(ctx, dev);
        if (list) {
            fz_drop_display_list(ctx, list);
        }
        dev = NULL;
    }
    fz_catch(ctx) {
        fz_drop_display_list(ctx, list);
        // fz_drop_separations(ctx, seps);
    }
    if (!list) {
        return pageInfo;
    }
    pageInfo->links = fz_load_links(ctx, page);

    fz_stext_page* stext = nullptr;
    fz_var(stext);

    fz_try(ctx) {
        stext = fz_new_stext_page_from_page(ctx, page, nullptr);
    }
    fz_catch(ctx) {
    }

    if (!stext) {
        return pageInfo;
    }
    FzLinkifyPageText(pageInfo, stext);
    fz_find_image_positions(ctx, pageInfo->images, stext);
    fz_drop_stext_page(ctx, stext);

    return pageInfo;
}

int EngineXps::GetPageNo(fz_page* page) {
    for (auto& pageInfo : _pages) {
        if (pageInfo->page == page) {
            return pageInfo->pageNo;
        }
    }
    return 0;
}

RectF EngineXps::PageMediabox(int pageNo) {
    FzPageInfo* pi = _pages[pageNo - 1];
    return pi->mediabox;
}

RectF EngineXps::PageContentBox(int pageNo, [[maybe_unused]] RenderTarget target) {
    FzPageInfo* pageInfo = GetFzPageInfo(pageNo, false);

    ScopedCritSec scope(ctxAccess);

    fz_cookie fzcookie = {};
    fz_rect rect = fz_empty_rect;
    fz_device* dev = nullptr;
    fz_display_list* list = nullptr;

    fz_rect pagerect = fz_bound_page(ctx, pageInfo->page);

    fz_var(dev);
    fz_var(list);

    RectF mediabox = pageInfo->mediabox;

    fz_try(ctx) {
        dev = fz_new_bbox_device(ctx, &rect);
        list = fz_new_display_list_from_page(ctx, pageInfo->page);
        fz_run_display_list(ctx, list, dev, fz_identity, pagerect, &fzcookie);
        fz_close_device(ctx, dev);
    }
    fz_always(ctx) {
        fz_drop_device(ctx, dev);
        if (list) {
            fz_drop_display_list(ctx, list);
        }
    }
    fz_catch(ctx) {
        return mediabox;
    }

    if (fz_is_infinite_rect(rect)) {
        return mediabox;
    }

    RectF rect2 = ToRectFl(rect);
    return rect2.Intersect(mediabox);
}

RectF EngineXps::Transform(const RectF& rect, int pageNo, float zoom, int rotation, bool inverse) {
    fz_matrix ctm = viewctm(pageNo, zoom, rotation);
    if (inverse) {
        ctm = fz_invert_matrix(ctm);
    }
    fz_rect rect2 = To_fz_rect(rect);
    rect2 = fz_transform_rect(rect2, ctm);
    return ToRectFl(rect2);
}

RenderedBitmap* EngineXps::RenderPage(RenderPageArgs& args) {
    FzPageInfo* pageInfo = GetFzPageInfo(args.pageNo, false);
    fz_page* page = pageInfo->page;
    if (!page) {
        return nullptr;
    }

    fz_cookie* fzcookie = nullptr;
    FitzAbortCookie* cookie = nullptr;
    if (args.cookie_out) {
        cookie = new FitzAbortCookie();
        *args.cookie_out = cookie;
        fzcookie = &cookie->cookie;
    }

    // TODO(port): I don't see why this lock is needed
    ScopedCritSec cs(ctxAccess);

    fz_rect pRect;
    if (args.pageRect) {
        pRect = To_fz_rect(*args.pageRect);
    } else {
        // TODO(port): use pageInfo->mediabox?
        pRect = fz_bound_page(ctx, page);
    }
    fz_matrix ctm = viewctm(page, args.zoom, args.rotation);
    fz_irect bbox = fz_round_rect(fz_transform_rect(pRect, ctm));

    fz_colorspace* colorspace = fz_device_rgb(ctx);
    fz_irect ibounds = bbox;
    fz_rect cliprect = fz_rect_from_irect(bbox);

    fz_pixmap* pix = nullptr;
    fz_device* dev = nullptr;
    RenderedBitmap* bitmap = nullptr;

    fz_var(dev);
    fz_var(pix);
    fz_var(bitmap);

    fz_try(ctx) {
        pix = fz_new_pixmap_with_bbox(ctx, colorspace, ibounds, nullptr, 1);
        // initialize with white background
        fz_clear_pixmap_with_value(ctx, pix, 0xff);

        // TODO: in printing different style. old code use pdf_run_page_with_usage(), with usage ="View"
        // or "Print". "Export" is not used
        dev = fz_new_draw_device(ctx, fz_identity, pix);
        // TODO: use fz_infinite_rect instead of cliprect?
        fz_run_page(ctx, page, dev, ctm, fzcookie);
        bitmap = new_rendered_fz_pixmap(ctx, pix);
        fz_close_device(ctx, dev);
    }
    fz_always(ctx) {
        if (dev) {
            fz_drop_device(ctx, dev);
        }
        fz_drop_pixmap(ctx, pix);
    }
    fz_catch(ctx) {
        delete bitmap;
        return nullptr;
    }
    return bitmap;
}

std::span<u8> EngineXps::GetFileData() {
    std::span<u8> res;
    ScopedCritSec scope(ctxAccess);

    fz_var(res);
    fz_try(ctx) {
        res = fz_extract_stream_data(ctx, _docStream);
    }
    fz_catch(ctx) {
        res = {};
    }

    if (!res.empty()) {
        return res;
    }

    auto path = FileName();
    if (!path) {
        return {};
    }
    return file::ReadFile(path);
}

bool EngineXps::SaveFileAs(const char* copyFileName, [[maybe_unused]] bool includeUserAnnots) {
    AutoFreeWstr dstPath = strconv::Utf8ToWstr(copyFileName);
    AutoFree d = GetFileData();
    if (!d.empty()) {
        bool ok = file::WriteFile(dstPath, d.AsSpan());
        if (ok) {
            return true;
        }
    }
    auto path = FileName();
    if (!path) {
        return false;
    }
    return CopyFileW(path, dstPath, FALSE);
}

WCHAR* EngineXps::ExtractFontList() {
    // load and parse all pages
    for (int i = 1; i <= PageCount(); i++) {
        GetFzPageInfo(i, false);
    }

    ScopedCritSec scope(ctxAccess);

    // collect a list of all included fonts
    WStrVec fonts;
#if 0
    for (xps_font_cache* font = _doc->font_table; font; font = font->next) {
        AutoFreeWstr path(strconv::FromUtf8(font->name));
        AutoFreeWstr name(strconv::FromUtf8(font->font->name));
        fonts.Append(str::Format(L"%s (%s)", name.Get(), path::GetBaseNameNoFree(path)));
    }
#endif
    if (fonts.size() == 0) {
        return nullptr;
    }

    fonts.SortNatural();
    return fonts.Join(L"\n");
}

WCHAR* EngineXps::GetProperty(DocumentProperty prop) {
    if (DocumentProperty::FontList == prop) {
        return ExtractFontList();
    }
    if (!_info) {
        return nullptr;
    }

    switch (prop) {
        case DocumentProperty::Title:
            return str::Dup(_info->title);
        case DocumentProperty::Author:
            return str::Dup(_info->author);
        case DocumentProperty::Subject:
            return str::Dup(_info->subject);
        case DocumentProperty::CreationDate:
            return str::Dup(_info->creation_date);
        case DocumentProperty::ModificationDate:
            return str::Dup(_info->modification_date);
        default:
            return nullptr;
    }
};

IPageElement* EngineXps::GetElementAtPos(int pageNo, PointF pt) {
    FzPageInfo* pageInfo = GetFzPageInfo(pageNo, false);
    return FzGetElementAtPos(pageInfo, pt);
}

// TODO: probably need to use fz_resolve_link(ctx, _doc, link->uri, &x, &y)
// in FzGetElements().
void EngineXps::ResolveLinks(Vec<IPageElement*>* els, fz_link* link) {
    float x, y;
    fz_location loc;
    while (link) {
        loc = fz_resolve_link(ctx, _doc, link->uri, &x, &y);
        if (loc.page != -1 && loc.chapter != -1) {
            // TODO: need to support uris that are chapter / page
            // maybe prefix uris with document type
        } else {
            logf("EngineXps::ResolveLink: unresolved uri '%s'\n", link->uri);
        }
        link = link->next;
    }
}

Vec<IPageElement*>* EngineXps::GetElements(int pageNo) {
    FzPageInfo* pageInfo = GetFzPageInfo(pageNo, true);
    if (!pageInfo) {
        return nullptr;
    }
    auto res = new Vec<IPageElement*>();

#if 0
    fz_link* links = pageInfo->links;
    ResolveLinks(res, links);
    // this is a hack: temporarily remove pageInfo->links so that FzGetElements()
    // doesn't process it
    pageInfo->links = nullptr;
#endif

    FzGetElements(res, pageInfo);
#if 0
    pageInfo->links = links;
#endif
    if (res->IsEmpty()) {
        delete res;
        return nullptr;
    }
    return res;
}

RenderedBitmap* EngineXps::GetImageForPageElement(IPageElement* ipel) {
    PageElement* pel = (PageElement*)ipel;
    auto r = pel->rect;
    int pageNo = pel->pageNo;
    int imageID = pel->imageID;
    return GetPageImage(pageNo, r, imageID);
}

PageText EngineXps::ExtractPageText(int pageNo) {
    FzPageInfo* pageInfo = GetFzPageInfo(pageNo, false);
    if (!pageInfo) {
        return {};
    }

    ScopedCritSec scope(ctxAccess);
    fz_stext_page* stext = nullptr;
    fz_var(stext);

    fz_try(ctx) {
        stext = fz_new_stext_page_from_page(ctx, pageInfo->page, nullptr);
    }
    fz_catch(ctx) {
    }
    if (!stext) {
        return {};
    }
    PageText res;
    res.text = fz_text_page_to_str(stext, &res.coords);
    fz_drop_stext_page(ctx, stext);
    res.len = (int)str::Len(res.text);
    return res;
}

RenderedBitmap* EngineXps::GetPageImage(int pageNo, RectF rect, int imageIdx) {
    FzPageInfo* pageInfo = GetFzPageInfo(pageNo, false);
    if (!pageInfo->page) {
        return nullptr;
    }

    Vec<FitzImagePos> positions;

    if (imageIdx >= positions.isize() || ToRectFl(positions.at(imageIdx).rect) != rect) {
        CrashIf(true);
        return nullptr;
    }

    ScopedCritSec scope(ctxAccess);
    fz_image* image = fz_find_image_at_idx(ctx, pageInfo, imageIdx);
    CrashIf(!image);
    if (!image) {
        return nullptr;
    }
    fz_pixmap* pixmap = nullptr;
    fz_try(ctx) {
        // SubmitCrashIf(true);
        // TODO(port): not sure if should provide subarea, w and h
        pixmap = fz_get_pixmap_from_image(ctx, image, nullptr, nullptr, nullptr, nullptr);
    }
    fz_catch(ctx) {
        return nullptr;
    }
    RenderedBitmap* bmp = new_rendered_fz_pixmap(ctx, pixmap);
    fz_drop_pixmap(ctx, pixmap);

    return bmp;
}

PageDestination* EngineXps::GetNamedDest(const WCHAR* nameW) {
    AutoFree name = strconv::WstrToUtf8(nameW);
    if (!str::StartsWith(name, "#")) {
        name.Set(str::Join("#", name));
    }
    auto* doc = xps_document_from_fz_document(_doc);
    xps_target* dest = doc->target;
    while (dest) {
        if (str::EndsWithI(dest->name, name)) {
            return newSimpleDest(dest->page + 1, RectF{});
        }
        dest = dest->next;
    }
    return nullptr;
}

TocItem* EngineXps::BuildTocTree(TocItem* parent, fz_outline* outline, int& idCounter) {
    TocItem* root = nullptr;
    TocItem* curr = nullptr;

    while (outline) {
        WCHAR* name = nullptr;
        if (outline->title) {
            name = strconv::Utf8ToWstr(outline->title);
            name = pdf_clean_string(name);
        }
        if (!name) {
            name = str::Dup(L"");
        }
        int pageNo = outline->page + 1;
        auto dest = newFzDestination(outline);

        TocItem* item = newTocItemWithDestination(parent, name, dest);
        free(name);
        item->isOpenDefault = outline->is_open;
        item->id = ++idCounter;
        item->fontFlags = outline->flags;
        item->pageNo = pageNo;

        if (outline->down) {
            item->child = BuildTocTree(item, outline->down, idCounter);
        }

        if (!root) {
            root = item;
            curr = item;
        } else {
            CrashIf(!curr);
            if (curr) {
                curr->next = item;
            }
            curr = item;
        }

        outline = outline->next;
    }

    return root;
}

TocTree* EngineXps::GetToc() {
    if (tocTree) {
        return tocTree;
    }

    int idCounter = 0;
    TocItem* root = BuildTocTree(nullptr, _outline, idCounter);
    if (!root) {
        return nullptr;
    }
    tocTree = new TocTree(root);
    return tocTree;
}

bool EngineXps::HasClipOptimizations(int pageNo) {
    FzPageInfo* pageInfo = GetFzPageInfo(pageNo, true);
    if (!pageInfo) {
        return false;
    }

    fz_rect mbox = To_fz_rect(PageMediabox(pageNo));
    // check if any image covers at least 90% of the page
    for (auto& img : pageInfo->images) {
        fz_rect ir = img.rect;
        if (fz_calc_overlap(mbox, ir) >= 0.9f) {
            return false;
        }
    }
    return true;
}

EngineBase* EngineXps::CreateFromFile(const WCHAR* fileName) {
    EngineXps* engine = new EngineXps();
    if (!engine || !fileName || !engine->Load(fileName)) {
        delete engine;
        return nullptr;
    }
    return engine;
}

EngineBase* EngineXps::CreateFromStream(IStream* stream) {
    EngineXps* engine = new EngineXps();
    if (!engine->Load(stream)) {
        delete engine;
        return nullptr;
    }
    return engine;
}

bool IsXpsDirectory(const WCHAR* path) {
    // allow opening uncompressed XPS files as well
    AutoFreeWstr relsPath(path::Join(path, L"_rels\\.rels"));
    return file::Exists(relsPath) || dir::Exists(relsPath);
}

bool IsXpsEngineSupportedFileType(Kind kind) {
    return kind == kindFileXps;
}

EngineBase* CreateXpsEngineFromFile(const WCHAR* fileName) {
    return EngineXps::CreateFromFile(fileName);
}

EngineBase* CreateXpsEngineFromStream(IStream* stream) {
    return EngineXps::CreateFromStream(stream);
}
