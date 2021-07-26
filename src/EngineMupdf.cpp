/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

extern "C" {
#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
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

#include "AppColors.h"
#include "wingui/TreeModel.h"

#include "EngineBase.h"
#include "EngineFzUtil.h"
#include "EngineMupdf.h"

#include "utils/Log.h"

// in mupdf_load_system_font.c
extern "C" void drop_cached_fonts_for_ctx(fz_context*);
extern "C" void pdf_install_load_system_font_funcs(fz_context* ctx);

Kind kindEngineMupdf = "engineMupdf";

static float layout_w = (float)FZ_DEFAULT_LAYOUT_W;
static float layout_h = (float)FZ_DEFAULT_LAYOUT_H;
static float layout_em = 28.f;

class EngineMupdf : public EngineBase {
  public:
    EngineMupdf();
    ~EngineMupdf() override;
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

    bool BenchLoadPage(int pageNo) override;

    Vec<IPageElement*>* GetElements(int pageNo) override;
    IPageElement* GetElementAtPos(int pageNo, PointF pt) override;
    void PerformPageAction(IPageElement* el, PageElementAction* action) override {
    }

    RenderedBitmap* GetImageForPageElement(IPageElement*) override;

    PageDestination* GetNamedDest(const WCHAR* name) override;
    TocTree* GetToc() override;

    WCHAR* GetPageLabel(int pageNo) const override;
    int GetPageByLabel(const WCHAR* label) const override;

    static EngineBase* CreateFromFile(const WCHAR* path, PasswordUI* pwdUI);
    static EngineBase* CreateFromStream(IStream* stream, PasswordUI* pwdUI);

    // make sure to never ask for pagesAccess in an ctxAccess
    // protected critical section in order to avoid deadlocks
    CRITICAL_SECTION* ctxAccess;
    CRITICAL_SECTION pagesAccess;

    CRITICAL_SECTION mutexes[FZ_LOCK_MAX];

    RenderedBitmap* GetPageImage(int pageNo, RectF rect, int imageIdx);

    fz_context* ctx = nullptr;
    fz_locks_context fz_locks_ctx;
    fz_document* _doc = nullptr;
    pdf_document* pdfdoc = nullptr;
    fz_stream* _docStream = nullptr;
    Vec<FzPageInfo*> _pages;
    fz_outline* outline = nullptr;
    fz_outline* attachments = nullptr;

    WStrVec* _pageLabels = nullptr;

    TocTree* tocTree = nullptr;

    bool Load(const WCHAR* filePath, PasswordUI* pwdUI = nullptr);
    bool Load(IStream* stream, PasswordUI* pwdUI = nullptr);
    // TODO(port): fz_stream can no-longer be re-opened (fz_clone_stream)
    // bool Load(fz_stream* stm, PasswordUI* pwdUI = nullptr);
    bool LoadFromStream(fz_stream* stm, PasswordUI* pwdUI = nullptr);
    bool FinishLoading();

    FzPageInfo* GetFzPageInfoFast(int pageNo);
    FzPageInfo* GetFzPageInfo(int pageNo, bool loadQuick);
    fz_matrix viewctm(int pageNo, float zoom, int rotation);
    fz_matrix viewctm(fz_page* page, float zoom, int rotation) const;
    TocItem* BuildTocTree(TocItem* parent, fz_outline* outline, int& idCounter, bool isAttachment);
};

static void fz_lock_context_cs(void* user, int lock) {
    EngineMupdf* e = (EngineMupdf*)user;
    EnterCriticalSection(&e->mutexes[lock]);
}

static void fz_unlock_context_cs(void* user, int lock) {
    EngineMupdf* e = (EngineMupdf*)user;
    LeaveCriticalSection(&e->mutexes[lock]);
}

static void fz_print_cb(void* user, const char* msg) {
    log(msg);
    if (!str::EndsWith(msg, "\n")) {
        log("\n");
    }
}

static void installFitzErrorCallbacks(fz_context* ctx) {
    fz_set_warning_callback(ctx, fz_print_cb, nullptr);
    fz_set_error_callback(ctx, fz_print_cb, nullptr);
}

EngineMupdf::EngineMupdf() {
    kind = kindEngineMupdf;
    defaultFileExt = L".epub";
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

    pdf_install_load_system_font_funcs(ctx);
    fz_register_document_handlers(ctx);
}

EngineMupdf::~EngineMupdf() {
    EnterCriticalSection(&pagesAccess);

    // TODO: remove this lock and see what happens
    EnterCriticalSection(ctxAccess);

    for (auto* pi : _pages) {
        if (pi->links) {
            fz_drop_link(ctx, pi->links);
        }
        if (pi->page) {
            fz_drop_page(ctx, pi->page);
        }
        DeleteVecMembers(pi->autoLinks);
        DeleteVecMembers(pi->comments);
    }

    DeleteVecMembers(_pages);

    fz_drop_outline(ctx, outline);

    fz_drop_document(ctx, _doc);
    drop_cached_fonts_for_ctx(ctx);
    fz_drop_context(ctx);

    for (size_t i = 0; i < dimof(mutexes); i++) {
        LeaveCriticalSection(&mutexes[i]);
        DeleteCriticalSection(&mutexes[i]);
    }
    LeaveCriticalSection(&pagesAccess);
    DeleteCriticalSection(&pagesAccess);
}

EngineBase* EngineMupdf::Clone() {
    CrashIf(true);
    return nullptr;
}

bool EngineMupdf::Load(const WCHAR* filePath, PasswordUI* pwdUI) {
    CrashIf(FileName() || _doc || !ctx);
    SetFileName(filePath);
    if (!ctx) {
        return false;
    }

    fz_stream* file = nullptr;
    fz_try(ctx) {
        file = fz_open_file2(ctx, filePath);
    }
    fz_catch(ctx) {
        file = nullptr;
    }

    if (!LoadFromStream(file, pwdUI)) {
        return false;
    }

    return FinishLoading();
}

bool EngineMupdf::Load(IStream* stream, PasswordUI* pwdUI) {
    CrashIf(FileName() || _doc || !ctx);
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
    if (!LoadFromStream(stm, pwdUI)) {
        return false;
    }
    return FinishLoading();
}

bool EngineMupdf::LoadFromStream(fz_stream* stm, PasswordUI* pwdUI) {
    if (!stm) {
        return false;
    }

    _doc = nullptr;
    fz_try(ctx) {
        char* fileNameA = ToUtf8Temp(FileName());
        _doc = fz_open_document_with_stream(ctx, fileNameA, stm);
        pdfdoc = pdf_specifics(ctx, _doc);
        fz_layout_document(ctx, _doc, layout_w, layout_h, layout_em);
    }
    fz_always(ctx) {
        fz_drop_stream(ctx, stm);
    }
    fz_catch(ctx) {
        return false;
    }

    _docStream = stm;

    isPasswordProtected = fz_needs_password(ctx, _doc);
    if (!isPasswordProtected) {
        return true;
    }

    if (!pwdUI) {
        return false;
    }

    bool ok = true;

    if (pdfdoc) {
        ok = false;
        u8 digest[16 + 32] = {0};
        fz_stream_fingerprint(ctx, pdfdoc->file, digest);

        bool saveKey = false;
        while (!ok) {
            AutoFreeWstr pwd(pwdUI->GetPassword(FileName(), digest, pdf_crypt_key(ctx, pdfdoc->crypt), &saveKey));
            if (!pwd) {
                // password not given or encryption key has been remembered
                ok = saveKey;
                break;
            }

            // MuPDF expects passwords to be UTF-8 encoded
            AutoFree pwdA(strconv::WstrToUtf8(pwd));
            ok = fz_authenticate_password(ctx, _doc, pwdA.Get());
            // according to the spec (1.7 ExtensionLevel 3), the password
            // for crypt revisions 5 and above are in SASLprep normalization
            if (!ok) {
                // TODO: this is only part of SASLprep
                pwd.Set(NormalizeString(pwd, 5 /* NormalizationKC */));
                if (pwd) {
                    pwdA = strconv::WstrToUtf8(pwd);
                    ok = fz_authenticate_password(ctx, _doc, pwdA.Get());
                }
            }
            // older Acrobat versions seem to have considered passwords to be in codepage 1252
            // note: such passwords aren't portable when stored as Unicode text
            if (!ok && GetACP() != 1252) {
                AutoFree pwd_ansi(strconv::WstrToAnsiV(pwd));
                AutoFreeWstr pwd_cp1252(strconv::StrToWstr(pwd_ansi.Get(), 1252));
                pwdA = strconv::WstrToUtf8(pwd_cp1252);
                ok = fz_authenticate_password(ctx, _doc, pwdA.Get());
            }
        }

        if (ok && saveKey) {
            memcpy(digest + 16, pdf_crypt_key(ctx, pdfdoc->crypt), 32);
            decryptionKey = _MemToHex(&digest);
        }
    }

    return ok;
}

bool EngineMupdf::FinishLoading() {
    pageCount = 0;
    fz_try(ctx) {
        // this call might throw the first time
        pageCount = fz_count_pages(ctx, _doc);
    }
    fz_catch(ctx) {
        return false;
    }
    if (pageCount == 0) {
        fz_warn(ctx, "document has no pages");
        return false;
    }

    // preferredLayout = GetPreferredLayout(ctx, doc);
    allowsPrinting = fz_has_permission(ctx, _doc, FZ_PERMISSION_PRINT);
    allowsCopyingText = fz_has_permission(ctx, _doc, FZ_PERMISSION_COPY);

    ScopedCritSec scope(ctxAccess);

    for (int i = 0; i < pageCount; i++) {
        fz_rect mbox{};
        fz_matrix page_ctm{};
        fz_try(ctx) {
            fz_page* page = fz_load_page(ctx, _doc, i);
            mbox = fz_bound_page(ctx, page);
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
        FzPageInfo* pageInfo = new FzPageInfo();
        pageInfo->mediabox = ToRectFl(mbox);
        pageInfo->pageNo = i + 1;
        _pages.Append(pageInfo);
    }

    fz_try(ctx) {
        outline = fz_load_outline(ctx, _doc);
    }
    fz_catch(ctx) {
        // ignore errors from pdf_load_outline()
        // this information is not critical and checking the
        // error might prevent loading some pdfs that would
        // otherwise get displayed
        fz_warn(ctx, "Couldn't load outline");
    }

    return true;
}

TocItem* EngineMupdf::BuildTocTree(TocItem* parent, fz_outline* outline, int& idCounter, bool isAttachment) {
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

        PageDestination* dest = nullptr;
        Kind kindRaw = nullptr;
        if (isAttachment) {
            continue;
            //kindRaw = kindTocFzOutlineAttachment;
            //dest = destFromAttachment(this, outline);
        } else {
            kindRaw = kindTocFzOutline;
            dest = newFzDestination(outline);
        }

        TocItem* item = newTocItemWithDestination(parent, name, dest);
        item->kindRaw = kindRaw;
        item->rawVal1 = str::Dup(outline->title);
        item->rawVal2 = str::Dup(outline->uri);

        free(name);
        item->isOpenDefault = outline->is_open;
        item->id = ++idCounter;
        item->fontFlags = outline->flags;
        item->pageNo = pageNo;
        CrashIf(!item->PageNumbersMatch());

        if (outline->n_color > 0) {
            item->color = ColorRefFromPdfFloat(ctx, outline->n_color, outline->color);
        }

        if (outline->down) {
            item->child = BuildTocTree(item, outline->down, idCounter, isAttachment);
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

// TODO: maybe build in FinishLoading
TocTree* EngineMupdf::GetToc() {
    if (tocTree) {
        return tocTree;
    }
    if (outline == nullptr && attachments == nullptr) {
        return nullptr;
    }

    int idCounter = 0;

    TocItem* root = nullptr;
    TocItem* att = nullptr;
    if (outline) {
        root = BuildTocTree(nullptr, outline, idCounter, false);
    }
    if (!attachments) {
        goto MakeTree;
    }
    att = BuildTocTree(nullptr, attachments, idCounter, true);
    if (root) {
        root->AddSiblingAtEnd(att);
    } else {
        root = att;
    }
MakeTree:
    if (!root) {
        return nullptr;
    }
    TocItem* realRoot = new TocItem();
    realRoot->child = root;
    tocTree = new TocTree(realRoot);
    return tocTree;
}

PageDestination* EngineMupdf::GetNamedDest(const WCHAR* name) {
    ScopedCritSec scope1(&pagesAccess);
    ScopedCritSec scope2(ctxAccess);

    pdf_document* doc = (pdf_document*)_doc;

    auto nameA(ToUtf8Temp(name));
    pdf_obj* dest = nullptr;

    fz_var(dest);
    fz_try(ctx) {
        pdf_obj* nameobj = pdf_new_string(ctx, nameA.Get(), (int)nameA.size());
        dest = pdf_lookup_dest(ctx, doc, nameobj);
        pdf_drop_obj(ctx, nameobj);
    }
    fz_catch(ctx) {
        return nullptr;
    }

    if (!dest) {
        return nullptr;
    }

    PageDestination* pageDest = nullptr;
    char* uri = nullptr;

    fz_var(uri);
    fz_try(ctx) {
        uri = pdf_parse_link_dest(ctx, doc, dest);
    }
    fz_catch(ctx) {
        return nullptr;
    }

    if (!uri) {
        return nullptr;
    }

    float x, y, zoom = 0;
    int pageNo = resolve_link(uri, &x, &y, &zoom);

    RectF r{x, y, 0, 0};
    pageDest = newSimpleDest(pageNo, r);
    if (zoom) {
        pageDest->zoom = zoom;
    }
    fz_free(ctx, uri);
    return pageDest;
}

// return a page but only if is fully loaded
FzPageInfo* EngineMupdf::GetFzPageInfoFast(int pageNo) {
    ScopedCritSec scope(&pagesAccess);
    CrashIf(pageNo < 1 || pageNo > pageCount);
    FzPageInfo* pageInfo = _pages[pageNo - 1];
    if (!pageInfo->page || !pageInfo->fullyLoaded) {
        return nullptr;
    }
    return pageInfo;
}

static fz_link* FixupPageLinks(fz_link* root) {
    // Links in PDF documents are added from bottom-most to top-most,
    // i.e. links that appear later in the list should be preferred
    // to links appearing before. Since we search from the start of
    // the (single-linked) list, we have to reverse the order of links
    // (http://code.google.com/p/sumatrapdf/issues/detail?id=1303 )
    fz_link* new_root = nullptr;
    while (root) {
        fz_link* tmp = root->next;
        root->next = new_root;
        new_root = root;
        root = tmp;

        // there are PDFs that have x,y positions in reverse order, so fix them up
        fz_link* link = new_root;
        if (link->rect.x0 > link->rect.x1) {
            std::swap(link->rect.x0, link->rect.x1);
        }
        if (link->rect.y0 > link->rect.y1) {
            std::swap(link->rect.y0, link->rect.y1);
        }
        CrashIf(link->rect.x1 < link->rect.x0);
        CrashIf(link->rect.y1 < link->rect.y0);
    }
    return new_root;
}

// Maybe: handle FZ_ERROR_TRYLATER, which can happen when parsing from network.
// (I don't think we read from network now).
// Maybe: when loading fully, cache extracted text in FzPageInfo
// so that we don't have to re-do fz_new_stext_page_from_page() when doing search
FzPageInfo* EngineMupdf::GetFzPageInfo(int pageNo, bool loadQuick) {
    // TODO: minimize time spent under pagesAccess when fully loading
    ScopedCritSec scope(&pagesAccess);

    CrashIf(pageNo < 1 || pageNo > pageCount);
    int pageIdx = pageNo - 1;
    FzPageInfo* pageInfo = _pages[pageIdx];

    ScopedCritSec ctxScope(ctxAccess);
    if (!pageInfo->page) {
        fz_try(ctx) {
            pageInfo->page = fz_load_page(ctx, _doc, pageIdx);
        }
        fz_catch(ctx) {
        }
    }

    fz_page* page = pageInfo->page;
    if (!page) {
        return nullptr;
    }

    if (pageInfo->commentsNeedRebuilding) {
        DeleteVecMembers(pageInfo->comments);
        //MakePageElementCommentsFromAnnotations(ctx, pageInfo);
        pageInfo->commentsNeedRebuilding = false;
    }

    if (loadQuick || pageInfo->fullyLoaded) {
        return pageInfo;
    }

    CrashIf(pageInfo->pageNo != pageNo);

    pageInfo->fullyLoaded = true;

    fz_stext_page* stext = nullptr;
    fz_var(stext);
    fz_stext_options opts{};
    opts.flags = FZ_STEXT_PRESERVE_IMAGES;
    fz_try(ctx) {
        stext = fz_new_stext_page_from_page(ctx, page, &opts);
    }
    fz_catch(ctx) {
    }

    auto links = fz_load_links(ctx, page);

    pageInfo->links = FixupPageLinks(links);
    //MakePageElementCommentsFromAnnotations(ctx, pageInfo);
    if (!stext) {
        return pageInfo;
    }

    FzLinkifyPageText(pageInfo, stext);
    fz_find_image_positions(ctx, pageInfo->images, stext);
    fz_drop_stext_page(ctx, stext);
    return pageInfo;
}

RectF EngineMupdf::PageMediabox(int pageNo) {
    FzPageInfo* pi = _pages[pageNo - 1];
    return pi->mediabox;
}

RectF EngineMupdf::PageContentBox(int pageNo, RenderTarget target) {
    FzPageInfo* pageInfo = GetFzPageInfo(pageNo, false);
    if (!pageInfo) {
        // maybe should return a dummy size. not sure how this
        // will play with layout. The page should fail to render
        // since the doc is broken and page is missing
        return RectF();
    }

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
        list = fz_new_display_list_from_page(ctx, pageInfo->page);
        if (list) {
            dev = fz_new_bbox_device(ctx, &rect);
            fz_run_display_list(ctx, list, dev, fz_identity, pagerect, &fzcookie);
            fz_close_device(ctx, dev);
        }
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

    if (!list) {
        return mediabox;
    }

    if (fz_is_infinite_rect(rect)) {
        return mediabox;
    }

    RectF rect2 = ToRectFl(rect);
    return rect2.Intersect(mediabox);
}

RectF EngineMupdf::Transform(const RectF& rect, int pageNo, float zoom, int rotation, bool inverse) {
    if (zoom <= 0) {
        char* name = str::Dup("");
        const WCHAR* nameW = FileName();
        if (nameW) {
            name = strconv::WstrToUtf8(nameW);
        }
        logf("doc: %s, pageNo: %d, zoom: %.2f\n", name, pageNo, zoom);
        free(name);
    }
    ReportIf(zoom <= 0);
    if (zoom <= 0) {
        zoom = 1;
    }
    fz_matrix ctm = viewctm(pageNo, zoom, rotation);
    if (inverse) {
        ctm = fz_invert_matrix(ctm);
    }
    fz_rect rect2 = To_fz_rect(rect);
    rect2 = fz_transform_rect(rect2, ctm);
    return ToRectFl(rect2);
}

RenderedBitmap* EngineMupdf::RenderPage(RenderPageArgs& args) {
    auto pageNo = args.pageNo;

    FzPageInfo* pageInfo = GetFzPageInfo(pageNo, false);
    if (!pageInfo || !pageInfo->page) {
        return nullptr;
    }
    fz_page* page = pageInfo->page;

    fz_cookie* fzcookie = nullptr;
    FitzAbortCookie* cookie = nullptr;
    if (args.cookie_out) {
        cookie = new FitzAbortCookie();
        *args.cookie_out = cookie;
        fzcookie = &cookie->cookie;
    }

    // TODO(port): I don't see why this lock is needed
    ScopedCritSec cs(ctxAccess);

    auto pageRect = args.pageRect;
    auto zoom = args.zoom * 120;
    auto rotation = args.rotation;
    //fz_rect pRect;
    fz_rect page_bounds;
    if (pageRect) {
        //pRect = To_fz_rect(*pageRect);
        page_bounds = To_fz_rect(*pageRect);
    } else {
        // TODO(port): use pageInfo->mediabox?
        //pRect = fz_bound_page(ctx, page);
        page_bounds = fz_bound_page(ctx, page);
    }

    //page_bounds = fz_bound_page(ctx, page);
    fz_matrix draw_page_ctm = fz_transform_page(page_bounds, zoom, rotation);
    fz_rect draw_page_bounds = fz_transform_rect(page_bounds, draw_page_ctm);

    //fz_matrix ctm = viewctm(page, zoom, rotation);
    //fz_irect bbox = fz_round_rect(fz_transform_rect(pRect, ctm));


    fz_colorspace* colorspace = fz_device_rgb(ctx);
    //fz_irect ibounds = bbox;
    //fz_rect cliprect = fz_rect_from_irect(bbox);

    fz_pixmap* pix = nullptr;
    fz_device* dev = nullptr;
    RenderedBitmap* bitmap = nullptr;
    fz_separations* seps = NULL;
    fz_pixmap* page_contents = NULL;
    fz_page* fzpage = page;

    fz_var(dev);
    fz_var(pix);
    fz_var(page_contents);
    fz_var(bitmap);

    const char* usage = "View";
    switch (args.target) {
        case RenderTarget::Print:
            usage = "Print";
            break;
    }

    fz_try(ctx) {

        {
            fz_irect bbox = fz_round_rect(fz_transform_rect(fz_bound_page(ctx, fzpage), draw_page_ctm));
            page_contents = fz_new_pixmap_with_bbox(ctx, fz_device_rgb(ctx), bbox, seps, 0);
            // pix = fz_new_pixmap_with_bbox(ctx, colorspace, ibounds, nullptr, 1);
            // initialize with white background
            // fz_clear_pixmap_with_value(ctx, pix, 0xff);

            fz_clear_pixmap(ctx, page_contents);

            // TODO: in printing different style. old code use pdf_run_page_with_usage(), with usage ="View"
            // or "Print". "Export" is not used
            // dev = fz_new_draw_device(ctx, fz_identity, pix);
            dev = fz_new_draw_device(ctx, draw_page_ctm, page_contents);
            fz_run_page_contents(ctx, fzpage, dev, fz_identity, NULL);
            fz_close_device(ctx, dev);
            fz_drop_device(ctx, dev);
        }

    	pix = fz_clone_pixmap_area_with_different_seps(ctx, page_contents, NULL, fz_device_rgb(ctx), NULL,
                                                       fz_default_color_params, NULL);
        {
            dev = fz_new_draw_device(ctx, draw_page_ctm, pix);
            fz_run_page_annots(ctx, page, dev, fz_identity, NULL);
            fz_run_page_widgets(ctx, page, dev, fz_identity, NULL);
            fz_close_device(ctx, dev);
            fz_drop_device(ctx, dev);
        }

        //bitmap = new_rendered_fz_pixmap(ctx, page_contents);
        bitmap = new_rendered_fz_pixmap(ctx, pix);
    }
    fz_always(ctx) {
        fz_drop_pixmap(ctx, page_contents);
        fz_drop_pixmap(ctx, pix);
    }
    fz_catch(ctx) {
        delete bitmap;
        return nullptr;
    }
    return bitmap;
}

IPageElement* EngineMupdf::GetElementAtPos(int pageNo, PointF pt) {
    FzPageInfo* pageInfo = GetFzPageInfoFast(pageNo);
    return FzGetElementAtPos(pageInfo, pt);
}

Vec<IPageElement*>* EngineMupdf::GetElements(int pageNo) {
    auto pageInfo = GetFzPageInfoFast(pageNo);
    auto res = new Vec<IPageElement*>();
    FzGetElements(res, pageInfo);
    if (res->IsEmpty()) {
        delete res;
        return nullptr;
    }
    return res;
}

RenderedBitmap* EngineMupdf::GetImageForPageElement(IPageElement* ipel) {
    PageElement* pel = (PageElement*)ipel;
    auto r = pel->rect;
    int pageNo = pel->pageNo;
    int imageID = pel->imageID;
    return GetPageImage(pageNo, r, imageID);
}

bool EngineMupdf::BenchLoadPage(int pageNo) {
    return GetFzPageInfo(pageNo, false) != nullptr;
}

fz_matrix EngineMupdf::viewctm(int pageNo, float zoom, int rotation) {
    const fz_rect tmpRc = To_fz_rect(PageMediabox(pageNo));
    return fz_create_view_ctm(tmpRc, zoom, rotation);
}

fz_matrix EngineMupdf::viewctm(fz_page* page, float zoom, int rotation) const {
    return fz_create_view_ctm(fz_bound_page(ctx, page), zoom, rotation);
}

RenderedBitmap* EngineMupdf::GetPageImage(int pageNo, RectF rect, int imageIdx) {
    FzPageInfo* pageInfo = GetFzPageInfo(pageNo, false);
    if (!pageInfo->page) {
        return nullptr;
    }
    auto& images = pageInfo->images;
    bool outOfBounds = imageIdx >= images.isize();
    fz_rect imgRect = images.at(imageIdx).rect;
    bool badRect = ToRectFl(imgRect) != rect;
    CrashIf(outOfBounds);
    CrashIf(badRect);
    if (outOfBounds || badRect) {
        return nullptr;
    }

    ScopedCritSec scope(ctxAccess);

    fz_image* image = fz_find_image_at_idx(ctx, pageInfo, imageIdx);
    CrashIf(!image);
    if (!image) {
        return nullptr;
    }

    RenderedBitmap* bmp = nullptr;
    fz_pixmap* pixmap = nullptr;
    fz_var(pixmap);
    fz_var(bmp);

    fz_try(ctx) {
        // TODO(port): not sure if should provide subarea, w and h
        pixmap = fz_get_pixmap_from_image(ctx, image, nullptr, nullptr, nullptr, nullptr);
        bmp = new_rendered_fz_pixmap(ctx, pixmap);
    }
    fz_always(ctx) {
        fz_drop_pixmap(ctx, pixmap);
    }
    fz_catch(ctx) {
        return nullptr;
    }

    return bmp;
}

PageText EngineMupdf::ExtractPageText(int pageNo) {
    FzPageInfo* pageInfo = GetFzPageInfo(pageNo, true);
    if (!pageInfo) {
        return {};
    }

    return {};

    ScopedCritSec scope(ctxAccess);

    fz_stext_page* stext = nullptr;
    fz_var(stext);
    fz_stext_options opts{};
    fz_try(ctx) {
        stext = fz_new_stext_page_from_page(ctx, pageInfo->page, &opts);
    }
    fz_catch(ctx) {
    }
    if (!stext) {
        return {};
    }
    PageText res;
    // TODO: convert to return PageText
    WCHAR* text = fz_text_page_to_str(stext, &res.coords);
    fz_drop_stext_page(ctx, stext);
    res.text = text;
    res.len = (int)str::Len(text);
    return res;
}

WCHAR* EngineMupdf::GetProperty(DocumentProperty prop) {
    if (!_doc) {
        return nullptr;
    }
    // TODO: implement me
    return nullptr;
}

std::span<u8> EngineMupdf::GetFileData() {
    std::span<u8> res;
    ScopedCritSec scope(ctxAccess);

    pdf_document* doc = pdf_document_from_fz_document(ctx, _doc);

    fz_var(res);
    fz_try(ctx) {
        res = fz_extract_stream_data(ctx, doc->file);
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

// TODO: proper support for includeUserAnnots or maybe just remove it
bool EngineMupdf::SaveFileAs(const char* copyFileName, bool includeUserAnnots) {
    auto dstPath = ToWstrTemp(copyFileName);
    AutoFree d = GetFileData();
    if (!d.empty()) {
        bool ok = file::WriteFile(dstPath, d.AsSpan());
        return ok;
    }
    auto path = FileName();
    if (!path) {
        return false;
    }
    bool ok = file::Copy(dstPath, path, false);
    return ok;
}

bool EngineMupdf::HasClipOptimizations(int pageNo) {
    FzPageInfo* pageInfo = GetFzPageInfoFast(pageNo);
    if (!pageInfo || !pageInfo->page) {
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

WCHAR* EngineMupdf::GetPageLabel(int pageNo) const {
    if (!_pageLabels || pageNo < 1 || PageCount() < pageNo) {
        return EngineBase::GetPageLabel(pageNo);
    }

    return str::Dup(_pageLabels->at(pageNo - 1));
}

int EngineMupdf::GetPageByLabel(const WCHAR* label) const {
    int pageNo = 0;
    if (_pageLabels) {
        pageNo = _pageLabels->Find(label) + 1;
    }

    if (!pageNo) {
        return EngineBase::GetPageByLabel(label);
    }

    return pageNo;
}


EngineBase* EngineMupdf::CreateFromFile(const WCHAR* path, PasswordUI* pwdUI) {
    if (str::IsEmpty(path)) {
        return nullptr;
    }
    EngineMupdf* engine = new EngineMupdf();
    if (!engine->Load(path, pwdUI)) {
        delete engine;
        return nullptr;
    }
    return engine;
}

EngineBase* EngineMupdf::CreateFromStream(IStream* stream, PasswordUI* pwdUI) {
    EngineMupdf* engine = new EngineMupdf();
    if (!engine->Load(stream, pwdUI)) {
        delete engine;
        return nullptr;
    }
    return engine;
}

bool IsMupdfEngineSupportedFileType(Kind kind) {
    return kind == kindFileEpub || kind == kindFileFb2;
}

EngineBase* CreateEngineMupdfFromFile(const WCHAR* path, PasswordUI* pwdUI) {
    return EngineMupdf::CreateFromFile(path, pwdUI);
}

EngineBase* CreateEngineMupdfFromStream(IStream* stream, PasswordUI* pwdUI) {
    return EngineMupdf::CreateFromStream(stream, pwdUI);
}
