/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct SelectionOnPage;
struct WatchedFile;
struct EditAnnotationsWindow;
struct MainWindow;

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
    AutoFreeStr filePath;
    MainWindow* win = nullptr;
    DocController* ctrl = nullptr;
    // text of win->hwndFrame when the tab is selected
    AutoFreeStr frameTitle;
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

    // TODO: arguably a hack
    bool ignoreNextAutoReload = false;

    WindowTab(MainWindow* win);
    ~WindowTab();

    bool IsAboutTab() const;

    DisplayModel* AsFixed() const;

    void SetFilePath(const char* path);

    // only if AsFixed()
    EngineBase* GetEngine() const;
    Kind GetEngineType() const;

    ChmModel* AsChm() const;

    const char* GetTabTitle() const;
    const char* GetPath() const;
    bool IsDocLoaded() const;
    void MoveDocBy(int dx, int dy) const;
    void ToggleZoom() const;
};

bool SaveDataToFile(HWND hwndParent, char* fileName, ByteSlice data);
