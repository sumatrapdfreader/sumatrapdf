/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct ChmFile;
enum class FileType : u8;
struct ChmTocTraceItem;
class BrowserDocView;
struct HtmlWindowCallback;
struct ChmCacheEntry;

struct ChmModel : DocController {
    explicit ChmModel(DocControllerCallback* cb);
    ~ChmModel() override;

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
    void SetInPresentation(bool) override;
    void SetZoomVirtual(float zoom, Point* fixPt) override;
    float GetZoomVirtual(bool absolute = false) const override;
    float GetNextZoomStep(float towards) const override;
    void SetViewPortSize(Size size) override;

    // table of contents
    TocTree* GetToc() override;
    void ScrollTo(int pageNo, RectF rect, float zoom) override;

    bool HandleLink(IPageDestination*, ILinkHandler*) override;

    IPageDestination* GetNamedDest(Str name) override;

    void GetDisplayState(FileState* ds) override;
    // asynchronously calls saveThumbnail (fails silently)
    void CreateThumbnail(Size size, const OnBitmapRendered* saveThumbnail) override;

    // for quick type determination and type-safe casting
    ChmModel* AsChm() override;

    static ChmModel* Create(Str fileName, DocControllerCallback* cb = nullptr);

    // the following is specific to ChmModel

    bool SetParentHwnd(HWND hwnd);
    void RemoveParentHwnd();

    void PrintCurrentPage(bool showUI) const;
    void FindInCurrentPage() const;
    bool CanFindInPage() const override;
    void FindStart(Str term, bool matchCase, bool wholeWord, int gen) override;
    void FindAllPages(Str term, bool matchCase, bool wholeWord, int gen) override;
    void FindGoto(int idx) override;
    void GoToPageWithFind(int pageNo, Str term, bool matchCase, bool wholeWord, int idx, int gen) override;
    void FindClear() override;
    void SelectAll() const;
    void CopySelection() const;
    LRESULT PassUIMsg(UINT msg, WPARAM wp, LPARAM lp) const;

    // for HtmlWindowCallback (called through htmlWindowCb)
    bool OnBeforeNavigate(Str url, bool newWindow);
    void OnDocumentComplete(Str url);
    void OnLButtonDown();
    Str GetDataForUrl(Str url);
    void DownloadData(Str url, Str data);
    void OnFindResult(int gen, int current, int total);
    void OnFindAllResult(Str payload);

    static bool IsSupportedFileType(FileType);

    Str fileName;
    ChmFile* doc = nullptr;
    TocTree* tocTree = nullptr;
    Mutex docAccess;
    Vec<ChmTocTraceItem>* tocTrace = nullptr;

    StrVec pages;
    int currentPageNo = 1;
    // url of the currently displayed page; may be a redirect/anchor url that
    // isn't in `pages`, so it's tracked separately from currentPageNo
    Str currentPageUrl;
    BrowserDocView* docView = nullptr;
    HtmlWindowCallback* htmlWindowCb = nullptr;
    float initZoom = kInvalidZoom;
    // intended zoom level, re-applied after every document load because the
    // hosted control resets to 100% when it's recreated (e.g. on tab switch)
    float zoomVirtual = 100.0f;
    // scroll position to restore once the current document finishes loading
    PointF htmlScrollPos = PointF(-1, -1);
    bool restoreHtmlScrollPos = false;
    // set when we already saved scroll pos before a programmatic navigation,
    // so the following OnBeforeNavigate doesn't save it again for the wrong page
    bool skipNextBeforeNavigateScrollSave = false;
    // pending in-page find to run when the next page finishes loading (set by
    // GoToPageWithFind when jumping to a match on another page)
    Str pendingFindTerm; // owned
    bool pendingFindMatchCase = false;
    bool pendingFindWholeWord = false;
    int pendingFindIdx = -1;
    int pendingFindGen = 0;
    bool hasPendingFind = false;
    // per-url remembered scroll positions (parallel arrays)
    StrVec htmlScrollUrls;
    Vec<PointF> htmlScrollPositions;

    Vec<ChmCacheEntry*> urlDataCache;
    // arena for strings that aren't freed until this ChmModel is deleted
    // (e.g. for titles and URLs for ChmTocItem and ChmCacheEntry)
    Arena* poolAlloc = nullptr;

    bool Load(Str fileName);
    bool DisplayPage(Str pageUrl);

    ChmCacheEntry* FindDataForUrl(Str url) const;

    void SaveHtmlScrollPos();
    void SaveHtmlScrollPosForPage(int pageNo);
    void SaveHtmlScrollPosForUrl(Str url, PointF pos);
    bool GetSavedHtmlScrollPosForPage(int pageNo, PointF* pos) const;
    bool GetSavedHtmlScrollPosForUrl(Str url, PointF* pos) const;
    void RestoreHtmlScrollPos();
    void ZoomTo(float zoomLevel) const;
};
