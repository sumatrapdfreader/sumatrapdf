/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct SelectionOnPage;
struct WatchedFile;
struct MainWindow;
struct LoadArgs;
namespace str {
struct Builder;
}
struct ReadAloudHighlightMap;
struct ThumbnailNavigationCache;

// per-tab state of one AI chat provider (see AIChatPanel.cpp)
struct AIChatTabState {
    Str sessionId;
    str::Builder chatLog;
    HANDLE process = nullptr;
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
    ThumbnailNavigationCache* thumbnailNavigationCache = nullptr;
    u64 loadStartedAt = 0;
    // network-drive copy progress while loading (-1 = not in copy phase)
    i64 loadCopyBytesCopied = -1;
    i64 loadCopyBytesTotal = 0;
    LoadArgs* pendingLoadArgs = nullptr;
    // FileWatcher token for unsubscribing
    WatchedFile* watcher = nullptr;
    // list of rectangles of the last rectangular, text or image selection
    // (split by page, in user coordinates)
    Vec<SelectionOnPage>* selectionOnPage = nullptr;
    TocTree* currToc = nullptr;   // not owned by us
    TabState* tabState = nullptr; // when lazy loading
    Annotation* selectedAnnotation = nullptr;
    HWND hwndPDFInfo = nullptr;
    HWND hwndPDFOutline = nullptr;
    // file size / mtime seen at the previous kAutoReloadTimerID tick, and when
    // the pending auto-reload was first scheduled. Used to wait out a writer
    // that is still producing the file (see kAutoReloadTimerID in Canvas.cpp)
    i64 autoReloadSize = -1;
    u64 autoReloadStartMs = 0;
    ReadAloudHighlightMap* readAloudHighlight = nullptr;
    Str filePath;
    Str displayName;
    // why the load failed, shown under the error message (owned; empty if we
    // couldn't tell). See FileLoadErrorReasonTemp()
    Str loadErrorReason;
    Str pendingFindText;
    // text of win->hwndFrame when the tab is selected
    Str frameTitle;
    // an array of ids for ToC items that have been expanded/collapsed by user
    Vec<int> tocState;
    Str readAloudText;
    // per-provider AI chat state, indexed by AIChatBackend
    // (0 = Claude, 1 = Grok, 2 = Codex, 3 = AntiGravity)
    AIChatTabState aiChat[4];
    Type type = Type::None;
    LoadState loadState = LoadState::None;
    // previous View settings, needed when unchecking the Fit Width/Page toolbar buttons
    float prevZoomVirtual{kInvalidZoom};
    DisplayMode prevDisplayMode{DisplayMode::Automatic};
    // per-document background color from FileState; kColorUnset = use default
    Color bgColor = kColorUnset;
    // per-document tab color from FileState; kColorUnset = use default
    Color tabColor = kColorUnset;

    // which AI chat sidebar is open for this tab
    // (-1 = none; 0 = Claude, 1 = Grok, 2 = Codex, 3 = AntiGravity)
    int aiChatPanelOpen = -1;
    int readAloudResumePos = -1;
    // utf8 offset in the highlight map where readAloudText[0] maps to
    int readAloudHighlightBase = 0;
    // current chunk within readAloudText (for WinRT-sized TTS segments)
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
    FILETIME autoReloadModTime{};

    // canvas dimensions when the document was last visible
    Rect canvasRc;

    // state of the table of contents
    bool showToc = false;
    bool showTocPresentation = false;
    // whether to auto-reload the document when the tab is selected
    bool reloadOnFocus = false;

    // TODO: terrible hack
    bool askedToSaveAnnotations = false;
    bool didScrollToSelectedAnnotation = false; // only automatically scroll once
    bool pendingShowSelectedAnnotation = false;
    bool hideAnnotations = false;
    // true if per-document background is explicitly set to checkered pattern
    bool bgColorCheckered = false;
    // a page of this tab has been painted from the render cache at least
    // once. Until then page placeholders paint in the theme background color
    // instead of the (possibly white) page color, so e.g. restoring a session
    // into a maximized window doesn't flash white in dark themes while the
    // first render is in flight
    bool everPaintedPage = false;
    // Skip the next file-watcher auto-reload (e.g. after we save annotations
    // and already call ReloadDocument ourselves). Consumed by kAutoReloadTimerID.
    bool ignoreNextAutoReload = false;
    // follow the spoken word while reading; disabled when the user scrolls away
    bool readAloudAutoScroll = false;

    WindowTab(MainWindow* win);
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
    void MoveDocBy(int dx, int dy) const;
    float NextToggleZoom() const;
    void ToggleZoom() const;
};

bool SaveDataToFile(HWND hwndParent, Str fileName, Str data);
