/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct SelectionOnPage;
struct MainWindow;
struct DocController;
struct DisplayModel;
struct ChmModel;
struct MarkdownModel;
class EngineBase;
struct TocTree;
struct TabState;
struct Annotation;
struct WatchedFile;
struct ReadAloudHighlightMap;

// ng: orig's WindowTab, minus the state of features this port has not reached:
// the PDF info / outline debug windows (step 13). They come back with the
// feature that uses them.

// per-tab state of one AI chat provider (see AIChatPanel.cpp)
// ng: orig's `HANDLE process` is a void*, so the header stays portable
struct AIChatTabState {
    Str sessionId;
    str::Builder chatLog;
    void* process = nullptr;
};

struct AutoScroll {
    bool on = false;
    bool paused = false;
    bool atEnd = false;
    int dir = 1;
    float accum = 0;
    i64 lastQpc = 0;
};

struct ReadingBarTab {
    bool on = false;
    float yFrac = 0.40f;
};

enum class SidebarContent {
    Bookmarks,
    Thumbnails,
    Favorites,
};

/* Data related to a single document loaded into a tab/window */
/* (none of these depend on MainWindow, so that a WindowTab could
   be moved between windows once this is supported) */
struct WindowTab {
    enum class LoadState {
        None,
        Loading,
        LoadedPending,
        Error,
    };
    enum class Type {
        None,
        About,
        Document,
        Favorites, // full-window favorites list (CmdFavoriteShowInTab)
    };
    MainWindow* win = nullptr;
    DocController* ctrl = nullptr;
    u64 loadStartedAt = 0;
    // list of rectangles of the last rectangular, text or image selection
    // (split by page, in user coordinates)
    Vec<SelectionOnPage>* selectionOnPage = nullptr;
    TocTree* currToc = nullptr;   // not owned by us
    TabState* tabState = nullptr; // when lazy loading
    Annotation* selectedAnnotation = nullptr;
    ReadAloudHighlightMap* readAloudHighlight = nullptr;
    // reload-on-change (ReloadModifiedDocuments); orig's WindowTab::watcher
    WatchedFile* watcher = nullptr;
    // the watcher fired: reload when the tab is shown, after the file went
    // quiet (orig's reloadOnFocus + the kAutoReloadTimerID timer)
    bool reloadOnFocus = false;
    // skip the next watcher event (we wrote the file ourselves)
    bool ignoreNextAutoReload = false;
    i64 autoReloadSize = -1;
    u64 autoReloadStartMs = 0;
    FILETIME autoReloadModTime{};
    int autoReloadLeftMs = 0;
    Str filePath;
    Str displayName;
    // why the load failed, shown under the error message (owned; empty if we
    // couldn't tell)
    Str loadErrorReason;
    // a command-line search waiting for this tab's document to load
    Str pendingFindText;
    // text of the window title when the tab is selected
    Str frameTitle;
    // an array of ids for ToC items that have been expanded/collapsed by user
    Vec<int> tocState;
    Str readAloudText;
    Type type = Type::None;
    LoadState loadState = LoadState::None;
    // previous View settings, needed when unchecking the Fit Width/Page toolbar buttons
    float prevZoomVirtual{kInvalidZoom};
    DisplayMode prevDisplayMode{DisplayMode::Automatic};
    // per-document background color from FileState; kColorUnset = use default
    Color bgColor = kColorUnset;
    // per-document tab color from FileState; kColorUnset = use default
    Color tabColor = kColorUnset;

    int readAloudResumePos = -1;
    // utf8 offset in the highlight map where readAloudText[0] maps to
    int readAloudHighlightBase = 0;
    // current chunk within readAloudText
    int readAloudChunkStart = 0;
    int readAloudChunkEnd = 0;
    // next chunk submitted to TtsQueueUtf8; 0 if none
    int readAloudQueuedEnd = 0;
    enum ReadAloudScope {
        ReadAloudScopeSmart = 1,
        ReadAloudScopeViewport = 2,
        ReadAloudScopeSelection = 3,
        ReadAloudScopeCursor = 4,
    };
    // how the current read-aloud session was started (for the playback bar label)
    int readAloudScope = 0;

    // canvas dimensions when the document was last visible
    Rect canvasRc;

    // state of the table of contents
    bool showToc = false;
    bool showTocPresentation = false;
    SidebarContent sidebarContent = SidebarContent::Bookmarks;
    // opened via CmdOpenFileNoHistory: do not write File History / Windows Recent
    bool skipHistory = false;
    bool hideAnnotations = false;
    // the "unsaved annotations" prompt was already shown while closing
    bool askedToSaveAnnotations = false;
    bool didScrollToSelectedAnnotation = false; // only automatically scroll once
    bool pendingShowSelectedAnnotation = false;
    // true if per-document background is explicitly set to checkered pattern
    bool bgColorCheckered = false;
    // a page of this document has been painted at least once; until then the
    // canvas uses the theme background so a light page doesn't flash
    bool everPaintedPage = false;
    // follow the spoken word while reading; disabled when the user scrolls away
    bool readAloudAutoScroll = false;
    AutoScroll autoScroll;
    ReadingBarTab readingBar;

    // per-provider AI chat state, indexed by AIChatBackend
    AIChatTabState aiChat[4];
    // AIChatBackend value of the panel open for this tab; -1 = none
    int aiChatPanelOpen = -1;

    explicit WindowTab(MainWindow* win);
    WindowTab(const WindowTab&) = delete;
    WindowTab& operator=(const WindowTab&) = delete;
    ~WindowTab();

    bool IsAboutTab() const;
    bool IsFavoritesTab() const;
    bool IsNonDocumentTab() const;

    DisplayModel* AsFixed() const;

    void SetFilePath(Str path);
    void SetDisplayName(Str name);

    EngineBase* GetEngine() const;
    Kind GetEngineType() const;

    ChmModel* AsChm() const;
    MarkdownModel* AsMarkdown() const;

    Str GetTabTitle() const;
    bool IsDocLoaded() const;
    float NextToggleZoom() const;
    void ToggleZoom() const;
    void MoveDocBy(int dx, int dy) const;
};

bool IsPdfDoc(WindowTab* tab);
