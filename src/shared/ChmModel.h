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
    bool HandleLink(IPageDestination*, ILinkHandler*) override;

    IPageDestination* GetNamedDest(Str name) override;

    void CreateThumbnail(Size size, const OnBitmapRendered* saveThumbnail) override;

    ChmModel* AsChm() override;

    static ChmModel* Create(Str fileName, DocControllerCallback* cb = nullptr);

    void FindAllPages(Str term, bool matchCase, bool wholeWord, int gen) override;

    bool OnBeforeNavigate(Str url, bool newWindow);
    void OnDocumentComplete(Str url);
    Str GetDataForUrl(Str url);
    void UpdateTheme();

    static bool IsSupportedFileType(FileType);

    Str fileName;
    ChmFile* doc = nullptr;
    TocTree* tocTree = nullptr;
    Mutex docAccess;
    Vec<ChmTocTraceItem>* tocTrace = nullptr;

    Vec<ChmCacheEntry*> urlDataCache;
    // arena for strings that aren't freed until this ChmModel is deleted
    // (e.g. for titles and URLs for ChmTocItem and ChmCacheEntry)
    Arena* poolAlloc = nullptr;

    bool Load(Str fileName);
    bool DisplayPage(Str pageUrl) override;

    ChmCacheEntry* FindDataForUrl(Str url) const;

    TempStr NormalizeScrollUrlTemp(Str url) const override;
    TempStr ScrollUrlForPageTemp(int pageNo) const override;
    BrowserViewCallback* CreateBrowserCallback() override;
};
