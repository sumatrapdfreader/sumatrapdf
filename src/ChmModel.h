/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct ChmFile;
struct ChmTocTraceItem;
class HtmlWindow;
class HtmlWindowCallback;
struct ChmCacheEntry;

struct ChmModel : Controller {
    explicit ChmModel(ControllerCallback* cb);
    ~ChmModel() override;

    // meta data
    const char* GetFilePath() const override;
    const char* GetDefaultFileExt() const override;
    int PageCount() const override;
    WCHAR* GetProperty(DocumentProperty prop) override;

    // page navigation (stateful)
    int CurrentPageNo() const override;
    void GoToPage(int pageNo, bool addNavPoint) override;
    bool CanNavigate(int dir) const override;
    void Navigate(int dir) override;

    // view settings
    void SetDisplayMode(DisplayMode mode, bool keepContinuous = false) override;
    DisplayMode GetDisplayMode() const override;
    void SetPresentationMode(bool enable) override;
    void SetZoomVirtual(float zoom, Point* fixPt) override;
    float GetZoomVirtual(bool absolute = false) const override;
    float GetNextZoomStep(float towards) const override;
    void SetViewPortSize(Size size) override;

    // table of contents
    TocTree* GetToc() override;
    void ScrollTo(int pageNo, RectF rect, float zoom) override;

    bool HandleLink(IPageDestination*, ILinkHandler*) override;

    IPageDestination* GetNamedDest(const char* name) override;

    void GetDisplayState(FileState* ds) override;
    // asynchronously calls saveThumbnail (fails silently)
    void CreateThumbnail(Size size, const onBitmapRenderedCb& saveThumbnail) override;

    // for quick type determination and type-safe casting
    ChmModel* AsChm() override;

    static ChmModel* Create(const char* fileName, ControllerCallback* cb = nullptr);

    // the following is specific to ChmModel

    bool SetParentHwnd(HWND hwnd);
    void RemoveParentHwnd();

    void PrintCurrentPage(bool showUI) const;
    void FindInCurrentPage() const;
    void SelectAll() const;
    void CopySelection() const;
    LRESULT PassUIMsg(UINT msg, WPARAM wp, LPARAM lp) const;

    // for HtmlWindowCallback (called through htmlWindowCb)
    bool OnBeforeNavigate(const WCHAR* url, bool newWindow);
    void OnDocumentComplete(const WCHAR* url);
    void OnLButtonDown();
    ByteSlice GetDataForUrl(const WCHAR* url);
    void DownloadData(const char* url, ByteSlice data);

    static bool IsSupportedFileType(Kind);

    AutoFreeStr fileName;
    ChmFile* doc = nullptr;
    TocTree* tocTree = nullptr;
    CRITICAL_SECTION docAccess;
    Vec<ChmTocTraceItem>* tocTrace = nullptr;

    StrVec pages;
    int currentPageNo = 1;
    HtmlWindow* htmlWindow = nullptr;
    HtmlWindowCallback* htmlWindowCb = nullptr;
    float initZoom = kInvalidZoom;

    Vec<ChmCacheEntry*> urlDataCache;
    // use a pool allocator for strings that aren't freed until this ChmModel
    // is deleted (e.g. for titles and URLs for ChmTocItem and ChmCacheEntry)
    PoolAllocator poolAlloc;

    bool Load(const char* fileName);
    void DisplayPage(const char* pageUrl);

    ChmCacheEntry* FindDataForUrl(const char* url) const;

    void ZoomTo(float zoomLevel) const;
};
