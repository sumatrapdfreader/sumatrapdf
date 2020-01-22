/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#pragma warning(disable : 4611) // interaction between '_setjmp' and C++ object destruction is non-portable

extern "C" {
#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
#include <../mupdf/source/xps/xps-imp.h>
}

#include "utils/BaseUtil.h"
#include "utils/Archive.h"
#include "utils/ScopedWin.h"
#include "utils/FileUtil.h"
#include "utils/HtmlParserLookup.h"
#include "utils/HtmlPullParser.h"
#include "utils/TrivialHtmlParser.h"
#include "utils/WinUtil.h"
#include "utils/ZipUtil.h"
#include "utils/Log.h"

#include "AppColors.h"
#include "wingui/TreeModel.h"
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

static fz_xml_doc* xps_open_and_parse(fz_context* ctx, xps_document* doc, char* path) {
    fz_xml_doc* root = nullptr;
    xps_part* part = xps_read_part(ctx, doc, path);

    int preserve_white = 0;
    int for_html = 0;
    fz_try(ctx) {
        root = fz_parse_xml(ctx, part->data, preserve_white, for_html);
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
            xps_resolve_url(ctx, xpsdoc, path, "", fz_xml_att(item, "Target"), nelem(path));
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
        if (fz_xml_is_tag(item, /*"dc:"*/ "title") && !props->title)
            props->title.Set(xps_get_core_prop(ctx, item));
        else if (fz_xml_is_tag(item, /*"dc:"*/ "creator") && !props->author)
            props->author.Set(xps_get_core_prop(ctx, item));
        else if (fz_xml_is_tag(item, /*"dc:"*/ "subject") && !props->subject)
            props->subject.Set(xps_get_core_prop(ctx, item));
        else if (fz_xml_is_tag(item, /*"dcterms:"*/ "created") && !props->creation_date)
            props->creation_date.Set(xps_get_core_prop(ctx, item));
        else if (fz_xml_is_tag(item, /*"dcterms:"*/ "modified") && !props->modification_date)
            props->modification_date.Set(xps_get_core_prop(ctx, item));
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

    RectD PageMediabox(int pageNo) override;
    RectD PageContentBox(int pageNo, RenderTarget target = RenderTarget::View) override;

    RenderedBitmap* RenderPage(RenderPageArgs& args) override;

    RectD Transform(RectD rect, int pageNo, float zoom, int rotation, bool inverse = false) override;

    std::string_view GetFileData() override;
    bool SaveFileAs(const char* copyFileName, bool includeUserAnnots = false) override;
    WCHAR* ExtractPageText(int pageNo, RectI** coordsOut = nullptr) override;
    bool HasClipOptimizations(int pageNo) override;
    WCHAR* GetProperty(DocumentProperty prop) override;

    void UpdateUserAnnotations(Vec<PageAnnotation>* list) override;

    RenderedBitmap* GetImageForPageElement(PageElement*) override;

    bool BenchLoadPage(int pageNo) override {
        return GetFzPageInfo(pageNo) != nullptr;
    }

    Vec<PageElement*>* GetElements(int pageNo) override;
    PageElement* GetElementAtPos(int pageNo, PointD pt) override;

    PageDestination* GetNamedDest(const WCHAR* name) override;
    TocTree* GetToc() override;

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

    Vec<PageAnnotation> userAnnots;

    TocTree* tocTree = nullptr;

    bool Load(const WCHAR* fileName);
    bool Load(IStream* stream);
    // TODO(port): fz_stream can't be re-opened anymore
    // bool Load(fz_stream* stm);
    bool LoadFromStream(fz_stream* stm);

    FzPageInfo* GetFzPageInfo(int pageNo, bool failIfBusy = false);
    fz_page* GetFzPage(int pageNo, bool failIfBusy = false);
    int GetPageNo(fz_page* page);
    fz_matrix viewctm(int pageNo, float zoom, int rotation) {
        const fz_rect tmpRect = RectD_to_fz_rect(PageMediabox(pageNo));
        return fz_create_view_ctm(tmpRect, zoom, rotation);
    }
    fz_matrix viewctm(fz_page* page, float zoom, int rotation) {
        fz_rect r = fz_bound_page(ctx, page);
        return fz_create_view_ctm(r, zoom, rotation);
    }

    TocItem* BuildTocTree(TocItem* parent, fz_outline* entry, int& idCounter);
    RenderedBitmap* GetPageImage(int pageNo, RectD rect, size_t imageIx);
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
    supportsAnnotations = true;
    supportsAnnotationsForSaving = false;

    for (size_t i = 0; i < dimof(mutexes); i++) {
        InitializeCriticalSection(&mutexes[i]);
    }
    InitializeCriticalSection(&pagesAccess);
    ctxAccess = &mutexes[FZ_LOCK_ALLOC];

    fz_locks_ctx.user = this;
    fz_locks_ctx.lock = fz_lock_context_cs;
    fz_locks_ctx.unlock = fz_unlock_context_cs;
    ctx = fz_new_context(nullptr, &fz_locks_ctx, FZ_STORE_UNLIMITED);
    installFitzErrorCallbacks(ctx);
}

EngineXps::~EngineXps() {
    EnterCriticalSection(&pagesAccess);
    EnterCriticalSection(ctxAccess);

    for (auto* pi : _pages) {
        if (pi->links) {
            fz_drop_link(ctx, pi->links);
        }
        if (pi->stext) {
            fz_drop_stext_page(ctx, pi->stext);
        }
        if (pi->list) {
            fz_drop_display_list(ctx, pi->list);
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
        return false;
    }

    EngineXps* clone = new EngineXps();
    bool ok = clone->Load(FileName());
    if (!ok) {
        delete clone;
        return nullptr;
    }

    clone->UpdateUserAnnotations(&userAnnots);

    return clone;
}

bool EngineXps::Load(const WCHAR* fileName) {
    AssertCrash(!FileName() && !_doc && !_docStream && ctx);
    SetFileName(fileName);
    if (!ctx)
        return false;

    if (dir::Exists(fileName)) {
        // load uncompressed documents as a recompressed ZIP stream
        ScopedComPtr<IStream> zipStream(OpenDirAsZipStream(fileName, true));
        if (!zipStream)
            return false;
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
    AssertCrash(!_doc && !_docStream && ctx);
    if (!ctx)
        return false;

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
        pageInfo->mediabox = fz_rect_to_RectD(mbox);
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
    GetFzPage(pageNo, failIfBusy);
    return _pages[pageNo - 1];
}

fz_page* EngineXps::GetFzPage(int pageNo, bool failIfBusy) {
    ScopedCritSec scope(&pagesAccess);

    CrashIf(pageNo < 1 || pageNo > pageCount);
    int pageIdx = pageNo - 1;
    FzPageInfo* pageInfo = _pages[pageNo - 1];
    // TODO: not sure what failIfBusy is supposed to do
    if (pageInfo->list || failIfBusy) {
        return pageInfo->page;
    }

    ScopedCritSec ctxScope(ctxAccess);

#if 0
    // was loaded in LoadFromStream
    fz_var(page);
    fz_try(ctx) {
        page = fz_load_page(ctx, _doc, pageNo - 1);
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
        dev = NULL;
    }
    fz_catch(ctx) {
        fz_drop_display_list(ctx, list);
        // fz_drop_separations(ctx, seps);
    }
    if (!list) {
        return page;
    }
    pageInfo->list = list;

    fz_try(ctx) {
        pageInfo->stext = fz_new_stext_page_from_page(ctx, page, nullptr);
    }
    fz_catch(ctx) {
        pageInfo->stext = nullptr;
    }

    pageInfo->links = fz_load_links(ctx, page);
    FzLinkifyPageText(pageInfo);

    return page;
}

int EngineXps::GetPageNo(fz_page* page) {
    for (auto& pageInfo : _pages) {
        if (pageInfo->page == page) {
            return pageInfo->pageNo;
        }
    }
    return 0;
}

RectD EngineXps::PageMediabox(int pageNo) {
    FzPageInfo* pi = _pages[pageNo - 1];
    return pi->mediabox;
}

RectD EngineXps::PageContentBox(int pageNo, RenderTarget target) {
    UNUSED(target);
    FzPageInfo* pageInfo = GetFzPageInfo(pageNo);

    ScopedCritSec scope(ctxAccess);

    fz_cookie fzcookie = {};
    fz_rect rect = fz_empty_rect;
    fz_device* dev = nullptr;

    fz_rect pagerect = fz_bound_page(ctx, pageInfo->page);

    fz_var(dev);

    RectD mediabox = pageInfo->mediabox;

    fz_try(ctx) {
        dev = fz_new_bbox_device(ctx, &rect);
        fz_run_display_list(ctx, pageInfo->list, dev, fz_identity, pagerect, &fzcookie);
        fz_close_device(ctx, dev);
    }
    fz_always(ctx) {
        fz_drop_device(ctx, dev);
    }
    fz_catch(ctx) {
        return mediabox;
    }

    if (fz_is_infinite_rect(rect)) {
        return mediabox;
    }

    RectD rect2 = fz_rect_to_RectD(rect);
    return rect2.Intersect(mediabox);
}

RectD EngineXps::Transform(RectD rect, int pageNo, float zoom, int rotation, bool inverse) {
    fz_matrix ctm = viewctm(pageNo, zoom, rotation);
    if (inverse) {
        ctm = fz_invert_matrix(ctm);
    }
    fz_rect rect2 = RectD_to_fz_rect(rect);
    rect2 = fz_transform_rect(rect2, ctm);
    return fz_rect_to_RectD(rect2);
}

RenderedBitmap* EngineXps::RenderPage(RenderPageArgs& args) {
    FzPageInfo* pageInfo = GetFzPageInfo(args.pageNo);
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
        pRect = RectD_to_fz_rect(*args.pageRect);
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
    fz_device* dev = NULL;
    RenderedBitmap* bitmap = nullptr;

    fz_var(dev);
    fz_var(pix);
    fz_var(bitmap);

    Vec<PageAnnotation> pageAnnots = fz_get_user_page_annots(userAnnots, args.pageNo);

    fz_try(ctx) {
        pix = fz_new_pixmap_with_bbox(ctx, colorspace, ibounds, nullptr, 1);
        // initialize with white background
        fz_clear_pixmap_with_value(ctx, pix, 0xff);

        // TODO: in printing different style. old code use pdf_run_page_with_usage(), with usage ="View"
        // or "Print". "Export" is not used
        dev = fz_new_draw_device(ctx, fz_identity, pix);
        // TODO: use fz_infinite_rect instead of cliprect?
        fz_run_page_transparency(ctx, pageAnnots, dev, cliprect, false, false);
        fz_run_display_list(ctx, pageInfo->list, dev, ctm, cliprect, fzcookie);
        fz_run_page_transparency(ctx, pageAnnots, dev, cliprect, true, false);
        fz_run_user_page_annots(ctx, pageAnnots, dev, ctm, cliprect, fzcookie);
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

std::string_view EngineXps::GetFileData() {
    std::string_view res;
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

bool EngineXps::SaveFileAs(const char* copyFileName, bool includeUserAnnots) {
    UNUSED(includeUserAnnots);
    AutoFreeWstr dstPath = strconv::Utf8ToWstr(copyFileName);
    AutoFree d = GetFileData();
    if (!d.empty()) {
        bool ok = file::WriteFile(dstPath, d.as_view());
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
        GetFzPageInfo(i);
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

void EngineXps::UpdateUserAnnotations(Vec<PageAnnotation>* list) {
    // TODO: use a new critical section to avoid blocking the UI thread
    ScopedCritSec scope(ctxAccess);
    if (list) {
        userAnnots = *list;
    } else {
        userAnnots.Reset();
    }
}

PageElement* EngineXps::GetElementAtPos(int pageNo, PointD pt) {
    FzPageInfo* pageInfo = GetFzPageInfo(pageNo);
    return FzGetElementAtPos(pageInfo, pt);
}

Vec<PageElement*>* EngineXps::GetElements(int pageNo) {
    fz_page* page = GetFzPage(pageNo, true);
    if (!page) {
        return nullptr;
    }
    FzPageInfo* pageInfo = _pages[pageNo - 1];
    return FzGetElements(pageInfo);
}

RenderedBitmap* EngineXps::GetImageForPageElement(PageElement* pel) {
    auto r = pel->rect;
    int pageNo = pel->pageNo;
    int imageID = pel->imageID;
    return GetPageImage(pageNo, r, imageID);
}

WCHAR* EngineXps::ExtractPageText(int pageNo, RectI** coordsOut) {
    FzPageInfo* pageInfo = GetFzPageInfo(pageNo);
    fz_stext_page* stext = pageInfo->stext;
    if (!stext) {
        return nullptr;
    }
    ScopedCritSec scope(ctxAccess);
    WCHAR* content = fz_text_page_to_str(stext, coordsOut);
    return content;
}

RenderedBitmap* EngineXps::GetPageImage(int pageNo, RectD rect, size_t imageIdx) {
    FzPageInfo* pageInfo = GetFzPageInfo(pageNo);
    if (!pageInfo->page) {
        return nullptr;
    }

    Vec<FitzImagePos> positions;

    if (imageIdx >= positions.size() || fz_rect_to_RectD(positions.at(imageIdx).rect) != rect) {
        AssertCrash(0);
        return nullptr;
    }

    ScopedCritSec scope(ctxAccess);

    fz_pixmap* pixmap = nullptr;
    fz_try(ctx) {
        fz_image* image = positions.at(imageIdx).image;
        // CrashMePort();
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
            return newSimpleDest(dest->page + 1, RectD{});
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

        if (outline->down) {
            item->child = BuildTocTree(item, outline->down, idCounter);
        }

        if (!root) {
            root = item;
            curr = item;
        } else {
            curr->next = item;
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

    fz_rect mbox = RectD_to_fz_rect(PageMediabox(pageNo));
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

bool IsXpsEngineSupportedFile(const WCHAR* fileName, bool sniff) {
    if (!sniff) {
        return str::EndsWithI(fileName, L".xps") || str::EndsWithI(fileName, L".oxps");
    }

    if (dir::Exists(fileName)) {
        // allow opening uncompressed XPS files as well
        AutoFreeWstr relsPath(path::Join(fileName, L"_rels\\.rels"));
        return file::Exists(relsPath) || dir::Exists(relsPath);
    }

    MultiFormatArchive* archive = OpenZipArchive(fileName, true);
    if (!archive) {
        return false;
    }

    bool res = archive->GetFileId("_rels/.rels") != (size_t)-1 ||
               archive->GetFileId("_rels/.rels/[0].piece") != (size_t)-1 ||
               archive->GetFileId("_rels/.rels/[0].last.piece") != (size_t)-1;
    delete archive;
    return res;
}

EngineBase* CreateXpsEngineFromFile(const WCHAR* fileName) {
    return EngineXps::CreateFromFile(fileName);
}

EngineBase* CreateXpsEngineFromStream(IStream* stream) {
    return EngineXps::CreateFromStream(stream);
}
