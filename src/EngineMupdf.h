/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "PdfCadDetect.h"

struct Annotation;
enum class AnnotationChange;
struct DarkModePageAnalysis;
struct DarkModeEngineCache;

struct FitzPageImageInfo {
    fz_rect rect = fz_unit_rect;
    fz_matrix transform;
    IPageElement* imageElement = nullptr;
    // kept reference to the drawn image, when known (used by the dark-mode
    // image classifier); dropped with the engine's context, not in the dtor
    fz_image* image = nullptr;
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
    // form fields (widgets). kept separate from annotations so they are
    // hit-testable for form filling without polluting the annotation list
    // (comments, edit-annotations panel) with form fields.
    Vec<Annotation*> widgets;
    // annotations + widgets are loaded together on first access; this guards
    // that (annotations.Size()==0 can't, since a page may have only widgets).
    bool annotsLoaded = false;
    // auto-detected links
    Vec<IPageElement*> autoLinks;
    // comments are made out of annotations
    Vec<IPageElement*> comments;

    Vec<IPageElement*> allElements;
    bool elementsNeedRebuilding = true;

    RectF mediabox;
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

    // smart dark mode (PdfDarkMode*.cpp): cached per-page analysis for the
    // object-level renderer, freed via PdfDarkModeInvalidatePage
    DarkModePageAnalysis* darkModeAnalysis = nullptr;
    u32 darkModeAnalysisHash = 0;
    // dark-mode legacy recolor: cached skip rects (device px, absolute) of
    // images whose colors should be preserved
    bool contentImagesCollected = false;
    u32 darkLegacySkipHash = 0;
    float darkLegacySkipZoom = 0.f;
    int darkLegacySkipRotation = 0;
    float darkLegacyArtworkPageBottom = 0.f;
    Vec<Rect> darkLegacySkipDevAbs;
};

class EngineMupdf : public EngineBase {
  public:
    EngineMupdf();
    ~EngineMupdf() override;
    EngineBase* Clone() override;

    RectF PageMediabox(int pageNo) override;
    RectF PageContentBox(int pageNo, RenderTarget target = RenderTarget::View) override;
    void GetPdfPageBoxes(int pageNo, Vec<PdfPageBox>& out) override;

    Pixmap* RenderPage(RenderPageArgs& args) override;

    RectF Transform(const RectF& rect, int pageNo, float zoom, int rotation, bool inverse = false) override;

    Str GetFileData() override;
    bool SaveFileAs(Str dstPath) override;
    PageText ExtractPageText(int pageNo) override;
    bool TryExtractPageText(int pageNo, PageText* out) override;
    void ReleaseTextExtractionThreadContext() override;

    bool HasClipOptimizations(int pageNo) override;
    TempStr GetPropertyTemp(DocProp prop) override;
    void GetProperties(Vec<PropValue>& propsOut) override;

    bool BenchLoadPage(int pageNo) override;

    Vec<IPageElement*> GetElements(int pageNo) override;
    bool TryGetElements(int pageNo, Vec<IPageElement*>* out) override;
    IPageElement* GetElementAtPos(int pageNo, PointF pt) override;
    bool HandleLink(IPageDestination*, ILinkHandler*) override;

    RenderedBitmap* GetImageForPageElement(IPageElement*) override;
    Str GetImageDataForPageElement(IPageElement*) override;

    IPageDestination* GetNamedDest(Str name) override;
    int GetOpenActionPageNo() override;
    bool HasToc() override;
    TocTree* GetToc() override;
    TocTree* BuildToc();
    void StartHeadingTocIfNeeded();
    bool HeadingTocPending() const;

    Func0 headingTocDoneCb;

    TempStr GetPageLabeTemp(int pageNo) const override;
    int GetPageByLabel(Str label) const override;

    fz_context* Ctx() const;

    // The base context the document and its MuJS engine are bound to (JS is
    // enabled on _ctx, so doc->js->ctx == _ctx). Operations that can run form
    // JavaScript -- i.e. field-value changes that regenerate widget appearances
    // -- MUST use this, NOT a per-thread Ctx() clone. pdf_js_execute() always
    // runs JS (and rethrows JS errors) on doc->js->ctx == _ctx; if the enclosing
    // fz_try frames live on a clone instead, a JS error rethrows on _ctx with no
    // matching handler and hits mupdf's uncaught-error abort. Safe to use from
    // the UI thread under docLock (nothing else drives _ctx's error stack).
    fz_context* BaseCtx() const { return _ctx; }

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
    //                         access, annotation mutations. Never acquire
    //                         pagesLock while holding docLock.
    //                         Anything that *runs a page* (display list build,
    //                         stext extraction, image collection) must hold
    //                         this too, not just renderLock: running a page
    //                         reads its annotations, and an annotation edit on
    //                         the UI thread frees those objects underneath it
    //                         (crash in pdf_annot_flags on a freed annot dict).
    //
    // docLock must NOT alias one of fz_locks[] -- mupdf takes those briefly
    // for its own internal coordination, and reusing one as a long-held outer
    // lock would serialize every cloned-context allocation across all threads.
    RecursiveMutex pagesLock;
    Mutex renderLock;
    RecursiveMutex docLock;

    // per-FZ_LOCK-index SRW locks used by mupdf via fz_locks_ctx
    // callbacks. Mupdf holds these only momentarily; do not hold them across
    // your own code.
    Mutex fz_locks[FZ_LOCK_MAX];

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

    AtomicInt headingTocCancel = 0;
    bool headingTocStarted = false;
    bool headingTocDone = false;
    AtomicInt annotLoadCancel = 0;
    bool annotLoadStarted = false;
    bool annotLoadDone = false;
    Func0 annotLoadDoneCb;
    Vec<int> annotLoadFirstPages;
    TocItem* pendingHeadingToc = nullptr;
    int pendingHeadingTocIdCounter = 0;

    // password used to decrypt the document (needed for re-encryption/decryption)
    TempStr pdfPassword;

    // used to track "dirty" state of annotations. not perfect because if we add and delete
    // the same annotation, we should be back to 0
    bool modifiedAnnotations = false;

    // how many journal operations we have open (see EngineMupdfBeginOperation).
    // MuPDF can't undo / redo while one is, e.g. during a resize drag
    int journalNesting = 0;
    // position in the undo history the file was last saved at
    int savedUndoPos = 0;

    // smart dark mode: engine-level image feature/processed caches
    DarkModeEngineCache* darkModeEngineCache = nullptr;

    // the ebook font (EBookUI.FontName, or this document's own override) that
    // we couldn't load, null if there was none or it loaded: the text silently
    // comes out in the default font, so the UI names it in a notification after
    // the document opens (issue #4600). owned by the engine
    Str ebookFontUnavailable;

    // reflowable docs (EPUB/HTML/…): user CSS without the theme overlay, plus
    // the layout size passed to fz_layout_document, so a later theme-color
    // change can restyle without reopening the file
    Str ebookUserCss;
    int ebookPublisherCss = 1;
    float ebookLayoutW = 0;
    float ebookLayoutH = 0;
    float ebookLayoutEm = 0;

    void ApplyReflowThemeCss();

    void GetBitmapRecolorSkipRects(int pageNo, float zoom, int rotation, const RectF& renderPageRect, Size bmpSize,
                                   Vec<Rect>& skipRects) override;

    // CAD/engineering-drawing enhancement (PdfCadDetect.cpp)
    bool cadDetectDone = false;
    bool cadDetectEnable = false;
    int cadDetectScore = 0;
    bool cadRasterDominant = false;
    bool cadHairlineVector = false;
    CadEnhanceOverride cadEnhanceOverride = CadEnhanceOverride::Unset;

    bool CadEnhanceActive() const;
    void RunCadDetection();
    void ToggleCadEnhanceOverride();

    bool Load(Str filePath, PasswordUI* pwdUI = nullptr);
    bool LoadFromStream(fz_stream* stm, Str nameHint, PasswordUI* pwdUI = nullptr);
    bool FinishLoading();
    RenderedBitmap* GetPageImage(int pageNo, RectF rect, int imageIdx);

    FzPageInfo* GetFzPageInfoCanFail(int pageNo);
    FzPageInfo* GetFzPageInfoFast(int pageNo);
    FzPageInfo* GetFzPageInfo(int pageNo, bool loadQuick, fz_cookie* cookie = nullptr);
    fz_matrix viewctm(int pageNo, float zoom, int rotation);
    fz_matrix viewctm(fz_page* page, float zoom, int rotation) const;
    TocItem* BuildTocTree(TocItem* parent, fz_outline* outline, int& idCounter, bool isAttachment, int depth);
    TempStr ExtractFontListTemp();

    Str LoadStreamFromPDFFile(Str filePath);
};

EngineMupdf* AsEngineMupdf(EngineBase* engine);

fz_rect ToFzRect(RectF rect);
RectF ToRectF(fz_rect rect);
void MarkNotificationAsModified(EngineMupdf*, Annotation*);
void MarkNotificationAsModified(EngineMupdf*, Annotation*, AnnotationChange);
Annotation* MakeAnnotationWrapper(EngineMupdf* engine, pdf_annot* annot, int pageNo);
int EngineMupdfUndoPos(EngineMupdf* e, int* stepsOut);
void EngineMupdfBeginOperation(EngineBase*, const char* name);
void EngineMupdfEndOperation(EngineBase*);

// Everything changed while this is alive becomes one undo step. Use it for a
// gesture that makes several changes (creating an annotation sets its geometry,
// colors and contents; a resize drag writes on every mouse move).
struct ScopedEngineOperation {
    EngineBase* engine = nullptr;

    ScopedEngineOperation(EngineBase* e, const char* name) : engine(e) { EngineMupdfBeginOperation(e, name); }
    ~ScopedEngineOperation() { EngineMupdfEndOperation(engine); }
};
