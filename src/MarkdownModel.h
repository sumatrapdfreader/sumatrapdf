/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

class BrowserDocView;
enum class FileType : u8;
struct HtmlWindowCallback;
struct MarkdownCacheEntry;
struct MarkdownLaunchTask;
struct MarkdownTocBuildTask;

struct MarkdownModel : DocController {
    explicit MarkdownModel(DocControllerCallback* cb);
    ~MarkdownModel() override;

    Str GetFilePath() const override;
    Str GetDefaultFileExt() const override;
    int PageCount() const override;
    TempStr GetPropertyTemp(DocProp prop) override;

    int CurrentPageNo() const override;
    void GoToPage(int pageNo, bool addNavPoint) override;
    bool CanNavigate(int dir) const override;
    void Navigate(int dir) override;

    void SetDisplayMode(DisplayMode mode, bool keepContinuous = false) override;
    DisplayMode GetDisplayMode() const override;
    void SetInPresentation(bool) override;
    void SetZoomVirtual(float zoom, Point* fixPt) override;
    float GetZoomVirtual(bool absolute = false) const override;
    float GetNextZoomStep(float towards) const override;
    void SetViewPortSize(Size size) override;

    TocTree* GetToc() override;
    void ScrollTo(int pageNo, RectF rect, float zoom) override;

    bool HandleLink(IPageDestination*, ILinkHandler*) override;
    IPageDestination* GetNamedDest(Str name) override;

    void GetDisplayState(FileState* fs) override;
    void CreateThumbnail(Size size, const OnBitmapRendered* saveThumbnail) override;

    MarkdownModel* AsMarkdown() override;

    static MarkdownModel* Create(Str fileName, DocControllerCallback* cb = nullptr);
    static bool IsSupportedFileType(FileType);
    // a subset of IsSupportedFileType: .html/.htm rendered raw in the browser view
    static bool IsHtmlFileType(FileType);

    bool SetParentHwnd(HWND hwnd);
    // hide for tab switch (keep WebView2 for fast re-show)
    void RemoveParentHwnd();
    // full teardown (tab/window close); DestroyWindow can pump messages
    void DestroyParentHwnd();

    void PrintCurrentPage(bool showUI) const;
    void FindInCurrentPage() const;
    bool CanFindInPage() const override;
    void FindStart(Str term, bool matchCase, bool wholeWord, int gen) override;
    void FindAllPages(Str term, bool matchCase, bool wholeWord, int gen) override;
    void FindGoto(int idx) override;
    void GoToPageWithFind(int pageNo, Str term, bool matchCase, bool wholeWord, int idx, int gen) override;
    void FindClear() override;
    void OnFindResult(int gen, int current, int total);
    void OnFindAllResult(Str payload);
    void SelectAll() const;
    void CopySelection() const;
    LRESULT PassUIMsg(UINT msg, WPARAM wp, LPARAM lp) const;

    bool OnBeforeNavigate(Str url, bool newWindow);
    void OnDocumentComplete(Str url);
    void OnLButtonDown();
    Str GetDataForUrl(Str url);
    void DownloadData(Str url, Str data);
    void UpdateTheme();

    Str fileName;
    Str baseDir;
    // true when displaying .html/.htm files: they are served to the browser raw
    // instead of being rendered from markdown, and the sibling TOC scans .html
    bool isHtml = false;
    StrVec pages;
    int currentPageNo = 1;
    Str currentPageUrl;
    BrowserDocView* docView = nullptr;
    HtmlWindowCallback* htmlWindowCb = nullptr;
    TocTree* tocTree = nullptr;
    // set while the full TOC (file headings included) is built in the background
    MarkdownTocBuildTask* tocBuildTask = nullptr;
    // set while opening a document a link points at is queued on the UI thread
    MarkdownLaunchTask* launchTask = nullptr;
    Mutex docAccess;
    float initZoom = kInvalidZoom;
    float zoomVirtual = 100.0f;
    PointF htmlScrollPos = PointF(-1, -1);
    bool restoreHtmlScrollPos = false;
    bool skipNextBeforeNavigateScrollSave = false;
    // pending in-page find to run when the next page finishes loading (set by
    // GoToPageWithFind when jumping to a match on another page)
    Str pendingFindTerm; // owned
    bool pendingFindMatchCase = false;
    bool pendingFindWholeWord = false;
    int pendingFindIdx = -1;
    int pendingFindGen = 0;
    bool hasPendingFind = false;
    StrVec htmlScrollUrls;
    Vec<PointF> htmlScrollPositions;
    Vec<MarkdownCacheEntry*> urlDataCache;
    Arena* poolAlloc = nullptr;

    bool Load(Str fileName);
    void SetToc(TocTree*);
    bool DisplayPage(Str pageUrl);

    MarkdownCacheEntry* FindDataForUrl(Str url) const;

    void SaveHtmlScrollPos();
    void SaveHtmlScrollPosForPage(int pageNo);
    void SaveHtmlScrollPosForUrl(Str url, PointF pos);
    bool GetSavedHtmlScrollPosForPage(int pageNo, PointF* pos) const;
    bool GetSavedHtmlScrollPosForUrl(Str url, PointF* pos) const;
    void RestoreHtmlScrollPos();
    void ZoomTo(float zoomLevel) const;

    TempStr FileToVirtualUrlTemp(Str filePath) const;
    TempStr VirtualUrlToFileTemp(Str url) const;
    TempStr LinkedDocPathTemp(Str url) const;
    bool MaybeLaunchLinkedDoc(Str url);
};