/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// define the following if you want shadows drawn around the pages
// #define DRAW_PAGE_SHADOWS

constexpr int kInvalidPageNo = -1;

struct Annotation;
struct PageRenderRequest;
enum class AnnotationType;

// a media box we haven't measured yet. Measuring a page of an image collection
// means reading the image's header off disk, which is slow on network drives,
// so continuous layout estimates the size of un-measured pages instead
// (see DisplayModel::PageMediaBoxForLayout)
inline bool IsMediaBoxKnown(const RectF& r) {
    return r.dx >= 0 && r.dy >= 0;
}

/* Describes many attributes of one page in one, convenient place */
struct PageInfo {
    /* data that is constant for a given page. page size in document units.
       {-1, -1} size means "not measured yet" (see IsMediaBoxKnown) */
    RectF mediaBox{0, 0, -1, -1};
    PageInfoState state = PageInfoState::Unknown;
    /* set by Relayout() when this page was laid out with the estimated media
       box because its own wasn't measured yet */
    bool usedEstimatedMediaBox = false;

    /* data that is calculated when needed. actual content size within a page (View target) */
    RectF contentBox;

    /* data that changes when zoom and rotation changes */
    /* position and size within total area after applying zoom and rotation.
       Represents display rectangle for a given page.
       Calculated in DisplayModel::Relayout() */
    Rect pos;

    /* data that changes due to scrolling. Calculated in DisplayModel::RecalcVisibleParts() */
    float visibleRatio; /* (0.0 = invisible, 1.0 = fully visible) */
    /* position of page relative to visible view port: pos.Offset(-viewPort.x, -viewPort.y) */
    Rect pageOnScreen;

    // when zoomVirtual in DisplayMode is kZoomFitPage, kZoomFitWidth
    // or kZoomFitContent, this is per-page zoom level
    float zoomReal;

    bool isShown = false;

    // set to true if rendering this page failed (e.g. corrupt image data)
    bool failedToRender = false;
};

/* The current scroll state (needed for saving/restoring the scroll position) */
/* coordinates are in user space units (per page) */
struct ScrollState {
    ScrollState() = default;
    ~ScrollState() = default;
    ScrollState(int page, double x, double y);
    bool operator==(const ScrollState& other) const;

    double x = 0;
    double y = 0;
    int page = 0;
    // zoom to restore along with the position, 0 for "leave the zoom alone".
    // Only navigation history sets it (AddNavPoint(rememberZoom)), so restoring
    // a scroll state for any other reason doesn't touch the zoom.
    float zoom = 0;
};

struct TextSelection;
struct TextSearch;
struct TextSel;
struct Synchronizer;

// TODO: in hindsight, zoomVirtual is not a good name since it's either
// virtual zoom level OR physical zoom level. Would be good to find
// better naming scheme (call it zoomLevel?)

/* Information needed to drive the display of a given document on a screen.
   You can think of it as a model in the MVC pardigm.
   All the display changes should be done through changing this model via
   API and re-displaying things based on new display information */
struct DisplayModel : DocController {
    DisplayModel(EngineBase* engine, DocControllerCallback* cb);
    DisplayModel(DisplayModel const&) = delete;
    DisplayModel& operator=(DisplayModel const&) = delete;

    ~DisplayModel() override;

    // meta data
    Str GetFilePath() const override;
    Str GetDefaultFileExt() const override;
    int PageCount() const override;
    TempStr GetPropertyTemp(DocProp prop) override;

    // page navigation (stateful)
    int CurrentPageNo() const override;
    void GoToPage(int pageNo, bool addNavPoint) override;
    bool CanNavigate(int dir) const override;
    void Navigate(int dir) override;

    // view settings
    void SetDisplayMode(DisplayMode mode, bool keepContinuous = false) override;
    DisplayMode GetDisplayMode() const override;
    void SetInPresentation(bool enable) override;
    void ApplyFullscreenDisplayMode(bool enable);
    void SetZoomVirtual(float zoom, Point* fixPt) override;
    float GetZoomVirtual(bool absolute = false) const override;
    float GetNextZoomStep(float towards) const override;
    void SetViewPortSize(Size size) override;

    // table of contents
    bool HasToc() override;
    TocTree* GetToc() override;
    void ScrollTo(int pageNo, RectF rect, float zoom) override;
    bool HandleLink(IPageDestination*, ILinkHandler*) override;
    IPageDestination* GetNamedDest(Str name) override;

    void GetDisplayState(FileState* fs) override;
    // asynchronously calls saveThumbnail (fails silently)
    void CreateThumbnail(Size size, const OnBitmapRendered* saveThumbnail) override;

    bool HasPageLabels() const override;
    TempStr GetPageLabeTemp(int pageNo) const override;
    int GetPageByLabel(Str label) const override;

    bool ValidPageNo(int pageNo) const override;
    bool GoToNextPage() override;
    bool GoToPrevPage(bool toBottom = false) override;
    bool GoToFirstPage() override;
    bool GoToLastPage() override;

    DisplayModel* AsFixed() override;

    EngineBase* GetEngine() const;
    Kind GetEngineType() const;

    // controller-specific data (easier to save here than on MainWindow)
    Kind engineType = nullptr;

    Synchronizer* pdfSync = nullptr;

    TextSelection* textSelection = nullptr;
    // access only from Search thread
    TextSearch* textSearch = nullptr;

    PageInfo* GetPageInfo(int pageNo) const;
    RectF PageMediaBox(int pageNo) const;
    RectF PageMediaBoxForLayout(int pageNo) const;
    void UpdateEstimatedMediaBox();
    bool EnsureMediaBoxesForVisiblePages();
    void RelayoutKeepingView();

    int GetRotation() const;
    float GetZoomReal(int pageNo) const;
    float MaxZoomForDocument() const;
    void Relayout(float zoomVirtual, int rotation);
    bool ViewportReadyForRelayout() const;

    Rect GetViewPort() const;
    bool IsHScrollbarVisible() const;
    bool IsVScrollbarVisible() const;
    bool NeedHScroll() const;
    bool NeedVScroll() const;
    bool CanScrollRight() const;
    bool CanScrollLeft() const;
    bool CanScrollDown() const;
    bool CanScrollUp() const;
    bool IsAtDocumentEnd() const;
    Size GetCanvasSize() const;

    bool PageShown(int pageNo) const;
    bool PageVisible(int pageNo) const;
    bool PageVisibleNearby(int pageNo) const;
    int FirstVisiblePageNo() const;
    bool FirstBookPageVisible() const;
    bool LastBookPageVisible() const;

    void ScrollXTo(int xOff);
    void ScrollXBy(int dx);
    void ScrollYTo(int yOff);
    void ScrollYBy(int dy, bool changePage);

    int yOffset();

    void RotateBy(int rotation);

    Str GetTextInRegion(int pageNo, RectF region) const;
    bool IsOverText(Point pt);
    IPageElement* GetElementAtPos(Point pt, int* pageNoOut);
    Annotation* GetAnnotationAtPos(Point pt, Annotation*);
    Annotation* GetWidgetAtPos(Point pt);

    int GetPageNoByPoint(Point pt) const;
    Point CvtToScreen(int pageNo, PointF pt);
    Rect CvtToScreen(int pageNo, RectF r);
    PointF CvtFromScreen(Point pt, int pageNo = kInvalidPageNo);
    RectF CvtFromScreen(Rect r, int pageNo = kInvalidPageNo);

    bool ShowResultRectToScreen(TextSel* res);
    bool ScrollScreenToRect(int pageNo, Rect rec);

    ScrollState GetScrollState();
    void SetScrollState(const ScrollState& state);

    void CopyNavHistory(DisplayModel& orig);

    void SetInitialViewSettings(DisplayMode displayMode, int newStartPage, Size viewPort, int screenDPI);
    void SetUiDpi(int dpi);
    void SetDisplayR2L(bool r2l);
    bool GetDisplayR2L() const;
    void SetUniformPageWidth(bool enable);
    bool GetUniformPageWidth() const;
    bool GoToPageHorizontal(bool toRight);

    bool ShouldCacheRendering(int pageNo) const;
    void RepaintDisplay();

    bool InPresentation() const;

    void BuildPagesInfo();
    float ZoomRealFromVirtualForPage(float zoomVirtual, int pageNo) const;
    SizeF PageSizeAfterRotation(int pageNo, bool fitToContent = false) const;
    bool ShouldTreatLandscapeAsSpread() const;
    void EnsureSpreadFlags() const;
    int FirstPageInRow(int pageNo) const;
    int LastPageInRow(int pageNo) const;
    void ChangeStartPage(int startPage);
    Point GetContentStart(int pageNo) const;
    void RecalcVisibleParts() const;
    void RenderVisibleParts();
    void AddNavPoint(bool rememberZoom = false);
    RectF GetContentBox(int pageNo) const;
    void CalcZoomReal(float zoomVirtual);
    void GoToPage(int pageNo, int scrollY, bool addNavPt = false, int scrollX = -1);
    bool GoToNextPage(bool keepViewOffset);
    bool GoToPrevPage(int scrollY);
    int GetPageNextToPoint(Point pt) const;

    EngineBase* engine = nullptr;

    /* an array of PageInfo, len of array is pageCount */
    PageInfo* pagesInfo = nullptr;

    /* Lazy media boxes: don't measure every page up-front, lay out un-measured
       pages with estimatedMediaBox and fix them up as they scroll into view.
       See DisplayModel::PageMediaBoxForLayout / EnsureMediaBoxesForVisiblePages */
    bool useLazyMediaBoxes = false;
    RectF estimatedMediaBox;
    // guards against re-entering EnsureMediaBoxesForVisiblePages() from the
    // relayout it triggers
    bool inMediaBoxUpdate = false;

    DisplayMode displayMode{DisplayMode::Automatic};
    /* In non-continuous mode is the first page from a file that we're
       displaying.
       No meaning in continuous mode. */
    int startPage = 1;

    /* size of virtual canvas containing all rendered pages. */
    Size canvasSize;
    /* size and position of the viewport on the canvas (resp size of the visible
       part of the canvase available for content (totalViewPortSize minus scroll bars)
       (canvasSize is always at least as big as viewPort.Size()) */
    Rect viewPort;
    /* total size of view port (draw area), including scroll bars */
    Size totalViewPortSize;

    // WindowMargin / PageSpacing settings scaled to uiDpi (see SetUiDpi)
    WindowMargin windowMargin;
    Size pageSpacing;
    int uiDpi = 96;

    /* real zoom value calculated from zoomVirtual. Same as
       zoomVirtual * 0.01 * dpiFactor
       except for kZoomFitPage, kZoomFitWidth and kZoomFitContent */
    float zoomReal{kInvalidZoom};
    float zoomVirtual{kInvalidZoom};
    // set while SetZoomVirtual() applies an explicit Fit Content request, to skip
    // the zoom-in damping in CalcZoomReal() (see the comment there)
    bool exactFitContent = false;
    int rotation = 0;
    /* dpi correction factor by which _zoomVirtual has to be multiplied in
       order to get _zoomReal */
    float dpiFactor{1.0f};
    float presZoomVirtual{kInvalidZoom};
    DisplayMode presDisplayMode{DisplayMode::Automatic};
    DisplayMode fsSavedDisplayMode{DisplayMode::Automatic};
    bool fsDisplayModeSaved = false;

    Vec<ScrollState> navHistory;
    /* index of the "current" history entry (to be updated on navigation),
       resp. number of Back history entries */
    int navHistoryIdx = 0;

    /* Stable-view nav point tracking (see DisplayModel.cpp): a view the user
       dwelled on is committed into navHistory by the next view change, so
       Back / Forward also work for plain scrolling and page turns, not just
       for ToC / link / bookmark jumps (which call AddNavPoint() directly) */
    struct StableNavPointState {
        ScrollState pending;
        ScrollState lastCommitted;
        DWORD64 pendingTick = 0;
        bool hasPending = false;
        bool hasLastCommitted = false;
        /* set while SetScrollState() restores a view (session restore,
           Back / Forward themselves) so the restore isn't recorded */
        bool suppress = false;
    } stableNavPoint;

    /* whether to display pages Left-to-Right or Right-to-Left.
       this value is extracted from the PDF document */
    bool displayR2L = false;
    bool uniformPageWidth = false;

    /* landscape image pages that occupy a full facing/book row
       (ComicBookUI / ImageUI LandscapeAsSpread; issues #1324, #872) */
    mutable Vec<u8> spreadFlags;
    mutable Vec<int> rowFirst;
    mutable Vec<int> rowLast;
    mutable bool spreadCacheValid = false;

    /* when we're in presentation mode, _pres* contains the pre-presentation values */
    bool inPresentation = false;

    /* allow resizing a window without triggering a new rendering (needed for window destruction) */
    bool pauseRendering = false;

    bool pendingRelayout = false;
    bool hasPendingScroll = false;
    ScrollState pendingScroll;

    void RenderFinished(PageRenderRequest* req);
    void RenderFinishedAsync(PageRenderRequest* req);
};

extern bool gPredictiveRender;
