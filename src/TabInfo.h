/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct SelectionOnPage;
struct WatchedFile;
struct EditAnnotationsWindow;
struct WindowInfo;

/* Data related to a single document loaded into a tab/window */
/* (none of these depend on WindowInfo, so that a TabInfo could
   be moved between windows once this is supported) */
struct TabInfo {
    AutoFreeWstr filePath;
    WindowInfo* win{nullptr};
    Controller* ctrl{nullptr};
    // text of win->hwndFrame when the tab is selected
    AutoFreeWstr frameTitle;
    // state of the table of contents
    bool showToc{false};
    bool showTocPresentation{false};
    // an array of ids for ToC items that have been expanded/collapsed by user
    Vec<int> tocState;
    // canvas dimensions when the document was last visible
    Rect canvasRc;
    // whether to auto-reload the document when the tab is selected
    bool reloadOnFocus{false};
    // FileWatcher token for unsubscribing
    WatchedFile* watcher{nullptr};
    // list of rectangles of the last rectangular, text or image selection
    // (split by page, in user coordinates)
    Vec<SelectionOnPage>* selectionOnPage{nullptr};
    // previous View settings, needed when unchecking the Fit Width/Page toolbar buttons
    float prevZoomVirtual{INVALID_ZOOM};
    DisplayMode prevDisplayMode{DisplayMode::Automatic};
    TocTree* currToc{nullptr}; // not owned by us
    EditAnnotationsWindow* editAnnotsWindow{nullptr};

    // TODO: terrible hack
    bool askedToSaveAnnotations{false};

    TabInfo(WindowInfo* win, const WCHAR* filePath);
    ~TabInfo();

    [[nodiscard]] DisplayModel* AsFixed() const;

    // only if AsFixed()
    [[nodiscard]] EngineBase* GetEngine() const;
    [[nodiscard]] Kind GetEngineType() const;

    [[nodiscard]] ChmModel* AsChm() const;

    [[nodiscard]] const WCHAR* GetTabTitle() const;
    [[nodiscard]] bool IsDocLoaded() const;
    void MoveDocBy(int dx, int dy) const;
    void ToggleZoom() const;
};

bool SaveDataToFile(HWND hwndParent, WCHAR* fileName, ByteSlice data);
