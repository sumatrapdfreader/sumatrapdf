/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// define the following if you want shadows drawn around the pages
// #define DRAW_PAGE_SHADOWS

constexpr int INVALID_PAGE_NO = -1;

// TODO: duplicated in GlobalPrefs.h
#define INVALID_ZOOM -99.0f

/* Describes many attributes of one page in one, convenient place */
struct PageInfo {
    /* data that is constant for a given page. page size in document units */
    RectD page{};

    /* data that is calculated when needed. actual content size within a page (View target) */
    RectD contentBox{};

    /* data that needs to be set before DisplayModel::Relayout().
       Determines whether a given page should be shown on the screen. */
    bool shown = false;

    /* data that changes when zoom and rotation changes */
    /* position and size within total area after applying zoom and rotation.
       Represents display rectangle for a given page.
       Calculated in DisplayModel::Relayout() */
    RectI pos{};

    /* data that changes due to scrolling. Calculated in DisplayModel::RecalcVisibleParts() */
    float visibleRatio; /* (0.0 = invisible, 1.0 = fully visible) */
    /* position of page relative to visible view port: pos.Offset(-viewPort.x, -viewPort.y) */
    RectI pageOnScreen{};

    // when zoomVirtual in DisplayMode is ZOOM_FIT_PAGE, ZOOM_FIT_WIDTH
    // or ZOOM_FIT_CONTENT, this is per-page zoom level
    float zoomReal;
};

/* The current scroll state (needed for saving/restoring the scroll position) */
/* coordinates are in user space units (per page) */
struct ScrollState {
    ScrollState() = default;
    explicit ScrollState(int page, double x, double y) : page(page), x(x), y(y) {
    }
    bool operator==(const ScrollState& other) const {
        return page == other.page && x == other.x && y == other.y;
    }

    int page = 0;
    double x = 0;
    double y = 0;
};

class PageTextCache;
class TextSelection;
class TextSearch;
struct TextSel;
class Synchronizer;

// TODO: in hindsight, zoomVirtual is not a good name since it's either
// virtual zoom level OR physical zoom level. Would be good to find
// better naming scheme (call it zoomLevel?)

/* Information needed to drive the display of a given document on a screen.
   You can think of it as a model in the MVC pardigm.
   All the display changes should be done through changing this model via
   API and re-displaying things based on new display information */
class DisplayModel : public Controller {
  public:
    DisplayModel(EngineBase* engine, ControllerCallback* cb);
    ~DisplayModel();

    // meta data
    const WCHAR* FilePath() const override {
        return engine->FileName();
    }
    const WCHAR* DefaultFileExt() const override {
        return engine->defaultFileExt;
    }
    int PageCount() const override {
        return engine->PageCount();
    }
    WCHAR* GetProperty(DocumentProperty prop) override {
        return engine->GetProperty(prop);
    }

    // page navigation (stateful)
    int CurrentPageNo() const override;
    void GoToPage(int pageNo, bool addNavPoint) override {
        GoToPage(pageNo, 0, addNavPoint);
    }
    bool CanNavigate(int dir) const override;
    void Navigate(int dir) override;

    // view settings
    void SetDisplayMode(DisplayMode mode, bool keepContinuous = false) override;
    DisplayMode GetDisplayMode() const override {
        return displayMode;
    }
    void SetPresentationMode(bool enable) override;
    void SetZoomVirtual(float zoom, PointI* fixPt) override;
    float GetZoomVirtual(bool absolute = false) const override;
    float GetNextZoomStep(float towards) const override;
    void SetViewPortSize(SizeI size) override;

    // table of contents
    TocTree* GetToc() override {
        if (!engine) {
            return false;
        }
        return engine->GetToc();
    }
    void ScrollToLink(PageDestination* dest) override;
    PageDestination* GetNamedDest(const WCHAR* name) override {
        return engine->GetNamedDest(name);
    }

    void GetDisplayState(DisplayState* ds) override;
    // asynchronously calls saveThumbnail (fails silently)
    void CreateThumbnail(SizeI size, const onBitmapRenderedCb& saveThumbnail) override {
        cb->RenderThumbnail(this, size, saveThumbnail);
    }

    // page labels (optional)
    bool HasPageLabels() const override {
        return engine->HasPageLabels();
    }
    WCHAR* GetPageLabel(int pageNo) const override {
        return engine->GetPageLabel(pageNo);
    }
    int GetPageByLabel(const WCHAR* label) const override {
        return engine->GetPageByLabel(label);
    }

    // common shortcuts
    bool ValidPageNo(int pageNo) const override {
        return 1 <= pageNo && pageNo <= engine->PageCount();
    }
    bool GoToNextPage() override;
    bool GoToPrevPage(bool toBottom = false) override {
        return GoToPrevPage(toBottom ? -1 : 0);
    }
    bool GoToFirstPage() override;
    bool GoToLastPage() override;

    // for quick type determination and type-safe casting
    DisplayModel* AsFixed() override {
        return this;
    }

  public:
    // the following is specific to DisplayModel

    EngineBase* GetEngine() const {
        return engine;
    }
    Kind GetEngineType() const {
        if (!engine) {
            return nullptr;
        }
        return engine->kind;
    }

    // controller-specific data (easier to save here than on WindowInfo)
    Kind engineType = nullptr;
    Vec<PageAnnotation>* userAnnots = nullptr;
    bool userAnnotsModified = false;
    Synchronizer* pdfSync = nullptr;

    PageTextCache* textCache = nullptr;
    TextSelection* textSelection = nullptr;
    // access only from Search thread
    TextSearch* textSearch = nullptr;

    PageInfo* GetPageInfo(int pageNo) const;

    /* current rotation selected by user */
    int GetRotation() const {
        return rotation;
    }
    float GetZoomReal(int pageNo) const;
    void Relayout(float zoomVirtual, int rotation);

    RectI GetViewPort() const {
        return viewPort;
    }
    bool NeedHScroll() const {
        return viewPort.dy < totalViewPortSize.dy;
    }
    bool NeedVScroll() const {
        return viewPort.dx < totalViewPortSize.dx;
    }
    SizeI GetCanvasSize() const {
        return canvasSize;
    }

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
    /* a "virtual" zoom level. Can be either a real zoom level in percent
       (i.e. 100.0 is original size) or one of virtual values ZOOM_FIT_PAGE,
       ZOOM_FIT_WIDTH or ZOOM_FIT_CONTENT, whose real value depends on draw area size */
    void RotateBy(int rotation);

    WCHAR* GetTextInRegion(int pageNo, RectD region);
    bool IsOverText(PointI pt);
    PageElement* GetElementAtPos(PointI pt);

    int GetPageNoByPoint(PointI pt);
    PointI CvtToScreen(int pageNo, PointD pt);
    RectI CvtToScreen(int pageNo, RectD r);
    PointD CvtFromScreen(PointI pt, int pageNo = INVALID_PAGE_NO);
    RectD CvtFromScreen(RectI r, int pageNo = INVALID_PAGE_NO);

    bool ShowResultRectToScreen(TextSel* res);

    ScrollState GetScrollState();
    void SetScrollState(ScrollState state);

    void CopyNavHistory(DisplayModel& orig);

    void SetInitialViewSettings(DisplayMode displayMode, int newStartPage, SizeI viewPort, int screenDPI);
    void SetDisplayR2L(bool r2l) {
        displayR2L = r2l;
    }
    bool GetDisplayR2L() const {
        return displayR2L;
    }

    bool ShouldCacheRendering(int pageNo);
    // called when we decide that the display needs to be redrawn
    void RepaintDisplay() {
        cb->Repaint();
    }

    /* allow resizing a window without triggering a new rendering (needed for window destruction) */
    bool dontRenderFlag = false;

    bool GetPresentationMode() const {
        return presentationMode;
    }

  protected:
    void BuildPagesInfo();
    float ZoomRealFromVirtualForPage(float zoomVirtual, int pageNo) const;
    SizeD PageSizeAfterRotation(int pageNo, bool fitToContent = false) const;
    void ChangeStartPage(int startPage);
    PointI GetContentStart(int pageNo);
    void RecalcVisibleParts();
    void RenderVisibleParts();
    void AddNavPoint();
    RectD GetContentBox(int pageNo);
    void CalcZoomReal(float zoomVirtual);
    void GoToPage(int pageNo, int scrollY, bool addNavPt = false, int scrollX = -1);
    bool GoToPrevPage(int scrollY);
    int GetPageNextToPoint(PointI pt);

    EngineBase* engine = nullptr;

    /* an array of PageInfo, len of array is pageCount */
    PageInfo* pagesInfo = nullptr;

    DisplayMode displayMode = DM_AUTOMATIC;
    /* In non-continuous mode is the first page from a file that we're
       displaying.
       No meaning in continous mode. */
    int startPage = 1;

    /* size of virtual canvas containing all rendered pages. */
    SizeI canvasSize;
    /* size and position of the viewport on the canvas (resp size of the visible
       part of the canvase available for content (totalViewPortSize minus scroll bars)
       (canvasSize is always at least as big as viewPort.Size()) */
    RectI viewPort;
    /* total size of view port (draw area), including scroll bars */
    SizeI totalViewPortSize;

    WindowMargin windowMargin;
    SizeI pageSpacing;

    /* real zoom value calculated from zoomVirtual. Same as
       zoomVirtual * 0.01 * dpiFactor
       except for ZOOM_FIT_PAGE, ZOOM_FIT_WIDTH and ZOOM_FIT_CONTENT */
    float zoomReal = INVALID_ZOOM;
    float zoomVirtual = INVALID_ZOOM;
    int rotation = 0;
    /* dpi correction factor by which _zoomVirtual has to be multiplied in
       order to get _zoomReal */
    float dpiFactor = 1.0f;
    /* whether to display pages Left-to-Right or Right-to-Left.
       this value is extracted from the PDF document */
    bool displayR2L = false;

    /* when we're in presentation mode, _pres* contains the pre-presentation values */
    bool presentationMode = false;
    float presZoomVirtual = INVALID_ZOOM;
    DisplayMode presDisplayMode = DM_AUTOMATIC;

    Vec<ScrollState> navHistory;
    /* index of the "current" history entry (to be updated on navigation),
       resp. number of Back history entries */
    size_t navHistoryIx = 0;
};

int NormalizeRotation(int rotation);
