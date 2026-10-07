/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct BrowserView;
enum class FileType : u8;
struct BrowserViewCallback;
struct MainWindow;
struct MarkdownLaunchTask;
struct MarkdownTocBuildTask;

struct MarkdownModel : BrowserDocController {
    explicit MarkdownModel(DocControllerCallback* cb);
    ~MarkdownModel() override;

    Str GetFilePath() const override;
    Str GetDefaultFileExt() const override;
    TempStr GetPropertyTemp(DocProp prop) override;

    void GoToPage(int pageNo, bool addNavPoint) override;

    TocTree* GetToc() override;
    IPageDestination* GetNamedDest(Str name) override;

    void CreateThumbnail(Size size, const OnBitmapRendered* saveThumbnail) override;

    MarkdownModel* AsMarkdown() override;

    static MarkdownModel* Create(Str fileName, DocControllerCallback* cb = nullptr);
    static bool IsSupportedFileType(FileType);
    // a subset of IsSupportedFileType: .html/.htm rendered raw in the browser view
    static bool IsHtmlFileType(FileType);

    void FindAllPages(Str term, bool matchCase, bool wholeWord, int gen) override;

    bool OnBeforeNavigate(Str url, bool newWindow);
    void OnDocumentComplete(Str url);
    Str GetDataForUrl(Str url);

    Str fileName;
    Str baseDir;
    // true when displaying .html/.htm files: they are served to the browser raw
    // instead of being rendered from markdown, and the sibling TOC scans .html
    bool isHtml = false;
    TocTree* tocTree = nullptr;
    // set while the full TOC (file headings included) is built in the background
    MarkdownTocBuildTask* tocBuildTask = nullptr;
    // set while opening a document a link points at is queued on the UI thread
    MarkdownLaunchTask* launchTask = nullptr;
    Arena* poolAlloc = nullptr;

    bool Load(Str fileName);
    void SetToc(TocTree*);
    bool DisplayPage(Str pageUrl) override;

    TempStr NormalizeScrollUrlTemp(Str url) const override;
    TempStr ScrollUrlForPageTemp(int pageNo) const override;
    BrowserViewCallback* CreateBrowserCallback() override;
    Str BrowserVirtualHost() const override;

    TempStr FileToVirtualUrlTemp(Str filePath) const;
    TempStr VirtualUrlToFileTemp(Str url) const;
    TempStr LinkedDocPathTemp(Str url) const;
    bool OpenLinkedDocument(Str url) override;
};
