/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct ChmFile;
enum class FileType : u8;
struct BrowserView;
struct BrowserViewCallback;
struct MainWindow;

struct ChmModel : BrowserDocController {
    explicit ChmModel(DocControllerCallback* cb);
    ~ChmModel() override;

    Str GetFilePath() const override;
    Str GetDefaultFileExt() const override;
    TempStr GetPropertyTemp(DocProp prop) override;

    void GoToPage(int pageNo, bool addNavPoint) override;

    TocTree* GetToc() override;

    IPageDestination* GetNamedDest(Str name) override;

    void CreateThumbnail(Size size, const OnBitmapRendered* saveThumbnail) override;

    ChmModel* AsChm() override;

    static ChmModel* Create(Str fileName, DocControllerCallback* cb = nullptr);

    void FindAllPages(Str term, bool matchCase, bool wholeWord, int gen) override;

    bool OnBeforeNavigate(Str url, bool newWindow);
    void OnDocumentComplete(Str url);
    Str GetDataForUrl(Str url);

    static bool IsSupportedFileType(FileType);

    Str fileName;
    ChmFile* doc = nullptr;
    TocTree* tocTree = nullptr;
    Vec<BrowserTocTraceItem>* tocTrace = nullptr;
    // arena for strings that aren't freed until this ChmModel is deleted
    // (e.g. TOC titles and URLs)
    Arena* poolAlloc = nullptr;

    bool Load(Str fileName);
    bool DisplayPage(Str pageUrl) override;

    TempStr NormalizeScrollUrlTemp(Str url) const override;
    TempStr ScrollUrlForPageTemp(int pageNo) const override;
    BrowserViewCallback* CreateBrowserCallback() override;
};
