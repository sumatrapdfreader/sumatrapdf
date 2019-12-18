/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
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

    if (!text)
        return nullptr;
    if (!fz_xml_text(text) || fz_xml_next(text)) {
        fz_warn(ctx, "non-text content for property %s", fz_xml_tag(item));
        return nullptr;
    }

    char *start, *end;
    for (start = fz_xml_text(text); str::IsWs(*start); start++)
        ;
    for (end = start + strlen(start); end > start && str::IsWs(*(end - 1)); end--)
        ;

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
class XpsImage;

static xps_document* xps_document_from_fz_document(fz_document* doc) {
    return (xps_document*)doc;
}

class XpsEngineImpl : public EngineBase {
  public:
    XpsEngineImpl();
    virtual ~XpsEngineImpl();
    EngineBase* Clone() override;

    int PageCount() const override {
        CrashIf(pageCount < 0);
        return pageCount;
    }

    RectD PageMediabox(int pageNo) override;
    RectD PageContentBox(int pageNo, RenderTarget target = RenderTarget::View) override;

    RenderedBitmap* RenderBitmap(int pageNo, float zoom, int rotation,
                                 RectD* pageRect = nullptr, /* if nullptr: defaults to the page's mediabox */
                                 RenderTarget target = RenderTarget::View, AbortCookie** cookie_out = nullptr) override;

    PointD Transform(PointD pt, int pageNo, float zoom, int rotation, bool inverse = false) override;
    RectD Transform(RectD rect, int pageNo, float zoom, int rotation, bool inverse = false) override;

    std::string_view GetFileData() override;
    bool SaveFileAs(const char* copyFileName, bool includeUserAnnots = false) override;
    WCHAR* ExtractPageText(int pageNo, RectI** coordsOut = nullptr) override;
    bool HasClipOptimizations(int pageNo) override;
    WCHAR* GetProperty(DocumentProperty prop) override;

    bool SupportsAnnotation(bool forSaving = false) const override;
    void UpdateUserAnnotations(Vec<PageAnnotation>* list) override;

    bool BenchLoadPage(int pageNo) override {
        return GetFzPageInfo(pageNo) != nullptr;
    }

    Vec<PageElement*>* GetElements(int pageNo) override;
    PageElement* GetElementAtPos(int pageNo, PointD pt) override;

    PageDestination* GetNamedDest(const WCHAR* name) override;
    DocTocTree* GetTocTree() override;

    static EngineBase* CreateFromFile(const WCHAR* fileName);
    static EngineBase* CreateFromStream(IStream* stream);

  public:
    // make sure to never ask for _pagesAccess in an ctxAccess
    // protected critical section in order to avoid deadlocks
    CRITICAL_SECTION ctxAccess;
    CRITICAL_SECTION _pagesAccess;
    int pageCount = -0;

    fz_context* ctx = nullptr;
    fz_locks_context fz_locks_ctx;
    fz_document* _doc = nullptr;
    fz_stream* _docStream = nullptr;
    Vec<FzPageInfo*> _pages;
    fz_outline* _outline = nullptr;
    xps_doc_props* _info = nullptr;
    fz_rect** imageRects = nullptr;

    Vec<PageAnnotation> userAnnots;

    DocTocTree* tocTree = nullptr;

    bool Load(const WCHAR* fileName);
    bool Load(IStream* stream);
    // TODO(port): fz_stream can't be re-opened anymore
    // bool Load(fz_stream* stm);
    bool LoadFromStream(fz_stream* stm);

    FzPageInfo* GetFzPageInfo(int pageNo, bool failIfBusy = false);
    fz_page* GetFzPage(int pageNo, bool failIfBusy = false);
    int GetPageNo(fz_page* page);
    fz_matrix viewctm(int pageNo, float zoom, int rotation) {
        const fz_rect tmpRect = fz_RectD_to_rect(PageMediabox(pageNo));
        return fz_create_view_ctm(tmpRect, zoom, rotation);
    }
    fz_matrix viewctm(fz_page* page, float zoom, int rotation) {
        fz_rect r = fz_bound_page(ctx, page);
        return fz_create_view_ctm(r, zoom, rotation);
    }

    XpsTocItem* BuildTocTree(fz_outline* entry, int& idCounter);
    void LinkifyPageText(FzPageInfo* pageInfo);
    RenderedBitmap* GetPageImage(int pageNo, RectD rect, size_t imageIx);
    WCHAR* ExtractFontList();
};

class XpsLink : public PageElement, public PageDestination {
  public:
    XpsEngineImpl* engine = nullptr;
    // must be one or the other
    fz_link* link = nullptr;
    fz_outline* outline = nullptr;
    RectD rect = {};

    XpsLink() = default;

    XpsLink(XpsEngineImpl* engine, int pageNo, fz_link* link, fz_outline* outline);

    PageElementType GetType() const override {
        return PageElementType::Link;
    }

    RectD GetRect() const override;
    WCHAR* GetValue() const override;
    virtual PageDestination* AsLink() {
        return this;
    }

    PageDestType UpdateDestType();
    int UpdateDestPageNo();
    RectD GetDestRect() const override;

    WCHAR* GetDestValue() const override {
        return GetValue();
    }
};

class XpsTocItem : public DocTocItem {
    XpsLink link;

  public:
    XpsTocItem(WCHAR* title, XpsLink link) : DocTocItem(title), link(link) {
    }

    PageDestination* GetLink() override {
        return &link;
    }
};

class XpsImage : public PageElement {
    XpsEngineImpl* engine;
    int pageNo;
    RectD rect;
    size_t imageIdx;

  public:
    XpsImage(XpsEngineImpl* engine, int pageNo, fz_rect rect, size_t imageIx)
        : engine(engine), pageNo(pageNo), rect(fz_rect_to_RectD(rect)), imageIdx(imageIdx) {
    }

    virtual PageElementType GetType() const {
        return PageElementType::Image;
    }
    virtual int GetPageNo() const {
        return pageNo;
    }
    virtual RectD GetRect() const {
        return rect;
    }
    virtual WCHAR* GetValue() const {
        return nullptr;
    }

    virtual RenderedBitmap* GetImage() {
        return engine->GetPageImage(pageNo, rect, imageIdx);
    }
};

static void fz_lock_context_cs(void* user, int lock) {
    UNUSED(lock);
    XpsEngineImpl* e = (XpsEngineImpl*)user;
    // we use a single critical section for all locks,
    // since that critical section (ctxAccess) should
    // be guarding all fz_context access anyway and
    // thus already be in place (in debug builds we
    // crash if that assertion doesn't hold)
    EnterCriticalSection(&e->ctxAccess);
}

static void fz_unlock_context_cs(void* user, int lock) {
    UNUSED(lock);
    XpsEngineImpl* e = (XpsEngineImpl*)user;
    LeaveCriticalSection(&e->ctxAccess);
}

static void fz_print_cb(void* user, const char* msg) {
    log(msg);
}

static void installFitzErrorCallbacks(fz_context* ctx) {
    fz_set_warning_callback(ctx, fz_print_cb, nullptr);
    fz_set_error_callback(ctx, fz_print_cb, nullptr);
}

XpsEngineImpl::XpsEngineImpl() {
    kind = kindEngineXps;
    defaultFileExt = L".xps";
    fileDPI = 72.0f;

    InitializeCriticalSection(&_pagesAccess);
    InitializeCriticalSection(&ctxAccess);

    fz_locks_ctx.user = this;
    fz_locks_ctx.lock = fz_lock_context_cs;
    fz_locks_ctx.unlock = fz_unlock_context_cs;
    ctx = fz_new_context(nullptr, &fz_locks_ctx, FZ_STORE_UNLIMITED);
    installFitzErrorCallbacks(ctx);
}

XpsEngineImpl::~XpsEngineImpl() {
    EnterCriticalSection(&_pagesAccess);
    EnterCriticalSection(&ctxAccess);

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

    LeaveCriticalSection(&ctxAccess);
    DeleteCriticalSection(&ctxAccess);

    LeaveCriticalSection(&_pagesAccess);
    DeleteCriticalSection(&_pagesAccess);
}

EngineBase* XpsEngineImpl::Clone() {
    ScopedCritSec scope(&ctxAccess);

    // TODO: we used to support cloning streams
    // but mupdf removed ability to clone fz_stream
    const WCHAR* path = FileName();
    if (!path) {
        return false;
    }

    XpsEngineImpl* clone = new XpsEngineImpl();
    bool ok = clone->Load(FileName());
    if (!ok) {
        delete clone;
        return nullptr;
    }

    clone->UpdateUserAnnotations(&userAnnots);

    return clone;
}

bool XpsEngineImpl::Load(const WCHAR* fileName) {
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

bool XpsEngineImpl::Load(IStream* stream) {
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

bool XpsEngineImpl::LoadFromStream(fz_stream* stm) {
    if (!stm) {
        return false;
    }

    _docStream = stm;
    fz_try(ctx) {
        _doc = xps_open_document_with_stream(ctx, stm);
        pageCount = fz_count_pages(ctx, _doc);
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

FzPageInfo* XpsEngineImpl::GetFzPageInfo(int pageNo, bool failIfBusy) {
    GetFzPage(pageNo, failIfBusy);
    return _pages[pageNo - 1];
}

fz_page* XpsEngineImpl::GetFzPage(int pageNo, bool failIfBusy) {
    ScopedCritSec scope(&_pagesAccess);

    CrashIf(pageNo < 1 || pageNo > pageCount);
    int pageIdx = pageNo - 1;
    FzPageInfo* pageInfo = _pages[pageNo - 1];
    // TODO: not sure what failIfBusy is supposed to do
    if (pageInfo->list || failIfBusy) {
        return pageInfo->page;
    }

    ScopedCritSec ctxScope(&ctxAccess);

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
    }
    fz_always(ctx) {
        fz_close_device(ctx, dev);
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
    LinkifyPageText(pageInfo);

    return page;
}

int XpsEngineImpl::GetPageNo(fz_page* page) {
    for (auto& pageInfo : _pages) {
        if (pageInfo->page == page) {
            return pageInfo->pageNo;
        }
    }
    return 0;
}

RectD XpsEngineImpl::PageMediabox(int pageNo) {
    FzPageInfo* pi = _pages[pageNo - 1];
    return pi->mediabox;
}

RectD XpsEngineImpl::PageContentBox(int pageNo, RenderTarget target) {
    UNUSED(target);
    FzPageInfo* pageInfo = GetFzPageInfo(pageNo);

    ScopedCritSec scope(&ctxAccess);

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

PointD XpsEngineImpl::Transform(PointD pt, int pageNo, float zoom, int rotation, bool inverse) {
    fz_matrix ctm = viewctm(pageNo, zoom, rotation);
    if (inverse)
        ctm = fz_invert_matrix(ctm);
    fz_point pt2 = {(float)pt.x, (float)pt.y};
    pt2 = fz_transform_point(pt2, ctm);
    return PointD(pt2.x, pt2.y);
}

RectD XpsEngineImpl::Transform(RectD rect, int pageNo, float zoom, int rotation, bool inverse) {
    fz_matrix ctm = viewctm(pageNo, zoom, rotation);
    if (inverse) {
        ctm = fz_invert_matrix(ctm);
    }
    fz_rect rect2 = fz_RectD_to_rect(rect);
    rect2 = fz_transform_rect(rect2, ctm);
    return fz_rect_to_RectD(rect2);
}

RenderedBitmap* XpsEngineImpl::RenderBitmap(int pageNo, float zoom, int rotation, RectD* pageRect, RenderTarget target,
                                            AbortCookie** cookie_out) {
    FzPageInfo* pageInfo = GetFzPageInfo(pageNo);
    fz_page* page = pageInfo->page;

    if (!page) {
        return nullptr;
    }

    fz_cookie* fzcookie = nullptr;
    FitzAbortCookie* cookie = nullptr;
    if (cookie_out) {
        cookie = new FitzAbortCookie();
        *cookie_out = cookie;
        fzcookie = &cookie->cookie;
    }

    // TODO(port): I don't see why this lock is needed
    EnterCriticalSection(&ctxAccess);

    fz_rect pRect;
    if (pageRect) {
        pRect = fz_RectD_to_rect(*pageRect);
    } else {
        // TODO(port): use pageInfo->mediabox?
        pRect = fz_bound_page(ctx, page);
    }
    fz_matrix ctm = viewctm(page, zoom, rotation);
    fz_irect bbox = fz_round_rect(fz_transform_rect(pRect, ctm));

    fz_colorspace* colorspace = fz_device_rgb(ctx);
    fz_irect ibounds = bbox;
    fz_rect cliprect = fz_rect_from_irect(bbox);

    fz_pixmap* pix = fz_new_pixmap_with_bbox(ctx, colorspace, ibounds, nullptr, 1);
    // initialize white background
    fz_clear_pixmap_with_value(ctx, pix, 0xff);

    fz_device* dev = NULL;
    fz_var(dev);
    fz_try(ctx) {
        // TODO: in printing different style. old code use pdf_run_page_with_usage(), with usage ="View"
        // or "Print". "Export" is not used
        dev = fz_new_draw_device(ctx, fz_identity, pix);
        // TODO: use fz_infinite_rect instead of cliprect?
        fz_run_display_list(ctx, pageInfo->list, dev, ctm, cliprect, fzcookie);
        fz_close_device(ctx, dev);
    }
    fz_always(ctx) {
        fz_drop_device(ctx, dev);
        LeaveCriticalSection(&ctxAccess);
    }
    fz_catch(ctx) {
        fz_drop_pixmap(ctx, pix);
        return nullptr;
    }

    RenderedBitmap* bitmap = new_rendered_fz_pixmap(ctx, pix);
    fz_drop_pixmap(ctx, pix);
    return bitmap;
}

std::string_view XpsEngineImpl::GetFileData() {
    u8* res = nullptr;
    ScopedCritSec scope(&ctxAccess);
    size_t cbCount;

    fz_var(res);
    fz_try(ctx) {
        res = fz_extract_stream_data(ctx, _docStream, &cbCount);
    }
    fz_catch(ctx) {
        res = nullptr;
    }
    if (res) {
        return {(char*)res, cbCount};
    }
    auto path = FileName();
    if (!path) {
        return {};
    }

    return file::ReadFile(path);
}

bool XpsEngineImpl::SaveFileAs(const char* copyFileName, bool includeUserAnnots) {
    UNUSED(includeUserAnnots);
    AutoFreeWstr dstPath = strconv::FromUtf8(copyFileName);
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

WCHAR* XpsEngineImpl::ExtractFontList() {
    // load and parse all pages
    for (int i = 1; i <= PageCount(); i++) {
        GetFzPageInfo(i);
    }

    ScopedCritSec scope(&ctxAccess);

    // collect a list of all included fonts
    WStrVec fonts;
#if 0
    for (xps_font_cache* font = _doc->font_table; font; font = font->next) {
        AutoFreeWstr path(strconv::FromUtf8(font->name));
        AutoFreeWstr name(strconv::FromUtf8(font->font->name));
        fonts.Append(str::Format(L"%s (%s)", name.Get(), path::GetBaseNameNoFree(path)));
    }
#endif
    if (fonts.size() == 0)
        return nullptr;

    fonts.SortNatural();
    return fonts.Join(L"\n");
}

WCHAR* XpsEngineImpl::GetProperty(DocumentProperty prop) {
    if (DocumentProperty::FontList == prop)
        return ExtractFontList();
    if (!_info)
        return nullptr;

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

bool XpsEngineImpl::SupportsAnnotation(bool forSaving) const {
    return !forSaving; // for now
}

void XpsEngineImpl::UpdateUserAnnotations(Vec<PageAnnotation>* list) {
    // TODO: use a new critical section to avoid blocking the UI thread
    ScopedCritSec scope(&ctxAccess);
    if (list)
        userAnnots = *list;
    else
        userAnnots.Reset();
}

PageElement* XpsEngineImpl::GetElementAtPos(int pageNo, PointD pt) {
    auto* pageInfo = GetFzPageInfo(pageNo);
    if (!pageInfo) {
        return nullptr;
    }

    fz_link* link = pageInfo->links;
    fz_point p = {(float)pt.x, (float)pt.y};
    while (link) {
        if (fz_is_pt_in_rect(link->rect, p)) {
            return new XpsLink(this, pageNo, link, nullptr);
        }
        link = link->next;
    }

    size_t imageIdx = 0;
    for (auto& img : pageInfo->images) {
        fz_rect ir = img.rect;
        if (fz_is_pt_in_rect(ir, p)) {
            return new XpsImage(this, pageNo, ir, imageIdx);
        }
        imageIdx++;
    }
    return nullptr;
}

Vec<PageElement*>* XpsEngineImpl::GetElements(int pageNo) {
    fz_page* page = GetFzPage(pageNo, true);
    if (!page)
        return nullptr;
    FzPageInfo* pageInfo = _pages[pageNo - 1];

    // since all elements lists are in last-to-first order, append
    // item types in inverse order and reverse the whole list at the end
    Vec<PageElement*>* els = new Vec<PageElement*>();

    size_t imageIdx = 0;
    for (auto& img : pageInfo->images) {
        fz_rect ir = img.rect;
        auto image = new XpsImage(this, pageNo, ir, imageIdx);
        els->Append(image);
        imageIdx++;
    }
    fz_link* link = pageInfo->links;
    while (link) {
        auto* el = new XpsLink(this, pageNo, link, nullptr);
        els->Append(el);
        link = link->next;
    }

    els->Reverse();
    return els;
}

void XpsEngineImpl::LinkifyPageText(FzPageInfo* pageInfo) {
    RectI* coords;
    fz_stext_page* stext = pageInfo->stext;
    if (!stext) {
        return;
    }
    ScopedCritSec scope(&ctxAccess);
    WCHAR* pageText = fz_text_page_to_str(stext, &coords);
    if (!pageText) {
        return;
    }

    LinkRectList* list = LinkifyText(pageText, coords);
    free(pageText);
    fz_page* page = pageInfo->page;

    for (size_t i = 0; i < list->links.size(); i++) {
        fz_rect bbox = list->coords.at(i);
        bool overlaps = false;
        fz_link* link = pageInfo->links;
        while (link && !overlaps) {
            overlaps = fz_calc_overlap(bbox, link->rect) >= 0.25f;
            link = link->next;
        }
        if (overlaps) {
            continue;
        }

        AutoFree uri(strconv::WstrToUtf8(list->links.at(i)));
        if (!uri.Get()) {
            continue;
        }

        // add links in top-to-bottom order (i.e. last-to-first)
        link = fz_new_link(ctx, bbox, _doc, uri.Get());
        link->next = pageInfo->links;
        pageInfo->links = link;
    }
    delete list;
    free(coords);
}

WCHAR* XpsEngineImpl::ExtractPageText(int pageNo, RectI** coordsOut) {
    FzPageInfo* pageInfo = GetFzPageInfo(pageNo);
    fz_stext_page* stext = pageInfo->stext;
    if (!stext) {
        return nullptr;
    }
    ScopedCritSec scope(&ctxAccess);
    WCHAR* content = fz_text_page_to_str(stext, coordsOut);
    return content;
}

RenderedBitmap* XpsEngineImpl::GetPageImage(int pageNo, RectD rect, size_t imageIdx) {
    FzPageInfo* pageInfo = GetFzPageInfo(pageNo);
    if (!pageInfo->page) {
        return nullptr;
    }

    Vec<FitzImagePos> positions;

    if (imageIdx >= positions.size() || fz_rect_to_RectD(positions.at(imageIdx).rect) != rect) {
        AssertCrash(0);
        return nullptr;
    }

    ScopedCritSec scope(&ctxAccess);

    fz_pixmap* pixmap = nullptr;
    fz_try(ctx) {
        fz_image* image = positions.at(imageIdx).image;
        CrashMePort();
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

PageDestination* XpsEngineImpl::GetNamedDest(const WCHAR* nameW) {
    AutoFree name = strconv::WstrToUtf8(nameW);
    if (!str::StartsWith(name, "#")) {
        name.Set(str::Join("#", name));
    }
    auto* doc = xps_document_from_fz_document(_doc);
    xps_target* dest = doc->target;
    while (dest) {
        if (str::EndsWithI(dest->name, name)) {
            return new SimpleDest(dest->page + 1, RectD{});
        }
        dest = dest->next;
    }
    return nullptr;
}

XpsTocItem* XpsEngineImpl::BuildTocTree(fz_outline* outline, int& idCounter) {
    XpsTocItem* root = nullptr;
    XpsTocItem* curr = nullptr;

    while (outline) {
        WCHAR* name = nullptr;
        if (outline->title) {
            name = strconv::FromUtf8(outline->title);
            name = pdf_clean_string(name);
        }
        if (!name) {
            name = str::Dup(L"");
        }
        int pageNo = outline->page + 1;
        XpsLink link(this, pageNo, nullptr, outline);
        XpsTocItem* item = new XpsTocItem(name, link);
        item->isOpenDefault = outline->is_open;
        item->id = ++idCounter;
        item->fontFlags = outline->flags;

        if (outline->down) {
            item->child = BuildTocTree(outline->down, idCounter);
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

DocTocTree* XpsEngineImpl::GetTocTree() {
    if (tocTree) {
        return tocTree;
    }

    int idCounter = 0;
    DocTocItem* root = BuildTocTree(_outline, idCounter);
    if (!root) {
        return nullptr;
    }
    tocTree = new DocTocTree(root);
    return tocTree;
}

bool XpsEngineImpl::HasClipOptimizations(int pageNo) {
    FzPageInfo* pageInfo = GetFzPageInfo(pageNo, true);
    if (!pageInfo) {
        return false;
    }

    fz_rect mbox = fz_RectD_to_rect(PageMediabox(pageNo));
    // check if any image covers at least 90% of the page
    for (auto& img : pageInfo->images) {
        fz_rect ir = img.rect;
        if (fz_calc_overlap(mbox, ir) >= 0.9f) {
            return false;
        }
    }
    return true;
}

XpsLink::XpsLink(XpsEngineImpl* engine, int pageNo, fz_link* link, fz_outline* outline) {
    this->engine = engine;
    PageElement::pageNo = pageNo;
    this->link = link;
    CrashIf(!link && !outline);
    this->link = link;
    this->outline = outline;

    destType = UpdateDestType();
    destPageNo = UpdateDestPageNo();
}

RectD XpsLink::GetRect() const {
    if (link) {
        RectD r(fz_rect_to_RectD(link->rect));
        return r;
    }
    return RectD();
}

static char* XpsLinkGetURI(const XpsLink* link) {
    if (link->link) {
        return link->link->uri;
    }
    if (link->outline) {
        return link->outline->uri;
    }
    return nullptr;
}

WCHAR* XpsLink::GetValue() const {
    if (outline) {
        WCHAR* path = strconv::FromUtf8(outline->uri);
        return path;
    }

    char* uri = XpsLinkGetURI(this);
    if (!uri) {
        return nullptr;
    }
    if (!is_external_link(uri)) {
        // other values: #1,115,208
        return nullptr;
    }
    WCHAR* path = strconv::FromUtf8(uri);
    return path;
}

PageDestType XpsLink::UpdateDestType() {
    if (outline) {
        return PageDestType::LaunchEmbedded;
    }

    char* uri = XpsLinkGetURI(this);
    // some outline entries are bad (issue 1245)
    if (!uri) {
        return PageDestType::None;
    }
    if (!is_external_link(uri)) {
        float x, y;
        int pageNo = resolve_link(uri, &x, &y);
        if (pageNo == -1) {
            // TODO: figure out what it could be
            CrashMePort();
            return PageDestType::None;
        }
        return PageDestType::ScrollTo;
    }
    if (str::StartsWith(uri, "file://")) {
        return PageDestType::LaunchFile;
    }
    if (str::StartsWithI(uri, "http://")) {
        return PageDestType::LaunchURL;
    }
    if (str::StartsWithI(uri, "https://")) {
        return PageDestType::LaunchURL;
    }
    if (str::StartsWithI(uri, "ftp://")) {
        return PageDestType::LaunchURL;
    }
    if (str::StartsWith(uri, "mailto:")) {
        return PageDestType::LaunchURL;
    }

    // TODO: PageDestType::LaunchEmbedded, PageDestType::LaunchURL, named destination
    CrashMePort();
    return PageDestType::None;
}

int XpsLink::UpdateDestPageNo() {
    char* uri = XpsLinkGetURI(this);
    CrashIf(!uri);
    if (!uri) {
        return 0;
    }
    if (is_external_link(uri)) {
        return 0;
    }
    float x, y;
    int pageNo = resolve_link(uri, &x, &y);
    if (pageNo == -1) {
        return 0;
    }
    return pageNo + 1;
#if 0
    if (link && FZ_LINK_GOTO == link->kind)
        return link->ld.gotor.page + 1;
    if (link && FZ_LINK_GOTOR == link->kind && !link->ld.gotor.dest)
        return link->ld.gotor.page + 1;
#endif
    return 0;
}

RectD XpsLink::GetDestRect() const {
    RectD result(DEST_USE_DEFAULT, DEST_USE_DEFAULT, DEST_USE_DEFAULT, DEST_USE_DEFAULT);
    char* uri = XpsLinkGetURI(this);
    CrashIf(!uri);
    if (!uri) {
        return result;
    }

    if (is_external_link(uri)) {
        return result;
    }
    float x, y;
    int pageNo = resolve_link(uri, &x, &y);
    CrashIf(pageNo < 0);

    result.x = (double)x;
    result.y = (double)y;
    return result;
}

EngineBase* XpsEngineImpl::CreateFromFile(const WCHAR* fileName) {
    XpsEngineImpl* engine = new XpsEngineImpl();
    if (!engine || !fileName || !engine->Load(fileName)) {
        delete engine;
        return nullptr;
    }
    return engine;
}

EngineBase* XpsEngineImpl::CreateFromStream(IStream* stream) {
    XpsEngineImpl* engine = new XpsEngineImpl();
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
    return XpsEngineImpl::CreateFromFile(fileName);
}

EngineBase* CreateXpsEngineFromStream(IStream* stream) {
    return XpsEngineImpl::CreateFromStream(stream);
}
