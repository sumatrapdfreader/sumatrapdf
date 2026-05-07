/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct Annotation;

struct FitzPageImageInfo {
    fz_rect rect = fz_unit_rect;
    fz_matrix transform;
    IPageElement* imageElement = nullptr;
    ~FitzPageImageInfo() { delete imageElement; }
};

struct FzPageInfo {
    int pageNo = 0; // 1-based
    fz_page* page = nullptr;

    // each containz fz_link for this page
    Vec<PageElementDestination*> links;
    // have to keep them alive because they are reverenced in links
    fz_link* retainedLinks = nullptr;

    Vec<Annotation*> annotations;
    // auto-detected links
    Vec<IPageElement*> autoLinks;
    // comments are made out of annotations
    Vec<IPageElement*> comments;

    Vec<IPageElement*> allElements;
    bool elementsNeedRebuilding = true;

    RectF mediabox{};
    Vec<FitzPageImageInfo*> images;

    // if false, only loaded page (fast)
    // if true, loaded expensive info (extracted text etc.)
    bool fullyLoaded = false;

    // cached "View" rendering of the page; built lazily under
    // EngineMupdf::renderLock. fz_display_list is safe to *replay* across
    // cloned contexts in principle, but the image objects it references are
    // not -- shared images (notably JBIG2 with shared dictionaries) trigger
    // races inside mupdf's image store on concurrent decode. So renderLock
    // is engine-wide, not per-page.
    fz_display_list* displayList = nullptr;
};

class EngineMupdf : public EngineBase {
  public:
    EngineMupdf();
    ~EngineMupdf() override;
    EngineBase* Clone() override;

    RectF PageMediabox(int pageNo) override;
    RectF PageContentBox(int pageNo, RenderTarget target = RenderTarget::View) override;

    RenderedBitmap* RenderPage(RenderPageArgs& args) override;

    RectF Transform(const RectF& rect, int pageNo, float zoom, int rotation, bool inverse = false) override;

    ByteSlice GetFileData() override;
    bool SaveFileAs(const char* copyFileName) override;
    PageText ExtractPageText(int pageNo) override;
    PageTextUtf8 ExtractPageTextUtf8(int pageNo) override;

    bool HasClipOptimizations(int pageNo) override;
    TempStr GetPropertyTemp(const char* name) override;
    void GetProperties(StrVec& keyValOut) override;

    bool BenchLoadPage(int pageNo) override;

    Vec<IPageElement*> GetElements(int pageNo) override;
    IPageElement* GetElementAtPos(int pageNo, PointF pt) override;
    bool HandleLink(IPageDestination*, ILinkHandler*) override;

    RenderedBitmap* GetImageForPageElement(IPageElement*) override;

    IPageDestination* GetNamedDest(const char* name) override;
    TocTree* GetToc() override;

    TempStr GetPageLabeTemp(int pageNo) const override;
    int GetPageByLabel(const char* label) const override;

    fz_context* Ctx() const;

    // Lock hierarchy (acquire in this order; never go upward):
    //   pagesLock           - protects the pages[] vector / FzPageInfo lookup
    //   renderLock          - serializes any mupdf call that may run a page
    //                         or replay a display list, i.e. anything that
    //                         can decode an image. Engine-wide (not per-page)
    //                         because shared image objects (e.g. JBIG2 with
    //                         shared dictionaries) race in mupdf's image
    //                         store under concurrent decode -- crashes
    //                         in template_image_compose_opt with use-after-
    //                         free on the source pixmap. Also acquired under
    //                         pagesLock inside GetFzPageInfo.
    //   docLock             - serializes document-scope mupdf operations:
    //                         outline, fonts, info, named dests, page-tree
    //                         access, annotation mutations. Independent of
    //                         renderLock; never acquire pagesLock while
    //                         holding docLock.
    //
    // docLock must NOT alias one of fz_locks[] -- mupdf takes those briefly
    // for its own internal coordination, and reusing one as a long-held outer
    // lock would serialize every cloned-context allocation across all threads.
    CRITICAL_SECTION pagesLock;
    CRITICAL_SECTION renderLock;
    CRITICAL_SECTION docLock;

    // per-FZ_LOCK-index critical sections used by mupdf via fz_locks_ctx
    // callbacks. Mupdf holds these only momentarily; do not hold them across
    // your own code.
    CRITICAL_SECTION fz_locks[FZ_LOCK_MAX];

    fz_context* _ctx = nullptr;
    fz_locks_context fz_locks_ctx;
    int displayDPI{96};
    fz_document* _doc = nullptr;
    pdf_document* pdfdoc = nullptr;
    Vec<FzPageInfo*> pages;
    fz_outline* outline = nullptr;
    fz_outline* attachments = nullptr;
    pdf_obj* pdfInfo = nullptr;
    StrVec* pageLabels = nullptr;

    TocTree* tocTree = nullptr;

    // password used to decrypt the document (needed for re-encryption/decryption)
    char* pdfPassword = nullptr;

    // used to track "dirty" state of annotations. not perfect because if we add and delete
    // the same annotation, we should be back to 0
    bool modifiedAnnotations = false;

    bool Load(const char* filePath, PasswordUI* pwdUI = nullptr);
    bool Load(IStream* stream, const char* nameHint, PasswordUI* pwdUI = nullptr);
    // TODO(port): fz_stream can no-longer be re-opened (fz_clone_stream)
    // bool Load(fz_stream* stm, PasswordUI* pwdUI = nullptr);
    bool LoadFromStream(fz_stream* stm, const char* nameHing, PasswordUI* pwdUI = nullptr);
    bool FinishLoading();
    RenderedBitmap* GetPageImage(int pageNo, RectF rect, int imageIdx);

    FzPageInfo* GetFzPageInfoCanFail(int pageNo);
    FzPageInfo* GetFzPageInfoFast(int pageNo);
    FzPageInfo* GetFzPageInfo(int pageNo, bool loadQuick, fz_cookie* cookie = nullptr);
    fz_matrix viewctm(int pageNo, float zoom, int rotation);
    fz_matrix viewctm(fz_page* page, float zoom, int rotation) const;
    TocItem* BuildTocTree(TocItem* parent, fz_outline* outline, int& idCounter, bool isAttachment);
    TempStr ExtractFontListTemp();

    ByteSlice LoadStreamFromPDFFile(const char* filePath);
};

EngineMupdf* AsEngineMupdf(EngineBase* engine);

fz_rect ToFzRect(RectF rect);
RectF ToRectF(fz_rect rect);
void MarkNotificationAsModified(EngineMupdf*, Annotation*, AnnotationChange = AnnotationChange::Modify);
Annotation* MakeAnnotationWrapper(EngineMupdf* engine, pdf_annot* annot, int pageNo);
