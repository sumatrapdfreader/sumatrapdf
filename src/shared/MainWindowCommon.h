/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- shared by MainWindowCommon.cpp and each app's MainWindow.cpp ---

struct LinkHandler : ILinkHandler {
    MainWindow* win = nullptr;

    explicit LinkHandler(MainWindow* w) {
        ReportIf(!w);
        win = w;
    }
    ~LinkHandler() override = default;

    void GotoLink(IPageDestination* dest) override;
    void GotoNamedDest(Str name) override;
    void GoToPage(int pageNo, bool addNavPoint) override;
    bool GoToNextPage() override;
    bool GoToPrevPage(bool toBottom = false) override;
    void ScrollTo(IPageDestination* dest) override;
    void ScrollTo(int pageNo, RectF rect, float zoom) override;
    void LaunchURL(Str uri) override;
    void LaunchFile(Str path, IPageDestination* remoteLink) override;
    TocItem* FindTocItem(TocItem* item, Str name, bool partially) override;
};

bool PathFromFileUriTemp(Str uri, TempStr* pathOut, Str* fragmentOut);
bool IsFileSupportedByContent(Str filePath);
TempStr NormalizeFuzzyTemp(Str str);
bool MatchFuzzy(Str s1, Str s2, bool partially);
