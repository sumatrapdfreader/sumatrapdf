/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

class BrowserDocView;
enum class FileType : u8;
struct HtmlWindowCallback;
struct MarkdownCacheEntry;

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

    void GetDisplayState(FileState* ds) override;
    void CreateThumbnail(Size size, const OnBitmapRendered* saveThumbnail) override;

    MarkdownModel* AsMarkdown() override;

    static MarkdownModel* Create(Str fileName, DocControllerCallback* cb = nullptr);
    static bool IsSupportedFileType(FileType);

    bool SetParentHwnd(HWND hwnd);
    void RemoveParentHwnd();

    void PrintCurrentPage(bool showUI) const;
    void FindInCurrentPage() const;
    bool CanFindInPage() const;
    void FindStart(Str term, bool matchCase, bool wholeWord, int gen) const;
    void FindAllPages(Str term, bool matchCase, bool wholeWord, int gen) const;
    void FindGoto(int idx) const;
    void GoToPageWithFind(int pageNo, Str term, bool matchCase, bool wholeWord, int idx, int gen);
    void FindClear() const;
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

    Str fileName;
    Str baseDir;
    StrVec pages;
    int currentPageNo = 1;
    Str currentPageUrl;
    BrowserDocView* docView = nullptr;
    HtmlWindowCallback* htmlWindowCb = nullptr;
    TocTree* tocTree = nullptr;
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
};