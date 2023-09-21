/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct DocController;
struct ChmModel;
struct DisplayModel;
struct IPageElement;
struct IPageDestination;
struct TocTree;
struct TocItem;
struct MainWindow;
struct FileState;
enum class DisplayMode;

using onBitmapRenderedCb = std::function<void(RenderedBitmap*)>;

// TODO: "format", "encryption", "info::Keywords" as in fz_lookup_metadata
enum class DocumentProperty {
    Title,
    Author,
    Copyright,
    Subject,
    CreationDate,
    ModificationDate,
    CreatorApp,
    UnsupportedFeatures,
    FontList,
    PdfVersion,
    PdfProducer,
    PdfFileStructure,
};

struct ILinkHandler {
    virtual ~ILinkHandler(){};
    virtual DocController* GetDocController() = 0;
    virtual void GotoLink(IPageDestination*) = 0;
    virtual void GotoNamedDest(const char*) = 0;
    virtual void ScrollTo(IPageDestination*) = 0;
    virtual void LaunchURL(const char*) = 0;
    virtual void LaunchFile(const char* path, IPageDestination*) = 0;
    virtual IPageDestination* FindTocItem(TocItem* item, const char* name, bool partially) = 0;
};

struct DocControllerCallback {
    virtual ~DocControllerCallback() = default;
    // tell the UI to show the pageNo as current page (which also syncs
    // the toc with the curent page). Needed for when a page change happens
    // indirectly or is initiated from within the model
    virtual void PageNoChanged(DocController* ctrl, int pageNo) = 0;
    // tell the UI to open the linked document or URL
    virtual void GotoLink(IPageDestination*) = 0;
    // DisplayModel //
    virtual void Repaint() = 0;
    virtual void UpdateScrollbars(Size canvas) = 0;
    virtual void RequestRendering(int pageNo) = 0;
    virtual void CleanUp(DisplayModel* dm) = 0;
    virtual void RenderThumbnail(DisplayModel* dm, Size size, const onBitmapRenderedCb&) = 0;
    // ChmModel //
    // tell the UI to move focus back to the main window
    // (if always == false, then focus is only moved if it's inside
    // an HtmlWindow and thus outside the reach of the main UI)
    virtual void FocusFrame(bool always) = 0;
    // tell the UI to let the user save the provided data to a file
    virtual void SaveDownload(const char* url, const ByteSlice&) = 0;
};

struct DocController {
    DocControllerCallback* cb;

    explicit DocController(DocControllerCallback* cb) : cb(cb) {
        CrashIf(!cb);
    }
    virtual ~DocController() = default;

    // meta data
    virtual const char* GetFilePath() const = 0;
    virtual const char* GetDefaultFileExt() const = 0;
    virtual int PageCount() const = 0;
    virtual char* GetProperty(DocumentProperty prop) = 0;

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

    virtual IPageDestination* GetNamedDest(const char* name) = 0;

    // get display state (pageNo, zoom, scroll etc. of the document)
    virtual void GetDisplayState(FileState* ds) = 0;
    // asynchronously calls saveThumbnail (fails silently)
    virtual void CreateThumbnail(Size size, const std::function<void(RenderedBitmap*)>& saveThumbnail) = 0;

    // page labels (optional)
    virtual bool HasPageLabels() const {
        return false;
    }
    virtual char* GetPageLabel(int pageNo) const {
        return str::Format("%d", pageNo);
    }
    virtual int GetPageByLabel(const char* label) const {
        return atoi(label);
    }

    // common shortcuts
    virtual bool ValidPageNo(int pageNo) const {
        return 1 <= pageNo && pageNo <= PageCount();
    }
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
    virtual DisplayModel* AsFixed() {
        return nullptr;
    }
    virtual ChmModel* AsChm() {
        return nullptr;
    }
};
