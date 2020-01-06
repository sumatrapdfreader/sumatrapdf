/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct DrawInstr;
struct EbookControls;
struct EbookFormattingData;
class FrameRateWnd;

class EbookController;
class EbookFormattingThread;
class HtmlFormatter;
class HtmlFormatterArgs;
class HtmlPage;

namespace mui {
class Control;
}
using namespace mui;

class EbookController : public Controller {
  public:
    EbookController(Doc doc, EbookControls* ctrls, ControllerCallback* cb);
    ~EbookController() override;

    const WCHAR* FilePath() const override {
        return doc.GetFilePath();
    }
    const WCHAR* DefaultFileExt() const override {
        return doc.GetDefaultFileExt();
    }
    int PageCount() const override {
        return GetMaxPageCount();
    }
    WCHAR* GetProperty(DocumentProperty prop) override {
        return doc.GetProperty(prop);
    }

    int CurrentPageNo() const override {
        return currPageNo;
    }
    void GoToPage(int pageNo, bool addNavPoint) override;
    bool CanNavigate(int dir) const override;
    void Navigate(int dir) override;

    void SetDisplayMode(DisplayMode mode, bool keepContinuous = false) override;
    DisplayMode GetDisplayMode() const override {
        return IsDoublePage() ? DM_FACING : DM_SINGLE_PAGE;
    }
    void SetPresentationMode(bool enable) override {
        UNUSED(enable); /* not supported */
    }
    void SetZoomVirtual(float zoom, PointI* fixPt) override {
        UNUSED(zoom);
        UNUSED(fixPt); /* not supported */
    }
    float GetZoomVirtual(bool absolute = false) const override {
        UNUSED(absolute);
        return 100;
    }
    float GetNextZoomStep(float towards) const override {
        UNUSED(towards);
        return 100;
    }
    void SetViewPortSize(SizeI size) override;

    TocTree* GetToc() override;
    void ScrollToLink(PageDestination* dest) override;
    PageDestination* GetNamedDest(const WCHAR* name) override;

    void GetDisplayState(DisplayState* ds) override;
    void CreateThumbnail(SizeI size, const onBitmapRenderedCb&) override;

    bool GoToNextPage() override;
    bool GoToPrevPage(bool toBottom = false) override;

    EbookController* AsEbook() override {
        return this;
    }

  public:
    // the following is specific to EbookController

    DocType GetDocType() const {
        return doc.Type();
    }
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam, bool& wasHandled);
    void EnableMessageHandling(bool enable) {
        handleMsgs = enable;
    }
    void UpdateDocumentColors();
    void RequestRepaint();
    void HandlePagesFromEbookLayout(EbookFormattingData* ebookLayout);
    void TriggerLayout();
    void StartLayouting(int startReparseIdxArg = -1, DisplayMode displayMode = DM_AUTOMATIC);
    int ResolvePageAnchor(const WCHAR* id);
    void CopyNavHistory(EbookController& orig);
    int CurrentTocPageNo() const;

    // call StartLayouting before using this EbookController
    static EbookController* Create(Doc doc, HWND hwnd, ControllerCallback* cb, FrameRateWnd*);

    static void DeleteEbookFormattingData(EbookFormattingData* data);

  protected:
    EbookControls* ctrls = nullptr;

    TocTree* tocTree = nullptr;
    Doc doc;

    // TODO: this should be recycled along with pages so that its
    // memory use doesn't grow without bounds
    PoolAllocator textAllocator;

    Vec<HtmlPage*>* pages = nullptr;

    // pages being sent from background formatting thread
    Vec<HtmlPage*>* incomingPages = nullptr;

    // currPageNo is in range 1..$numberOfPages.
    int currPageNo = 0;
    // reparseIdx of the current page (the first one if we're showing 2)
    int currPageReparseIdx = 0;

    // size of the page for which pages were generated
    SizeI pageSize;

    EbookFormattingThread* formattingThread = nullptr;
    int formattingThreadNo = -1;

    // whether HandleMessage passes messages on to ctrls->mainWnd
    bool handleMsgs = false;

    // parallel lists mapping anchor IDs to reparseIdxs
    WStrVec* pageAnchorIds = nullptr;
    Vec<int>* pageAnchorIdxs = nullptr;

    Vec<int> navHistory;
    size_t navHistoryIdx = 0;

    Vec<HtmlPage*>* GetPages();
    void UpdateStatus();
    bool FormattingInProgress() const {
        return formattingThread != nullptr;
    }
    void StopFormattingThread();
    void CloseCurrentDocument();
    int GetMaxPageCount() const;
    bool IsDoublePage() const;
    void ExtractPageAnchors();
    void AddNavPoint();
    void OnClickedLink(int pageNo, DrawInstr* link);

    // event handlers
    void ClickedNext(Control* c, int x, int y);
    void ClickedPrev(Control* c, int x, int y);
    void ClickedProgress(Control* c, int x, int y);
    void SizeChangedPage(Control* c, int dx, int dy);
    void ClickedPage1(Control* c, int x, int y);
    void ClickedPage2(Control* c, int x, int y);
};

HtmlFormatterArgs* CreateFormatterArgsDoc(Doc doc, int dx, int dy, Allocator* textAllocator = nullptr);
