/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct ChmFile;
enum class FileType : u8;
struct ChmTocTraceItem;
struct BrowserView;
struct BrowserViewCallback;
struct ChmCacheEntry;
struct MainWindow;

struct ChmModel : BrowserDocController {
    explicit ChmModel(DocControllerCallback* cb);
    ~ChmModel() override;

    Str GetFilePath() const override;
    Str GetDefaultFileExt() const override;
    TempStr GetPropertyTemp(DocProp prop) override;

    void GoToPage(int pageNo, bool addNavPoint) override;

    TocTree* GetToc() override;
    void ScrollTo(int pageNo, RectF rect, float zoom) override;

    bool HandleLink(IPageDestination*, ILinkHandler*) override;

    IPageDestination* GetNamedDest(Str name) override;

    void GetDisplayState(FileState* fs) override;
    void CreateThumbnail(Size size, const OnBitmapRendered* saveThumbnail) override;

    ChmModel* AsChm() override;

    static ChmModel* Create(Str fileName, DocControllerCallback* cb = nullptr);

    bool SetParentWindow(MainWindow* win, HWND hwndParent);
    void RemoveParentWindow();
    void DestroyParentWindow();

    void FindAllPages(Str term, bool matchCase, bool wholeWord, int gen) override;

    bool OnBeforeNavigate(Str url, bool newWindow);
    void OnDocumentComplete(Str url);
    void OnLButtonDown();
    Str GetDataForUrl(Str url);
    void DownloadData(Str url, Str data);
    void UpdateTheme();

    static bool IsSupportedFileType(FileType);

    Str fileName;
    ChmFile* doc = nullptr;
    TocTree* tocTree = nullptr;
    Mutex docAccess;
    Vec<ChmTocTraceItem>* tocTrace = nullptr;

    BrowserViewCallback* browserCb = nullptr;

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
};
