/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct DocController;
struct ChmModel;
struct MarkdownModel;
struct DisplayModel;
struct IPageElement;
struct IPageDestination;
struct ILinkHandler;
struct TocTree;
struct TocItem;
struct MainWindow;
struct FileState;
struct RenderedBitmap;
enum class DisplayMode;
enum class DocProp : u8;

using OnBitmapRendered = Func1<RenderedBitmap*>;

struct DocControllerCallback {
    virtual ~DocControllerCallback() = default;
    // tell the UI to show the pageNo as current page (which also syncs
    // the toc with the curent page). Needed for when a page change happens
    // indirectly or is initiated from within the model
    virtual void PageNoChanged(DocController* ctrl, int pageNo) = 0;
    virtual void ZoomChanged(DocController* ctrl, float zoomVirtual) = 0;
    // tell the UI to open the linked document or URL
    virtual void GotoLink(IPageDestination*) = 0;
    // DisplayModel //
    virtual void Repaint() = 0;
    virtual void UpdateScrollbars(Size canvas) = 0;
    virtual void RequestRendering(DisplayModel* dm, int pageNo) = 0;
    // start (or continue) chained predictive rendering anchored to originPageNo
    virtual void RequestPredictiveRendering(DisplayModel* dm, int originPageNo, const int* pages, int nPages) = 0;
    virtual void CleanUp(DisplayModel* dm) = 0;
    virtual void RenderThumbnail(DisplayModel* dm, Size size, const OnBitmapRendered*) = 0;
    // ChmModel //
    // tell the UI to move focus back to the main window
    // (if always == false, then focus is only moved if it's inside
    // an HtmlWindow and thus outside the reach of the main UI)
    virtual void FocusFrame(bool always) = 0;
    // tell the UI to let the user save the provided data to a file
    virtual void SaveDownload(Str url, Str) = 0;
    // MarkdownModel //
    // in-page find result from the webview: search generation, 1-based
    // current match and total match count on the current page
    virtual void FindResultReceived(int gen, int current, int total) = 0;
    // all-pages find result from the webview (raw 'mdfindall' payload)
    virtual void FindAllResultReceived(Str payload) = 0;
};

struct DocController {
    DocControllerCallback* cb;

    explicit DocController(DocControllerCallback* cb) : cb(cb) { ReportIf(!cb); }
    virtual ~DocController() = default;

    // meta data
    virtual Str GetFilePath() const = 0;
    virtual Str GetDefaultFileExt() const = 0;
    virtual int PageCount() const = 0;
    virtual TempStr GetPropertyTemp(DocProp prop) = 0;

    // page navigation (stateful)
    virtual int CurrentPageNo() const = 0;
    virtual void GoToPage(int pageNo, bool addNavPoint) = 0;
    virtual bool CanNavigate(int dir) const = 0;
    virtual void Navigate(int dir) = 0;

    // view settings
    virtual void SetDisplayMode(DisplayMode mode, bool keepContinuous = false) = 0;
    virtual DisplayMode GetDisplayMode() const = 0;
    virtual void SetInPresentation(bool enable) = 0;
    virtual void SetZoomVirtual(float zoom, Point* fixPt) = 0;
    virtual float GetZoomVirtual(bool absolute = false) const = 0;
    virtual float GetNextZoomStep(float towards) const = 0;
    virtual void SetViewPortSize(Size size) = 0;

    // table of contents
    bool HasToc() {
        auto* tree = GetToc();
        return tree != nullptr;
    }
    virtual TocTree* GetToc() = 0;
    virtual void ScrollTo(int pageNo, RectF rect, float zoom) = 0;

    virtual IPageDestination* GetNamedDest(Str name) = 0;

    // get display state (pageNo, zoom, scroll etc. of the document)
    virtual void GetDisplayState(FileState* ds) = 0;
    // asynchronously calls saveThumbnail (fails silently)
    virtual void CreateThumbnail(Size size, const OnBitmapRendered* saveThumbnail) = 0;

    // page labels (optional)
    virtual bool HasPageLabels() const { return false; }
    virtual TempStr GetPageLabeTemp(int pageNo) const { return fmt("%d", pageNo); }
    virtual int GetPageByLabel(Str label) const { return ParseInt(label); }

    // common shortcuts
    virtual bool ValidPageNo(int pageNo) const { return 1 <= pageNo && pageNo <= PageCount(); }
    virtual bool GoToNextPage() {
        if (CurrentPageNo() == PageCount()) {
            return false;
        }
        GoToPage(CurrentPageNo() + 1, false);
        return true;
    }
    virtual bool GoToPrevPage(__unused bool toBottom = false) {
        if (CurrentPageNo() == 1) {
            return false;
        }
        GoToPage(CurrentPageNo() - 1, false);
        return true;
    }
    virtual bool GoToFirstPage() {
        if (CurrentPageNo() == 1) {
            return false;
        }
        GoToPage(1, true);
        return true;
    }
    virtual bool GoToLastPage() {
        if (CurrentPageNo() == PageCount()) {
            return false;
        }
        GoToPage(PageCount(), true);
        return true;
    }

    virtual bool HandleLink(IPageDestination*, ILinkHandler*) {
        // TODO: over-ride in ChmModel
        return false;
    }

    // for quick type determination and type-safe casting
    virtual DisplayModel* AsFixed() { return nullptr; }
    virtual ChmModel* AsChm() { return nullptr; }
    virtual MarkdownModel* AsMarkdown() { return nullptr; }
};

inline bool IsBrowserDocController(DocController* ctrl) {
    return ctrl && (ctrl->AsChm() || ctrl->AsMarkdown());
}
