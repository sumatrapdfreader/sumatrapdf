/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct SelectionOnPage;
struct WatchedFile;
struct EditAnnotationsWindow;
struct MainWindow;
struct StrBuilder;

/* Data related to a single document loaded into a tab/window */
/* (none of these depend on MainWindow, so that a WindowTab could
   be moved between windows once this is supported) */
struct WindowTab {
    enum class Type {
        None,
        About,
        Document,
    };
    Type type = Type::None;
    const char* filePath = nullptr;
    const char* displayName = nullptr;
    MainWindow* win = nullptr;
    DocController* ctrl = nullptr;
    // text of win->hwndFrame when the tab is selected
    const char* frameTitle = nullptr;
    // state of the table of contents
    bool showToc = false;
    bool showTocPresentation = false;
    // an array of ids for ToC items that have been expanded/collapsed by user
    Vec<int> tocState;
    // canvas dimensions when the document was last visible
    Rect canvasRc;
    // whether to auto-reload the document when the tab is selected
    bool reloadOnFocus = false;
    // FileWatcher token for unsubscribing
    WatchedFile* watcher = nullptr;
    // list of rectangles of the last rectangular, text or image selection
    // (split by page, in user coordinates)
    Vec<SelectionOnPage>* selectionOnPage = nullptr;
    // previous View settings, needed when unchecking the Fit Width/Page toolbar buttons
    float prevZoomVirtual{kInvalidZoom};
    DisplayMode prevDisplayMode{DisplayMode::Automatic};
    TocTree* currToc = nullptr; // not owned by us
    EditAnnotationsWindow* editAnnotsWindow = nullptr;
    Rect lastEditAnnotsWindowPos = {};

    // TODO: terrible hack
    bool askedToSaveAnnotations = false;

    TabState* tabState = nullptr; // when lazy loading

    Annotation* selectedAnnotation = nullptr;
    bool didScrollToSelectedAnnotation = false; // only automatically scroll once

    bool hideAnnotations = false;

    HWND hwndPDFInfo = nullptr;
    HWND hwndPDFOutline = nullptr;

    // per-document background color from FileState; kColorUnset = use default
    COLORREF bgColor = kColorUnset;
    // true if per-document background is explicitly set to checkered pattern
    bool bgColorCheckered = false;
    // per-document tab color from FileState; kColorUnset = use default
    COLORREF tabColor = kColorUnset;

    // TODO: arguably a hack
    bool ignoreNextAutoReload = false;

    // Claude Code session for this tab
    char* claudeSessionId = nullptr;
    StrBuilder* claudeChatLog = nullptr;
    HANDLE claudeProcess = nullptr;

    // Grok Build session for this tab
    char* grokSessionId = nullptr;
    StrBuilder* grokChatLog = nullptr;
    HANDLE grokProcess = nullptr;

    // OpenAI Codex session for this tab
    char* codexSessionId = nullptr;
    StrBuilder* codexChatLog = nullptr;
    HANDLE codexProcess = nullptr;

    // read aloud: cleaned text that was being read and the utf8 offset
    // within it where the user stopped reading; enables "Continue reading"
    // (reset when the document is closed or reloaded)
    char* readAloudText = nullptr;
    int readAloudResumePos = -1;

    WindowTab(MainWindow* win);
    ~WindowTab();

    bool IsAboutTab() const;

    DisplayModel* AsFixed() const;

    void SetFilePath(const char* path);
    void SetDisplayName(const char* name);

    // only if AsFixed()
    EngineBase* GetEngine() const;
    Kind GetEngineType() const;

    ChmModel* AsChm() const;

    const char* GetTabTitle() const;
    bool IsDocLoaded() const;
    void MoveDocBy(int dx, int dy) const;
    void ToggleZoom() const;
};

bool SaveDataToFile(HWND hwndParent, char* fileName, ByteSlice data);
