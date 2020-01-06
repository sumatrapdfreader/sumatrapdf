/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

class Controller;
class ChmModel;
class DisplayModel;
class EbookController;
struct EbookFormattingData;

typedef std::function<void(RenderedBitmap*)> onBitmapRenderedCb;

class ControllerCallback {
  public:
    virtual ~ControllerCallback() {
    }
    // tell the UI to show the pageNo as current page (which also syncs
    // the toc with the curent page). Needed for when a page change happens
    // indirectly or is initiated from within the model
    virtual void PageNoChanged(Controller* ctrl, int pageNo) = 0;
    // tell the UI to open the linked document or URL
    virtual void GotoLink(PageDestination* dest) = 0;
    // DisplayModel //
    virtual void Repaint() = 0;
    virtual void UpdateScrollbars(SizeI canvas) = 0;
    virtual void RequestRendering(int pageNo) = 0;
    virtual void CleanUp(DisplayModel* dm) = 0;
    virtual void RenderThumbnail(DisplayModel* dm, SizeI size, const onBitmapRenderedCb&) = 0;
    // ChmModel //
    // tell the UI to move focus back to the main window
    // (if always == false, then focus is only moved if it's inside
    // an HtmlWindow and thus outside the reach of the main UI)
    virtual void FocusFrame(bool always) = 0;
    // tell the UI to let the user save the provided data to a file
    virtual void SaveDownload(const WCHAR* url, std::string_view data) = 0;
    // EbookController //
    virtual void HandleLayoutedPages(EbookController* ctrl, EbookFormattingData* data) = 0;
    virtual void RequestDelayedLayout(int delay) = 0;
};

class Controller {
  protected:
    ControllerCallback* cb;

  public:
    explicit Controller(ControllerCallback* cb) : cb(cb) {
        CrashIf(!cb);
    }
    virtual ~Controller() {
    }

    // meta data
    virtual const WCHAR* FilePath() const = 0;
    virtual const WCHAR* DefaultFileExt() const = 0;
    virtual int PageCount() const = 0;
    virtual WCHAR* GetProperty(DocumentProperty prop) = 0;

    // page navigation (stateful)
    virtual int CurrentPageNo() const = 0;
    virtual void GoToPage(int pageNo, bool addNavPoint) = 0;
    virtual bool CanNavigate(int dir) const = 0;
    virtual void Navigate(int dir) = 0;

    // view settings
    virtual void SetDisplayMode(DisplayMode mode, bool keepContinuous = false) = 0;
    virtual DisplayMode GetDisplayMode() const = 0;
    virtual void SetPresentationMode(bool enable) = 0;
    virtual void SetZoomVirtual(float zoom, PointI* fixPt) = 0;
    virtual float GetZoomVirtual(bool absolute = false) const = 0;
    virtual float GetNextZoomStep(float towards) const = 0;
    virtual void SetViewPortSize(SizeI size) = 0;

    // table of contents
    bool HacToc() {
        auto* tree = GetToc();
        return tree != nullptr;
    }
    virtual TocTree* GetToc() = 0;
    virtual void ScrollToLink(PageDestination* dest) = 0;
    virtual PageDestination* GetNamedDest(const WCHAR* name) = 0;

    // get display state (pageNo, zoom, scroll etc. of the document)
    virtual void GetDisplayState(DisplayState* ds) = 0;
    // asynchronously calls saveThumbnail (fails silently)
    virtual void CreateThumbnail(SizeI size, const std::function<void(RenderedBitmap*)>& saveThumbnail) = 0;

    // page labels (optional)
    virtual bool HasPageLabels() const {
        return false;
    }
    virtual WCHAR* GetPageLabel(int pageNo) const {
        return str::Format(L"%d", pageNo);
    }
    virtual int GetPageByLabel(const WCHAR* label) const {
        return _wtoi(label);
    }

    // common shortcuts
    virtual bool ValidPageNo(int pageNo) const {
        return 1 <= pageNo && pageNo <= PageCount();
    }
    virtual bool GoToNextPage() {
        if (CurrentPageNo() == PageCount())
            return false;
        GoToPage(CurrentPageNo() + 1, false);
        return true;
    }
    virtual bool GoToPrevPage(bool toBottom = false) {
        UNUSED(toBottom);
        if (CurrentPageNo() == 1)
            return false;
        GoToPage(CurrentPageNo() - 1, false);
        return true;
    }
    virtual bool GoToFirstPage() {
        if (CurrentPageNo() == 1)
            return false;
        GoToPage(1, true);
        return true;
    }
    virtual bool GoToLastPage() {
        if (CurrentPageNo() == PageCount())
            return false;
        GoToPage(PageCount(), true);
        return true;
    }

    // for quick type determination and type-safe casting
    virtual DisplayModel* AsFixed() {
        return nullptr;
    }
    virtual ChmModel* AsChm() {
        return nullptr;
    }
    virtual EbookController* AsEbook() {
        return nullptr;
    }
};
