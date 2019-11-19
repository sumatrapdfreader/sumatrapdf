///// XPS-specific extensions to Fitz/MuXPS /////
extern "C" {
#include <../mupdf/source/xps/xps-imp.h>
}

// TODO: use http://schemas.openxps.org/oxps/v1.0 as well once NS actually matters
#define NS_XPS_MICROSOFT "http://schemas.microsoft.com/xps/2005/06"

fz_rect xps_bound_page_quick(xps_document* doc, int number) {
    fz_rect bounds = fz_empty_rect;
    CrashMePort();
#if 0
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
        auto tmp = str::conv::ToUtf8(s, n);
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
#endif
    return bounds;
}

class xps_doc_props {
  public:
    AutoFreeW title;
    AutoFreeW author;
    AutoFreeW subject;
    AutoFreeW creation_date;
    AutoFreeW modification_date;
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

    return str::conv::FromHtmlUtf8(start, end - start);
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

struct XpsPageRun {
    fz_page* page;
    fz_display_list* list;
    size_t size_est;
    int refs;

    XpsPageRun(fz_page* page, fz_display_list* list) : page(page), list(list), size_est(0), refs(1) {
    }
};

class XpsTocItem;
class XpsImage;

class XpsEngineImpl : public BaseEngine {
    friend XpsImage;

  public:
    XpsEngineImpl();
    virtual ~XpsEngineImpl();
    BaseEngine* Clone() override;

    int PageCount() const override {
        CrashMePort();
        return 0;
#if 0
        return _doc ? xps_count_pages(ctx, _doc, 0) : 0;
#endif
    }

    RectD PageMediabox(int pageNo) override;
    RectD PageContentBox(int pageNo, RenderTarget target = RenderTarget::View) override;

    RenderedBitmap* RenderBitmap(int pageNo, float zoom, int rotation,
                                 RectD* pageRect = nullptr, /* if nullptr: defaults to the page's mediabox */
                                 RenderTarget target = RenderTarget::View, AbortCookie** cookie_out = nullptr) override;

    PointD Transform(PointD pt, int pageNo, float zoom, int rotation, bool inverse = false) override;
    RectD Transform(RectD rect, int pageNo, float zoom, int rotation, bool inverse = false) override;

    unsigned char* GetFileData(size_t* cbCount) override;
    bool SaveFileAs(const char* copyFileName, bool includeUserAnnots = false) override;
    WCHAR* ExtractPageText(int pageNo, const WCHAR* lineSep, RectI** coordsOut = nullptr,
                           RenderTarget target = RenderTarget::View) override {
        UNUSED(target);
        return ExtractPageText(GetXpsPage(pageNo), lineSep, coordsOut);
    }
    bool HasClipOptimizations(int pageNo) override;
    WCHAR* GetProperty(DocumentProperty prop) override;

    bool SupportsAnnotation(bool forSaving = false) const override;
    void UpdateUserAnnotations(Vec<PageAnnotation>* list) override;

    float GetFileDPI() const override {
        return 72.0f;
    }
    const WCHAR* GetDefaultFileExt() const override {
        return L".xps";
    }

    bool BenchLoadPage(int pageNo) override {
        return GetXpsPage(pageNo) != nullptr;
    }

    Vec<PageElement*>* GetElements(int pageNo) override;
    PageElement* GetElementAtPos(int pageNo, PointD pt) override;

    PageDestination* GetNamedDest(const WCHAR* name) override;
    bool HasTocTree() const override {
        return _outline != nullptr;
    }
    DocTocTree* GetTocTree() override;

    fz_rect FindDestRect(const char* target);

    static BaseEngine* CreateFromFile(const WCHAR* fileName);
    static BaseEngine* CreateFromStream(IStream* stream);

  protected:
    // make sure to never ask for _pagesAccess in an ctxAccess
    // protected critical section in order to avoid deadlocks
    CRITICAL_SECTION ctxAccess;
    fz_context* ctx;
    fz_locks_context fz_locks_ctx;
    // xps_document* _doc;
    fz_document* _doc;
    fz_stream* _docStream;

    CRITICAL_SECTION _pagesAccess;
    fz_page** _pages;

    bool Load(const WCHAR* fileName);
    bool Load(IStream* stream);
    // TODO(port): fz_stream can't be re-opened anymore
    // bool Load(fz_stream* stm);
    bool LoadFromStream(fz_stream* stm);

    fz_page* GetXpsPage(int pageNo, bool failIfBusy = false);
    int GetPageNo(fz_page* page);
    fz_matrix viewctm(int pageNo, float zoom, int rotation) {
        const fz_rect tmpRect = fz_RectD_to_rect(PageMediabox(pageNo));
        return fz_create_view_ctm(tmpRect, zoom, rotation);
    }
    fz_matrix viewctm(fz_page* page, float zoom, int rotation) {
        fz_rect r = fz_bound_page(ctx, page);
        return fz_create_view_ctm(r, zoom, rotation);
    }
    WCHAR* ExtractPageText(fz_page* page, const WCHAR* lineSep, RectI** coordsOut = nullptr, bool cacheRun = false);

    Vec<XpsPageRun*> runCache; // ordered most recently used first
    XpsPageRun* CreatePageRun(fz_page* page, fz_display_list* list);
    XpsPageRun* GetPageRun(fz_page* page, bool tryOnly = false);
    bool RunPage(fz_page* page, fz_device* dev, const fz_matrix* ctm, const fz_rect cliprect = {}, bool cacheRun = true,
                 FitzAbortCookie* cookie = nullptr);
    void DropPageRun(XpsPageRun* run, bool forceRemove = false);

    DocTocTree* BuildTocTree(fz_outline* entry, int& idCounter);
    void LinkifyPageText(fz_page* page, int pageNo);
    RenderedBitmap* GetPageImage(int pageNo, RectD rect, size_t imageIx);
    WCHAR* ExtractFontList();

    RectD* _mediaboxes;
    fz_outline* _outline;
    xps_doc_props* _info;
    fz_rect** imageRects;

    Vec<PageAnnotation> userAnnots;
};

class XpsLink : public PageElement, public PageDestination {
    XpsEngineImpl* engine = nullptr;
    u8* link = nullptr; // owned by a fz_link or fz_outline
    RectD rect = {};

  public:
    XpsLink() = default;

    XpsLink(XpsEngineImpl* engine, u8* link, fz_rect rect = fz_empty_rect, int pageNo = -1) {
        this->pageNo = pageNo;
        this->engine = engine;
        this->link = link;
        this->rect = fz_rect_to_RectD(rect);
    }

    PageElementType GetType() const override {
        return PageElementType::Link;
    }

    RectD GetRect() const override {
        return rect;
    }
    WCHAR* GetValue() const override {
        CrashMePort();
#if 0
        if (link && FZ_LINK_URI == link->kind)
            return str::conv::FromUtf8(link->ld.uri.uri);
#endif
        return nullptr;
    }
    virtual PageDestination* AsLink() {
        return this;
    }

    PageDestType GetDestType() const override {
        if (!link)
            return PageDestType::None;
        CrashMePort();
#if 0
        if (FZ_LINK_GOTO == link->kind)
            return PageDestType::ScrollTo;
        if (FZ_LINK_URI == link->kind)
            return PageDestType::LaunchURL;
#endif
        return PageDestType::None;
    }
    int GetDestPageNo() const override {
        CrashMePort();
#if 0
        if (!link || link->kind != FZ_LINK_GOTO)
            return 0;
        return link->ld.gotor.page + 1;
#endif
        return 0;
    }
    RectD GetDestRect() const override {
        CrashMePort();
#if 0
        if (!engine || !link || link->kind != FZ_LINK_GOTO || !link->ld.gotor.dest)
            return RectD(DEST_USE_DEFAULT, DEST_USE_DEFAULT, DEST_USE_DEFAULT, DEST_USE_DEFAULT);
        return fz_rect_to_RectD(engine->FindDestRect(link->ld.gotor.dest));
#endif
        return RectD(DEST_USE_DEFAULT, DEST_USE_DEFAULT, DEST_USE_DEFAULT, DEST_USE_DEFAULT);
    }
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
    size_t imageIx;

  public:
    XpsImage(XpsEngineImpl* engine, int pageNo, fz_rect rect, size_t imageIx)
        : engine(engine), pageNo(pageNo), rect(fz_rect_to_RectD(rect)), imageIx(imageIx) {
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
        return engine->GetPageImage(pageNo, rect, imageIx);
    }
};

XpsEngineImpl::XpsEngineImpl()
    : _doc(nullptr),
      _docStream(nullptr),
      _pages(nullptr),
      _mediaboxes(nullptr),
      _outline(nullptr),
      _info(nullptr),
      imageRects(nullptr) {
    InitializeCriticalSection(&_pagesAccess);
    InitializeCriticalSection(&ctxAccess);

    fz_locks_ctx.user = &ctxAccess;
    fz_locks_ctx.lock = fz_lock_context_cs;
    fz_locks_ctx.unlock = fz_unlock_context_cs;
    ctx = fz_new_context(nullptr, &fz_locks_ctx, MAX_CONTEXT_MEMORY);
}

XpsEngineImpl::~XpsEngineImpl() {
    EnterCriticalSection(&_pagesAccess);
    EnterCriticalSection(&ctxAccess);

    if (_pages) {
        // xps_pages are freed by xps_close_document -> xps_free_page_list
        AssertCrash(_doc);
        free(_pages);
    }

    fz_drop_outline(ctx, _outline);
    delete _info;

    if (imageRects) {
        for (int i = 0; i < PageCount(); i++)
            free(imageRects[i]);
        free(imageRects);
    }

    while (runCache.size() > 0) {
        AssertCrash(runCache.Last()->refs == 1);
        DropPageRun(runCache.Last(), true);
    }

    CrashMePort();
    pdf_drop_document(ctx, (pdf_document*)_doc);
    // xps_close_document(_doc);
    _doc = nullptr;
    fz_drop_stream(ctx, _docStream);
    _docStream = nullptr;
    fz_drop_context(ctx);
    ctx = nullptr;

    free(_mediaboxes);

    LeaveCriticalSection(&ctxAccess);
    DeleteCriticalSection(&ctxAccess);
    LeaveCriticalSection(&_pagesAccess);
    DeleteCriticalSection(&_pagesAccess);
}

BaseEngine* XpsEngineImpl::Clone() {
    ScopedCritSec scope(&ctxAccess);

    XpsEngineImpl* clone = new XpsEngineImpl();
    bool ok;
    if (FileName()) {
        ok = clone->Load(FileName());
    } else {
        CrashMePort();
        // ok = clone->Load(_docStream);
    }
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

// TODO(port): fz_stream can't be re-opened anymore
#if 0
bool XpsEngineImpl::Load(fz_stream* stm) {
    AssertCrash(!FileName() && !_doc && !_docStream && ctx);
    if (!ctx)
        return false;

    fz_try(ctx) { stm = fz_clone_stream(ctx, stm); }
    fz_catch(ctx) { return false; }
    return LoadFromStream(stm);
}
#endif

bool XpsEngineImpl::LoadFromStream(fz_stream* stm) {
    if (!stm)
        return false;

    _docStream = stm;
    fz_try(ctx) {
        _doc = xps_open_document_with_stream(ctx, stm);
    }
    fz_catch(ctx) {
        return false;
    }

    if (PageCount() == 0) {
        fz_warn(ctx, "document has no pages");
        return false;
    }

    _pages = AllocArray<fz_page*>(PageCount());
    _mediaboxes = AllocArray<RectD>(PageCount());
    imageRects = AllocArray<fz_rect*>(PageCount());

    if (!_pages || !_mediaboxes || !imageRects)
        return false;

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

fz_page* XpsEngineImpl::GetXpsPage(int pageNo, bool failIfBusy) {
    if (!_pages)
        return nullptr;
    if (failIfBusy)
        return _pages[pageNo - 1];

    ScopedCritSec scope(&_pagesAccess);

    fz_page* page = _pages[pageNo - 1];
    if (!page) {
        ScopedCritSec ctxScope(&ctxAccess);
        fz_var(page);
        fz_try(ctx) {
            // caution: two calls to xps_load_page return the
            // same xps_page object (without reference counting)
            // TODO(port): now has concept of chapters
            page = fz_load_page(ctx, _doc, pageNo - 1);
            _pages[pageNo - 1] = page;
            LinkifyPageText(page, pageNo);
            // TODO(port)
            // AssertCrash(page->links_resolved);
        }
        fz_catch(ctx) {
        }
    }

    return page;
}

int XpsEngineImpl::GetPageNo(fz_page* page) {
    for (int i = 0; i < PageCount(); i++)
        if (page == _pages[i])
            return i + 1;
    return 0;
}

XpsPageRun* XpsEngineImpl::CreatePageRun(fz_page* page, fz_display_list* list) {
    Vec<FitzImagePos> positions;

    // save the image rectangles for this page
    int pageNo = GetPageNo(page);
    if (!imageRects[pageNo - 1] && positions.size() > 0) {
        // the list of page image rectangles is terminated with a null-rectangle
        fz_rect* rects = AllocArray<fz_rect>(positions.size() + 1);
        if (rects) {
            for (size_t i = 0; i < positions.size(); i++) {
                rects[i] = positions.at(i).rect;
            }
            imageRects[pageNo - 1] = rects;
        }
    }

    return new XpsPageRun(page, list);
}

XpsPageRun* XpsEngineImpl::GetPageRun(fz_page* page, bool tryOnly) {
    ScopedCritSec scope(&_pagesAccess);

    XpsPageRun* result = nullptr;

    for (size_t i = 0; i < runCache.size(); i++) {
        if (runCache.at(i)->page == page) {
            result = runCache.at(i);
            break;
        }
    }
    if (!result && !tryOnly) {
        size_t mem = 0;
        for (size_t i = 0; i < runCache.size(); i++) {
            // drop page runs that take up too much memory due to huge images
            // (except for the very recently used ones)
            if (i >= 2 && mem + runCache.at(i)->size_est >= MAX_PAGE_RUN_MEMORY)
                DropPageRun(runCache.at(i--), true);
            else
                mem += runCache.at(i)->size_est;
        }
        if (runCache.size() >= MAX_PAGE_RUN_CACHE) {
            AssertCrash(runCache.size() == MAX_PAGE_RUN_CACHE);
            DropPageRun(runCache.Last(), true);
        }

        ScopedCritSec ctxScope(&ctxAccess);

        fz_display_list* list = nullptr;
        fz_device* dev = nullptr;
        fz_var(list);
        fz_var(dev);
        fz_try(ctx) {
            // TODO(port): calc mediabox
            list = fz_new_display_list(ctx, fz_infinite_rect);
            dev = fz_new_list_device(ctx, list);
            fz_run_page(ctx, page, dev, fz_identity, nullptr);
        }
        fz_catch(ctx) {
            fz_drop_display_list(ctx, list);
            list = nullptr;
        }
        fz_drop_device(ctx, dev);

        if (list) {
            result = CreatePageRun(page, list);
            runCache.InsertAt(0, result);
        }
    } else if (result && result != runCache.at(0)) {
        // keep the list Most Recently Used first
        runCache.Remove(result);
        runCache.InsertAt(0, result);
    }

    if (result)
        result->refs++;
    return result;
}

bool XpsEngineImpl::RunPage(fz_page* page, fz_device* dev, const fz_matrix* ctm, fz_rect cliprect, bool cacheRun,
                            FitzAbortCookie* cookie) {
    bool ok = true;
    CrashMePort();
#if 0
    XpsPageRun* run = GetPageRun(page, !cacheRun);
    if (run) {
        EnterCriticalSection(&ctxAccess);
        Vec<PageAnnotation> pageAnnots = fz_get_user_page_annots(userAnnots, GetPageNo(page));
        fz_try(ctx) {
            fz_rect pagerect;
            fz_begin_page(ctx, dev, xps_bound_page(_doc, page, &pagerect), ctm);
            fz_run_page_transparency(ctx, pageAnnots, dev, cliprect, false);
            fz_run_display_list(ctx, run->list, dev, ctm, cliprect, cookie ? &cookie->cookie : nullptr);
            fz_run_page_transparency(ctx, pageAnnots, dev, cliprect, true);
            fz_run_user_page_annots(ctx, pageAnnots, dev, ctm, cliprect, cookie ? &cookie->cookie : nullptr);
            fz_end_page(ctx, dev);
        }
        fz_catch(ctx) { ok = false; }
        LeaveCriticalSection(&ctxAccess);
        DropPageRun(run);
    } else {
        ScopedCritSec scope(&ctxAccess);
        Vec<PageAnnotation> pageAnnots = fz_get_user_page_annots(userAnnots, GetPageNo(page));
        fz_try(ctx) {
            fz_rect pagerect;
            fz_begin_page(ctx, dev, xps_bound_page(_doc, page, &pagerect), ctm);
            fz_run_page_transparency(ctx, pageAnnots, dev, cliprect, false);
            fz_run_page(ctx, _doc, page, dev, ctm, cookie ? &cookie->cookie : nullptr);
            fz_run_page_transparency(ctx, pageAnnots, dev, cliprect, true);
            fz_run_user_page_annots(ctx, pageAnnots, dev, ctm, cliprect, cookie ? &cookie->cookie : nullptr);
            fz_end_page(ctx, dev);
        }
        fz_catch(ctx) { ok = false; }
    }

    EnterCriticalSection(&ctxAccess);
    fz_drop_device(ctx, dev);
    LeaveCriticalSection(&ctxAccess);
#endif
    return ok && !(cookie && cookie->cookie.abort);
}

void XpsEngineImpl::DropPageRun(XpsPageRun* run, bool forceRemove) {
    ScopedCritSec scope(&_pagesAccess);
    run->refs--;

    if (0 == run->refs || forceRemove)
        runCache.Remove(run);

    if (0 == run->refs) {
        ScopedCritSec ctxScope(&ctxAccess);
        fz_drop_display_list(ctx, run->list);
        delete run;
    }
}

RectD XpsEngineImpl::PageMediabox(int pageNo) {
    AssertCrash(1 <= pageNo && pageNo <= PageCount());
    if (!_mediaboxes)
        return RectD();

    RectD mbox = _mediaboxes[pageNo - 1];
    if (!mbox.IsEmpty())
        return mbox;

    fz_page* page = GetXpsPage(pageNo, true);
    if (!page) {
        ScopedCritSec scope(&ctxAccess);
        fz_try(ctx) {
            mbox = fz_rect_to_RectD(fz_bound_page(ctx, page));
        }
        fz_catch(ctx) {
        }
        if (!mbox.IsEmpty()) {
            _mediaboxes[pageNo - 1] = mbox;
            return _mediaboxes[pageNo - 1];
        }
    }
    if (!page && (page = GetXpsPage(pageNo)) == nullptr)
        return RectD();

    _mediaboxes[pageNo - 1] = fz_rect_to_RectD(fz_bound_page(ctx, page));
    return _mediaboxes[pageNo - 1];
}

RectD XpsEngineImpl::PageContentBox(int pageNo, RenderTarget target) {
    UNUSED(target);
    AssertCrash(1 <= pageNo && pageNo <= PageCount());
    fz_page* page = GetXpsPage(pageNo);
    if (!page)
        return RectD();

    fz_rect rect = fz_empty_rect;
    fz_device* dev = nullptr;
    EnterCriticalSection(&ctxAccess);
    fz_try(ctx) {
        dev = fz_new_bbox_device(ctx, &rect);
    }
    fz_catch(ctx) {
        LeaveCriticalSection(&ctxAccess);
        return RectD();
    }
    LeaveCriticalSection(&ctxAccess);

    fz_rect pagerect = fz_bound_page(ctx, page);
    bool ok = RunPage(page, dev, &fz_identity, pagerect, false);
    if (!ok)
        return PageMediabox(pageNo);
    if (fz_is_infinite_rect(rect))
        return PageMediabox(pageNo);

    RectD rect2 = fz_rect_to_RectD(rect);
    return rect2.Intersect(PageMediabox(pageNo));
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
    UNUSED(target);
    fz_page* page = GetXpsPage(pageNo);
    if (!page)
        return nullptr;

    fz_rect pRect;
    if (pageRect)
        pRect = fz_RectD_to_rect(*pageRect);
    else
        pRect = fz_bound_page(ctx, page);
    fz_matrix ctm = viewctm(page, zoom, rotation);
    fz_rect r = pRect;
    fz_irect bbox = fz_round_rect(fz_transform_rect(r, ctm));

    fz_pixmap* image = nullptr;
    EnterCriticalSection(&ctxAccess);
    fz_try(ctx) {
        fz_colorspace* colorspace = fz_device_rgb(ctx);
        image = fz_new_pixmap_with_bbox(ctx, colorspace, bbox, nullptr, 1);
        fz_clear_pixmap_with_value(ctx, image, 0xFF); // initialize white background
    }
    fz_catch(ctx) {
        LeaveCriticalSection(&ctxAccess);
        return nullptr;
    }

    fz_device* dev = nullptr;
    fz_try(ctx) {
        dev = fz_new_draw_device(ctx, fz_identity, image);
    }
    fz_catch(ctx) {
        fz_drop_pixmap(ctx, image);
        LeaveCriticalSection(&ctxAccess);
        return nullptr;
    }
    LeaveCriticalSection(&ctxAccess);

    FitzAbortCookie* cookie = nullptr;
    if (cookie_out)
        *cookie_out = cookie = new FitzAbortCookie();
    fz_rect cliprect = fz_rect_from_irect(bbox);
    bool ok = RunPage(page, dev, &ctm, cliprect, true, cookie);

    ScopedCritSec scope(&ctxAccess);

    RenderedBitmap* bitmap = nullptr;
    if (ok)
        bitmap = new_rendered_fz_pixmap(ctx, image);
    fz_drop_pixmap(ctx, image);
    return bitmap;
}

WCHAR* XpsEngineImpl::ExtractPageText(fz_page* page, const WCHAR* lineSep, RectI** coordsOut, bool cacheRun) {
    if (!page)
        return nullptr;
    CrashMePort();
#if 0
    fz_text_sheet* sheet = nullptr;
    fz_text_page* text = nullptr;
    fz_device* dev = nullptr;
    fz_var(sheet);
    fz_var(text);

    EnterCriticalSection(&ctxAccess);
    fz_try(ctx) {
        sheet = fz_new_text_sheet(ctx);
        text = fz_new_text_page(ctx);
        dev = fz_new_text_device(ctx, sheet, text);
    }
    fz_catch(ctx) {
        fz_drop_text_page(ctx, text);
        fz_drop_text_sheet(ctx, sheet);
        LeaveCriticalSection(&ctxAccess);
        return nullptr;
    }
    LeaveCriticalSection(&ctxAccess);

    if (!cacheRun)
        fz_enable_device_hints(dev, FZ_NO_CACHE);

    // use an infinite rectangle as bounds (instead of a mediabox) to ensure that
    // the extracted text is consistent between cached runs using a list device and
    // fresh runs (otherwise the list device omits text outside the mediabox bounds)
    RunPage(page, dev, &fz_identity, nullptr, cacheRun);

    ScopedCritSec scope(&ctxAccess);

    WCHAR* content = fz_text_page_to_str(text, lineSep, coordsOut);
    fz_drop_text_page(ctx, text);
    fz_drop_text_sheet(ctx, sheet);
    return content;
#endif
    return nullptr;
}

u8* XpsEngineImpl::GetFileData(size_t* cbCount) {
    u8* res = nullptr;
    ScopedCritSec scope(&ctxAccess);
    fz_try(ctx) {
        res = fz_extract_stream_data(ctx, _docStream, cbCount);
    }
    fz_catch(ctx) {
        res = nullptr;
        if (FileName()) {
            OwnedData data(file::ReadFile(FileName()));
            if (cbCount) {
                *cbCount = data.size;
            }
            res = (u8*)data.StealData();
        }
    }
    return res;
}

bool XpsEngineImpl::SaveFileAs(const char* copyFileName, bool includeUserAnnots) {
    UNUSED(includeUserAnnots);
    size_t dataLen;
    AutoFreeW dstPath(str::conv::FromUtf8(copyFileName));
    ScopedMem<unsigned char> data(GetFileData(&dataLen));
    if (data) {
        bool ok = file::WriteFile(dstPath, data.Get(), dataLen);
        if (ok)
            return true;
    }
    if (!FileName())
        return false;
    return CopyFileW(FileName(), dstPath, FALSE);
}

WCHAR* XpsEngineImpl::ExtractFontList() {
    // load and parse all pages
    for (int i = 1; i <= PageCount(); i++) {
        GetXpsPage(i);
    }

    ScopedCritSec scope(&ctxAccess);

    // collect a list of all included fonts
    WStrVec fonts;
    CrashMePort();
#if 0
    for (xps_font_cache* font = _doc->font_table; font; font = font->next) {
        AutoFreeW path(str::conv::FromUtf8(font->name));
        AutoFreeW name(str::conv::FromUtf8(font->font->name));
        fonts.Append(str::Format(L"%s (%s)", name.Get(), path::GetBaseName(path)));
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
    fz_page* page = GetXpsPage(pageNo, true);
    if (!page)
        return nullptr;
    CrashMePort();
#if 0
    fz_point p = {(float)pt.x, (float)pt.y};
    for (fz_link* link = page->links; link; link = link->next)
        if (fz_is_pt_in_rect(link->rect, p))
            return new XpsLink(this, &link->dest, link->rect, pageNo);

    if (imageRects[pageNo - 1]) {
        for (int i = 0; !fz_is_empty_rect(imageRects[pageNo - 1][i]); i++)
            if (fz_is_pt_in_rect(imageRects[pageNo - 1][i], p))
                return new XpsImage(this, pageNo, imageRects[pageNo - 1][i], i);
    }
#endif
    return nullptr;
}

Vec<PageElement*>* XpsEngineImpl::GetElements(int pageNo) {
    fz_page* page = GetXpsPage(pageNo, true);
    if (!page)
        return nullptr;
    // since all elements lists are in last-to-first order, append
    // item types in inverse order and reverse the whole list at the end
    Vec<PageElement*>* els = new Vec<PageElement*>();
    if (!els)
        return nullptr;

    CrashMePort();
#if 0
    if (imageRects[pageNo - 1]) {
        for (int i = 0; !fz_is_empty_rect(imageRects[pageNo - 1][i]); i++) {
            els->Append(new XpsImage(this, pageNo, imageRects[pageNo - 1][i], i));
        }
    }

    for (fz_link* link = page->links; link; link = link->next) {
        els->Append(new XpsLink(this, &link->dest, link->rect, pageNo));
    }
#endif
    els->Reverse();
    return els;
}

void XpsEngineImpl::LinkifyPageText(fz_page* page, int pageNo) {
    UNUSED(pageNo);
    // make MuXPS extract all links and named destinations from the page
    AssertCrash(!GetPageRun(page, true));
    CrashMePort();
#if 0
    XpsPageRun* run = GetPageRun(page);
    AssertCrash(!run == !page->links_resolved);
    if (run)
        DropPageRun(run);
    else
        page->links_resolved = 1;
    AssertCrash(!page->links || page->links->refs == 1);

    RectI* coords;
    AutoFreeW pageText(ExtractPageText(page, L"\n", &coords, true));
    if (!pageText)
        return;

    LinkRectList* list = LinkifyText(pageText, coords);
    for (size_t i = 0; i < list->links.size(); i++) {
        bool overlaps = false;
        for (fz_link* next = page->links; next && !overlaps; next = next->next)
            overlaps = fz_calc_overlap(list->coords.at(i), next->rect) >= 0.25f;
        if (!overlaps) {
            OwnedData uri(str::conv::ToUtf8(list->links.at(i)));
            if (!uri.Get())
                continue;
            fz_link_dest ld = {FZ_LINK_URI, 0};
            ld.ld.uri.uri = fz_strdup(ctx, uri.Get());
            // add links in top-to-bottom order (i.e. last-to-first)
            fz_link* link = fz_new_link(ctx, &list->coords.at(i), ld);
            CrashIf(!link); // TODO: if fz_new_link throws, there are memory leaks
            link->next = page->links;
            page->links = link;
        }
    }

    delete list;
    free(coords);
#endif
}

RenderedBitmap* XpsEngineImpl::GetPageImage(int pageNo, RectD rect, size_t imageIdx) {
    fz_page* page = GetXpsPage(pageNo);
    if (!page)
        return nullptr;

    Vec<FitzImagePos> positions;

    if (imageIdx >= positions.size() || fz_rect_to_RectD(positions.at(imageIdx).rect) != rect) {
        AssertCrash(0);
        return nullptr;
    }

    ScopedCritSec scope(&ctxAccess);

    fz_pixmap* pixmap = nullptr;
    fz_try(ctx) {
        fz_image* image = positions.at(imageIdx).image;
        // TODO(port): pass dimensions?
        CrashMePort();
        pixmap = fz_get_pixmap_from_image(ctx, image, nullptr, nullptr, nullptr, nullptr);
    }
    fz_catch(ctx) {
        return nullptr;
    }
    RenderedBitmap* bmp = new_rendered_fz_pixmap(ctx, pixmap);
    fz_drop_pixmap(ctx, pixmap);

    return bmp;
}

fz_rect XpsEngineImpl::FindDestRect(const char* target) {
    if (str::IsEmpty(target))
        return fz_empty_rect;

    CrashMePort();
#if 0
    xps_target* found = xps_lookup_link_target_obj(_doc, (char*)target);
    if (!found)
        return fz_empty_rect;
    if (fz_is_empty_rect(found->rect)) {
        // ensure that the target rectangle could have been
        // updated through LinkifyPageText -> xps_extract_anchor_info
        GetXpsPage(found->page + 1);
    }
    return found->rect;
#endif
    return fz_empty_rect;
}

PageDestination* XpsEngineImpl::GetNamedDest(const WCHAR* name) {
    CrashMePort();
#if 0
    OwnedData name_utf8(str::conv::ToUtf8(name));
    if (!str::StartsWith(name_utf8.Get(), "#")) {
        name_utf8.TakeOwnership(str::Join("#", name_utf8.Get()));
    }

    for (xps_target* dest = _doc->target; dest; dest = dest->next) {
        if (str::EndsWithI(dest->name, name_utf8.Get())) {
            return new SimpleDest(dest->page + 1, fz_rect_to_RectD(dest->rect));
        }
    }
#endif
    return nullptr;
}

DocTocTree* XpsEngineImpl::BuildTocTree(fz_outline* entry, int& idCounter) {
    XpsTocItem* node = nullptr;
    CrashMePort();
#if 0
    for (; entry; entry = entry->next) {
        WCHAR* name = entry->title ? str::conv::FromUtf8(entry->title) : str::Dup(L"");
        XpsTocItem* item = new XpsTocItem(name, XpsLink(this, &entry->dest));
        item->id = ++idCounter;
        item->open = entry->is_open;

        if (FZ_LINK_GOTO == entry->dest.kind)
            item->pageNo = entry->dest.ld.gotor.page + 1;
        if (entry->down)
            item->child = BuildTocTree(entry->down, idCounter);

        if (!node)
            node = item;
        else
            node->AddSibling(item);
    }
#endif

    return new DocTocTree(node);
}

DocTocTree* XpsEngineImpl::GetTocTree() {
    if (!HasTocTree())
        return nullptr;

    int idCounter = 0;
    return BuildTocTree(_outline, idCounter);
}

bool XpsEngineImpl::HasClipOptimizations(int pageNo) {
    fz_page* page = GetXpsPage(pageNo, true);
    // GetXpsPage extracts imageRects for us
    if (!page || !imageRects[pageNo - 1])
        return true;

    fz_rect mbox = fz_RectD_to_rect(PageMediabox(pageNo));
    // check if any image covers at least 90% of the page
    for (int i = 0; !fz_is_empty_rect(imageRects[pageNo - 1][i]); i++)
        if (fz_calc_overlap(mbox, imageRects[pageNo - 1][i]) >= 0.9f)
            return false;
    return true;
}

BaseEngine* XpsEngineImpl::CreateFromFile(const WCHAR* fileName) {
    XpsEngineImpl* engine = new XpsEngineImpl();
    if (!engine || !fileName || !engine->Load(fileName)) {
        delete engine;
        return nullptr;
    }
    return engine;
}

BaseEngine* XpsEngineImpl::CreateFromStream(IStream* stream) {
    XpsEngineImpl* engine = new XpsEngineImpl();
    if (!engine->Load(stream)) {
        delete engine;
        return nullptr;
    }
    return engine;
}

namespace XpsEngine {

bool IsSupportedFile(const WCHAR* fileName, bool sniff) {
    if (!sniff) {
        return str::EndsWithI(fileName, L".xps") || str::EndsWithI(fileName, L".oxps");
    }

    if (dir::Exists(fileName)) {
        // allow opening uncompressed XPS files as well
        AutoFreeW relsPath(path::Join(fileName, L"_rels\\.rels"));
        return file::Exists(relsPath) || dir::Exists(relsPath);
    }

    Archive* archive = OpenZipArchive(fileName, true);
    bool res = archive->GetFileId("_rels/.rels") != (size_t)-1 ||
               archive->GetFileId("_rels/.rels/[0].piece") != (size_t)-1 ||
               archive->GetFileId("_rels/.rels/[0].last.piece") != (size_t)-1;
    delete archive;
    return res;
}

BaseEngine* CreateFromFile(const WCHAR* fileName) {
    return XpsEngineImpl::CreateFromFile(fileName);
}

BaseEngine* CreateFromStream(IStream* stream) {
    return XpsEngineImpl::CreateFromStream(stream);
}

} // namespace XpsEngine
