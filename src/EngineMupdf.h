/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct Annotation;

struct FitzPageImageInfo {
    fz_rect rect = fz_unit_rect;
    fz_matrix transform;
    IPageElement* imageElement = nullptr;
    ~FitzPageImageInfo() {
        delete imageElement;
    }
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

    bool HasClipOptimizations(int pageNo) override;
    TempStr GetPropertyTemp(DocumentProperty prop) override;

    bool BenchLoadPage(int pageNo) override;

    Vec<IPageElement*> GetElements(int pageNo) override;
    IPageElement* GetElementAtPos(int pageNo, PointF pt) override;
    bool HandleLink(IPageDestination*, ILinkHandler*) override;

    RenderedBitmap* GetImageForPageElement(IPageElement*) override;

    IPageDestination* GetNamedDest(const char* name) override;
    TocTree* GetToc() override;

    TempStr GetPageLabeTemp(int pageNo) const override;
    int GetPageByLabel(const char* label) const override;

    // make sure to never ask for pagesAccess in an ctxAccess
    // protected critical section in order to avoid deadlocks
    CRITICAL_SECTION* ctxAccess;
    CRITICAL_SECTION pagesAccess;

    CRITICAL_SECTION mutexes[FZ_LOCK_MAX];

    fz_context* ctx = nullptr;
    fz_locks_context fz_locks_ctx;
    int displayDPI{96};
    fz_document* _doc = nullptr;
    pdf_document* pdfdoc = nullptr;
    fz_stream* docStream = nullptr;
    Vec<FzPageInfo*> pages;
    fz_outline* outline = nullptr;
    fz_outline* attachments = nullptr;
    pdf_obj* pdfInfo = nullptr;
    StrVec* pageLabels = nullptr;

    TocTree* tocTree = nullptr;

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

    FzPageInfo* GetFzPageInfoFast(int pageNo);
    FzPageInfo* GetFzPageInfo(int pageNo, bool loadQuick);
    fz_matrix viewctm(int pageNo, float zoom, int rotation);
    fz_matrix viewctm(fz_page* page, float zoom, int rotation) const;
    TocItem* BuildTocTree(TocItem* parent, fz_outline* outline, int& idCounter, bool isAttachment);
    TempStr ExtractFontListTemp();

    ByteSlice LoadStreamFromPDFFile(const char* filePath);
};

EngineMupdf* AsEngineMupdf(EngineBase* engine);

fz_rect ToFzRect(RectF rect);
RectF ToRectF(fz_rect rect);
RenderedBitmap* NewRenderedFzPixmap(fz_context* ctx, fz_pixmap* pixmap);
void MarkNotificationAsModified(EngineMupdf*, Annotation*, AnnotationChange = AnnotationChange::Modify);
Annotation* MakeAnnotationWrapper(EngineMupdf* engine, pdf_annot* annot, int pageNo);
