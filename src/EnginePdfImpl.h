
class EnginePdf : public EngineBase {
  public:
    EnginePdf();
    ~EnginePdf() override;
    EngineBase* Clone() override;

    RectF PageMediabox(int pageNo) override;
    RectF PageContentBox(int pageNo, RenderTarget target = RenderTarget::View) override;

    RenderedBitmap* RenderPage(RenderPageArgs& args) override;

    RectF Transform(const RectF& rect, int pageNo, float zoom, int rotation, bool inverse = false) override;

    std::span<u8> GetFileData() override;
    bool SaveFileAs(const char* copyFileName, bool includeUserAnnots = false) override;
    bool SaveFileAsPdf(const char* pdfFileName, bool includeUserAnnots = false);
    PageText ExtractPageText(int pageNo) override;

    bool HasClipOptimizations(int pageNo) override;
    WCHAR* GetProperty(DocumentProperty prop) override;

    bool BenchLoadPage(int pageNo) override;

    Vec<IPageElement*>* GetElements(int pageNo) override;
    IPageElement* GetElementAtPos(int pageNo, PointF pt) override;
    RenderedBitmap* GetImageForPageElement(IPageElement*) override;

    PageDestination* GetNamedDest(const WCHAR* name) override;
    TocTree* GetToc() override;

    WCHAR* GetPageLabel(int pageNo) const override;
    int GetPageByLabel(const WCHAR* label) const override;

    int GetAnnotations(Vec<Annotation*>* annotsOut);

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
    fz_stream* _docStream = nullptr;
    Vec<FzPageInfo> _pages;
    fz_outline* outline = nullptr;
    fz_outline* attachments = nullptr;
    pdf_obj* _info = nullptr;
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
    fz_matrix viewctm(fz_page* page, float zoom, int rotation);
    TocItem* BuildTocTree(TocItem* parent, fz_outline* outline, int& idCounter, bool isAttachment);
    WCHAR* ExtractFontList();

    std::span<u8> LoadStreamFromPDFFile(const WCHAR* filePath);
    void InvalideAnnotationsForPage(int pageNo);
};

EnginePdf* AsEnginePdf(EngineBase* engine);
