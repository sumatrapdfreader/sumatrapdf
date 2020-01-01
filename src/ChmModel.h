/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

class ChmDoc;
struct ChmTocTraceItem;
class HtmlWindow;
class HtmlWindowCallback;
class ChmCacheEntry;

class ChmModel : public Controller {
  public:
    explicit ChmModel(ControllerCallback* cb);
    ~ChmModel() override;

    // meta data
    const WCHAR* FilePath() const override {
        return fileName;
    }
    const WCHAR* DefaultFileExt() const override {
        return L".chm";
    }
    int PageCount() const override;
    WCHAR* GetProperty(DocumentProperty prop) override;

    // page navigation (stateful)
    int CurrentPageNo() const override {
        return currentPageNo;
    }
    void GoToPage(int pageNo, bool addNavPoint) override {
        UNUSED(addNavPoint);
        CrashIf(!ValidPageNo(pageNo));
        if (ValidPageNo(pageNo))
            DisplayPage(pages.at(pageNo - 1));
    }
    bool CanNavigate(int dir) const override;
    void Navigate(int dir) override;

    // view settings
    void SetDisplayMode(DisplayMode mode, bool keepContinuous = false) override {
        UNUSED(mode);
        UNUSED(keepContinuous); /* not supported */
    }
    DisplayMode GetDisplayMode() const override {
        return DM_SINGLE_PAGE;
    }
    void SetPresentationMode(bool enable) override {
        UNUSED(enable); /* not supported */
    }
    void SetZoomVirtual(float zoom, PointI* fixPt) override;
    float GetZoomVirtual(bool absolute = false) const override;
    float GetNextZoomStep(float towards) const override;
    void SetViewPortSize(SizeI size) override {
        UNUSED(size); /* not needed(?) */
    }

    // table of contents
    DocTocTree* GetTocTree() override;
    void ScrollToLink(PageDestination* dest) override;
    PageDestination* GetNamedDest(const WCHAR* name) override;

    void GetDisplayState(DisplayState* ds) override;
    // asynchronously calls saveThumbnail (fails silently)
    void CreateThumbnail(SizeI size, const onBitmapRenderedCb& saveThumbnail) override;

    // for quick type determination and type-safe casting
    ChmModel* AsChm() override {
        return this;
    }

    static bool IsSupportedFile(const WCHAR* fileName, bool sniff = false);
    static ChmModel* Create(const WCHAR* fileName, ControllerCallback* cb = nullptr);

  public:
    // the following is specific to ChmModel

    bool SetParentHwnd(HWND hwnd);
    void RemoveParentHwnd();

    void PrintCurrentPage(bool showUI);
    void FindInCurrentPage();
    void SelectAll();
    void CopySelection();
    LRESULT PassUIMsg(UINT msg, WPARAM wParam, LPARAM lParam);

    // for HtmlWindowCallback (called through htmlWindowCb)
    bool OnBeforeNavigate(const WCHAR* url, bool newWindow);
    void OnDocumentComplete(const WCHAR* url);
    void OnLButtonDown();
    std::string_view GetDataForUrl(const WCHAR* url);
    void DownloadData(const WCHAR* url, std::string_view data);

  protected:
    AutoFreeWstr fileName;
    ChmDoc* doc = nullptr;
    DocTocTree* tocTree = nullptr;
    CRITICAL_SECTION docAccess;
    Vec<ChmTocTraceItem>* tocTrace = nullptr;

    WStrList pages;
    int currentPageNo = 1;
    HtmlWindow* htmlWindow = nullptr;
    HtmlWindowCallback* htmlWindowCb = nullptr;
    float initZoom = INVALID_ZOOM;

    Vec<ChmCacheEntry*> urlDataCache;
    // use a pool allocator for strings that aren't freed until this ChmModel
    // is deleted (e.g. for titles and URLs for ChmTocItem and ChmCacheEntry)
    PoolAllocator poolAlloc;

    bool Load(const WCHAR* fileName);
    void DisplayPage(const WCHAR* pageUrl);

    ChmCacheEntry* FindDataForUrl(const WCHAR* url);

    void ZoomTo(float zoomLevel);
};
